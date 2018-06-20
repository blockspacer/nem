#include "test.h"

static const char *bad_path = "/nonexistent";

START_TEST(err_init_no_exe)
{
	NEM_kq_t kq;
	ck_err(NEM_kq_init_root(&kq));
	NEM_child_t child;
	NEM_err_t err = NEM_child_init(&child, &kq, bad_path, NULL);
	ck_assert(!NEM_err_ok(err));
	NEM_kq_free(&kq);
}
END_TEST

static void
never_called(NEM_thunk1_t *thunk, void *varg)
{
	NEM_panic("never_called called");
}

START_TEST(err_init_no_exe_thunk)
{
	NEM_kq_t kq;
	ck_err(NEM_kq_init_root(&kq));
	NEM_child_t child;
	NEM_err_t err = NEM_child_init(
		&child,
		&kq,
		bad_path,
		NEM_thunk1_new_ptr(&never_called, NULL)
	);
	ck_assert(!NEM_err_ok(err));
	NEM_kq_free(&kq);
}
END_TEST

typedef struct {
	NEM_child_t  child;
	NEM_kq_t     kq;
	NEM_thunk_t *on_kq;
	char        *out;
	size_t       out_len;
	const char **env;
	const char **args;
	int          fds[2];
	int          done;
	int          expected_code;
	const char  *expected_out;
}
work_t;

static void
work_init(work_t *this)
{
	bzero(this, sizeof(*this));
	NEM_kq_init_root(&this->kq);
}

static void
work_done_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *this = NEM_thunk1_ptr(thunk);
	NEM_child_ca *ca = varg;
	ck_assert_int_eq(this->expected_code, ca->exitcode);
	if (NULL != this->expected_out) {
		ck_assert_ptr_ne(NULL, this->out);
		ck_assert_str_eq(this->expected_out, this->out);
	}
	this->done = 1;
	NEM_kq_stop(&this->kq);
}

static void
work_run_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *this = NEM_thunk1_ptr(thunk);
	NEM_child_ca *ca = varg;
	ck_assert_ptr_ne(NULL, ca);
	ck_assert_int_eq(STDOUT_FILENO, dup2(this->fds[1], STDOUT_FILENO));

	if (NULL != this->env) {
		ca->env = (char**) this->env;
	}
	if (NULL != this->args) {
		ca->args = (char**) this->args;
	}
}

static void
work_kq_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *this = NEM_thunk_ptr(thunk);
	struct kevent *kev = varg;
	this->out = NEM_panic_if_null(realloc(
		this->out, this->out_len + kev->data
	));
	ck_assert_int_eq(
		kev->data,
		read(this->fds[0], this->out + this->out_len, kev->data)
	);
	this->out_len += kev->data;
}

static void
work_free(work_t *this)
{
	ck_assert_int_eq(1, this->done);
	close(this->fds[0]);
	close(this->fds[1]);
	free(this->out);
	NEM_child_free(&this->child);
	NEM_kq_free(&this->kq);
}

static void
work_run(work_t *this, const char *cmd)
{
	ck_assert_int_eq(0, pipe2(this->fds, O_CLOEXEC));
	this->on_kq = NEM_thunk_new_ptr(&work_kq_cb, this);

	struct kevent kev;
	EV_SET(&kev, this->fds[0], EVFILT_READ, EV_ADD, 0, 0, this->on_kq);
	ck_assert_int_eq(0, kevent(this->kq.kq, &kev, 1, NULL, 0, NULL));

	ck_err(NEM_child_init(
		&this->child,
		&this->kq,
		cmd,
		NEM_thunk1_new_ptr(&work_run_cb, this)
	));
	ck_err(NEM_child_on_close(
		&this->child,
		NEM_thunk1_new_ptr(&work_done_cb, this)
	));

	ck_err(NEM_kq_run(&this->kq));
	work_free(this);
}

START_TEST(echo)
{
	work_t work;
	work_init(&work);
	work.expected_out = "\n";
	work_run(&work, "/bin/echo");
}
END_TEST

START_TEST(echo_hello)
{
	work_t work;
	work_init(&work);
	const char *args[] = { "/bin/echo", "hello", NULL };
	work.args = args;
	work.expected_out = "hello\n";
	work_run(&work, "/bin/echo");
}
END_TEST

START_TEST(echo_hello_world)
{
	work_t work;
	work_init(&work);
	const char *args[] = { "/bin/echo", "hello", "world", NULL };
	work.args = args;
	work.expected_out = "hello world\n";
	work_run(&work, "/bin/echo");
}
END_TEST

START_TEST(env_1)
{
	work_t work;
	work_init(&work);
	const char *args[] = { "/bin/sh", "-c", "echo $FOO", NULL };
	const char *env[] = { "FOO=bar", NULL };
	work.args = args;
	work.env = env;
	work.expected_out = "bar\n";
	work_run(&work, "/bin/sh");
}
END_TEST

START_TEST(env_2)
{
	work_t work;
	work_init(&work);
	const char *args[] = { "/bin/sh", "-c", "echo $FOO; echo $BAR", NULL };
	const char *env[] = { "FOO=bar", "BAR=baz", NULL };
	work.args = args;
	work.env = env;
	work.expected_out = "bar\nbaz\n";
	work_run(&work, "/bin/sh");
}
END_TEST

START_TEST(execute_order_66)
{
	work_t work;
	work_init(&work);
	const char *args[] = { "/bin/sh", "-c", "exit 66", NULL };
	work.args = args;
	work.expected_code = 66;
	work_run(&work, "/bin/sh");
}
END_TEST

Suite*
suite_child()
{
	tcase_t tests[] = {
		{ "err_init_no_exe",       &err_init_no_exe       },
		{ "err_init_no_exe_thunk", &err_init_no_exe_thunk },
		{ "echo",                  &echo                  },
		{ "echo_hello",            &echo_hello            },
		{ "echo_hello_world",      &echo_hello_world      },
		{ "env_1",                 &env_1                 },
		{ "env_2",                 &env_2                 },
		{ "execute_order_66",      &execute_order_66      },
	};

	return tcase_build_suite("child", tests, sizeof(tests));
}
