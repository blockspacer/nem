# NOTES

Want the following services to have the cluster boostrap itself.

 * `nem-rootd`
   - `nem-gitd`
   - `nem-builderd`
   - `nem-artifactd`

## `nem-rootd`

Host process that mounts filesystem images and starts jails.

## `nem-gitd`

Process that manages a set of git repositories. Provides interaction over
SSH to effectively run `git receive-pack`. Needs a bunch of extra utilities
to function properly.

## `nem-builderd`

On-demand process that can take a bunch of files from `nem-gitd` and some kind
of build manifest (?) and produce a set of build artifacts. 

## `nem-artifactd`

Process that stores build artifacts and logs.
