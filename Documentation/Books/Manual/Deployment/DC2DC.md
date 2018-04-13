<!-- don't edit here, its from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# Datacenter to datacenter replication deployment

This chapter describes how to deploy all the components needed for _datacenter to
datacenter replication_.

For a general introduction to _datacenter to datacenter replication_, please refer
to the [Datacenter to datacenter replication](../Scalability/DC2DC/README.md) chapter.

[Requirements](../Scalability/DC2DC/Requirements.md) can be found in this section.

Deployment steps:

- [Cluster](DC2DC/Cluster.md)
- [ArangoSync Master](DC2DC/ArangoSyncMaster.md)
- [ArangoSync Workers](DC2DC/ArangoSyncWorkers.md)
- [Prometheus & Grafana (optional)](DC2DC/PrometheusGrafana.md)

When using the `kafka` type message queue, you also have to deploy:

- [Kafka & Zookeeper](DC2DC/KafkaZookeeper.md)
