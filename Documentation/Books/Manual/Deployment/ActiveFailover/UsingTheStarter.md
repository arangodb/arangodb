<!-- don't edit here, it's from https://@github.com/arangodb-helper/arangodb.git / docs/Manual/ -->
Using the ArangoDB Starter
==========================

This section describes how to start an Active Failover setup the tool [_Starter_](../../Programs/Starter/README.md)
(the _arangodb_ binary program).

Local Tests
-----------

If you want to start a local _Active Failover_ setup quickly, use the `--starter.local`
option of the _Starter_. This will start all servers within the context of a single
starter process:

```bash
arangodb --starter.local --starter.mode=activefailover --starter.data-dir=./localdata
```

**Note:** When you restart the _Starter_, it remembers the original `--starter.local` flag.

Multiple Machines
-----------------

If you want to start an Active Failover setup using the _Starter_, use the `--starter.mode=activefailover`
option of the _Starter_. A 3 "machine" _Agency_ is started as well as 2 single servers,
that perform asynchronous replication and failover:

```bash
arangodb --starter.mode=activefailover --server.storage-engine=rocksdb --starter.data-dir=./data --starter.join A,B,C
```

Run the above command on machine A, B & C.

The _Starter_ will decide on which 2 machines to run a single server instance.
To override this decision (only valid while bootstrapping), add a
`--cluster.start-single=false` to the machine where the single server
instance should _not_ be started.

Once all the processes started by the _Starter_ are up and running, and joined the
Active Failover setup (this may take a while depending on your system), the _Starter_ will inform
you where to connect the Active Failover from a Browser, shell or your program.

For a full list of options of the _Starter_ please refer to [this](../../Programs/Starter/Options.md)
section.

Using the ArangoDB Starter in Docker
------------------------------------

The _Starter_ can also be used to launch an Active Failover setup based on _Docker_
containers. To do this, you can use the normal Docker arguments, combined with
`--starter.mode=activefailover`:

```bash
export IP=<IP of docker host>
docker volume create arangodb
docker run -it --name=adb --rm -p 8528:8528 \
    -v arangodb:/data \
    -v /var/run/docker.sock:/var/run/docker.sock \
    arangodb/arangodb-starter \
    --starter.address=$IP \
    --starter.mode=activefailover \
    --starter.join=A,B,C
```

Run the above command on machine A, B & C.

The _Starter_ will decide on which 2 machines to run a single server instance.
To override this decision (only valid while bootstrapping), add a
`--cluster.start-single=false` to the machine where the single server
instance should _not_ be started.

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

