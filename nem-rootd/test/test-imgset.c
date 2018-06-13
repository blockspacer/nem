#include "test.h"
#include "imgset.h"

START_TEST(init_free)
{
	NEM_imgset_t set;
	NEM_imgset_init(&set);
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(err_img_bad_id)
{
	NEM_img_t
		tmp = {
			.name = strdup("hello"),
			.id   = 0,
		},
		*img = &tmp;

	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_assert(!NEM_err_ok(
		NEM_imgset_add_img(&set, &img)
	));
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(err_img_bad_name)
{
	NEM_img_t
		tmp1 = {
			.name = NULL,
			.id   = 1,
		},
		tmp2 = {
			.name = strdup(""),
			.id   = 2,
		},
		*img1 = &tmp1,
		*img2 = &tmp2;

	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_assert(!NEM_err_ok(
		NEM_imgset_add_img(&set, &img1)
	));
	ck_assert(!NEM_err_ok(
		NEM_imgset_add_img(&set, &img2)
	));
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(err_img_dupe_name_bad_id)
{
	NEM_img_t
		tmp1 = {
			.name = strdup("hello"),
			.id   = 1,
		},
		tmp2 = {
			.name = strdup("hello"),
			.id   = 2,
		},
		*img1 = &tmp1,
		*img2 = &tmp2;

	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_err(NEM_imgset_add_img(&set, &img1));
	ck_assert(!NEM_err_ok(
		NEM_imgset_add_img(&set, &img2)
	));
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(err_img_dupe_id_bad_name)
{
	NEM_img_t
		tmp1 = {
			.name = strdup("hello"),
			.id   = 1,
		},
		tmp2 = {
			.name = strdup("world"),
			.id   = 1,
		},
		*img1 = &tmp1,
		*img2 = &tmp2;

	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_err(NEM_imgset_add_img(&set, &img1));
	ck_assert(!NEM_err_ok(
		NEM_imgset_add_img(&set, &img2)
	));
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(img_dupe)
{
	NEM_img_t
		tmp1 = {
			.name = strdup("hello"),
			.id   = 1,
		},
		tmp2 = {
			.name = strdup("hello"),
			.id   = 1,
		},
		*img1 = &tmp1,
		*img2 = &tmp2;
	
	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_err(NEM_imgset_add_img(&set, &img1));
	ck_err(NEM_imgset_add_img(&set, &img2));
	ck_assert_ptr_eq(img1, img2);
	NEM_imgset_free(&set);
}
END_TEST

static const char *SHA256 =
	"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

START_TEST(err_ver_bad_id)
{
	NEM_imgver_t
		tmp = {
			.id      = 0,
			.sha256  = strdup(SHA256),
			.version = strdup("hello"),
		},
		*ver = &tmp;

	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_assert(!NEM_err_ok(NEM_imgset_add_ver(&set, &ver, NULL)));
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(err_ver_bad_hash)
{
	NEM_imgver_t
		tmp1 = {
			.id      = 1,
			.sha256  = NULL,
			.version = strdup("hello"),
		},
		tmp2 = {
			.id      = 2,
			.sha256  = strdup("abcdef123"),
			.version = strdup("world"),
		},
		*ver1 = &tmp1,
		*ver2 = &tmp2;

	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_assert(!NEM_err_ok(NEM_imgset_add_ver(&set, &ver1, NULL)));
	ck_assert(!NEM_err_ok(NEM_imgset_add_ver(&set, &ver2, NULL)));
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(err_ver_bad_ver)
{
	NEM_imgver_t
		tmp1 = {
			.id      = 1,
			.sha256  = strdup(SHA256),
			.version = NULL,
		},
		tmp2 = {
			.id      = 2,
			.sha256  = strdup(SHA256),
			.version = strdup(""),
		},
		*ver1 = &tmp1,
		*ver2 = &tmp2;
	
	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_assert(!NEM_err_ok(NEM_imgset_add_ver(&set, &ver1, NULL)));
	ck_assert(!NEM_err_ok(NEM_imgset_add_ver(&set, &ver2, NULL)));
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(err_ver_bad_hex)
{
	NEM_imgver_t
		tmp = {
			.id     = 1,
			.sha256 = strdup(
				"e3b0c44298fc1c149afbf4c8996fb924"
				"27ae41e4649b934ca495991b7852b85q"
			),
			.version = strdup("version"),
		},
		*ver = &tmp;

	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_assert(!NEM_err_ok(NEM_imgset_add_ver(&set, &ver, NULL)));
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(err_ver_bad_dupe_id)
{
	NEM_imgver_t
		tmp1 = {
			.id      = 1,
			.sha256  = strdup(SHA256),
			.version = strdup("hello"),
		},
		tmp2 = {
			.id      = 2,
			.sha256  = strdup(SHA256),
			.version = strdup("hello"),
		},
		*ver1 = &tmp1,
		*ver2 = &tmp2;

	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_err(NEM_imgset_add_ver(&set, &ver1, NULL));
	ck_assert(!NEM_err_ok(NEM_imgset_add_ver(&set, &ver2, NULL)));
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(err_ver_bad_dupe_hash)
{
	NEM_imgver_t
		tmp1 = {
			.id      = 1,
			.sha256  = strdup(SHA256),
			.version = strdup("hello"),
		},
		tmp2 = {
			.id      = 1,
			.sha256  = strdup(
					"e3b0c44298fc1c149afbf4c8996fb924"
					"27ae41e4649b934ca495991b7852b853"
			),
			.version = strdup("hello"),
		},
		*ver1 = &tmp1,
		*ver2 = &tmp2;

	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_err(NEM_imgset_add_ver(&set, &ver1, NULL));
	ck_assert(!NEM_err_ok(NEM_imgset_add_ver(&set, &ver2, NULL)));
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(err_ver_bad_dupe_ver)
{
	NEM_imgver_t
		tmp1 = {
			.id      = 1,
			.sha256  = strdup(SHA256),
			.version = strdup("hello"),
		},
		tmp2 = {
			.id      = 1,
			.sha256  = strdup(SHA256),
			.version = strdup("hello4"),
		},
		*ver1 = &tmp1,
		*ver2 = &tmp2;

	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_err(NEM_imgset_add_ver(&set, &ver1, NULL));
	ck_assert(!NEM_err_ok(NEM_imgset_add_ver(&set, &ver2, NULL)));
	NEM_imgset_free(&set);
}
END_TEST

START_TEST(ver_link)
{
	NEM_imgver_t
		tmpv = {
			.id      = 1,
			.sha256  = strdup(SHA256),
			.version = strdup("hello"),
		},
		*ver = &tmpv;

	NEM_img_t 
		tmpi = {
			.id   = 1,
			.name = strdup("world"),
		},
		*img = &tmpi;

	NEM_imgset_t set;
	NEM_imgset_init(&set);
	ck_err(NEM_imgset_add_img(&set, &img));
	ck_err(NEM_imgset_add_ver(&set, &ver, img));
	NEM_imgset_free(&set);
}
END_TEST

Suite*
suite_imgset()
{
	tcase_t tests[] = {
		{ "init_free",                &init_free                },
		{ "err_img_bad_id",           &err_img_bad_id           },
		{ "err_img_bad_name",         &err_img_bad_name         },
		{ "err_img_dupe_name_bad_id", &err_img_dupe_name_bad_id },
		{ "err_img_dupe_id_bad_name", &err_img_dupe_id_bad_name },
		{ "img_dupe",                 &img_dupe                 },
		{ "err_ver_bad_id",           &err_ver_bad_id           },
		{ "err_ver_bad_hash",         &err_ver_bad_hash         },
		{ "err_ver_bad_ver",          &err_ver_bad_ver          },
		{ "err_ver_bad_hex",          &err_ver_bad_hex          },
		{ "err_ver_bad_dupe_id",      &err_ver_bad_dupe_id      },
		{ "err_ver_bad_dupe_hash",    &err_ver_bad_dupe_hash    },
		{ "err_ver_bad_dupe_ver",     &err_ver_bad_dupe_ver     },
		{ "ver_link",                 &ver_link                 },
	};

	return tcase_build_suite("imgset", tests, sizeof(tests));
}
