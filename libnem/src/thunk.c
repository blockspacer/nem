#include <sys/types.h>
#include <stdlib.h>

#include "nem.h"

NEM_thunk1_t*
NEM_thunk1_new(NEM_thunk1_fn fn, size_t sz)
{
	size_t len = sizeof(NEM_thunk1_t) + sz;
	NEM_thunk1_t *this = NEM_malloc(len);
	this->fn = fn;
	return this;
}
NEM_thunk_t*
NEM_thunk_new(NEM_thunk_fn fn, size_t sz)
{
	size_t len = sizeof(NEM_thunk_t) + sz;
	NEM_thunk_t *this = NEM_malloc(len);
	this->fn = fn;
	return this;
}

NEM_thunk1_t*
NEM_thunk1_new_ptr(NEM_thunk1_fn fn, void *ptr)
{
	NEM_thunk1_t *thunk = NEM_thunk1_new(fn, sizeof(void*));
	*(void**) thunk->data = ptr;
	return thunk;
}
NEM_thunk_t*
NEM_thunk_new_ptr(NEM_thunk_fn fn, void *ptr)
{
	NEM_thunk_t *thunk = NEM_thunk_new(fn, sizeof(void*));
	*(void**) thunk->data = ptr;
	return thunk;
}

void*
NEM_thunk1_ptr(NEM_thunk1_t *this)
{
	return *(void**) this->data;
}
void*
NEM_thunk_ptr(NEM_thunk_t *this)
{
	return *(void**) this->data;
}

void
NEM_thunk1_discard(NEM_thunk1_t **this)
{
	if (NULL == *this) {
		return;
	}

	free(*this);
	*this = NULL;
}

void
NEM_thunk_free(NEM_thunk_t *this)
{
	free(this);
}
