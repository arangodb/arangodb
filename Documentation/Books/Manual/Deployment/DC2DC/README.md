<!-- don't edit here, its from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# Datacenter to datacenter replication deployment

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

This chapter describes how to deploy all the components needed for _datacenter to
datacenter replication_.

For a general introduction to _datacenter to datacenter replication_, please refer
to the [Datacenter to datacenter replication](../../Architecture/DeploymentModes/DC2DC/README.md) chapter.

[Requirements](../../Architecture/DeploymentModes/DC2DC/Requirements.md) can be found in this section.

Deployment steps:

- [Cluster](Cluster.md)
- [ArangoSync Master](ArangoSyncMaster.md)
- [ArangoSync Workers](ArangoSyncWorkers.md)
- [Prometheus & Grafana (optional)](PrometheusGrafana.md)

When using the `kafka` type message queue, you also have to deploy:

- [Kafka & Zookeeper](KafkaZookeeper.md)
