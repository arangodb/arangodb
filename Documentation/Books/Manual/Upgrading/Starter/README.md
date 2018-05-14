Upgrading _Starter_ Deployments
===============================

This procedure is intended to perform a rolling upgrade of an ArangoDB _Starter_
deployment to a new minor version of ArangoDB. 

Please note that this rolling upgrade procedure was introduced with versions 3.3.8
and 3.2.15. Therefore the upgrade version has to be 3.3.8/3.2.15 or above.  

Upgrade Procedure
-----------------

The following procedure has to be executed on every ArangoDB _Starter_ instance.
It is assumed that a _Starter_ deployment with mode `single`, `activefailover` or
`cluster` is running.

### Install the new ArangoDB version binary

Installing the new ArangoDB version binary also includes the latest ArangoDB _Starter_
binary, which is necessary to perform the rolling upgrade.

The first step is to install the new ArangoDB package. 

**Note:** you do not have to stop the _Starter_ processes before doing that.

For example, if you want to upgrade to `3.3.8-1` on Debian or Ubuntu, either call

```
$ apt install arangodb=3.3.8
```

(`apt-get` on older versions) if you have added the ArangoDB repository. Or
install a specific package using

```
$ dpkg -i arangodb3-3.3.8-1_amd64.deb
```

after you have downloaded the corresponding file from https://download.arangodb.com/.

#### Stop the Standalone Instance

As the package will have started the standalone instance, you might want to
stop it now. Otherwise it might block a port for the Starter.

```
$ service arangodb3 stop
```

Also, you might want to remove the standalone instance from the default
runlevels to prevent it to start on the next reboot; again, how this is done
depends on your distribution and init system. For example, on older Debian and
Ubuntu systems using a SystemV-compatible init, you can use

```
$ update-rc.d -f arangodb3 remove
```

### Stop the _Starter_ without stopping the ArangoDB Server processes

Now all **arangodb** processes have to be stopped.

Please note that no **arangod** processes should be stopped.

#### Have a look at the running _arangodb_ processes

```
ps -C arangodb -fww
```

The output does not only show the PID's of all _arangodb_ processes but also the
used commands, which can be useful for the following restart of all _arangodb_ processes.

For example: 

```
ps -C arangodb -fww
UID        PID  PPID  C STIME TTY          TIME CMD
max      29419  3684  0 11:46 pts/1    00:00:00 arangodb --starter.data-dir=./db1
max      29504  3695  0 11:46 pts/2    00:00:00 arangodb --starter.data-dir=./db2 --starter.join 127.0.0.1
max      29513  3898  0 11:46 pts/4    00:00:00 arangodb --starter.data-dir=./db3 --starter.join 127.0.0.1
```

#### Stop all running _arangodb_ processes

In order to stop the _arangodb_ processes, leaving the _arangod_ processes they 
have started up and running (as we want for a rolling upgrade), we will need to
use a command like `kill -9`:

```
kill -9 <pid=of-starter>
```    
    
### Restart the _Starter_

When using a supervisor like _SystemD_, this will happens automatically. In case
the _Starter_ was initiated manually, the _arangodb_ processes have to be started
manually with the same command that has been used before.

### Send an HTTP `POST` request to all _Starters_

The `POST` request with an empty body hast to be sent to `/database-auto-upgrade`
on all _Starters_ one by one. Once the upgrade on the first _Starter_ has finished,
the same request can be sent to the next one. The default port of the first _Starter_
is 8528. In a local test (all _Starters_ running on the same machine), the ports
of the additional _Starters_ are increased by 5 (before 3.3.8/3.2.15) or 10 (since
3.3.8/3.2.15).

Please not that the _Starter_ port is also shown in the _Starter_ output e.g.
`Listening on 0.0.0.0:8528 (:8528)`.

As the port of the _Starter_ is a configurable variable, please identify and use
the one of your specific setup.

#### `POST` request via _curl_

```
curl -X POST --dump - http://localhost:8538/database-auto-upgrade

HTTP/1.1 200 OK
Date: Wed, 09 May 2018 10:35:35 GMT
Content-Length: 2
Content-Type: text/plain; charset=utf-8
```

The response `200 OK` signals that the request was accepted and the upgrade process
for this starter has begun.

### _Starter_ response

The _Starter_ will respond to the HTTP `POST` request depending on the deployment mode.

#### Deployment mode `single`

The _Starter_ will:

- Restart the single server with an additional `--database.auto-upgrade=true` argument.
  The server will perform the auto-upgrade and then stop.
  After that the _Starter_ will automatically restart it with its normal arguments.

#### Deployment mode `activefailover`

The _Starter_ will:
 
- Perform a version check on all servers, ensuring it supports the upgrade procedure.
  TODO: Specify minimal patch version for 3.2, 3.3 & 3.4,
- Turning off supervision in the Agency and wait for it to be confirmed.
- Restarting one agent at a time with an additional `--database.auto-upgrade=true` argument.
  The agent will perform the auto-upgrade and then stop.
  After that the Starter will automatically restart it with its normal arguments.
- Restarting one resilient single server at a time with an additional `--database.auto-upgrade=true` argument.
  This server will perform the auto-upgrade and then stop.
  After that the Starter will automatically restart it with its normal arguments.
- Turning on supervision in the Agency and wait for it to be confirmed.

