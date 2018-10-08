<!-- don't edit here, it's from https://@github.com/arangodb-helper/arangodb.git / docs/Manual/ -->
Using the ArangoDB Starter
==========================

This section describes how to start an ArangoDB stand-alone instance using the tool
[_Starter_](../../Programs/Starter/README.md) (the _arangodb_ binary program).

Local Start
-----------

If you want to start a stand-alone instance of ArangoDB, use the `--starter.mode=single`
option of the _Starter_: 

```bash
arangodb --starter.mode=single
```

Using the ArangoDB Starter in Docker
------------------------------------

The _Starter_ can also be used to launch a stand-alone instance based on _Docker_
containers:

```bash
export IP=<IP of docker host>
docker volume create arangodb
docker run -it --name=adb --rm -p 8528:8528 \
    -v arangodb:/data \
    -v /var/run/docker.sock:/var/run/docker.sock \
    arangodb/arangodb-starter \
    --starter.address=$IP \
    --starter.mode=single 
```
