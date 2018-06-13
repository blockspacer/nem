#include "nem.h"
#include "nem-marshal-macros.h"
#include "c-config.h"
#include "c-args.h"
#include "utils.h"

static NEM_rootd_config_t static_config = {0};

#define TYPE NEM_rootd_config_jail_t
static const NEM_marshal_field_t NEM_rootd_config_jail_fs[] = {
	{ "name",   NEM_MARSHAL_STRING, O(name),   -1, NULL, },
	{ "config", NEM_MARSHAL_STRING, O(config), -1, NULL, },
};
MAP(NEM_rootd_config_jail_m, NEM_rootd_config_jail_fs);
#undef TYPE

#define TYPE NEM_rootd_config_t
static const NEM_marshal_field_t NEM_rootd_config_fs[] = {
	{ "rundir",    NEM_MARSHAL_STRING, O(rundir),    -1, NULL },
	{ "configdir", NEM_MARSHAL_STRING, O(configdir), -1, NULL },
	{
		"jails",
		NEM_MARSHAL_ARRAY|NEM_MARSHAL_STRUCT, 
		O(jails), O(jails_len),
		&NEM_rootd_config_jail_m,
	},
};
MAP(NEM_rootd_config_m, NEM_rootd_config_fs);
#undef TYPE

static void
config_free(NEM_rootd_config_t *this)
{
	for (size_t i = 0; i < this->jails_len; i += 1) {
		NEM_jailimg_t *jail_cfg = this->jails[i].img_config;
		if (NULL != jail_cfg) {
			NEM_unmarshal_free(
				&NEM_jailimg_m,
				jail_cfg,
				sizeof(*jail_cfg)
			);
		}
	}

	NEM_unmarshal_free(&NEM_rootd_config_m, this, sizeof(*this));
}

static NEM_err_t
open_jail_config(NEM_rootd_config_jail_t *this, NEM_rootd_config_t *cfg)
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
		&NEM_rootd_config_jail_m,
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
	bzero(&static_config, sizeof(static_config));
	static_config.is_init = (1 == getpid());
	static_config.is_root = (0 == geteuid());

	char *config_path = strdup(NEM_rootd_config_path());
	NEM_err_t err = NEM_path_abs(&config_path);
	if (!NEM_err_ok(err)) {
		NEM_panicf_errno(
			"error: unable to resolve path %s",
			NEM_rootd_config_path()
		);
	}

	NEM_file_t file;
	err = NEM_file_init(&file, config_path);
	if (!NEM_err_ok(err)) {
		NEM_panicf(
			"error: unable to find config at %s: %s",
			config_path,
			NEM_err_string(err)
		);
	}
	free(config_path);
	if (0 == NEM_file_len(&file)) {
		NEM_file_free(&file);
		return NEM_err_static("config file is empty");
	}

	err = NEM_unmarshal_yaml(
		&NEM_rootd_config_m,
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

	// If rundir isn't set, err, I guess it's the current directory?
	if (NULL == static_config.rundir) {
		static_config.rundir = strdup("./");
	}
	err = NEM_ensure_dir(static_config.rundir);
	if (!NEM_err_ok(err)) {
		config_free(&static_config);
		return err;
	}

	// If configdir is unset, default it to rundir/config.
	if (NULL == static_config.configdir) {
		asprintf(
			(char**)&static_config.configdir,
			"%s/config/",
			static_config.rundir
		);
	}
	err = NEM_ensure_dir(static_config.configdir);
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
teardown(NEM_app_t *app)
{
	config_free(&static_config);
}

const NEM_app_comp_t NEM_rootd_c_config = {
	.name     = "config",
	.setup    = &setup,
	.teardown = &teardown,
};

const char*
NEM_rootd_run_root()
{
	NEM_panic_if_null((void*)static_config.rundir);
	return static_config.rundir;
}

const char*
NEM_rootd_config_root()
{
	NEM_panic_if_null((void*)static_config.configdir);
	return static_config.configdir;
}

const char*
NEM_rootd_routerd_path()
{
	// XXX
	return "/home/lye/code/go/src/nem.rocks/routerd/routerd";
}
