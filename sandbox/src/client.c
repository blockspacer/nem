#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "nem.h"

static void
log_msg(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	char *msgstr;
	size_t len = vasprintf(&msgstr, format, ap);
	va_end(ap);

	NEM_msg_t *msg = NEM_msg_alloc(0, 0);
	NEM_msg_set_body(msg, msgstr, len + 1);
	write(STDOUT_FILENO, &msg->packed, sizeof(msg->packed));
	if (NULL != msg->header) {
		write(STDOUT_FILENO, msg->header, msg->packed.header_len);
	}
	if (NULL != msg->body) {
		write(STDOUT_FILENO, msg->body, msg->packed.body_len);
	}
	NEM_msg_free(msg);
}

int
main(int argc, char *argv[])
{
	ssize_t r;
	NEM_msg_t *msg = NEM_msg_alloc(0, 0);

	log_msg("read fixed header (%lu)", sizeof(msg->packed));
	r = read(STDIN_FILENO, &msg->packed, sizeof(msg->packed));
	if (r != sizeof(msg->packed)) {
		NEM_panicf("short read: %ld", r);
	}

	msg->header = NEM_malloc(msg->packed.header_len);
	msg->body = NEM_malloc(msg->packed.body_len);

	log_msg("read metadata (%lu)", (size_t) msg->packed.header_len);
	r = read(STDIN_FILENO, msg->header, msg->packed.header_len);
	if (r != msg->packed.header_len) {
		NEM_panicf("short read: %ld", r);
	}

	log_msg("read data (%lu)", (size_t) msg->packed.body_len);
	r = read(STDIN_FILENO, msg->body, msg->packed.body_len);
	if (r != msg->packed.body_len) {
		NEM_panicf("short read: %ld", r);
	}

	log_msg("echo %s", msg->body);
	NEM_msg_free(msg);
	return 0;
}
