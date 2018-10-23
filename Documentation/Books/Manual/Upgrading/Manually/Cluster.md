Manually Upgrading a _Cluster_ Deployment
=========================================

This page will guide you through the process of a manual upgrade of a [_cluster_](../../Architecture/DeploymentModes/Cluster/README.md)
setup. The different nodes in a _cluster_ can be upgraded one at a time without
incurring downtime of the _cluster_ and very short downtimes of the single nodes.

The manual upgrade procedure described in this _Section_ can be used to upgrade
to a new hotfix, or to perform an upgrade to a new minor version of ArangoDB.
Please refer to the [Upgrade Paths](../GeneralInfo/README.md#upgrade-paths) section
for detailed information.

Preparations
------------

The ArangoDB installation packages (e.g. for Debian or Ubuntu) set up a
convenient standalone instance of `arangod`. During installation, this instance's
database will be upgraded (see [`--database.auto-upgrade`](../../Programs/Arangod/Database.md#auto-upgrade))
and the service will be (re)started.

You have to make sure that your _cluster_ deployment is independent of this
standalone instance. Specifically, make sure that the database directory as
well as the socket used by the standalone instance provided by the package are
separate from the ones in your _cluster_ configuration. Also, that you haven't
modified the init script or systemd unit file for the standalone instance in way
that it would start or stop your _cluster_ instance instead.

You can read about the details on how to deploy your _cluster_ indendently of the
standalone instance in the [_cluster_ deployment preliminary](../../Deployment/Cluster/PreliminaryInformation.md).

In the following, we assume that you don't use the standalone instance from the
package but only a manually started _cluster_ instance, and we will move the
standalone instance out of the way if necessary so you have to make as little
changes as possible to the running _cluster_.

### Install the new ArangoDB version binary

The first step is to install the new ArangoDB package. 

**Note:** you do not have to stop the _cluster_ (_arangod_) processes before upgrading it.

For example, if you want to upgrade to `3.3.9-1` on Debian or Ubuntu, either call

```
$ apt install arangodb=3.3.9
```

(`apt-get` on older versions) if you have added the ArangoDB repository. Or
install a specific package using

```
$ dpkg -i arangodb3-3.3.9-1_amd64.deb
```

after you have downloaded the corresponding file from https://download.arangodb.com/.

#### Stop the Standalone Instance

As the package will automatically start the standalone instance, you might want to
stop it now, as otherwise this standalone instance that is started on your machine
can create some confusion later. As you are starting the _cluster_ processes manually
you do not need this standalone instance, and you can hence stop it:

```
$ service arangodb3 stop
```

Also, you might want to remove the standalone instance from the default
_runlevels_ to prevent it to start on the next reboot of your machine. How this
is done depends on your distribution and _init_ system. For example, on older Debian
and Ubuntu systems using a SystemV-compatible _init_, you can use:

```
$ update-rc.d -f arangodb3 remove
```

Set supervision in maintenance mode
-----------------------------------

**Important**: Maintenance mode is supported from versions 3.3.8/3.2.14.

It is required to disable _cluster_ supervision in order to upgrade your _cluster_. The
following API calls will activate and de-activate the Maintenance mode of the Supervision job:

You might use _curl_ to send the API call.

### Activate Maintenance mode

`curl -u username:password <coordinator>/_admin/cluster/maintenance -XPUT -d'"on"'`

For Example:
```
curl http://localhost:7002/_admin/cluster/maintenance -XPUT -d'"on"'

{"error":false,"warning":"Cluster supervision deactivated. 
It will be reactivated automatically in 60 minutes unless this call is repeated until then."}
```
**Note:** In case the manual upgrade takes longer than 60 minutes, the API call has to be resend.

### Deactivate Maintenance mode

The _cluster_ supervision reactivates 60 minutes after disabling it.
It can be manually reactivated by the following API call:

`curl -u username:password <coordinator>/_admin/cluster/maintenance -XPUT -d'"off"'`

For example:
```
curl http://localhost:7002/_admin/cluster/maintenance -XPUT -d'"off"'

{"error":false,"warning":"Cluster supervision reactivated."}
```

Upgrade the _cluster_ processes
-------------------------------

Now all the _cluster_ (_Agents_, _DBServers_ and _Coordinators_) processes (_arangod_) have to be
upgraded on each node.

**Note:** The maintenance mode has to be activated.

In order to stop the _arangod_ processes we will need to use a command like `kill -15`:

```
kill -15 <pid-of-arangod-process>
```

The _pid_ associated to your _cluster_ can be checked using a command like _ps_:


```
ps -C arangod -fww
```

The output of the command above does not only show the PID's of all _arangod_ 
processes but also the used commands, which can be useful for the following
restart of all _arangod_ processes.

The output below is from a test machine where three _Agents_, two _DBServers_
and two _Coordinators_ are running locally. In a more production-like scenario,
you will find only one instance of each one running:

```
ps -C arangod -fww
UID        PID  PPID  C STIME TTY          TIME CMD
max      29075  8072  0 13:50 pts/2    00:00:42 arangod --server.endpoint tcp://0.0.0.0:5001 --agency.my-address=tcp://127.0.0.1:5001 --server.authentication false --agency.activate true --agency.size 3 --agency.endpoint tcp://127.0.0.1:5001 --agency.supervision true --log.file a1 --javascript.app-path /tmp --database.directory agent1
max      29208  8072  2 13:51 pts/2    00:02:08 arangod --server.endpoint tcp://0.0.0.0:5002 --agency.my-address=tcp://127.0.0.1:5002 --server.authentication false --agency.activate true --agency.size 3 --agency.endpoint tcp://127.0.0.1:5001 --agency.supervision true --log.file a2 --javascript.app-path /tmp --database.directory agent2
max      29329 16224  0 13:51 pts/3    00:00:42 arangod --server.endpoint tcp://0.0.0.0:5003 --agency.my-address=tcp://127.0.0.1:5003 --server.authentication false --agency.activate true --agency.size 3 --agency.endpoint tcp://127.0.0.1:5001 --agency.supervision true --log.file a3 --javascript.app-path /tmp --database.directory agent3
max      29461 16224  1 13:53 pts/3    00:01:11 arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:6001 --cluster.my-address tcp://127.0.0.1:6001 --cluster.my-role PRIMARY --cluster.agency-endpoint tcp://127.0.0.1:5001 --cluster.agency-endpoint tcp://127.0.0.1:5002 --cluster.agency-endpoint tcp://127.0.0.1:5003 --log.file db1 --javascript.app-path /tmp --database.directory dbserver1
max      29596  8072  0 13:54 pts/2    00:00:56 arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:6002 --cluster.my-address tcp://127.0.0.1:6002 --cluster.my-role PRIMARY --cluster.agency-endpoint tcp://127.0.0.1:5001 --cluster.agency-endpoint tcp://127.0.0.1:5002 --cluster.agency-endpoint tcp://127.0.0.1:5003 --log.file db2 --javascript.app-path /tmp --database.directory dbserver2
max      29824 16224  1 13:55 pts/3    00:01:53 arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:7001 --cluster.my-address tcp://127.0.0.1:7001 --cluster.my-role COORDINATOR --cluster.agency-endpoint tcp://127.0.0.1:5001 --cluster.agency-endpoint tcp://127.0.0.1:5002 --cluster.agency-endpoint tcp://127.0.0.1:5003 --log.file c1 --javascript.app-path /tmp --database.directory coordinator1
max      29938 16224  2 13:56 pts/3    00:02:13 arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:7002 --cluster.my-address tcp://127.0.0.1:7002 --cluster.my-role COORDINATOR --cluster.agency-endpoint tcp://127.0.0.1:5001 --cluster.agency-endpoint tcp://127.0.0.1:5002 --cluster.agency-endpoint tcp://127.0.0.1:5003 --log.file c2 --javascript.app-path /tmp --database.directory coordinator2

```

### Upgrade a _cluster_ node

The following procedure is upgrading _Agent_, _DBServer_ and _Coordinator_ on one node.

**Note:** The starting commands of _Agent_, _DBServer_ and _Coordinator_ have to be reused.

#### Stop the _Agent_

```
kill -15 <pid-of-agent>
```

#### Upgrade the _Agent_

The _arangod_ process of the _Agent_ has to be upgraded using the same command that has
been used before with the additional option:

```
--database.auto-upgrade=true
```

The _Agent_ will stop automatically after the upgrade.

#### Restart the _Agent_

The _arangod_ process of the _Agent_ has to be restarted using the same command that has
been used before (without the additional option).

#### Stop the _DBServer_

```
kill -15 <pid-of-dbserver>
```

#### Upgrade the _DBServer_

The _arangod_ process of the _DBServer_ has to be upgraded using the same command that has
been used before with the additional option:

```
--database.auto-upgrade=true
```

The _DBServer_ will stop automatically after the upgrade.

#### Restart the _DBServer_

The _arangod_ process of the _DBServer_ has to be restarted using the same command that has
been used before (without the additional option).

#### Stop the _Coordinator_

```
kill -15 <pid-of-coordinator>
```

#### Upgrade the _Coordinator_

The _arangod_ process of the _Coordinator_ has to be upgraded using the same command that has
been used before with the additional option:

```
--database.auto-upgrade=true
```

The _Coordinator_ will stop automatically after the upgrade.

#### Restart the _Coordinator_

The _arangod_ process of the _Coordinator_ has to be restarted using the same command that has
been used before (without the additional option).

After repeating this process on every node all _Agents_, _DBServers_ and _Coordinators_ are upgraded and the manual upgrade
has successfully finished.

The _cluster_ supervision is reactivated by the API call:

`curl -u username:password <coordinator>/_admin/cluster/maintenance -XPUT -d'"off"'`
