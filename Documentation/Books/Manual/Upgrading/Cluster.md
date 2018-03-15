Upgrading a cluster
===================

This page will guide you through the process of an in-place upgrade (see also
[General Upgrade Information](GeneralUpgradeInformation.md)). The different
nodes in a cluster can be updated one at a time without incurring downtime
of the cluster and very short downtimes of the single nodes.

Preparations
------------

The ArangoDB installation packages (e.g. for Debian or Ubuntu) set up a
convenient standalone instance of `arangod`. During installation, this instance's
database will be upgraded (see [`--database.auto-upgrade` in General
Options](../../Administration/Configuration/GeneralArangod.md#database-upgrade))
and the service will be (re)started. Therefore, you have to make sure that this
automatic upgrade doesn't interfere with the cluster.

In the following, we assume that you don't use the standalone instance from the
package but only a cluster instance, and we will move the standalone instance
out of the way if necessary so you have to make as little changes as possible to
the running cluster.

### Use a different configuration file for the cluster instance

The configuration file used for the standalone instance is
`/etc/arangodb3/arangod.conf` (on Linux), and you should use a different one for
the cluster instance(s). If you're using the starter binary `arangodb`, that is
automatically the case. Otherwise, you might have to copy the configuration
somewhere else and pass it to your `arangod` cluster instance via
`--configuration`.

### Use a different data directory for the standalone instance

The data directory is configured in `arangod.conf`:
```
[database]
directory = /var/lib/arangodb3
```

You have to make sure that the cluster instance uses a different data directory
as the standalone instance. If that isn't already the case, change the
`database.directory` entry in `arangod.conf` as seen above to a different
directory

```
# in arangod.conf:
[database]
directory = /var/lib/arangodb3.standalone
```

and create it with the correct permissions:

```
$ mkdir -vp /var/lib/arangodb3.standalone
$ chown -c arangodb:arangodb /var/lib/arangodb3.standalone
$ chmod -c 0700 /var/lib/arangodb3.standalone
```

### Use a different socket for the standalone instance

The standalone instance must use a different socket, i.e. it cannot use the
same port on the same network interface than the cluster. For that, change the
standalone instance's port in `/etc/arangodb3/arangod.conf`
```
[server]
endpoint = tcp://127.0.0.1:8529
```
to something unused, e.g.
```
[server]
endpoint = tcp://127.1.2.3:45678
```
.

### Use a different init script for the cluster instance

This section applies to SystemV-compatible init systems (e.g. sysvinit, OpenRC,
upstart). The steps are different for systemd.

The package install scripts use the default init script `/etc/init.d/arangodb3`
(on Linux) to stop and start ArangoDB during the installation. If you're using
an init script for your cluster instance, make sure it is named differently.
In addition, the installation might overwrite your init script otherwise.

If you've previously changed the default init script, move it out of the way
```
$ mv -vi /etc/init.d/arangodb3 /etc/init.d/arangodb3.cluster
```
and add it to the autostart; how this is done depends on your distribution and
init system. On older Debian and Ubuntu systems, you can use `update-rc.d`:

```
$ update-rc.d arangodb3.cluster defaults
```

Make sure your init script uses a different `PIDFILE` than the default script!


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
