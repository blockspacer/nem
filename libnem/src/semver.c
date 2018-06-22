#include "nem.h"

NEM_err_t
NEM_semver_init(NEM_semver_t *this, const char *str)
{
	if (NULL == str || 0 == str[0]) {
		return NEM_err_static("NEM_semver_init: null or empty string");
	}

	const char *ptr = str;
	char *endptr;

	this->major = strtol(ptr, &endptr, 10);
	if (ptr == endptr) {
		return NEM_err_static("NEM_semver_init: major is not numeric");
	}
	if ('.' != *endptr) {
		return NEM_err_static("NEM_semver_init: major not followed by dot");
	}

	ptr = endptr + 1;
	this->minor = strtol(ptr, &endptr, 10);
	if (ptr == endptr) {
		return NEM_err_static("NEM_semver_init: minor is not numeric");
	}
	if ('.' != *endptr) {
		return NEM_err_static("NEM_semver_init: minor not followed by dot");
	}

	ptr = endptr + 1;
	this->patch = strtol(ptr, &endptr, 10);
	if (ptr == endptr) {
		return NEM_err_static("NEM_semver_init: patch is not numeric");
	}
	if ('\0' != *endptr) {
		return NEM_err_static("NEM_semver_init: patch followed by garbage");
	}

	return NEM_err_none;
}

NEM_err_t
NEM_semver_init_match(
	NEM_semver_t       *this,
	NEM_semver_match_t *match,
	const char         *str
) {
	if (NULL == str) {
		return NEM_err_static("NEM_semver_init_match: null string");
	}

	if ('^' == str[0]) {
		*match = NEM_SEMVER_MATCH_MAJOR;
		str += 1;
	}
	else if ('~' == str[0]) {
		*match = NEM_SEMVER_MATCH_MINOR;
		str += 1;
	}
	else {
		*match = NEM_SEMVER_MATCH_EXACT;
	}

	return NEM_semver_init(this, str);
}

int
NEM_semver_cmp(
	const NEM_semver_t *base,
	const NEM_semver_t *test,
	NEM_semver_match_t method
) {
	switch (method) {
		case NEM_SEMVER_MATCH_EXACT:
			if (
				base->major == test->major 
				&& base->minor == test->minor
				&& base->patch == test->patch
			) {
				return 0;
			}
			else {
				return -1;
			}

		case NEM_SEMVER_MATCH_MAJOR:
			if (base->major != test->major) {
				return -1;
			}
			if (base->minor < test->minor) {
				return 1;
			}
			if (base->minor > test->minor) {
				return -1;
			}
			if (base->patch == test->patch) {
				return 0;
			}
			return (base->patch < test->patch) ? 1 : -1;

		case NEM_SEMVER_MATCH_MINOR:
			if (base->major != test->major) {
				return -1;
			}
			if (base->minor != test->minor) {
				return -1;
			}
			if (base->patch == test->patch) {
				return 0;
			}
			return (base->patch < test->patch) ? 1 : -1;
	}
}
