#include "nem.h"
#include "nem-marshal-macros.h"
#include "config.h"
#include "args.h"

static NEM_hostd_config_t static_config = {0};

#define TYPE NEM_hostd_config_jail_t
static const NEM_marshal_field_t NEM_hostd_config_jail_fs[] = {
	{ "name",   NEM_MARSHAL_STRING, O(name),   -1, NULL, },
	{ "config", NEM_MARSHAL_STRING, O(config), -1, NULL, },
};
MAP(NEM_hostd_config_jail_m, NEM_hostd_config_jail_fs);
#undef TYPE

#define TYPE NEM_hostd_config_t
static const NEM_marshal_field_t NEM_hostd_config_fs[] = {
	{ "rundir",    NEM_MARSHAL_STRING, O(rundir),    -1, NULL },
	{ "configdir", NEM_MARSHAL_STRING, O(configdir), -1, NULL },
	{
		"jails",
		NEM_MARSHAL_ARRAY|NEM_MARSHAL_STRUCT, 
		O(jails), O(jails_len),
		&NEM_hostd_config_jail_m,
	},
};
MAP(NEM_hostd_config_m, NEM_hostd_config_fs);
#undef TYPE

static void
config_free(NEM_hostd_config_t *this)
{
	for (size_t i = 0; i < this->jails_len; i += 1) {
		NEM_hostd_jailimg_t *jail_cfg = this->jails[i].img_config;
		if (NULL != jail_cfg) {
			NEM_unmarshal_free(
				&NEM_hostd_jailimg_m,
				jail_cfg,
				sizeof(*jail_cfg)
			);
		}
	}

	NEM_unmarshal_free(&NEM_hostd_config_m, this, sizeof(*this));
}

static NEM_err_t
open_jail_config(NEM_hostd_config_jail_t *this, NEM_hostd_config_t *cfg)
{
	char *config_path = NULL;
	asprintf(
		&config_path,
		"%s/jails/%s.yaml",
		cfg->configdir,
		this->config
	);

	NEM_file_t file;
	NEM_err_t err = NEM_file_init(&file, config_path);
	if (!NEM_err_ok(err)) {
		free(config_path);
		return err;
	}

	this->img_config = NEM_malloc(sizeof(*this->img_config));
	err = NEM_unmarshal_yaml(
		&NEM_hostd_config_jail_m,
		this->img_config,
		sizeof(*this->img_config),
		NEM_file_data(&file),
		NEM_file_len(&file)
	);
	NEM_file_free(&file);

	return err;
}

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	static_config.is_init = (1 == getpid());
	static_config.is_root = (0 == geteuid());

	const char *config_path = NEM_hostd_args()->config_path;
	if (NULL == config_path) {
		// Default this to config.yaml in the current directory.
		config_path = "config.yaml";
	}

	NEM_file_t file;
	NEM_err_t err = NEM_file_init(&file, config_path);
	if (!NEM_err_ok(err)) {
		return err;
	}

	err = NEM_unmarshal_yaml(
		&NEM_hostd_config_m,
		&static_config,
		sizeof(static_config),
		NEM_file_data(&file),
		NEM_file_len(&file)
	);

	NEM_file_free(&file);

	if (!NEM_err_ok(err)) {
		config_free(&static_config);
		return err;
	}

	for (size_t i = 0; i < static_config.jails_len; i += 1) {
		err = open_jail_config(&static_config.jails[i], &static_config);
		if (!NEM_err_ok(err)) {
			config_free(&static_config);
			return err;
		}
	}

	return err;
}

static void
teardown()
{
	config_free(&static_config);
}

const NEM_app_comp_t NEM_hostd_c_config = {
	.name     = "config",
	.setup    = &setup,
	.teardown = &teardown,
};

const NEM_hostd_config_t*
NEM_hostd_config()
{
	return &static_config;
}
