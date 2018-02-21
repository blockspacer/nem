#include <sys/types.h>
#include <sys/event.h>
#include "test.h"

static void
init_free_cb(NEM_thunk1_t *thunk, void *varg)
{
	bool *running = NEM_thunk1_ptr(thunk);
	*running = false;
}

START_TEST(init_free)
{
	int kq = kqueue();
	bool running = true;

	NEM_unixchan_t chan;
	ck_err(NEM_unixchan_init(&chan, kq, NEM_thunk1_new_ptr(
		&init_free_cb,
		&running
	)));

	while (running) {
		struct kevent trig;
		if (-1 == kevent(kq, NULL, 0, &trig, 1, NULL)) {
			NEM_panic("kevent");
		}
		if (EV_ERROR == (trig.flags & EV_ERROR)) {
			fprintf(stderr, "EV_ERROR: %s", strerror(trig.data));
			break;
		}

		NEM_thunk_t *thunk = trig.udata;
		if (NULL == trig.udata) {
			NEM_panicf("NULL udata filter=%d", trig.filter);
		}
		NEM_thunk_invoke(thunk, &trig);
	}
}
END_TEST

Suite*
suite_unixchan()
{
	tcase_t tests[] = {
		{ "init_free", &init_free },
	};

	return tcase_build_suite("unixchan", tests, sizeof(tests));
}
