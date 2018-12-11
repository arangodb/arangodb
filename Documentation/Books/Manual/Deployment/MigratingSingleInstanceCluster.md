Migrating from a _Single Instance_ to a _Cluster_
==================================================

{% hint 'warning' %}
Before migrating from a _Single Instance_ to a Cluster,
please make sure to read the 
["Single Instance vs. Cluster"](../Architecture/DifferenceSingleCluster.md)
section
{% endhint %}

To migrate from a _Single Instance_ to a _Cluster_ you will need
to take a backup from the _Single Instance_ and restore it into
the _Cluster_. The tools [_arangodump_](../Programs/Arangodump/README.md)
and [_arangorestore_](../Programs/Arangorestore/README.md) can be used
for such scope.

After that, the _arangod_ server should be restarted with the desired storage
engine selected (this can be done by setting the option *--server.storage-engine*) 
and using a non-existing data directory.

When the server is up and running with the desired storage engine, the data
can be re-imported using the tool

{% hint 'warning' %}
If both your _Single Instance_ and _Cluster_ are running on the same
machine, they should have distinct data directories. It is not possible
to start a _Cluster_ on the data directory of a _Single Instance_.
{% endhint %}
