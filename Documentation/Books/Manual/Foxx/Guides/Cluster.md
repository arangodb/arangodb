Foxx in a cluster setup
=======================

When running ArangoDB in a cluster the Foxx services will run on each coordinator. Installing, upgrading and uninstalling services on any coordinator will automatically distribute the service to the other coordinators, making deployments as easy as in single-server mode.

The same considerations that apply to writing Foxx services for a standalone server also apply to writing services for a cluster:

You should avoid any kind of file system state beyond the deployed service bundle itself. Don't [write data to the file system](Files.md) or encode any expectations of the file system state other than the files in the service folder that were installed as part of the service (e.g. don't use file uploads or custom log files).

Additionally, special precautions need to be taken when using the [development mode in a cluster](DevelopmentMode.md#In-a-cluster).
