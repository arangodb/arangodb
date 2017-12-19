#How to run ArangoDB in a Docker container

##Problem

How do you make ArangoDB run in a Docker container?

##Solution

ArangoDB is now available as an [official repository in the Docker Hub](https://hub.docker.com/_/arangodb/) (@see documentation there). Furthermore, it could be used from our own repository:

Running ArangoDB - using the ArangoDB repository - is as simple as...

    docker run -p 8529:8529 arangodb/arangodb

I've created an automated build repository on docker, so that you can easily start a docker container with the latest stable release. Thanks to frodenas, hipertracker, joaodubas, webwurst who also created dockerfiles.

ArangoDB generates a random default password for the web-interface which you can copy from the shell.

    Status: Downloaded newer image for arangodb/arangodb:2.7.0
    starting ArangoDB in stand-alone mode
    creating initial user, please wait ...
    ========================================================================
    ArangoDB User: "root"
    ArangoDB Password: "X7GFlFfFn1OTpmvD"
    ========================================================================

Please note that this recipe is a general overview. There is also a recipe explaining how to install an application consisting of a node.js application and an ArangoDB database server.

You can get more information about [Docker and how to use it](https://github.com/arangodb/arangodb-docker) in our Docker repository.

###Usage

First of all you need to start an ArangoDB instance.

In order to start an ArangoDB instance run

    unix> docker run --name arangodb-instance -d arangodb/arangodb

By default ArangoDB listen on port 8529 for request and the image includes `EXPOST 8529`. If you link an application container, it is automatically available in the linked container. See the following examples.

###Using the instance

In order to use the running instance from an application, link the container

    unix> docker run --name my-app --link arangodb-instance

###Running the image

In order to start an ArangoDB instance run

    unix> docker run -p 8529:8529 -d arangodb/arangodb

ArangoDB listen on port 8529 for request and the image includes `EXPOST 8529`. The `-p 8529:8529` exposes this port on the host.

In order to get a list of supported options, run

    unix> docker run -e help=1 arangodb/arangodb

###Persistent Data

*note: if you're running a docker machine on [OS X](https://docs.docker.com/v1.8/installation/mac/) or [Windows](https://docs.docker.com/windows/step_one/), [read first about using docker inside of virtual machines](#docker-inside-of-a-virtual-machine)*

ArangoDB use the volume `/var/lib/arangodb` as database directory to store the collection data and the volume `/var/lib/arangodb-apps` as apps directory to store any extensions. These directory are marked as docker volumes.

See `docker run -e help=1 arangodb` for all volumes.

You can map the container's volumes to a directory on the host, so that the data is kept between runs of the container. This path `/tmp/arangodb` is in general not the correct place to store you persistent files - it is just an example!

    unix> mkdir /tmp/arangodb
    unix> docker run -p 8529:8529 -d \
              -v /tmp/arangodb:/var/lib/arangodb \
              arangodb/arangodb

This will use the `/tmp/arangodb` directory of the host as database directory for ArangoDB inside the container.

Alternatively you can create a container holding the data.

    unix> docker run -d --name arangodb-persist -v /var/lib/arangodb ubuntu:14.04 true

And use this data container in your ArangoDB container.

    unix> docker run --volumes-from arangodb-persist -p 8529:8529 arangodb/arangodb

If want to save a few bytes for you can alternatively use [tianon/true][3] or [progrium/busybox][4] for creating the volume only containers. For example

    unix> docker run -d --name arangodb-persist -v /var/lib/arangodb tianon/true true

###Building an image

Simple clone the repository and execute the following command in the arangodb-docker folder

    unix> docker build -t arangodb .

This will create a image named arangodb.

##Update note
that we have changed the location of the data files, in order
to be compatible with the official docker image (see
https://github.com/docker-library/official-images/pull/728):

- `/var/lib/arangodb` instead of `/data`
- `/var/lib/arangodb-apps` instead of `/apps`
- `/var/log/arangodb` instead of `/logs`

##Docker inside of a virtual machine
There are some setups in which you may want to run a docker container inside of a virtual machine - either you want reproduceable tests for deployment, or its not directly compatible with your hosts os (aka OS X or Windows).
If you only want to mount filesystems native to the VM as persistent volume, everything is fine. But if you need to mount filesystems from your host into the VM and then in term into the docker container, things get complicated.
Even more if the Virtual Machine part is abstracted away from the user - if you're on OS X with `boot to docker` or on windows.
[Please read the upstream documentation howto mount persistent volumes in this case.](https://docs.docker.com/engine/userguide/dockervolumes/#mount-a-host-directory-as-a-data-volume).

##Comment

A good explanation about persistence and docker container can be found here: 

* [Docker In-depth: Volumes][1]
* [Why Docker Data Containers are Good Using host directories][2]


**Author:** [Frank Celler](https://github.com/fceller)

**Tags:** #docker #howto

[1]: http://container42.com/2014/11/03/docker-indepth-volumes/
[2]: https://medium.com/@ramangupta/why-docker-data-containers-are-good-589b3c6c749e
[3]: https://registry.hub.docker.com/u/tianon/true/
[4]: https://registry.hub.docker.com/u/progrium/busybox/


