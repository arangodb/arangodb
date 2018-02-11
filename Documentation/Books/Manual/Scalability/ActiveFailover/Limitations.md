# Active Failover Limitations

The _Active Failover_ setup in ArangoDB has a few limitations. Some of these limitations may be removed in later versions of ArangoDB:

- even though it is already possible to have several _followers_ of the same _leader_, currently only one _follower_ is officially supported
- currently it is not possible to read from the _followers_
