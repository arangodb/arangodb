---
layout: default
description: Datacenter to datacenter replication deployment overview
title: DC2DC Replication Deployment
---
# Datacenter to datacenter replication deployment

{% hint 'info' %}
Datacenter to datacenter replication is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/){:target="_blank"},
also available as [**managed service**](https://www.arangodb.com/managed-service/){:target="_blank"}.
{% endhint %}

This chapter describes how to deploy all the components needed for
_datacenter to datacenter replication_.

For a general introduction to _datacenter to datacenter replication_, please refer
to the [Datacenter to datacenter replication](architecture-deployment-modes-dc2-dc.html) chapter.

[Requirements](architecture-deployment-modes-dc2-dc-requirements.html) can be found in this section.

Deployment steps:

- [Cluster](deployment-dc2-dc-cluster.html)
- [ArangoSync Master](deployment-dc2-dc-arango-sync-master.html)
- [ArangoSync Workers](deployment-dc2-dc-arango-sync-workers.html)
- [Prometheus & Grafana (optional)](deployment-dc2-dc-prometheus-grafana.html)

When using the `kafka` type message queue, you also have to deploy:

- [Kafka & Zookeeper](deployment-dc2-dc-kafka-zookeeper.html)
