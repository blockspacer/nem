#include "nem.h"

NEM_err_t
NEM_pmsg_validate(const NEM_pmsg_t *this)
{
	if (NEM_PMSG_MAGIC != this->magic) {
		return NEM_err_static("NEM_pmsg_validate: invalid magic");
	}
	if (NEM_PMSG_VERSION != this->version) {
		return NEM_err_static("NEM_pmsg_validate: invalid version");
	}
	if (NEM_PMSG_HDRMAX < this->header_len) {
		return NEM_err_static("NEM_pmsg_validate: header length exceeds max");
	}
	if (NEM_PMSG_BODYMAX < this->body_len) {
		return NEM_err_static("NEM_pmsg_validate: body length exceeds max");
	}

	return NEM_err_none;
}

NEM_msg_t*
NEM_msg_new(size_t header_len, size_t body_len)
{
	if (header_len > NEM_PMSG_HDRMAX) {
		return NULL;
	}
	if (body_len > NEM_PMSG_BODYMAX) {
		return NULL;
	}

	NEM_msg_t *msg = NEM_malloc(sizeof(NEM_msg_t) + header_len + body_len);
	msg->flags = NEM_MSGFLAG_HEADER_INLINE | NEM_MSGFLAG_BODY_INLINE;

	if (header_len > 0) {
		msg->header = &msg->appended[0];
	}
	if (body_len > 0) {
		msg->body = &msg->appended[header_len];
	}

	msg->packed.magic = NEM_PMSG_MAGIC;
	msg->packed.version = NEM_PMSG_VERSION;
	msg->packed.header_len = (uint16_t) header_len;
	msg->packed.body_len = (uint32_t) body_len;
	return msg;
}

NEM_msg_t*
NEM_msg_new_reply(NEM_msg_t *msg, size_t hlen, size_t blen)
{
	NEM_msg_t *this = NEM_msg_new(hlen, blen);
	this->packed.flags |= NEM_PMSGFLAG_REPLY;
	this->packed.seq = msg->packed.seq;
	this->packed.service_id = msg->packed.service_id;
	this->packed.command_id = msg->packed.command_id;
	return this;
}

void
NEM_msg_free(NEM_msg_t *this)
{
	if (NULL == this) {
		return;
	}
	if (!(this->flags & NEM_MSGFLAG_HEADER_INLINE)) {
		free(this->header);
	}
	if (!(this->flags & NEM_MSGFLAG_BODY_INLINE)) {
		free(this->body);
	}
	free(this);
}

NEM_err_t
NEM_msg_set_fd(NEM_msg_t *this, int fd)
{
	if (0 != this->fd) {
		return NEM_err_static("NEM_msg_set_fd: already set");
	}

	this->fd = fd;
	this->flags |= NEM_MSGFLAG_HAS_FD;
	this->packed.flags |= NEM_PMSGFLAG_FD;
	return NEM_err_none;
}

NEM_err_t
NEM_msg_set_header_raw(NEM_msg_t *this, void *header, size_t len)
{
	if (len > NEM_PMSG_HDRMAX) {
		return NEM_err_static("NEM_msg_set_header: too long");
	}

	if (!(this->flags & NEM_MSGFLAG_HEADER_INLINE)) {
		free(this->header);
	}

	this->flags &= ~NEM_MSGFLAG_HEADER_INLINE;
	this->header = header;
	this->packed.header_len = (uint16_t) len;
	return NEM_err_none;
}

NEM_err_t
NEM_msg_set_header(NEM_msg_t *this, NEM_msghdr_t *hdr)
{
	void *bs;
	size_t len;
	NEM_err_t err = NEM_msghdr_pack(hdr, &bs, &len);
	if (!NEM_err_ok(err)) {
		return err;
	}

	err = NEM_msg_set_header_raw(this, bs, len);
	if (!NEM_err_ok(err)) {
		free(bs);
	}

	return err;
}

NEM_msghdr_t*
NEM_msg_header(const NEM_msg_t *this)
{
	NEM_msghdr_t *hdr = NULL;
	if (0 == this->packed.header_len) {
		return NULL;
	}

	NEM_err_t err = NEM_msghdr_new(&hdr, this->header, this->packed.header_len);
	if (!NEM_err_ok(err)) {
		return NULL;
	}

	return hdr;
}

NEM_err_t
NEM_msg_set_body(NEM_msg_t *this, void *body, size_t len)
{
	if (len > NEM_PMSG_BODYMAX) {
		return NEM_err_static("NEM_msg_set_body: too long");
	}

	if (!(this->flags & NEM_MSGFLAG_BODY_INLINE)) {
		free(this->body);
	}

	this->flags &= ~NEM_MSGFLAG_BODY_INLINE;
	this->body = body;
	this->packed.body_len = (uint32_t) len;
	return NEM_err_none;
}
