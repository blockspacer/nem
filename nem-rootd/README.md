# nem-rootd

`nem-rootd` is the root daemon for a NEM host. It must be run as root and is 
responsible for:

 * Handling the startup process. Eventually this will replace init.
 * Spinning up other unjailed hosts (nem-routerd).
 * Jail allocation, running, and freeing.

## Jails

Jails are defined by some metadata, specifically:

 * How the jail fs is set up (base image, second image, etc).
 * What the root process of the jail is. This must speak the NEM protocol.
 * What dependencies the jail has w.r.t network services.
 * What network services the jail can access, and with what rights.

The last bits are a bit poorly-defined.

### Jail FS

There are a couple of different ways that a jail can be set up. There are 
several kinds of mounts we need to support.

 * Single image. A single read-only filesystem image is used as the root.
 * MD image. A vnode-backed fixed-size file image is created and mounted r/w.
 * Disk image. A disk-packed partition or nullfs is created and mounted r/w.

Realistically, each jail is going to want several of these simultaneously
(e.g. one or more ro single images that define the application, and one or
more r/w images for storage). Partitioning is kind of a mess, but has better
i/o numbers than vnode-backed md images. We don't need to make distinct 
disk partitions; those can technically be grouped together and we can nullfs
mount them into multiple jails (though this prevents accurate disk tracking).

Anyway each jail needs a thing that describes the images mounted, in which
order. Figure this goes in a TOML file. With the other bit.
