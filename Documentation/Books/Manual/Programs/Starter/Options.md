<!-- don't edit here, it's from https://@github.com/arangodb-helper/arangodb.git / docs/Manual/ -->
# Option reference

The ArangoDB Starter provides a lot of options to control various aspects
of the cluster or database you want to run.

Below you'll find a list of all options and their semantics.

## Common options

- `--starter.data-dir=path`

`path` is the directory in which all data is stored. (default "./")

In the directory, there will be a single file `setup.json` used for
restarts and a directory for each instances that runs on this machine.
Different instances of `arangodb` must use different data directories.

- `--starter.join=address`

Join a cluster with master at address `address` (default "").
Address can be an host address or name, followed with an optional port.

E.g. these are valid arguments.

```bash
--starter.join=localhost
--starter.join=localhost:5678
--starter.join=192.168.23.1:8528
--starter.join=192.168.23.1
```

- `--starter.local`

Start a local (test) cluster. Since all servers are running on a single machine
this is really not intended for production setups.

- `--starter.mode=cluster|single|activefailover`

Select what kind of database configuration you want.
This can be a `cluster` configuration (which is the default),
a `single` server configuration or a `activefailover` configuration with
2 single services configured to take over when needed.

Note that when running a `single` server configuration you will lose all
high availability features that a cluster provides you.

- `--cluster.agency-size=int`

number of agents in agency (default 3).

This number has to be positive and odd, and anything beyond 5 probably
does not make sense. The default 3 allows for the failure of one agent.

- `--starter.address=addr`

`addr` is the address under which this server is reachable from the
outside.

Use this option only in the case that `--cluster.agency-size` is set to 1. 
In a single agent setup, the sole starter has to start on its own with
no reliable way to learn its own address. Using this option the master will 
know under which address it can be reached from the outside. If you specify
`localhost` here, then all instances must run on the local machine.

- `--starter.host=addr`

`addr` is the address to which this server binds. (default "0.0.0.0")

Usually there is no need to specify this option.
Only when you want to bind the starter to specific network device,
would you set this.
Note that setting this option to `127.0.0.1` will make this starter
unreachable for other starters, which is only allowed for
`single` server deployments or when using `--starter.local`.

- `--docker.image=image`

`image` is the name of a Docker image to run instead of the normal
executable. For each started instance a Docker container is launched.
Usually one would use the Docker image `arangodb/arangodb`.

- `--docker.container=containerName`

`containerName` is the name of a Docker container that is used to run the
executable. If you do not provide this argument but run the starter inside
a docker container, the starter will auto-detect its container name.

## Authentication options

The arango starter by default creates a cluster that uses no authentication.

To create a cluster that uses authentication, create a file containing a random JWT secret (single line)
and pass it through the `--auth.jwt-secret-path` option.

For example:

```bash
arangodb create jwt-secret --secret=jwtSecret
arangodb --auth.jwt-secret=./jwtSecret
```

All starters used in the cluster must have the same JWT secret.

