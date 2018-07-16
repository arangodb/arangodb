Starting Manually
=================

This section describes how to start an ArangoDB stand-alone instance by manually
starting the needed process.

Local Start
-----------

We will assume that your IP is 127.0.0.1 and that the port 8529 is free:

```
arangod --server.endpoint tcp://0.0.0.0:8529 \
	--database.directory standalone &
```

Manual Start in Docker
----------------------

Manually starting a stand-alone instance via Docker is basically the same as described
in the paragraph above. 

A bit of extra care has to be invested due to the way in which Docker isolates its network. 
By default it fully isolates the network and by doing so an endpoint like `--server.endpoint tcp://0.0.0.0:8529`
will only bind to all interfaces inside the Docker container which does not include
any external interface on the host machine. This may be sufficient if you just want
to access it locally but in case you want to expose it to the outside you must
facilitate Dockers port forwarding using the `-p` command line option. Be sure to
check the [official Docker documentation](https://docs.docker.com/engine/reference/run/).

You can simply use the `-p` flag in Docker to make the individual processes available on the host
machine or you could use Docker's [links](https://docs.docker.com/engine/reference/run/)
to enable process intercommunication.

An example configuration might look like this:

```
docker run -e ARANGO_NO_AUTH=1 -p 192.168.1.1:10000:8529 arangodb/arangodb arangod \
	--server.endpoint tcp://0.0.0.0:8529\
```

This will start a single server within a Docker container with an isolated network. 
Within the Docker container it will bind to all interfaces (this will be 127.0.0.1:8529
and some internal Docker IP on port 8529). By supplying `-p 192.168.1.1:10000:8529`
we are establishing a port forwarding from our local IP (192.168.1.1 port 10000 in
this example) to port 8529 inside the container. 

### Authentication

To start the official Docker container you will have to decide on an authentication
method, otherwise the container will not start.

Provide one of the arguments to Docker as an environment variable. There are three
options:

1. ARANGO_NO_AUTH=1

   Disable authentication completely. Useful for local testing or for operating
   in a trusted network (without a public interface).
        
2. ARANGO_ROOT_PASSWORD=password

   Start ArangoDB with the given password for root.
        
3. ARANGO_RANDOM_ROOT_PASSWORD=1

   Let ArangoDB generate a random root password.
       
For an in depth guide about Docker and ArangoDB please check the official documentation:
https://hub.docker.com/r/arangodb/arangodb/ . Note that we are using the image
`arangodb/arangodb` here which is always the most current one. There is also the
"official" one called `arangodb` whose documentation is here: https://hub.docker.com/_/arangodb/
