Cluster Limitations
===================

ArangoDB has no built-in limitations to horizontal scalability. The
central resilient _Agency_ will easily sustain hundreds of _DBservers_
and _Coordinators_, and the usual database operations work completely
decentrally and do not require assistance of the _Agency_.

Likewise, the supervision process in the _Agency_ can easily deal
with lots of servers, since all its activities are not performance
critical.

Obviously, an ArangoDB Cluster is limited by the available resources
of CPU, memory, disk and network bandwidth and latency.
