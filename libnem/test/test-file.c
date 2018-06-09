#include "test.h"

START_TEST(read_file)
{
	NEM_file_t file;
	ck_err(NEM_file_init(&file, "test/data/hello"));
	ck_assert_int_eq(6, NEM_file_len(&file));
	ck_assert_str_eq("hello", NEM_file_data(&file));
	NEM_file_free(&file);
}
END_TEST

START_TEST(err_read_dir)
{
	NEM_file_t file;
	NEM_err_t err = NEM_file_init(&file, "test/data");
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(err_read_noent)
{
	NEM_file_t file;
	NEM_err_t err = NEM_file_init(&file, "test/data/NOTHING");
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(read_empty)
{
	NEM_file_t file;
	ck_err(NEM_file_init(&file, "test/data/empty"));
	ck_assert_int_eq(0, NEM_file_len(&file));
	ck_assert_ptr_eq(NULL, NEM_file_data(&file));
	NEM_file_free(&file);
}
END_TEST

Suite*
suite_file()
{
	tcase_t tests[] = {
		{ "read_file",      &read_file      },
		{ "err_read_dir",   &err_read_dir   },
		{ "err_read_noent", &err_read_noent },
		{ "read_empty",     &read_empty     },
	};

	return tcase_build_suite("file", tests, sizeof(tests));
}
