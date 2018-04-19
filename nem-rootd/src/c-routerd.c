#include "nem.h"
#include "nemsvc.h"
#include "c-state.h"
#include "svc-daemon.h"
#include "svc-imghost.h"

#include <sys/capsicum.h>

typedef struct {
	int         port;
	int         fd;
	const char *proto;
	const char *cert;
	const char *key;
}
port_t;

static NEM_child_t  child;
static NEM_svcmux_t svcs;
static bool         is_running = false;
static bool         want_running = true;
static bool         shutdown_sent = false;
static port_t      *ports;
static size_t       ports_len;

static NEM_err_t routerd_start(NEM_app_t *app);

static void
routerd_restart(NEM_thunk1_t *thunk, void *varg)
{
	if (!want_running) {
		if (NEM_rootd_verbose()) {
			printf("c-routerd: nevermind, leave it dead\n");
		}
		return;
	}
	if (is_running) {
		if (NEM_rootd_verbose()) {
			printf("c-routerd: child resurrected itself?!\n");
		}
		return;
	}

	NEM_app_t *app = NEM_thunk1_ptr(thunk);

	NEM_err_t err = routerd_start(app);
	if (!NEM_err_ok(err)) {
		if (NEM_rootd_verbose()) {
			printf(
				"c-routerd: couldn't start child: %s\n",
				NEM_err_string(err)
			);
		}

		NEM_kq_after(&app->kq, 1000, NEM_thunk1_new_ptr(
			&routerd_restart,
			app
		));
	}
}

static void
on_child_died(NEM_thunk1_t *thunk, void *varg)
{
	if (is_running) {
		// NB: Dirty hack to detect that we're shutting down and that
		// this was called due to the child being freed.
		NEM_child_free(&child);
	}
	is_running = false;

	if (!want_running) {
		if (NEM_rootd_verbose()) {
			printf("c-routerd: child died? good riddance\n");
		}
		return;
	}

	const int delay_ms = 1000;

	if (NEM_rootd_verbose()) {
		printf("c-routerd: child died? restarting in %dms\n", delay_ms);
	}

	NEM_app_t *app = NEM_thunk1_ptr(thunk);
	NEM_kq_after(&app->kq, delay_ms, NEM_thunk1_new_ptr(
		&routerd_restart,
		app
	));
}

static void
routerd_enter(NEM_thunk1_t *thunk, void *varg)
{
	NEM_child_t *child = NEM_thunk1_ptr(thunk);

	int rand_fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (0 > rand_fd) {
		if (NEM_rootd_verbose()) {
			printf("open /dev/urandom: %s\n", strerror(errno));
			return;
		}
	}
	if (0 > dup2(rand_fd, NEM_KQ_PARENT_FILENO + 1)) {
		if (NEM_rootd_verbose()) {
			printf("dup2: %s\n", strerror(errno));
			return;
		}
	}

	if (!NEM_rootd_capsicum()) {
		return;
	}

	if (0 != cap_enter()) {
		if (NEM_rootd_verbose()) {
			printf("cap_enter: %s\n", strerror(errno));
			return;
		}
	}

	cap_rights_t exe_rights;
	cap_rights_init(&exe_rights, CAP_FEXECVE);
	if (0 != cap_rights_limit(child->exe_fd, &exe_rights)) {
		if (NEM_rootd_verbose()) {
			printf("cap_rights_limit: %s\n", strerror(errno));
		}
	}

	cap_rights_t rights;
	cap_rights_init(&rights,
		CAP_READ,
		CAP_WRITE,
		CAP_FCNTL,
		CAP_GETSOCKOPT,
		CAP_GETSOCKNAME,
		CAP_GETPEERNAME,
		CAP_EVENT,
		CAP_KQUEUE_EVENT
	);

	int fds[] = {
		NEM_KQ_PARENT_FILENO,
		NEM_KQ_PARENT_FILENO + 1,
		STDIN_FILENO,
		STDOUT_FILENO,
		STDERR_FILENO,
	};
	for (size_t i = 0; i < NEM_ARRSIZE(fds); i += 1) {
		int fd = fds[i];

		if (0 != cap_rights_limit(fd, &rights)) {
			if (NEM_rootd_verbose()) {
				printf("cap_rights_limit: %s\n", strerror(errno));
				return;
			}
		}
	}
}

static void
routerd_send_port_cb(NEM_thunk_t *thunk, void *varg)
{
	NEM_txn_ca *ca = varg;
	port_t *port = NEM_thunk_inlineptr(thunk);
	NEM_msghdr_t *hdr = NEM_msg_header(ca->msg);

	if (NEM_rootd_verbose()) {
		if (!NEM_err_ok(ca->err)) {
			printf(
				"c-routerd: error sending port %d: %s\n",
				port->port,
				NEM_err_string(ca->err)
			);
		}
		else if (NULL != hdr && NULL != hdr->err) {
			printf(
				"c-routerd: error sending port %d: %s\n",
				port->port,
				hdr->err->reason ? hdr->err->reason : "(no reason)"
			);
		}
		else {
			printf("c-routerd: sent port %d\n", port->port);
		}
	}

	NEM_msghdr_free(hdr);
}

