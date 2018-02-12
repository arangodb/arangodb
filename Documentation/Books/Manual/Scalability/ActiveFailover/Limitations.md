# Active Failover Limitations

The _Active Failover_ setup in ArangoDB has a few limitations. Some of these limitations may be removed in later versions of ArangoDB:

- Even though it is already possible to have several _followers_ of the same _leader_, currently only one _follower_ is officially supported
- Should you add more than one Follower, be aware that during a failover situation there is no preference for followers
  which are more up to date with the failed _leader_.
- At the moment it is not possible to read from _followers_
- All requests will be redirected to the _leader_