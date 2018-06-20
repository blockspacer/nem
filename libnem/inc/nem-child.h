#include "nem.h"

typedef enum {
	CHILD_RUNNING,
	CHILD_STOPPED,
}
NEM_child_state_t;

// NEM_child_t is a child process that conforms to the NEM messaging protocol.
// The child should allocate a NEM_app_t and initialize it with NEM_app_init.
// It receives a file descriptor on NEM_APP_FILENO which is used for
// communication with the parent process (which is always going to be
// nem-rootd).
typedef struct {
	int               kq;
	int               exe_fd;
	int               exitcode;
	pid_t             pid;
	NEM_child_state_t state;
	NEM_fd_t          fd;
	NEM_txnmgr_t      txnmgr;
	NEM_thunk_t      *on_kevent;
	NEM_thunk1_t     *on_close;
}
NEM_child_t;

typedef struct {
	// args and env are NULL-terminated lists of NULL-terminated strings that 
	// are set-able during the thunk invocation of NEM_child_init. These
	// control the child invocation. They can be heap allocated freely since
	// they're executed post-fork but pre-exec.
	char **args;
	char **env;

	// exitcode is set when the on_close callback is set.
	int exitcode;
}
NEM_child_ca;

// NEM_child_init initializes and forks a child process. The caller may pass
// a thunk1 to execute between fork/execve -- or NULL. If an error is returned
// the NEM_child_t is automatically freed.
NEM_err_t NEM_child_init(
	NEM_child_t  *this,
	NEM_kq_t     *kq,
	const char   *path,
	NEM_thunk1_t *preexec
);

// NEM_child_stop kills the child.
void NEM_child_stop(NEM_child_t *this);

// NEM_child_free kills the child if it's running and frees all resources.
void NEM_child_free(NEM_child_t *this);

// NEM_child_on_close binds a handler to be called when the child process
// exits. This is called even if the child is killed.
NEM_err_t NEM_child_on_close(NEM_child_t *this, NEM_thunk1_t *thunk);
