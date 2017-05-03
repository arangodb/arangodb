Automatic native Clusters
-------------------------
Similar as the Mesos framework aranges an ArangoDB cluster in a DC/OS environment for you, ArangoDBStarter can do this for you in a plain environment.

With ArangoDBSarter you launch a primary node. It will bind a network port, and output the commands you need to cut'n'paste into the other nodes: 

    # arangodb 
    2017/04/21 08:16:11 Starting arangodb version 0.6.0, build ddddb35
    2017/04/21 08:16:11 Serving as master with ID '85c05e3b' on :4000...
    2017/04/21 08:16:11 Waiting for 3 servers to show up.
    2017/04/21 08:16:11 Use the following commands to start other servers:
    
    arangodb --dataDir=./db2 --join 127.0.0.1
    
    arangodb --dataDir=./db3 --join 127.0.0.1
    
    2017/04/21 08:16:11 Listening on 0.0.0.0:4000 (:4000)

So you cut the lines `arangodb --dataDir=./db2 --join 127.0.0.1` and execute them for the other nodes. If you run it on another node on your network, replace the `--join 127.0.0.1` by the public IP of the first host.

Once the two other processes joined the cluster, and started their ArangoDB server processes (this may take a while depending on your system), it will inform you where to connect the Cluster from a Browser, shell or your programm:

    2017/04/21 08:17:26 Your cluster can now be accessed with a browser at
                        `http://localhost:4002` or
    2017/04/21 08:17:26 using `arangosh --server.endpoint tcp://localhost:4002`.

Automatic Docker Clusters
-------------------------
ArangoDBStarter can also be used to [launch clusters based on docker containers](https://github.com/arangodb-helper/ArangoDBStarter#running-in-docker).
Its a bit more complicated, since you need to provide information about your environment that can't be autodetected.

In the Docker world you need to take care about where persistant data is stored, since containers are intended to be volatile. We use a volume named `arangodb1` here: 

    docker volume create arangodb1

(You can use any type of docker volume that fits your setup instead.)

We then need to determine the the IP of the docker host where you intend to run ArangoDB starter on. 
Depending on your host os, [ipconfig](https://en.wikipedia.org/wiki/Ipconfig) can be used, on a Linux host `/sbin/ip route`:

    192.168.1.0/24 dev eth0 proto kernel scope link src 192.168.1.32

So this example uses the IP `192.168.1.32`:

    docker run -it --name=adb1 --rm -p 4000:4000 \
        -v arangodb1:/data \
        -v /var/run/docker.sock:/var/run/docker.sock \
        arangodb/arangodb-starter \
        --dockerContainer=adb1 --ownAddress=192.168.1.32

It will start the master instance, and command you to start the slave instances:

    2017/04/21 08:39:48 Starting arangodb version 0.6.0, build ddddb35
    2017/04/21 08:39:48 Serving as master with ID '9a67f35a' on 192.168.1.32:4000...
    2017/04/21 08:39:48 Waiting for 3 servers to show up.
    2017/04/21 08:39:48 Use the following commands to start other servers:
    
    docker volume create arangodb2 && \
        docker run -it --name=adb2 --rm -p 4005:4000 -v arangodb2:/data \
        -v /var/run/docker.sock:/var/run/docker.sock arangodb/arangodb-starter \
        --dockerContainer=adb2 --ownAddress=192.168.1.32 --join=192.168.1.32
    
    docker volume create arangodb3 && \
        docker run -it --name=adb3 --rm -p 4010:4000 -v arangodb3:/data \
        -v /var/run/docker.sock:/var/run/docker.sock arangodb/arangodb-starter \
        --dockerContainer=adb3 --ownAddress=192.168.1.32 --join=192.168.1.32
    
    2017/04/21 08:39:48 Listening on 0.0.0.0:4000 (192.168.1.32:4000)

Once you start the other instances, it will continue like this: 

    2017/04/21 08:39:56 Added master '9a67f35a': 192.168.1.32, portOffset: 0
    2017/04/21 08:39:56 Added new peer '715c4f0c': 192.168.1.32, portOffset: 5
    2017/04/21 08:40:03 Added new peer '65f9c97c': 192.168.1.32, portOffset: 10
    2017/04/21 08:40:03 Starting service...
    2017/04/21 08:40:03 Looking for a running instance of agent on port 4001
    2017/04/21 08:40:03 Starting agent on port 4001
    2017/04/21 08:40:04 Looking for a running instance of dbserver on port 4003
    2017/04/21 08:40:04 Starting dbserver on port 4003
    2017/04/21 08:40:05 Looking for a running instance of coordinator on port 4002
    2017/04/21 08:40:05 Starting coordinator on port 4002
    2017/04/21 08:40:16 agent up and running.
    2017/04/21 08:40:25 dbserver up and running.
    2017/04/21 08:40:45 coordinator up and running.

And at least it tells you where you can work with your cluster:

    2017/04/21 08:40:45 Your cluster can now be accessed with a browser at
                        `http://192.168.1.32:4002` or
    2017/04/21 08:40:45 using `arangosh --server.endpoint tcp://192.168.1.32:4002`.

Under the hood
--------------
The first ArangoDBStarter you ran (as shown above) will become the master in your setup, the `--join` will be the slaves.

The master determines which ArangoDB server processes to launch on which slave, and how they should communicate. 
It will then launch the server processes and monitor them. Once it has detected that the setup is complete you will get the prompt. The master will save the setup for subsequent starts. 

More complicated setup options [can be found in ArangoDBStarters Readme](https://github.com/arangodb-helper/ArangoDBStarter#starting-an-arangodb-cluster-the-easy-way). 

