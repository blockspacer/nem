#include "test.h"
#include "jd.h"

#define SHA256HASH "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

START_TEST(parse_image)
{
	const char *toml =
		"name = 'yep'\n"
		"exe_path = '/bin/run'\n"
		"[[mount]]\n"
		"dest = '/'\n"
		"type = 'image'\n"
		"image = 'foobar'\n"
		"sha256 = '" SHA256HASH "'\n";

	NEM_jd_t jd;
	ck_err(NEM_jd_init_toml(&jd, toml, strlen(toml)));
	ck_assert_str_eq(jd.name, "yep");
	ck_assert_str_eq(jd.exe_path, "/bin/run");
	ck_assert_int_eq(jd.num_mounts, 1);
	ck_assert_int_eq(jd.mounts[0].type, NEM_JD_MOUNTTYPE_IMAGE);
	ck_assert_str_eq(jd.mounts[0].image.name, "foobar");
	// XXX: Check the sha256. We're storing binary, but have a hex string.
	NEM_jd_free(&jd);
}
END_TEST

START_TEST(parse_vnode)
{
	const char *toml = 
		"name = 'yep'\n"
		"exe_path = '/bin/run'\n"
		"[[mount]]\n"
		"dest = '/'\n"
		"type = 'vnode'\n"
		"capacity = '1M'\n";

	NEM_jd_t jd;
	ck_err(NEM_jd_init_toml(&jd, toml, strlen(toml)));
	ck_assert_str_eq(jd.name, "yep");
	ck_assert_str_eq(jd.exe_path, "/bin/run");
	ck_assert_int_eq(jd.num_mounts, 1);
	ck_assert_int_eq(jd.mounts[0].type, NEM_JD_MOUNTTYPE_VNODE);
	ck_assert_int_eq(jd.mounts[0].vnode.len, 1024*1024);
	ck_assert(!jd.mounts[0].vnode.persist);
	NEM_jd_free(&jd);
}
END_TEST

START_TEST(parse_shared)
{
	const char *toml =
		"name = 'yep'\n"
		"exe_path = '/bin/run'\n"
		"[[mount]]\n"
		"dest = '/'\n"
		"type = 'shared'\n"
		"capacity = '1G'\n";

	NEM_jd_t jd;
	ck_err(NEM_jd_init_toml(&jd, toml, strlen(toml)));
	ck_assert_str_eq(jd.name, "yep");
	ck_assert_str_eq(jd.exe_path, "/bin/run");
	ck_assert_int_eq(jd.num_mounts, 1);
	ck_assert_int_eq(jd.mounts[0].type, NEM_JD_MOUNTTYPE_SHARED);
	ck_assert_int_eq(jd.mounts[0].shared.len, 1024*1024*1024);
	NEM_jd_free(&jd);
}
END_TEST

START_TEST(err_no_name)
{
	const char *toml =
		"exe_path = '/bin/run'\n"
		"[[mount]]\n"
		"dest = '/'\n"
		"type = 'shared'\n"
		"capacity = '1G'\n";

	NEM_jd_t jd;
	NEM_err_t err = NEM_jd_init_toml(&jd, toml, strlen(toml));
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(err_no_exe_path)
{
	const char *toml = 
		"name = 'yep'\n"
		"[[mount]]\n"
		"dest = '/'\n"
		"type = 'shared'\n"
		"capacity = '1G'\n";

	NEM_jd_t jd;
	NEM_err_t err = NEM_jd_init_toml(&jd, toml, strlen(toml));
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(err_no_mounts)
{
	const char *toml = 
		"name = 'yep'\n"
		"exe_path = '/bin/run'\n";
	
	NEM_jd_t jd;
	NEM_err_t err = NEM_jd_init_toml(&jd, toml, strlen(toml));
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(err_invalid_type)
{
	const char *toml = 
		"name = 'yep'\n"
		"exe_path = '/bin/run'\n"
		"[[mount]]\n"
		"type = 'foo'\n";

	NEM_jd_t jd;
	NEM_err_t err = NEM_jd_init_toml(&jd, toml, strlen(toml));
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(two_mounts)
{
	const char *toml = 
		"name = 'yep'\n"
		"exe_path = '/bin/run'\n"
		"[[mount]]\n"
		"dest = '/'\n"
		"type = 'shared'\n"
		"capacity = '1b'\n"
		"[[mount]]\n"
		"dest = '/bar'\n"
		"type = 'vnode'\n"
		"capacity = '1k'\n"
		"persist = true\n";

	NEM_jd_t jd;
	ck_err(NEM_jd_init_toml(&jd, toml, strlen(toml)));
	ck_assert_str_eq(jd.name, "yep");
	ck_assert_str_eq(jd.exe_path, "/bin/run");
	ck_assert_int_eq(jd.num_mounts, 2);
	ck_assert_int_eq(jd.mounts[0].type, NEM_JD_MOUNTTYPE_SHARED);
	ck_assert_int_eq(jd.mounts[0].shared.len, 1);
	ck_assert_int_eq(jd.mounts[1].type, NEM_JD_MOUNTTYPE_VNODE);
	ck_assert_int_eq(jd.mounts[1].vnode.len, 1024);
	ck_assert(jd.mounts[1].vnode.persist);
	NEM_jd_free(&jd);
}
END_TEST

START_TEST(err_silly_mounts)
{
	const char *toml = 
		"name = 'yep'\n"
		"exe_path = '/bin/run'\n"
		"mount = false\n";

	NEM_jd_t jd;
	NEM_err_t err = NEM_jd_init_toml(&jd, toml, strlen(toml));
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(err_no_type)
{
	const char *toml = 
		"name = 'yep'\n"
		"exe_path = '/bin/run'\n"
		"[[mount]]\n";

	NEM_jd_t jd;
	NEM_err_t err = NEM_jd_init_toml(&jd, toml, strlen(toml));
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(err_bad_capacity)
{
	const char *toml =
		"name = 'yep'\n"
		"exe_path = '/bin/run'\n"
		"[[mount]]\n"
		"type = 'shared'\n"
		"capacity = 'yay'\n";

	NEM_jd_t jd;
	NEM_err_t err = NEM_jd_init_toml(&jd, toml, strlen(toml));
	ck_assert(!NEM_err_ok(err));
}
END_TEST

Suite*
suite_jd()
{
	tcase_t tests[] = {
		{ "parse_image",      &parse_image      },
		{ "parse_vnode",      &parse_vnode      },
		{ "parse_shared",     &parse_shared     },
		{ "err_no_name",      &err_no_name      },
		{ "err_no_exe_path",  &err_no_exe_path  },
		{ "err_no_mounts",    &err_no_mounts    },
		{ "err_invalid_type", &err_invalid_type },
		{ "two_mounts",       &two_mounts       },
		{ "err_silly_mounts", &err_silly_mounts },
		{ "err_no_type",      &err_no_type      },
		{ "err_bad_capacity", &err_bad_capacity },
	};

	return tcase_build_suite("jd", tests, sizeof(tests));
}
