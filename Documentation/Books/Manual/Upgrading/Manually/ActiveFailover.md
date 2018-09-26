Manually Upgrading an _Active Failover_ Deployment
=========================================

This page will guide you through the process of a manual upgrade of an [_Active Failover_](../../Architecture/DeploymentModes/ActiveFailover/README.md)
setup. The different nodes can be upgraded one at a time without
incurring a _prolonged_ downtime of the entire system. The downtimes of the individual nodes
should also stay fairly low.

The manual upgrade procedure described in this section can be used to upgrade
to a new hotfix version, or to perform an upgrade to a new minor version of ArangoDB.

Preparations
------------

The ArangoDB installation packages (e.g. for Debian or Ubuntu) set up a
convenient standalone instance of `arangod`. During installation, this instance's
database will be upgraded (see [`--database.auto-upgrade` in General
Options](../../Programs/Arangod/Database.md#auto-upgrade))
and the service will be (re)started.

You have to make sure that your _Active Failover_ deployment is independent of this
standalone instance. Specifically, make sure that the database directory as
well as the socket used by the standalone instance provided by the package are
separate from the ones in your _Active Failover_ configuration. Also, that you haven't
modified the init script or systemd unit file for the standalone instance in a way
that it would start or stop your  _Active Failover_ instance instead.

<!--
You can read about the details on how to deploy your _Active Failover_  setup indendently of the
standalone instance in the [ _Active Failover_ deployment preliminary](../../Deployment/Cluster/PreliminaryInformation.md).

In the following, we assume that you don't use the standalone instance from the
package but only a manually started _cluster_ instance, and we will move the
standalone instance out of the way if necessary so you have to make as little
changes as possible to the running _cluster_.
-->

### Install the new ArangoDB version binary

The first step is to install the new ArangoDB package. 

**Note:** you do not have to stop the _Active Failover_ (_arangod_) processes before upgrading it.

For example, if you want to upgrade to `3.3.16` on Debian or Ubuntu, either call

```
$ apt install arangodb=3.3.16-1
```

(`apt-get` on older versions) if you have added the ArangoDB repository. Or
install a specific package using

```
$ dpkg -i arangodb3-3.3.16-1_amd64.deb
```

after you have downloaded the corresponding file from https://download.arangodb.com/.


#### Stop the Standalone Instance

As the package will automatically start the standalone instance, you might want to
stop that instance now, as otherwise it can create some confusion later. As you are 
starting the _Active Failover_ processes manually
you will not need the automatically installed and started standalone instance, 
and you should hence stop it via:

```
$ service arangodb3 stop
```

Also, you might want to remove the standalone instance from the default
_runlevels_ to prevent it from starting on the next reboots of your machine. How this
is done depends on your distribution and _init_ system. For example, on older Debian
and Ubuntu systems using a SystemV-compatible _init_, you can use:

```
$ update-rc.d -f arangodb3 remove
```

### Set supervision into maintenance mode

**Important**: Supervision maintenance mode is supported from ArangoDB versions 
3.3.8/3.2.14 or higher.

You have two main choices when performing an upgrade of the _Active Failover_ setup: 

- Upgrade while incurring a leader-to-follower switch (with reduced downtime) 
- An upgrade with no leader-to-follower switch. 

Turning the maintenance mode _on_ will enable the latter case. You might have a short 
downtime during the leader upgrade, but there will be no potential loss of _acknowledged_ operations. 

To enable the maintenance mode means to essentially disable the Agency supervision for a limited amount 
of time during the upgrade procedure. The following API calls will 
activate and deactivate the maintenance mode of the supervision job:

You might use _curl_ to send the API calls.
The following examples assume there is an _Active Failover_ node running on `localhost` on port 7002.

#### Activate Maintenance mode:

`curl -u username:password <single-server>/_admin/cluster/maintenance -XPUT -d'"on"'`

For example:
```
curl -u "root:" http://localhost:7002/_admin/cluster/maintenance -XPUT -d'"on"'

{"error":false,"warning":"Cluster supervision deactivated. 
It will be reactivated automatically in 60 minutes unless this call is repeated until then."}
```
**Note:** In case the manual upgrade takes longer than 60 minutes, the API call has to be resent.


#### Deactivate Maintenance mode:

The _cluster_ supervision resumes automatically 60 minutes after disabling it.
It can be manually reactivated earlier at any point using the following API call:

`curl -u username:password <single-server>/_admin/cluster/maintenance -XPUT -d'"off"'`

For example:
```
curl -u "root:" http://localhost:7002/_admin/cluster/maintenance -XPUT -d'"off"'

{"error":false,"warning":"Cluster supervision reactivated."}
```

### Upgrade the _Active Failover_ processes

Now all the _Active Failover_ (_Agents_, _Single-Server_) processes (_arangod_) have to be
upgraded on each node.

**Note:** Please read the section regarding the maintenance mode above

In order to stop an _arangod_ process we will need to use a command like `kill -15`:

```
kill -15 <pid-of-arangod-process>
```

The _pid_ associated to your _Active Failover setup_ can be checked using a command like _ps_:


```
ps -C arangod -fww
```

The output of the command above does not only show the process ids of all _arangod_ 
processes but also the used commands, which is useful for the following
restarts of all _arangod_ processes.

The output below is from a test machine where three _Agents_ and two _Single-Servers_
were running locally. In a more production-like scenario, you will find only one instance of each 
type running per machine:

```
ps -C arangod -fww
UID        PID  PPID  C STIME TTY          TIME CMD
max      29075  8072  0 13:50 pts/2    00:00:42 arangod --server.endpoint tcp://0.0.0.0:5001 --agency.my-address=tcp://127.0.0.1:5001 --server.authentication false --agency.activate true --agency.size 3 --agency.endpoint tcp://127.0.0.1:5001 --agency.supervision true --log.file a1 --javascript.app-path /tmp --database.directory agent1
max      29208  8072  2 13:51 pts/2    00:02:08 arangod --server.endpoint tcp://0.0.0.0:5002 --agency.my-address=tcp://127.0.0.1:5002 --server.authentication false --agency.activate true --agency.size 3 --agency.endpoint tcp://127.0.0.1:5001 --agency.supervision true --log.file a2 --javascript.app-path /tmp --database.directory agent2
max      29329 16224  0 13:51 pts/3    00:00:42 arangod --server.endpoint tcp://0.0.0.0:5003 --agency.my-address=tcp://127.0.0.1:5003 --server.authentication false --agency.activate true --agency.size 3 --agency.endpoint tcp://127.0.0.1:5001 --agency.supervision true --log.file a3 --javascript.app-path /tmp --database.directory agent3
max      29824 16224  1 13:55 pts/3    00:01:53 arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:7001 --cluster.my-address tcp://127.0.0.1:7001 --cluster.my-role SINGLE --cluster.agency-endpoint tcp://127.0.0.1:5001 --cluster.agency-endpoint tcp://127.0.0.1:5002 --cluster.agency-endpoint tcp://127.0.0.1:5003 --log.file c1 --javascript.app-path /tmp --database.directory single1
max      29938 16224  2 13:56 pts/3    00:02:13 arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:7002 --cluster.my-address tcp://127.0.0.1:7002 --cluster.my-role SINGLE --cluster.agency-endpoint tcp://127.0.0.1:5001 --cluster.agency-endpoint tcp://127.0.0.1:5002 --cluster.agency-endpoint tcp://127.0.0.1:5003 --log.file c2 --javascript.app-path /tmp --database.directory single2
```

**Note:** The start commands of _Agent_ and _Single Server_ are required for restarting the processes later.

The recommended procedure for upgrading an _Active Failover_ setup is to stop, upgrade 
and restart the _arangod_ instances one by one on all participating servers, 
starting first with all _Agent_ instances, and then following with the _Active Failover_ 
instances themselves. When upgrading the _Active Failover_ instances, the followers should
be upgraded first.

To figure out the node containing the followers you can consult the cluster endpoints API:
```
curl http://<single-server>:7002/_api/cluster/endpoints
```
This will yield a list of endpoints, the _first_ of which is always the leader node.


##### Stopping, upgrading and restarting an instance

To stop an instance, the currently running process has to be identified using the `ps`
command above. 

Let's assume we are about to upgrade an _Agent_ instance, so we have to look in the `ps`
output for an agent instance first, and note its process id (pid) and start command.

The process can then be stopped using the following command:

```
kill -15 <pid-of-agent>
```

The instance then has to be upgraded using the same command that was used before (in the `ps` output), 
but with the additional option:

```
--database.auto-upgrade=true
```

After the upgrade procecure has finishing successfully, the instance will remain stopped.
So it has to be restarted using the command from the `ps` output before
(this time without the `--database.auto-upgrade` option).


Once an _Agent_ was upgraded and restarted successfully, repeat the procedure for the
other _Agent_ instances in the setup and then repeat the procedure for the _Active Failover_
instances, there starting with the followers.

##### Final words

The _Agency_ supervision then needs to be reactivated by issuing the following API call 
to the leader:

`curl -u username:password <single-server>/_admin/cluster/maintenance -XPUT -d'"off"'`
