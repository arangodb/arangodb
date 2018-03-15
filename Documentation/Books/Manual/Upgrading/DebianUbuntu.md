Upgrading a cluster on Debian & Ubuntu
======================================

Preparations
------------

The ArangoDB installation packages (e.g. for Debian or Ubuntu) set up a
convenient standalone instance of `arangod`. During installation, this instance's
database will be upgraded (see [`--database.auto-upgrade` in General
Options](../../Administration/Configuration/GeneralArangod.md#database-upgrade))
and the service will be (re)started.

You have to make sure that your cluster deployment is independent of this
standalone instance. Specifically, make sure that the database directory as
well as the socket used by the standalone instance provided by the package are
separate from the ones in your cluster configuration. Also, that you haven't
modified the init script or systemd unit file for the standalone instance in way
that it would start or stop your cluster instance instead.

You can read about the details on how to deploy your cluster indendently of the
standalone instance in the [cluster deployment preliminary](../../Cluster/Deployment/Preliminary.md).

In the following, we assume that you don't use the standalone instance from the
package but only a cluster instance, and we will move the standalone instance
out of the way if necessary so you have to make as little changes as possible to
the running cluster.

Install the new version
-----------------------

Now you can install the new package version. You don't have to stop the cluster
process(es) before doing that.

For example, if you want to upgrade to `3.2.12-1` on Debian or Ubuntu, either call

```
$ apt install arangodb=3.2.12-1
```

(`apt-get` on older versions) if you've added the ArangoDB repository. Or
install a specific package using

```
$ dpkg -i arangodb3-3.2.12-1_amd64.deb
```

after you've downloaded the file. See https://download.arangodb.com/.

Stop standalone instance
------------------------

As the package will have started the standalone instance, you might want to
stop it now.

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

Restart arangod processes
-------------------------

Now all `arangod` processes have to be restarted.

### Using the starter

If you're using the [ArangoDB starter](../../Manual/GettingStarted/Starter),
shut down the processes one by one. The starter process `arangodb` will restart
them for you afterwards.

#### Have a look at the running processes

```
ps -C arangod -fww
```

#### Stop the agent (if applicable)

```
pkill -f '\<arangod\>.*agent'
```

#### Stop the DBServer (if applicable)

```
pkill -f '\<arangod\>.*dbserver'
```

#### Stop the coordinator (if applicable)

```
pkill -f '\<arangod\>.*coordinator'
```

#### Check if the processes are up again

```
ps -C arangod -fww
```

If you haven't used the starter but executed `arangod` directly, you still have
to stop the process(es) (by sending `SIGTERM`), but start it (or them) again
manually.

After repeating this process on every node you're done!
