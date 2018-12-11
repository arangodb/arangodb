Active Failover Limitations
===========================

The _Active Failover_ setup in ArangoDB has a few limitations. Some of these limitations 
may be removed in later versions of ArangoDB:

- Should you add more than one _follower_, be aware that during a _failover_ situation
 the failover attempts to pick the most up to date follower as the new leader on a **best-effort** basis. 
- In contrast to full ArangoDB Cluster (with synchronous replication), there is **no guarantee** on 
  how many database operations may have been lost during a failover.
- Should you be using the [ArangoDB Starter](../../../Programs/Starter/README.md) 
  or the [Kubernetes Operator](../../../Deployment/Kubernetes/README.md) to manage your Active-Failover
  deployment, be aware that upgrading might trigger an unintentional failover between machines.

