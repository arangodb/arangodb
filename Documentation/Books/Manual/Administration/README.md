Administration
==============

Tools
-----

Deployments of ArangoDB servers can be managed with the following tools:

- [**Web interface**](): [arangod]() serves a graphical web interface to be accessed via the server port with a browser. It provides basic and advanced functionality to interact with the server and its data.

- **ArangoShell**: [arangosh]() is a V8 shell to interact with any local or remote ArangoDB server through a JavaScript interface. It can be used to automate tasks. Some developers may prefer it over the web interface, especially for simple CRUD. It is not to be confused with general command lines like Bash or PowerShell.

- **RESTful API**: arangod has an [HTTP interface]() through which it can be fully managed. The official client tools including arangosh and the Web interface talk to this bare metal interface. It is also relevant for [driver]() developers.

- [**ArangoDB Starter**](): This deployment tool helps to start arangod instances, like for a cluster or a resilient setup.

<!-- In case of a cluster, the web interface can be reached via any of the coordinators. (?) -->

Topics
------
