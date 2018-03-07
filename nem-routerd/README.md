# nem-routerd

`nem-routerd` is the routing daemon for a NEM host. It should be a singleton
on each host running outside of a jail (ideally using capsicum to restrict
operations to network operations). It's responsible for:

 * Routing local-only requests.
 * Routing requests externally.
 * Maintaining cluster state.

