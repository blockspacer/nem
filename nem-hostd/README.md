# nem-hostd

## config.yaml

```
# rootdir is the root directory for the daemon to run in; all paths are
# relative to the rootdir. A bunch of stuff gets dumped in here. Maybe it
# should be configurable later.
rootdir: /usr/local/nem

# jails contains a list of jail objects that should run under this host
jails:
    # name is an arbitrary name for the jail.
  - name: foo
    # config is a symbolic reference to the jail config that should be
    # loaded for this jail.
    config: foo 
```

## jails/jailname.yaml

```
# jail_id is the symbolic name for this jail.
jail_id: foo

# isolation_flags indicate how the root process is executed. By default it
# runs exe_path with jail+no_network.
isolation_flags:
  - (no_)jail
  - (no_)capsicum
  - (no_)network

# exe_path is a path to a nem executable (e.g. NEM_KQ_PARENT_FILENO is
# a unix fd for communication) which is executed when the jail starts.
# When the process terminates, the jail is torn down.
exe_path: /bin/start

# images is a list of filesystem images that are mounted before exe_path
# is executed. Each image should have a type field and most should have
# a name field. name/semver/sha256 are used to resolve an actual image
# from the image database.
images:
	# image-type is a ufs2 filesystem in a file. These can be created
	# with makefs and can be versioned. image-types are always read-only.
  - type: image
	dest: /
	name: base.img
	semver: ^1.2.3 # optional, omitted uses latest.
	sha256: hurr   # optional

	# singlefile-type is a file that just gets copied into the filesystem
	# on boot. The dest path must be writable.
  - type: singlefile
	dest: /etc/resolv.conf
	name: default-resolv.conf

	# vnode-type is a read/write filesystem that's created on-demand. It
	# has a fixed size (len). If persist is not set, the backing file is
	# destroyed when the jail is terminated.
  - type: vnode
	dest: /var/db
	len: 1000000
	persist: true

	# shared-type is a read/write filesystem that can be mounted
	# on multiple jails. I'm not really sure what this would be used for.
  - type: shared
	dest: /var/shared
	name: shared.img
	len: 1000000
```
