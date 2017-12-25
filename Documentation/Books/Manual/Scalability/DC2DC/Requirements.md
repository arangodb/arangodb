# Requirements 

To use _ArangoSync_ you need the following:

- Two datacenters, each running an ArangoDB Enterprise cluster, version 3.3 or higher.
- A network connection between both datacenters with accessible endpoints
  for several components (see individual components for details).
- TLS certificates for ArangoSync master instances (can be self-signed).
- TLS certificates for Kafka brokers (can be self-signed).
- Optional (but recommended) TLS certificates for ArangoDB clusters (can be self-signed).
- Client certificates CA for ArangoSync masters (typically self-signed).
- Client certificates for ArangoSync masters (typically self-signed).
- At least 2 instances of the ArangoSync master in each datacenter.
- One instances of the ArangoSync worker on every machine in each datacenter.

Note: In several places you will need a (x509) certificate. 
<br/>The [..\..\Security\DC2DC\README.md](#certificates) section provides more guidance for creating 
and renewing these certificates.

Besides the above list, you probably want to use the following:

- An orchestrator to keep all components running. In this tutorial we will use `systemd` as an example.
- A log file collector for centralized collection & access to the logs of all components.
- A metrics collector & viewing solution such as Prometheus + Grafana.
