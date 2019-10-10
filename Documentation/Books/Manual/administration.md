---
layout: default
description: Deployments of ArangoDB servers can be managed with the following tools
---
Administration
==============

Tools
-----

Deployments of ArangoDB servers can be managed with the following tools:

- [**Web interface**](programs-web-interface.html):
  [_Arangod_](programs-arangod.html) serves a graphical web interface to
  be accessed with a browser via the server port. It provides basic and advanced
  functionality to interact with the server and its data.
  
  {%- comment %}TODO: In case of a cluster, the web interface can be reached via any of the coordinators. What about other deployment modes?{% endcomment %}

- **ArangoShell**: [_Arangosh_](programs-arangosh.html) is a V8 shell to
  interact with any local or remote ArangoDB server through a JavaScript
  interface. It can be used to automate tasks. Some developers may prefer it over
  the web interface, especially for simple CRUD. It is not to be confused with
  general command lines like Bash or PowerShell.

- **RESTful API**: _Arangod_ has an [HTTP interface](../http/) through
  which it can be fully managed. The official client tools including _Arangosh_ and
  the Web interface talk to this bare metal interface. It is also relevant for
  [driver](../drivers/) developers.

- [**ArangoDB Starter**](programs-starter.html): This deployment tool
  helps to start _Arangod_ instances, like for a Cluster or an Active Failover setup.
  
For a full list of tools, please refer to the [Programs & Tools](programs.html) chapter.

Deployment Administration
-------------------------

- [Master/Slave](administration-master-slave.html)
- [Active Failover](administration-active-failover.html)
- [Cluster](administration-cluster.html)
- [Datacenter to datacenter replication](administration-dc2-dc.html)
- [ArangoDB Starter Administration](administration-starter.html)

Other Topics
------------

- [Configuration](administration-configuration.html)
- [Backup & Restore](backup-restore.html)
- [Import & Export](administration-import-export.html)
- [User Management](administration-managing-users.html)
- [Switch Storage Engine](administration-engine-switch-engine.html)

