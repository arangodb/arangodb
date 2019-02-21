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

If you use an ArangoDB version of 3.4 or above and use the Enterprise
Edition Docker image, you have to set the license key in an environment
variable by adding this option to the above `docker` command:

```
    -e ARANGO_LICENSE_KEY=<thekey>
```

You can get a free evaluation license key by visiting

     https://www.arangodb.com/download-arangodb-enterprise/

Then replace `<thekey>` above with the actual license key. The start
will then hand on the license key to the Docker container it launches
for ArangoDB.

### TLS verified Docker services

Oftentimes, one needs to harden Docker services using client certificate 
and TLS verification. The Docker API allows subsequently only certified access.
As the ArangoDB starter starts the ArangoDB cluster instances using this Docker API, 
it is mandatory that the ArangoDB starter is deployed with the proper certificates
handed to it, so that the above command is modified as follows:

```bash
export IP=<IP of docker host>
export DOCKER_TLS_VERIFY=1
export DOCKER_CERT_PATH=/path/to/certificate
docker volume create arangodb
docker run -it --name=adb --rm -p 8528:8528 \
    -v arangodb:/data \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -v /path/to/certificate:/path/to/certificate
    arangodb/arangodb-starter \
    --starter.address=$IP \
    --starter.mode=single
```

Note that the enviroment variables `DOCKER_TLS_VERIFY` and `DOCKER_CERT_PATH` 
as well as the additional mountpoint containing the certificate have been added above. 
directory. The assignment of `DOCKER_CERT_PATH` is optional, in which case it 
is mandatory that the certificates are stored in `$HOME/.docker`. So
the command would then be as follows

```bash
export IP=<IP of docker host>
docker volume create arangodb
docker run -it --name=adb --rm -p 8528:8528 \
    -v arangodb:/data \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -v /path/to/cert:/root/.docker \
    -e DOCKER_TLS_VERIFY=1 \
    arangodb/arangodb-starter \
    --starter.address=$IP \
    --starter.mode=single
```

