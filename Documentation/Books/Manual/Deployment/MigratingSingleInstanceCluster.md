Migrating from a _Single Instance_ to a _Cluster_
==================================================

{% hint 'info' %}
Before migrating from a _Single Instance_ to a Cluster,
please read about
[**Single Instance vs. Cluster**](../Architecture/SingleInstanceVsCluster.md)
{% endhint %}

To migrate from a _Single Instance_ to a _Cluster_ you will need
to take a backup from the _Single Instance_ and restore it into
the _Cluster_ with the tools [_arangodump_](../Programs/Arangodump/README.md)
and [_arangorestore_](../Programs/Arangorestore/README.md).

{% hint 'warning' %}
If you have developed your application using a _Single Instance_
and you would like to use a _Cluster_ now, before upgrading your production
system please test your application with the _Cluster_ first.

If both your _Single Instance_ and _Cluster_ are running on the same
machine, they should have distinct data directories. It is not possible
to start a _Cluster_ on the data directory of a _Single Instance_.
{% endhint %}