To use a JWT secret to access the database, use `arangodb auth header`.
See [Using authentication tokens](./Security.md#using-authentication-tokens) for details.

## SSL options

The arango starter by default creates a cluster that uses no unencrypted connections (no SSL).

To create a cluster that uses encrypted connections, you can use an existing server key file (.pem format)
or let the starter create one for you.

To use an existing server key file use the `--ssl.keyfile` option like this:

```bash
arangodb --ssl.keyfile=myServer.pem
```

Use [`arangodb create tls keyfile`](./Security.md) to create a server key file.

To let the starter created a self-signed server key file, use the `--ssl.auto-key` option like this:

```bash
arangodb --ssl.auto-key
```

All starters used to make a cluster must be using SSL or not.
You cannot have one starter using SSL and another not using SSL.

If you start a starter using SSL, it's own HTTP server (see API) will also
use SSL.

Note that all starters can use different server key files.

Additional SSL options:

- `--ssl.cafile=path`

Configure the servers to require a client certificate in their communication to the servers using the CA certificate in a file with given path.

- `--ssl.auto-server-name=name`

name of the server that will be used in the self-signed certificate created by the `--ssl.auto-key` option.

- `--ssl.auto-organization=name`

name of the server that will be used in the self-signed certificate created by the `--ssl.auto-key` option.

## Other database options

Options for `arangod` that are not supported by the starter can still be passed to
the database servers using a pass through option.
Every option that start with a pass through prefix is passed through to the commandline
of one or more server instances.

- `--all.<section>.<key>=<value>` is pass as `--<section>.<key>=<value>` to all servers started by this starter.
- `--coordinators.<section>.<key>=<value>` is passed as `--<section>.<key>=<value>` to all coordinators started by this starter.
- `--dbservers.<section>.<key>=<value>` is passed as `--<section>.<key>=<value>` to all dbservers started by this starter.
- `--agents.<section>.<key>=<value>` is passed as `--<section>.<key>=<value>` to all agents started by this starter.

Some options are essential to the function of the starter. Therefore these options cannot be passed through like this.

Example:

To activate HTTP request logging at debug level for all coordinators, use a command like this.

```bash
arangodb --coordinators.log.level=requests=debug
```

## Datacenter to datacenter replication options

- `--sync.start-master=bool`

Should an ArangoSync master instance be started (only relevant when starter.sync is enabled, defaults to `true`)

- `--sync.start-worker=bool`

Should an ArangoSync worker instance be started (only relevant when starter.sync is enabled, defaults to `true`)

- `--sync.monitoring.token=<token>`

Bearer token used to access ArangoSync monitoring endpoints.

- `--sync.master.jwt-secret=<secret>`

Path of file containing JWT secret used to access the Sync Master (from Sync Worker).

- `--sync.mq.type=<message queue type>`

Type of message queue used by the Sync Master (defaults to "direct").

- `--sync.server.keyfile=<path of keyfile>`

TLS keyfile of local sync master.

- `--sync.server.client-cafile=<path of CA certificate>`

CA Certificate used for client certificate verification.

## Other `arangosync` options

Options for `arangosync` that are not supported by the starter can still be passed to
the syncmasters & syncworkers using a pass through option.
Every option that start with a pass through prefix is passed through to the commandline
of one or more `arangosync` instances.

- `--sync.<section>.<key>=<value>` is pass as `--<section>.<key>=<value>` to all arangosync instances started by this starter.
- `--syncmasters.<section>.<key>=<value>` is passed as `--<section>.<key>=<value>` to all syncmasters started by this starter.
- `--syncworkers.<section>.<key>=<value>` is passed as `--<section>.<key>=<value>` to all syncworkers started by this starter.

Some options are essential to the function of the starter. Therefore these options cannot be passed through like this.

Example:

To set a custom token TTL for direct message queue, use a command like this.

```bash
arangodb --syncmasters.mq.direct-token-ttl=12h ...
```

## Esoteric options

- `--version`

show the version of the starter.

- `--starter.port=int`

port for arangodb master (default 8528). See below under "Technical
explanation as to what happens" for a description of how the ports of
the other servers are derived from this number.

This is the port used for communication of the `arangodb` instances
amongst each other.

- `--starter.disable-ipv6=bool`

if disabled, the starter will configure the `arangod` servers
to bind to address `0.0.0.0` (all IPv4 interfaces)
instead of binding to `[::]` (all IPv4 and all IPv6 interfaces).

This is useful when IPv6 has actively been disabled on your machine.

- `--server.arangod=path`

path to the `arangod` executable (default varies from platform to
platform, an executable is searched in various places).

This option only has to be specified if the standard search fails.

- `--server.js-dir=path`

path to JS library directory (default varies from platform to platform,
this is coupled to the search for the executable).

This option only has to be specified if the standard search fails.

- `--server.storage-engine=mmfiles|rocksdb`

Sets the storage engine used by the `arangod` servers.
The value `rocksdb` is only allowed on `arangod` version 3.2 and up.

On `arangod` version 3.3 and earlier, the default value is `mmfiles`.
On `arangod` version 3.4 and later, the default value is `rocksdb`.

- `--cluster.start-coordinator=bool`

This indicates whether or not a coordinator instance should be started
(default true).

- `--cluster.start-dbserver=bool`

This indicates whether or not a DB server instance should be started
(default true).

- `--server.rr=path`

path to rr executable to use if non-empty (default ""). Expert and
debugging only.

- `--log.color=bool`

If set to `true`, console log output is colorized.
The default is `true` when a terminal is attached to stdin,
`false` otherwise or when running on Windows.

- `--log.console=bool`

If set to `true`, log output is written to the console (default `true`).

- `--log.file=bool`

If set to `true`, log output is written to the file (default `true`).
The log file, called `arangodb.log`, can be found in the directory
specified using `--log.dir` or if that is not set, the directory
specified using `--starter.data-dir`.

- `--log.verbose=bool`

show more information (default `false`).

- `--log.dir=path`

set a custom directory to which all log files will be written to.
When using the Starter in docker, make sure that this directory is
mounted as a volume for the Starter.

Note: When using a custom log directory, all database server files will be named as `arangod-<role>-<port>.log`.
The log for the starter itself is still called `arangodb.log`.

- `--log.rotate-files-to-keep=int`

set the number of old log files to keep when rotating log files of server components (default 5).

- `--log.rotate-interval=duration`

set the interval between rotations of log files of server components (default `24h`).
Use a value of `0` to disable automatic log rotation.

Note: The starter will always perform log rotation when it receives a `HUP` signal.

- `--starter.unique-port-offsets=bool`

If set to true, all port offsets (of slaves) will be made globally unique.
By default (value is false), port offsets will be unique per slave address.

- `--docker.user=user`

`user` is an expression to be used for `docker run` with the `--user`
option. One can give a user id or a user id and a group id, separated
by a colon. The purpose of this option is to limit the access rights
of the process in the Docker container.

- `--docker.endpoint=endpoint`

`endpoint` is the URL used to reach the docker host. This is needed to run
the executable in docker. The default value is "unix:///var/run/docker.sock".

- `--docker.imagePullPolicy=Always|IfNotPresent|Never`

`docker.imagePullPolicy` determines if the docker image is being pull from the docker hub.
If set to `Always`, the image is always pulled and an error causes the starter to fail.
If set to `IfNotPresent`, the image is not pull if it is always available locally.
If set to `Never`, the image is never pulled (when it is not available locally an error occurs).
The default value is `Always` is the `docker.image` has the `:latest` tag or `IfNotPresent` otherwise.

- `--docker.net-mode=mode`

If `docker.net-mode` is set, all docker container will be started
with the `--net=<mode>` option.

- `--docker.privileged=bool`

If `docker.privileged` is set, all docker containers will be started
with the `--privileged` option turned on.

- `--docker.tty=bool`

If `docker.tty` is set, all docker containers will be started with a TTY.
If the starter itself is running in a docker container without a TTY
this option is overwritten to `false`.

- `--starter.debug-cluster=bool`

IF `starter.debug-cluster` is set, the start will record the status codes it receives
upon "server ready" requests to the log. This option is mainly intended for internal testing.

## Environment variables

It is possibe to replace all commandline arguments for the starter with environment variables.
To do so, set an environment variable named `ARANGODB_` + `<name of command line option in uppercase>`,
where all dashes, underscores and dots are replased with underscores.

E.g.

```bash
ARANGODB_DOCKER_TTY=true arangodb
```

is equal to:

```bash
arangodb --docker.tty=true
```
