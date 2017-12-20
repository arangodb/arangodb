Limitations
-----------

ArangoDB has no built-in limitations to horizontal scalability. The
central resilient Agency will easily sustain hundreds of DBservers
and coordinators, and the usual database operations work completely
decentrally and do not require assistance of the Agency.

Likewise, the supervision process in the Agency can easily deal
with lots of servers, since all its activities are not performance
critical.

Obviously, an ArangoDB cluster is limited by the available resources
of CPU, memory, disk and network bandwidth and latency.
