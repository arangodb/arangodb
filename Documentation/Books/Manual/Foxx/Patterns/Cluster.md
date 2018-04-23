Foxx in a cluster setup
=======================

When running ArangoDB in a cluster the Foxx services will run on each coordinator. Installing, upgrading and uninstalling services on any coordinator will automatically distribute the service to the other coordinators, making deployments as easy as in single-server mode. However, this means there are some limitations:

You should avoid any kind of file system state beyond the deployed service bundle itself. Don't write data to the file system or encode any expectations of the file system state other than the files in the service folder that were installed as part of the service (e.g. file uploads or custom log files).

Additionally, the [development mode](DevelopmentMode.md) will lead to an inconsistent state of the cluster until it is disabled. While a service is running in development mode you can make changes to the service on the filesystem of any coordinator and see them reflected in real time just like when running ArangoDB as a single server. However the changes made on one coordinator will not be reflected across other coordinators until the development mode is disabled. When disabling the development mode for a service, the coordinator will create a new bundle and distribute it across the service like a manual upgrade of the service.

For these reasons it is strongly recommended not to use development mode in a cluster with multiple coordinators unless you are sure that no requests or changes will be made to other coordinators while you are modifying the service. Using development mode in a production cluster is extremely unsafe and highly discouraged.
