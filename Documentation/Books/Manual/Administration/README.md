Administration
==============

Tools
-----

Deployments of ArangoDB servers can be managed with the following tools:

- [**Web interface**](../Programs/WebInterface/README.md):
  [Arangod](../Programs/Arangod/README.md) serves a graphical web interface to
  be accessed with a browser via the server port. It provides basic and advanced
  functionality to interact with the server and its data.
{### TODO: In case of a cluster, the web interface can be reached via any of the coordinators. What about other deployment modes? ###}

- **ArangoShell**: [Arangosh](../Programs/Arangosh/README.md) is a V8 shell to
  interact with any local or remote ArangoDB server through a JavaScript
  interface. It can be used to automate tasks. Some developers may prefer it over
  the web interface, especially for simple CRUD. It is not to be confused with
  general command lines like Bash or PowerShell.

- **RESTful API**: Arangod has an [HTTP interface](../../HTTP/index.html) through
  which it can be fully managed. The official client tools including Arangosh and
  the Web interface talk to this bare metal interface. It is also relevant for
  [driver](../../Drivers/index.html) developers.

- [**ArangoDB Starter**](../Programs/Starter/README.md): This deployment tool
  helps to start Arangod instances, like for a cluster or a resilient setup.

Topics
------

- [Backup & Restore](BackupRestore.md)
- [Import & Export](ImportExport.md)
