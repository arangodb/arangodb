!SECTION ArangoDB Cluster and Docker

!SUBSECTION Networking

A bit of extra care has to be invested due to the way in which Docker isolates its network. By default it fully isolates the network and by doing so an endpoint like `--server.endpoint tcp://0.0.0.0:8529` will only bind to all interfaces inside the Docker container which does not include any external interface on the host machine. This may be sufficient if you just want to access it locally but in case you want to expose it to the outside you must facilitate Dockers port forwarding using the `-p` command line option. Be sure to check the [official Docker documentation](https://docs.docker.com/engine/reference/run/).

To simply make arangodb available on all host interfaces on port 8529:

`docker run -p 8529:8529 -e ARANGO_NO_AUTH=1 arangodb`

Another possibility is to start Docker via network mode `host`. This is possible but generally not recommended. To do it anyway check the Docker documentation for details.

!SUBSUBSECTION Docker and Cluster tasks

To start the cluster via Docker is basically the same as starting [locally](Local.md) or on [multiple machines](Distributed.md). However just like with the single networking image we will face networking issues. You can simply use the `-p` flag to make the individual task available on the host machine or you could use Docker's [links](https://docs.docker.com/engine/reference/run/) to enable task intercommunication.

Please note that there are some flags that specify how ArangoDB can reach a task from the outside. These are very important and built for this exact usecase. An example configuration might look like this:

```
docker run -e ARANGO_NO_AUTH=1 -p 192.168.1.1:10000:8529 arangodb/arangodb arangod --server.endpoint tcp://0.0.0.0:8529 --cluster.my-address tcp://192.168.1.1:10000 --cluster.my-local-info db1 --cluster.my-role PRIMARY --cluster.agency-endpoint tcp://192.168.1.1:5001 --cluster.agency-endpoint tcp://192.168.1.2:5002 --cluster.agency-endpoint tcp://192.168.1.3:5003
```

This will start a primary DB server within a Docker container with an isolated network. Within the Docker container it will bind to all interfaces (this will be 127.0.0.1:8529 and some internal Docker ip on port 8529). By supplying `-p 192.168.1.1:10000:8529` we are establishing a port forwarding from our local IP (192.168.1.1 port 10000 in this example) to port 8529 inside the container. Within the command we are telling arangod how it can be reached from the outside `--cluster.my-address tcp://192.168.1.1:10000`. This information will be forwarded to the agency so that the other tasks in your cluster can see how this particular DBServer may be reached.
