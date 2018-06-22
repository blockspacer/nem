#pragma once

typedef enum {
	NEM_SEMVER_MATCH_EXACT, // no prefix
	NEM_SEMVER_MATCH_MAJOR, // ^prefix
	NEM_SEMVER_MATCH_MINOR  // ~prefix
}
NEM_semver_match_t;

typedef struct {
	long major;
	long minor;
	long patch;
}
NEM_semver_t;

NEM_err_t NEM_semver_init(NEM_semver_t *this, const char *str);
NEM_err_t NEM_semver_init_match(
	NEM_semver_t       *this,
	NEM_semver_match_t *match,
	const char         *str
);

int NEM_semver_cmp(
	const NEM_semver_t *base,
	const NEM_semver_t *test,
	NEM_semver_match_t method
);
