# ArangoDB, NodeJS and Docker

## Problem

I'm looking for a head start in using the ArangoDB docker image.

## Solution

We will use the guesser game for ArangoDB from

```
https://github.com/arangodb/guesser
```

This is a simple game guessing animals or things. It learns while playing
and stores the learned information in an ArangoDB instance. The game is written using the
express framework.

**Note**: You need to switch to the docker branch.

The game has the two components

* front-end with node.js and express
* back-end with ArangoDB and Foxx

Therefore the guesser game needs two docker containers, one container for the node.js
server to run the front-end code and one container for ArangoDB for the storage back-end.

### Node Server

The game is itself can be install via NPM or from github. There is an image available from
dockerhub called `arangodb/example-guesser` which is based on the Dockerfile
from github.

You can either build the docker container locally or simply use the available one from
docker hub.

```
unix> docker run -p 8000:8000 -e nolink=1 arangodb/example-guesser
Starting without a database link
Using DB-Server http://localhost:8529
Guesser app server listening at http://0.0.0.0:8000
```

This will start-up node and the guesser game is available on port 8000. Now point your
browser to port 8000. You should see the start-up screen. However, without a storage
backend it will be pretty useless. Therefore, stop the container and proceed with the next
step.

If you want to build the container locally, check out the guesser game from

```
https://github.com/arangodb/example-guesser
```

Switch into the `docker/node` subdirectory and execute `docker build .`.

### ArangoDB

ArangoDB is already available on docker, so we start an instance

```
unix> docker run --name arangodb-guesser arangodb/arangodb
show all options:
  docker run -e help=1 arangodb

starting ArangoDB in stand-alone mode
```

That's it. Note that in an productive environment you would need to attach a storage
container to it. We ignore this here for the sake of simplicity.

### Guesser Game


#### Some Testing

Use the guesser game image to start the ArangoDB shell and link the ArangoDB instance to
it.

```
unix> docker run --link arangodb-guesser:db-link -it arangodb/example-guesser arangosh --server.endpoint @DB_LINK_PORT_8529_TCP@
```

The parameter `--link arangodb-guesser:db-link` links the running ArangoDB into the
application container and sets an environment variable `DB_LINK_PORT_8529_TCP` which
points to the exposed port of the ArangoDB container:

```
DB_LINK_PORT_8529_TCP=tcp://172.17.0.17:8529
```

Your IP may vary. The command `arangosh ...` at the end of docker command executes the
ArangoDB shell instead of the default node command.

```
Welcome to arangosh 2.3.1 [linux]. Copyright (c) ArangoDB GmbH
Using Google V8 3.16.14 JavaScript engine, READLINE 6.3, ICU 52.1

Pretty printing values.
Connected to ArangoDB 'tcp://172.17.0.17:8529' version: 2.3.1, database: '_system', username: 'root'

Type 'tutorial' for a tutorial or 'help' to see common examples
arangosh [_system]> 
```

The important line is

```
Connected to ArangoDB 'tcp://172.17.0.17:8529' version: 2.3.1, database: '_system', username: 'root'
```

It tells you that the application container was able to connect to the database
back-end. Press `Control-D` to exit.

#### Start Up The Game

Ready to play? Start the front-end container with the database link and initialize the database.

```
unix> docker run --link arangodb-guesser:db-link -p 8000:8000 -e init=1 arangodb/example-guesser
```

Use your browser to play the game at the address http://127.0.0.1:8000/.
The

```
-e init=1
```

is only need the first time you start-up the front-end and only once. The next time you
run the front-end or if you start a second front-end server use

```
unix> docker run --link arangodb-guesser:db-link -p 8000:8000 arangodb/example-guesser
```


**Author**: [Frank Celler](https://github.com/fceller)

**Tags**: #docker
