!SECTION Installing Foxx on a cluster

The Foxx installation process is agnostic of the cluster architecture.
You simply use the foxx manager as described [earlier](../Install/README.md) and connect it to any of your coordinators.
This will automatically spread the Foxx across all coordinators in the cluster.

!SUBSECTION Development Mode

Within a Cluster the development mode of Foxx is totally unsupported. The spreading of information will not be executed on each change on disc.
Please develop your Foxx in a single server development environment and upload it to the cluster in production mode.

!SUBSECTION Restrictions

The code you write in Foxx should be independent from the server it is running on.
This means you should avoid using the file-system module `fs` as this is writing on the local file system.
This will get out of sync if you use more than one server.
If you install 3rd party libraries please make sure they do not rely on the local file system.
All other operations should be save and the code for single server and cluster is identical.
