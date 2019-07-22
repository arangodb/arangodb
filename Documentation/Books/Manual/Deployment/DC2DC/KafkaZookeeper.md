<!-- don't edit here, it's from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# Kafka & Zookeeper

{% hint 'tip' %}
We recommend to use DirectMQ instead of Kafka as message queue,
because it is simpler to use and tailored to the needs of ArangoDB.
It also removes the need for Zookeeper.

DirectMQ is available since ArangoSync v0.5.0
(ArangoDB Enterprise Edition v3.3.8).
{% endhint %}

## Recommended deployment environment

Since the Kafka brokers are really CPU and memory intensive,
it is recommended to run Zookeeper & Kafka on dedicated machines.
Consider these machines "pets".
