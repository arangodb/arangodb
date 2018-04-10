Preliminary Information
=======================

For Debian/Ubuntu Systems
-------------------------

### Use a different configuration file for the Cluster instance

The configuration file used for the standalone instance is
`/etc/arangodb3/arangod.conf` (on Linux), and you should use a different one for
the cluster instance(s). If you are using the _Starter_ binary `arangodb`, that is
automatically the case. Otherwise, you might have to copy the configuration
somewhere else and pass it to your `arangod` cluster instance via
`--configuration`.

### Use a different data directory for the standalone instance

The data directory is configured in `arangod.conf`:

```
[database]
directory = /var/lib/arangodb3
```

You have to make sure that the Cluster instance uses a different data directory
as the standalone instance. If that is not already the case, change the
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
same port on the same network interface than the Cluster. For that, change the
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

### Use a different _init_ script for the Cluster instance

This section applies to SystemV-compatible init systems (e.g. sysvinit, OpenRC,
upstart). The steps are different for systemd.

The package install scripts use the default _init_ script `/etc/init.d/arangodb3`
(on Linux) to stop and start ArangoDB during the installation. If you are using
an _init_ script for your Cluster instance, make sure it is named differently.
In addition, the installation might overwrite your _init_ script otherwise.

If you have previously changed the default _init_ script, move it out of the way

```
$ mv -vi /etc/init.d/arangodb3 /etc/init.d/arangodb3.cluster
```

and add it to the _autostart_; how this is done depends on your distribution and
_init_ system. On older Debian and Ubuntu systems, you can use `update-rc.d`:

```
$ update-rc.d arangodb3.cluster defaults
```

Make sure your _init_ script uses a different `PIDFILE` than the default script!
