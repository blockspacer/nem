#include <sys/types.h>
#include <sys/event.h>

#include "test.h"
#include "nem.h"

static const int INVALID_FD = 93485;

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

Suite*
suite_pipechan()
{
	tcase_t tests[] = {
		{ "err_invalid_kq",  &err_invalid_kq  },
		{ "err_invalid_fds", &err_invalid_fds },
		{ "init_free",       &init_free       },
	};

	return tcase_build_suite("pipechan", tests, sizeof(tests));
}
