#pragma once

/*
 *
 * NEM_thunk - manual closures.
 *
 */

typedef struct NEM_thunk1_t NEM_thunk1_t;
typedef struct NEM_thunk_t NEM_thunk_t;
typedef void(*NEM_thunk1_fn)(NEM_thunk1_t*, void*);
typedef void(*NEM_thunk_fn)(NEM_thunk_t*, void*);

// NEM_thunk1_t is a heap-allocated thunk which is called at most once,
// then freed. The thunk's allocation is unsized -- additional data may
// be stuffed inline with the thunk and accessed during execution. During 
// execution, the function pointer is provided with both the thunk pointer
// (so that it may access the associated data) and whatever data the callback
// sends (via void*).
struct NEM_thunk1_t {
	NEM_thunk1_fn fn;
	NEM_ALIGN char data[];
};
// NEM_thunk_t is identical to a NEM_thunk1_t except that it does not
// get freed by NEM_thunk_invoke -- the caller must manually release it
// with NEM_thunk_free. All of the NEM_thunk1_* functions (except for
// NEM_thunk_discard) have a NEM_thunk_* alias for these.
struct NEM_thunk_t {
	NEM_thunk_fn fn;
	NEM_ALIGN char data[];
};

// NEM_thunk1_new allocates and initializes a new NEM_thunk1_t. If sz is
// provided, &thunk->data is a region of space with sz bytes available
// for arbitrary use. If sz is not provided, thunk->data is just an
// extra pointer.
//
// The extra space should be explicitly cast to a pointer of the type
// you'd like to store. This seems a bit awkward, so e.g.,
//
//     int i = 42;
//     NEM_thunk1_t *foo = NEM_thunk1_new(&fn, sizeof(int));
//     *(int*) foo->data = i;
//
//     NEM_thunk1_t *bar = NEM_thunk1_new(&fn, sizeof(int*));
//     *(int**) foo->data = &i;
//
// You for the former, you can use NEM_thunk1_inlineptr for an implicit
// cast, e.g.
//
//     int i = 42;
//     NEM_thunk1_t *foo = NEM_thunk1_new(&fn, sizeof(int));
//     int *tmp = NEM_thunk1_inlineptr(foo);
//
// For the latter, NEM_thunk1_new_ptr and NEM_thunk1_ptr provide useful
// helpers:
//
//     int i = 42;
//     NEM_thunk1_t *foo = NEM_thunk1_new_ptr(&fn, &i);
//     int *tmp = NEM_thunk1_ptr(foo);
//
// The consumption needs jumping through the same hoops:
//
//     void fn(NEM_thunk1_t *thunk, void *unused) {
//         int i = *(int*) thunk->data;   // use NEM_thunk1_ptr
//         int *i = *(int**) thunk->data; // use NEM_thunk1_inlineptr
//     }
//
// It should be noted that NEM_thunk1_new_ptr should never be used to store
// function pointers which may have a different size on a platform this will
// never run on because lol.
//
// The returned object should be passed to either NEM_thunk1_invoke or
// NEM_thunk1_discard (but never both -- they both free).
NEM_thunk1_t* NEM_thunk1_new(NEM_thunk1_fn fn, size_t sz);
NEM_thunk_t* NEM_thunk_new(NEM_thunk_fn fn, size_t sz);

// NEM_thunk1_new_ptr allocates a new thunk with the specified function sized
// to store a pointer, then puts the pointer into the data field.
NEM_thunk1_t* NEM_thunk1_new_ptr(NEM_thunk1_fn fn, void *ptr);
NEM_thunk_t *NEM_thunk_new_ptr(NEM_thunk_fn fn, void *ptr);

// NEM_thunk1_ptr returns the data field as a void*, mirroring the value
// e.g. passed to NEM_thunk1_new_ptr. This should _not_ be used without
// NEM_thunk1_new_ptr.
void* NEM_thunk1_ptr(NEM_thunk1_t *this);
void* NEM_thunk_ptr(NEM_thunk_t *this);

// NEM_thunk1_inlineptr returns the data field as a void*. It's basically
// to avoid an explicit cast. This should _not_ be used without 
// NEM_thunk1_new.
static inline void*
NEM_thunk1_inlineptr(NEM_thunk1_t *this)
{
	return (void*) &this->data[0];
}
static inline void*
NEM_thunk_inlineptr(NEM_thunk_t *this)
{
	return (void*) &this->data[0];
}

// NEM_thunk1_invoke invokes the wrapped function and immediately discards
// (i.e., free's) the provided NEM_thunk1_t.
#define NEM_thunk1_invoke(vthis, vdata) {\
	NEM_thunk1_t **NEM_THUNK_this = (vthis); \
	void *NEM_THUNK_data = (vdata); \
	NEM_thunk1_t *NEM_THUNK_copy = *NEM_THUNK_this; \
	*NEM_THUNK_this = NULL; \
	NEM_THUNK_copy->fn(NEM_THUNK_copy, NEM_THUNK_data); \
	free(NEM_THUNK_copy); \
}

#define NEM_thunk_invoke(vthis, vdata) {\
	NEM_thunk_t *NEM_THUNK_this = (vthis); \
	void *NEM_THUNK_data = (vdata); \
	NEM_THUNK_this->fn(NEM_THUNK_this, NEM_THUNK_data); \
}

// NEM_thunk1_discard free's the NEM_thunk1_t without invocation.
void NEM_thunk1_discard(NEM_thunk1_t **this);
void NEM_thunk_free(NEM_thunk_t *this);
