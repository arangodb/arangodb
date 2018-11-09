Automatic native Clusters
-------------------------
Similarly to how the Mesos framework aranges an ArangoDB cluster in a
DC/OS environment for you, `arangodb` can do this for you in a plain
environment.

By invoking the first `arangodb` you launch a primary node. It will
bind a network port, and output the commands you need to cut'n'paste
into the other nodes. Let's review the process of such a startup on
three hosts named `h01`, `h02`, and `h03`: 

    arangodb@h01 ~> arangodb --starter.address h01 --starter.port 4000
	2018-11-09T07:18:45-05:00 |INFO| Starting arangodb version 0.13.7, build bdac926 component=arangodb
	2018-11-09T07:18:45-05:00 |INFO| Serving as master with ID '0191e3d2' on h01:4000... component=arangodb
	2018-11-09T07:18:45-05:00 |INFO| Waiting for 3 servers to show up.
	component=arangodb
	2018-11-09T07:18:45-05:00 |INFO| Use the following commands to start other servers: component=arangodb

	arangodb --starter.data-dir=./db2 --starter.join h01:4000

	arangodb --starter.data-dir=./db3 --starter.join h01:4000

	2018-11-09T07:18:45-05:00 |INFO| ArangoDB Starter listening on 0.0.0.0:4000 (h01:4000) component=arangodb

So you cut the lines `arangodb --data.dir=./db2 --starter.join
127.0.0.1` and execute them for the other nodes. If you run it on
another node on your network, replace the `--starter.join 127.0.0.1`
by the public IP of the first host. 

    arangodbh02 ~> arangodb --starter.data-dir=./db2 --starter.join h01:4000
    2018-11-09T09:04:07-05:00 |INFO| Starting arangodb version 0.13.7, build bdac926 component=arangodb
	2018-11-09T09:04:08-05:00 |INFO| Contacting master http://h01:4000... component=arangodb
	2018-11-09T09:04:08-05:00 |INFO| Waiting for 3 servers to show up... component=arangodb
	2018-11-09T09:04:08-05:00 |INFO| ArangoDB Starter listening on 0.0.0.0:4000 (:4000) component=arangodb

    arangodbh03 ~> arangodb --starter.data-dir=./db3 --starter.join h01:4000
    2018-11-09T09:04:07-05:00 |INFO| Starting arangodb version 0.13.7, build bdac926 component=arangodb
	2018-11-09T09:04:08-05:00 |INFO| Contacting master http://h01:4000... component=arangodb
	2018-11-09T09:04:08-05:00 |INFO| Waiting for 3 servers to show up... component=arangodb
	2018-11-09T09:04:08-05:00 |INFO| ArangoDB Starter listening on 0.0.0.0:4000 (:4000) component=arangodb

Once the two other processes joined the cluster, and started their ArangoDB server processes (this may take a while depending on your system), it will inform you where to connect the Cluster from a Browser, shell or your programm:

    ...
    2018-11-09T09:10:00-05:00 |INFO| coordinator up and running. component=arangodb

At this point you may access your cluster at either coordinator
endpoint, http://h01:4002/, http://h02:4002/ or http://h03:4002/.

Automatic native local test Clusters
------------------------------------

If you only want a local test cluster, you can run a single starter with the `--starter.local` argument.
It will start a 3 "machine" cluster on your local PC.

```
arangodb --starter.local
```

Note. A local cluster is intended only for test purposes since a failure of 
a single PC will bring down the entire cluster.

