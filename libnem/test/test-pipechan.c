#include <sys/types.h>
#include <sys/event.h>

#include "test.h"
#include "nem.h"

static const int INVALID_FD = 93485;

static void
run_kq(int kq, bool *running)
{
	while (*running) {
		struct kevent trig;
		if (-1 == kevent(kq, NULL, 0, &trig, 1, NULL)) {
			NEM_panic("kevent");
		}
		if (EV_ERROR == (trig.flags & EV_ERROR)) {
			NEM_panicf("EV_ERROR: %s", strerror(trig.data));
		}

		NEM_thunk_t *thunk = trig.udata;
		if (NULL == trig.udata) {
			NEM_panicf("NULL udata filter=%d", trig.filter);
		}
		NEM_thunk_invoke(thunk, &trig);
	}
}

START_TEST(err_invalid_kq)
{
	int fds[2];
	ck_assert_int_ne(-1, pipe2(fds, O_CLOEXEC));

	NEM_pipechan_t chan;
	NEM_err_t err = NEM_pipechan_init(&chan, INVALID_FD, fds[0], fds[1]);
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(err_invalid_fds)
{
	int kq = kqueue();
	ck_assert_int_ne(-1, kq);

	NEM_pipechan_t chan;
	NEM_err_t err = NEM_pipechan_init(&chan, kq, INVALID_FD, INVALID_FD);
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(init_free)
{
	int fds[2];
	int kq = kqueue();
	ck_assert_int_ne(-1, kq);
	ck_assert_int_ne(-1, pipe2(fds, O_CLOEXEC));

	NEM_pipechan_t chan;
	ck_err(NEM_pipechan_init(&chan, kq, fds[0], fds[1]));
	NEM_pipechan_free(&chan);
}
END_TEST

static void
send_recv_1_msg(NEM_thunk_t *thunk, void *varg)
{
	bool *running = NEM_thunk_ptr(thunk);
	*running = false;

	NEM_msg_t *msg = varg;
	ck_assert_str_eq(msg->header, "hi");
	ck_assert_str_eq(msg->body, "hello");
}

START_TEST(send_recv_1)
{
	int fds[2];
	int kq = kqueue();
	bool running = true;
	ck_assert_int_ne(-1, kq);
	ck_assert_int_ne(-1, pipe2(fds, O_CLOEXEC));
	
	NEM_pipechan_t chan1, chan2;
	ck_err(NEM_pipechan_init(&chan1, kq, fds[0], fds[1]));
	ck_err(NEM_pipechan_init(&chan2, kq, fds[1], fds[0]));

	NEM_pipechan_on_msg(&chan1, NEM_thunk_new_ptr(
		&send_recv_1_msg,
		&running
	));

	NEM_msg_t *msg = NEM_msg_alloc(3, 6);
	strcpy(msg->header, "hi");
	strcpy(msg->body, "hello");
	NEM_pipechan_send_msg(&chan2, msg);

	run_kq(kq, &running);

	NEM_pipechan_free(&chan1);
	NEM_pipechan_free(&chan2);
}
END_TEST

Suite*
suite_pipechan()
{
	tcase_t tests[] = {
		{ "err_invalid_kq",  &err_invalid_kq  },
		{ "err_invalid_fds", &err_invalid_fds },
		{ "init_free",       &init_free       },
		{ "send_recv_1",     &send_recv_1     },
	};

	return tcase_build_suite("pipechan", tests, sizeof(tests));
}
