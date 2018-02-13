#pragma once

// NEM_pmsg_t is the packed fixed header for the NEM framing protocol. This
// protocol is a request/response protocol that chunks large requests over
// multiple messages. The framing protocol is designed to work over
// pipes/unix/tcp sockets.
#pragma pack(push, 0)
typedef struct {
	uint32_t magic;      // Should be NEM_PMSG_MAGIC.
	uint8_t  version;    // NEM_PMSG_VERSION.
	uint8_t  flags;      // Or'd NEM_PMSGFLAG_*s.
	uint16_t header_len; // Length of header, in bytes.
	uint64_t seq;        // Sequence number of request/reply.
	uint32_t body_len;   // Length of body in bytes.
	uint32_t padding;
}
NEM_pmsg_t;
#pragma pack(pop)

_Static_assert(sizeof(NEM_pmsg_t) == 24, "NEM_pmsg_t size cannot change");

static const uint32_t NEM_PMSG_MAGIC   = 0x6e656d21;
static const uint8_t  NEM_PMSG_VERSION = 1;

static const size_t NEM_PMSG_HDRMAX = 64000;
static const size_t NEM_PMSG_BODYMAX = 1048576; // 2**20 bytes.

static const uint8_t
	NEM_PMSGFLAG_REPLY    = 1 << 0, // This message is a reply to request seq.
	NEM_PMSGFLAG_CONTINUE = 1 << 1, // Additional messages continue the body.
	NEM_PMSGFLAG_CANCEL   = 1 << 2, // Cancel future replies to this seq.
	NEM_PMSGFLAG_FD       = 1 << 3; // File descriptor follows fixed header.

// NEM_msg_t is the unpacked version of NEM_pmsg_t. It can be allocated such
// that the headers/body are immediately preceded by the packed contents. The
// intent is to abstract that packing so they can be easily unpacked and
// have additional non-network metadata attached.
typedef struct {
	void *header; // Header buffer. Appended if NEM_MSGFLAG_HEADER_INLINE.
	void *body;   // Body buffer. Appended if NEM_MSGFLAG_BODY_INLINE.
	int   fd;     // Attached fd if NEM_MSGFLAG_HAS_FD.
	int   flags;  // NEM_MSGFLAG_*s OR'd.

	// NB: packed+appended can be sent as a single blob.
	NEM_pmsg_t packed;
	char       appended[];
}
NEM_msg_t;

static const uint8_t
	NEM_MSGFLAG_HEADER_INLINE = 1 << 0, // header is inlined.
	NEM_MSGFLAG_BODY_INLINE  = 1 << 1, // body is inlined.
	NEM_MSGFLAG_HAS_FD       = 1 << 2; // fd is set.

// NEM_msg_alloc allocates a new NEM_msg_t. If header_len/body_len are
// provided, the header/body fields are pre-allocated with a single
// allocation. They can be 0; the caller must allocate them manually
// and attach with NEM_msg_set_{header,body}.
//
// If {header,body}_len exceed maximums, NULL is returned.
NEM_msg_t* NEM_msg_alloc(size_t header_len, size_t body_len);

// NEM_msg_free frees a message allocated with NEM_msg_alloc.
void NEM_msg_free(NEM_msg_t *this);

// NEM_msg_set_fd attaches a file descriptor to the message.
NEM_err_t NEM_msg_set_fd(NEM_msg_t *this, int fd);

// NEM_msg_set_header attaches a header buffer to the message. The buffer
// becomes owned by the message and will be freed with NEM_msg_free (or
// when a different buffer is attached).
NEM_err_t NEM_msg_set_header(NEM_msg_t *this, void *header, size_t len);

// NEM_msg_set_body does the same thing as NEM_msg_set_header, but with
// a different field.
NEM_err_t NEM_msg_set_body(NEM_msg_t *this, void *body, size_t len);
