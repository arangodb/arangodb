<!-- don't edit here, it's from https://@github.com/arangodb-helper/arangodb.git / docs/Manual/ -->
Using the ArangoDB Starter
==========================

This section describes how to start a Cluster using the tool [_Starter_](../../Programs/Starter/README.md)
(the _arangodb_ binary program).

Local Tests
-----------

If you only want a local test Cluster, you can run a single _Starter_ with the 
`--starter.local` argument. It will start a 3 "machine" Cluster on your local PC:

```
arangodb --starter.local --starter.data-dir=./localdata
```

**Note:** a local Cluster is intended only for test purposes since a failure of 
a single PC will bring down the entire Cluster.

Multiple Machines
-----------------

If you want to start a Cluster using the _Starter_, you can use the following command:

```
arangodb --server.storage-engine=rocksdb --starter.data-dir=./data --starter.join A,B,C
```

Run the above command on machine A, B & C.

Once all the processes started by the _Starter_ are up and running, and joined the
Cluster (this may take a while depending on your system), the _Starter_ will inform
you where to connect the Cluster from a Browser, shell or your program.

For a full list of options of the _Starter_ please refer to [this](../../Programs/Starter/Options.md)
section.

Using the ArangoDB Starter in Docker
------------------------------------

The _Starter_ can also be used to launch Clusters based on _Docker_ containers:
   
```bash
export IP=<IP of docker host>
docker volume create arangodb
docker run -it --name=adb --rm -p 8528:8528 \
    -v arangodb:/data \
    -v /var/run/docker.sock:/var/run/docker.sock \
    arangodb/arangodb-starter \
    --starter.address=$IP \
    --starter.join=A,B,C
```

Run the above command on machine A, B & C.

If you use an ArangoDB version of 3.4 or above and use the Enterprise
Edition Docker image, you have to set the license key in an environment
variable by adding this option to the above `docker` command:

```
    -e ARANGO_LICENSE_KEY=<thekey>
```

You can get a free evaluation license key by visiting

     https://www.arangodb.com/download-arangodb-enterprise/

Then replace `<thekey>` above with the actual license key. The start
will then hand on the license key to the Docker containers it launches
for ArangoDB.

Under the Hood
--------------
The first `arangodb` you ran will become the _master_ of your _Starter_
setup, the other `arangodb` instances will become the _slaves_ of your _Starter_
setup. Please do not confuse the terms _master_ and _slave_ above with the master/slave
technology of ArangoDB. The terms above refers to the _Starter_ setup.

The _Starter_ _master_ determines which ArangoDB server processes to launch on which
_Starter_ _slave_, and how they should communicate. 

It will then launch the server processes and monitor them. Once it has detected
that the setup is complete you will get the prompt. 

The _Starter_ _master_ will save the setup for subsequent starts. 