#### Deployment mode `cluster`

The _Starter_ will:
 
- Perform a version check on all servers, ensuring it supports the upgrade procedure.
  TODO: Specify minimal patch version for 3.2, 3.3 & 3.4,
- Turning off supervision in the Agency and wait for it to be confirmed.
- Restarting one agent at a time with an additional `--database.auto-upgrade=true` argument.
  The agent will perform the auto-upgrade and then stop.
  After that the Starter will automatically restart it with its normal arguments.
- Restarting one dbserver at a time with an additional `--database.auto-upgrade=true` argument.
  This dbserver will perform the auto-upgrade and then stop.
  After that the Starter will automatically restart it with its normal arguments.
- Restarting one coordinator at a time with an additional `--database.auto-upgrade=true` argument.
  This coordinator will perform the auto-upgrade and then stop.
  After that the Starter will automatically restart it with its normal arguments.
- Turning on supervision in the Agency and wait for it to be confirmed.

Once all servers in the starter have upgraded, repeat the procedure for the next starter.

### Example: Rolling Upgrade in a Cluster

In this example we will perform a rolling upgrade of an ArangoDB Cluster setup
from version 3.3.7 to version 3.3.8. 

Once a `POST` request to the first _Starter_ is sent, the following output is shown
when the upgrade has finished:

```
2018/05/09 12:33:02 Upgrading agent
2018/05/09 12:33:05 restarting agent
2018/05/09 12:33:05 Looking for a running instance of agent on port 8531
2018/05/09 12:33:05 Starting agent on port 8531
2018/05/09 12:33:05 Agency is not yet healthy: Agent http://localhost:8531 is not responding
2018/05/09 12:33:06 restarting agent
2018/05/09 12:33:06 Looking for a running instance of agent on port 8531
2018/05/09 12:33:06 Starting agent on port 8531
2018/05/09 12:33:07 agent up and running (version 3.3.8).
2018/05/09 12:33:10 Upgrading dbserver
2018/05/09 12:33:15 restarting dbserver
2018/05/09 12:33:15 Looking for a running instance of dbserver on port 8530
2018/05/09 12:33:15 Starting dbserver on port 8530
2018/05/09 12:33:15 DBServers are not yet all responding: Get http://localhost:8530/_admin/server/id: dial tcp 127.0.0.1:8530: connect: connection refused
2018/05/09 12:33:15 restarting dbserver
2018/05/09 12:33:15 Looking for a running instance of dbserver on port 8530
2018/05/09 12:33:15 Starting dbserver on port 8530
2018/05/09 12:33:16 dbserver up and running (version 3.3.8).
2018/05/09 12:33:20 Upgrading coordinator
2018/05/09 12:33:23 restarting coordinator
2018/05/09 12:33:23 Looking for a running instance of coordinator on port 8529
2018/05/09 12:33:23 Starting coordinator on port 8529
2018/05/09 12:33:23 Coordinator are not yet all responding: Get http://localhost:8529/_admin/server/id: dial tcp 127.0.0.1:8529: connect: connection refused
2018/05/09 12:33:23 restarting coordinator
2018/05/09 12:33:23 Looking for a running instance of coordinator on port 8529
2018/05/09 12:33:23 Starting coordinator on port 8529
2018/05/09 12:33:24 coordinator up and running (version 3.3.8).
2018/05/09 12:33:24 Your cluster can now be accessed with a browser at `http://localhost:8529` or
2018/05/09 12:33:24 using `arangosh --server.endpoint tcp://localhost:8529`.
2018/05/09 12:33:28 Server versions:
2018/05/09 12:33:28 agent 1        3.3.8
2018/05/09 12:33:28 agent 2        3.3.7
2018/05/09 12:33:28 agent 3        3.3.7
2018/05/09 12:33:28 dbserver 1     3.3.8
2018/05/09 12:33:28 dbserver 2     3.3.7
2018/05/09 12:33:28 dbserver 3     3.3.7
2018/05/09 12:33:28 coordinator 1  3.3.8
2018/05/09 12:33:28 coordinator 2  3.3.7
2018/05/09 12:33:28 coordinator 3  3.3.7
2018/05/09 12:33:28 Upgrading of all servers controlled by this starter done, you can continue with the next starter now.
```

_Agent_ 1, _DBSserver_ 1 and _Coordinator_ 1 are successively updated and the last
messages indicate that the `POST` request can be sent so the next _Starter_. After this
procedure has been repeated for every _Starter_ the last _Starter_ will show:

```
2018/05/09 12:35:59 Server versions:
2018/05/09 12:35:59 agent 1        3.3.8
2018/05/09 12:35:59 agent 2        3.3.8
2018/05/09 12:35:59 agent 3        3.3.8
2018/05/09 12:35:59 dbserver 1     3.3.8
2018/05/09 12:35:59 dbserver 2     3.3.8
2018/05/09 12:35:59 dbserver 3     3.3.8
2018/05/09 12:35:59 coordinator 1  3.3.8
2018/05/09 12:35:59 coordinator 2  3.3.8
2018/05/09 12:35:59 coordinator 3  3.3.8
2018/05/09 12:35:59 Upgrading done.
```

All _Agents_, _DBServers_ and _Coordinators_ are upgraded and the rolling upgrade
has successfully finished.