static void
routerd_send_port(port_t *port)
{
	if (!is_running) {
		return;
	}

	void *req_bs;
	size_t req_len;
	NEM_svc_router_bind_t req = {
		.port     = port->port,
		.proto    = port->proto,
		.cert_pem = port->cert,
		.key_pem  = port->key,
	};
	NEM_panic_if_err(NEM_marshal_bson(
		&NEM_svc_router_bind_m,
		&req_bs,
		&req_len,
		&req,
		sizeof(req)
	));

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	msg->packed.service_id = NEM_svcid_router;
	msg->packed.command_id = NEM_cmdid_router_bind;
	NEM_msg_set_body(msg, req_bs, req_len);
	NEM_panic_if_err(NEM_msg_set_fd(msg, port->fd));

	NEM_thunk_t *thunk = NEM_thunk_new(
		&routerd_send_port_cb,
		sizeof(port_t)
	);
	*(port_t*)NEM_thunk_inlineptr(thunk) = *port;
	
	NEM_txnmgr_req1(&child.txnmgr, NULL, msg, thunk);
}

static void
routerd_send_ports()
{
	for (size_t i = 0; i < ports_len; i += 1) {
		routerd_send_port(&ports[i]);
	}
}

NEM_err_t
NEM_rootd_routerd_bind_http(int port)
{
	if (0 >= port || port >= UINT16_MAX) {
		return NEM_err_static("NEM_rootd_router_bind_http: invalid port");
	}

	int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (-1 == fd) {
		return NEM_err_errno();
	}

	struct sockaddr_in addr = {};
	addr.sin_len = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (-1 == bind(fd, (struct sockaddr*) &addr, sizeof(addr))) {
		close(fd);
		if (NEM_rootd_verbose()) {
			printf("c-routerd: unable to bind port %d\n", port);
		}
		return NEM_err_errno();
	}
	
	if (-1 == listen(fd, 1)) {
		close(fd);
		return NEM_err_errno();
	}

	port_t new_port = {
		.port  = port,
		.fd    = fd,
		.proto = "http",
	};

	ports = NEM_panic_if_null(realloc(
		ports,
		(ports_len + 1) * sizeof(port_t)
	));
	ports[ports_len] = new_port;
	ports_len += 1;

	routerd_send_port(&new_port);

	return NEM_err_none;
}

static NEM_err_t
routerd_start(NEM_app_t *app)
{
	if (is_running) {
		return NEM_err_static("routerd_start: already running?");
	}

	NEM_err_t err = NEM_child_init(
		&child,
		&app->kq,
		NEM_rootd_routerd_path(),
		// XXX: May want to bind stdout/stderr to something
		// so we can track/log output. Just leave them the same
		// as the parent stdout/stderr for now.
		NEM_thunk1_new_ptr(&routerd_enter, &child)
	);
	if (!NEM_err_ok(err)) {
		return err;
	}
	err = NEM_child_on_close(
		&child,
		NEM_thunk1_new_ptr(
			&on_child_died,
			app
		)
	);
	if (!NEM_err_ok(err)) {
		NEM_child_free(&child);
		return err;
	}

	NEM_txnmgr_set_mux(&child.txnmgr, &svcs);

	if (NEM_rootd_verbose()) {
		printf("\nc-routerd: routerd running, pid=%d\n", child.pid); 
	}

	is_running = true;
	routerd_send_ports();

	return NEM_err_none;
}

static void
on_unknown_message(NEM_thunk_t *thunk, void *varg)
{
	NEM_txn_ca *ca = varg;

	if (NEM_rootd_verbose()) {
		printf(
			"c-routerd: unhandled message %s/%s\n",
			NEM_svcid_to_string(ca->msg->packed.service_id),
			NEM_cmdid_to_string(
				ca->msg->packed.service_id,
				ca->msg->packed.command_id
			)
		);
	}

	if (NULL != ca->txnin) {
		NEM_txnin_reply_err(ca->txnin, NEM_err_static("no handler"));
	}
}

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	if (NEM_rootd_verbose()) {
		printf("c-routerd: setup\n");
	}

	NEM_svcmux_init(&svcs);
	NEM_rootd_svc_daemon_bind(&svcs);
	NEM_rootd_svc_imghost_bind(&svcs);
	NEM_svcmux_set_default(&svcs, NEM_thunk_new_ptr(
		&on_unknown_message,
		app
	));

	return routerd_start(app);
}

static void
on_shutdown_msg(NEM_thunk_t *thunk, void *varg)
{
	NEM_txn_ca *ca = varg;
	if (!NEM_err_ok(ca->err)) {
		if (NEM_rootd_verbose()) {
			printf(
				"c-routerd: error sending shutdown message: %s\n",
				NEM_err_string(ca->err)
			);
		}
	}
	else if (ca->done) {
		if (NEM_rootd_verbose()) {
			printf("c-routerd: client shutdown acknowledged\n");
		}
	}
}

static bool
try_shutdown(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-routerd: try-shutdown\n");
	}

	want_running = false;

	if (is_running && !shutdown_sent) {
		shutdown_sent = true;

		NEM_msg_t *msg = NEM_msg_new(0, 0);
		msg->packed.service_id = NEM_svcid_daemon;
		msg->packed.command_id = NEM_cmdid_daemon_stop;
		NEM_txnmgr_req1(
			&child.txnmgr,
			NULL,
			msg,
			NEM_thunk_new(&on_shutdown_msg, 0)
		);
	}

	return !is_running;
}

static void
teardown(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-routerd: teardown\n");
	}
	if (is_running) {
		if (NEM_rootd_verbose()) {
			printf("c-routerd: killing child\n");
		}
		// NB: Set is_running first to signal that the child shouldn't
		// be freed by the on_child_died.
		want_running = false;
		is_running = false;
		NEM_child_free(&child);
	}

	NEM_svcmux_unref(&svcs);
}

const NEM_app_comp_t NEM_rootd_c_routerd = {
	.name         = "c-routerd",
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};
