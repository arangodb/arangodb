<!-- don't edit here, it's from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# Kafka & Zookeeper

{% hint 'tip' %}
We recommend to use DirectMQ instead of Kafka as message queue.
{% endhint %}

## Recommended deployment environment

Since the Kafka brokers are really CPU and memory intensive,
it is recommended to run Zookeeper & Kafka on dedicated machines.

Consider these machines "pets".
