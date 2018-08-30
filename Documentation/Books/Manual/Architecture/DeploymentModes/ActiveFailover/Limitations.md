Active Failover Limitations
===========================

The _Active Failover_ setup in ArangoDB has a few limitations. Some of these limitations 
may be removed in later versions of ArangoDB:

- Even though it is already possible to have several _followers_ of the same _leader_,
 currently only one _follower_ is officially supported
- Should you add more than one _follower_, be aware that during a _failover_ situation
 the failover attempts to pick the most up to date follower as a new leader, 
 but there is **no guarantee** on how much operations may have been lost.