Automatic Docker Clusters
-------------------------
ArangoDBStarter can also be used to [launch clusters based on docker containers](https://github.com/arangodb-helper/arangodb#running-in-docker).
Its a bit more complicated, since you need to provide information about your environment that can't be autodetected.

In the Docker world you need to take care about where persistant data is stored, since containers are intended to be volatile. We use a volume named `arangodb1` here: 

    docker volume create arangodb1

(You can use any type of docker volume that fits your setup instead.)

We then need to determine the the IP of the docker host where you
intend to run ArangoDB starter on. Depending on your operating system
execute `ip addr, ifconfig or ipconfig` to determine your local ip
address. 

    192.168.1.0/24 dev eth0 proto kernel scope link src 192.168.1.32

So this example uses the IP `192.168.1.32`:

    docker run -it --name=adb1 --rm -p 8528:8528 \
        -v arangodb1:/data \
        -v /var/run/docker.sock:/var/run/docker.sock \
        arangodb/arangodb-starter \
        --starter.address=192.168.1.32

It will start the master instance, and command you to start the slave instances:

    Unable to find image 'arangodb/arangodb-starter:latest' locally
    latest: Pulling from arangodb/arangodb-starter
    Digest: sha256:b87d20c0b4757b7daa4cb7a9f55cb130c90a09ddfd0366a91970bcf31a7fd5a4
    Status: Downloaded newer image for arangodb/arangodb-starter:latest
    2017/06/12 13:26:14 Starting arangodb version 0.7.1, build f128884
    2017/06/12 13:26:14 Serving as master with ID '46a2b40d' on 192.168.1.32:8528...
    2017/06/12 13:26:14 Waiting for 3 servers to show up.
    2017/06/12 13:26:14 Use the following commands to start other servers:

    docker volume create arangodb2 && \
      docker run -it --name=adb2 --rm -p 8533:8528 -v arangodb2:/data \
      -v /var/run/docker.sock:/var/run/docker.sock arangodb/arangodb-starter:0.7 \
      --starter.address=192.168.1.32 --starter.join=192.168.1.32

    docker volume create arangodb3 && \
      docker run -it --name=adb3 --rm -p 8538:8528 -v arangodb3:/data \
      -v /var/run/docker.sock:/var/run/docker.sock arangodb/arangodb-starter:0.7 \
      --starter.address=192.168.1.32 --starter.join=192.168.1.32

Once you start the other instances, it will continue like this: 

    2017/05/11 09:05:45 Added master 'fc673b3b': 192.168.1.32, portOffset: 0
    2017/05/11 09:05:45 Added new peer 'e98ea757': 192.168.1.32, portOffset: 5
    2017/05/11 09:05:50 Added new peer 'eb01d0ef': 192.168.1.32, portOffset: 10
    2017/05/11 09:05:51 Starting service...
    2017/05/11 09:05:51 Looking for a running instance of agent on port 8531
    2017/05/11 09:05:51 Starting agent on port 8531
    2017/05/11 09:05:52 Looking for a running instance of dbserver on port 8530
    2017/05/11 09:05:52 Starting dbserver on port 8530
    2017/05/11 09:05:53 Looking for a running instance of coordinator on port 8529
    2017/05/11 09:05:53 Starting coordinator on port 8529
    2017/05/11 09:05:58 agent up and running (version 3.2.devel).
    2017/05/11 09:06:15 dbserver up and running (version 3.2.devel).
    2017/05/11 09:06:31 coordinator up and running (version 3.2.devel).

And at least it tells you where you can work with your cluster:

    2017/05/11 09:06:31 Your cluster can now be accessed with a browser at `http://192.168.1.32:8529` or
    2017/05/11 09:06:31 using `arangosh --server.endpoint tcp://192.168.1.32:8529`.

Under the hood
--------------
The first `arangodb` you ran (as shown above) will become the master in your setup, the `--starter.join` will be the slaves.

The master determines which ArangoDB server processes to launch on which slave, and how they should communicate. 
It will then launch the server processes and monitor them. Once it has detected that the setup is complete you will get the prompt. The master will save the setup for subsequent starts. 

More complicated setup options [can be found in ArangoDBStarters Readme](https://github.com/arangodb-helper/arangodb#starting-an-arangodb-cluster-the-easy-way). 

