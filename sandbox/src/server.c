#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <stdlib.h>
#define _WITH_DPRINTF
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>

#include "nem.h"

int
main(int argc, char *argv[])
{
	if (0 != chdir(dirname(argv[0]))) {
		NEM_panic("chdir");
	}

	int kq = kqueue();
	if (-1 == kq) {
		NEM_panic("kqueue");
	}

	int to_child[2];
	int from_child[2];

	if (0 != pipe2(to_child, O_CLOEXEC)) {
		NEM_panic("pipe");
	}
	if (0 != pipe2(from_child, O_CLOEXEC)) {
		NEM_panic("pipe2");
	}

	close(STDIN_FILENO);

	pid_t child_pid = fork();
	if (0 == child_pid) {
		if (STDIN_FILENO != dup2(to_child[1], STDIN_FILENO)) {
			NEM_panic("dup2 stdin");
		}
		if (STDOUT_FILENO != dup2(from_child[1], STDOUT_FILENO)) {
			NEM_panic("dup2 stdout");
		}
		if (STDERR_FILENO != dup2(from_child[1], STDERR_FILENO)) {
			NEM_panic("dup2 stderr");
		}

		char *args[] = { NULL };
		char *env[] = { NULL };

		if (-1 == execve("./client", args, env)) {
			NEM_panic("execve");
		}
	}

	struct kevent ev[3];
	EV_SET(&ev[0], to_child[0], EVFILT_WRITE, EV_ADD, 0, 0, 0);
	EV_SET(&ev[1], from_child[0], EVFILT_READ, EV_ADD, 0, 0, 0);
	EV_SET(&ev[2], child_pid, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, 0);
	if (-1 == kevent(kq, ev, 3, NULL, 0, NULL)) {
		NEM_panic("kevent");
	}

	for (;;) {
		struct kevent trig;
		fprintf(stdout, "[parent] waiting\n");
		if (-1 == kevent(kq, NULL, 0, &trig, 1, NULL)) {
			NEM_panic("kevent");
		}
		if (EV_ERROR == (trig.flags & EV_ERROR)) {
			fprintf(stderr, "EV_ERROR: %s", strerror(trig.data));
			exit(1);
		}

		switch (trig.filter) {
			case EVFILT_WRITE:
				fprintf(stdout, "[parent] child write ready\n");
				dprintf(to_child[0], "hello");
				close(to_child[0]);
				break;

			case EVFILT_READ: {
				fprintf(stdout, "[parent] child read ready (%lu)\n", trig.data);
				if (0 < trig.data) {
					char *data = calloc(1, ((size_t)trig.data) + 1);
					if (0 > read(trig.ident, data, trig.data)) {
						NEM_panic("read");
					}
					fprintf(stdout, "[child] %s\n", data);
					free(data);
				}
				break;
			}

			case EVFILT_PROC:
				fprintf(stdout, "[parent] proc exit\n");
				goto done;
		}
	}
done:

	close(from_child[0]);
	close(kq);
}
