Rolling Upgrade Using the _Starter_
=============================
This procedure is intended to perform a rolling upgrade of a ArangoDB Starter deployment to a new minor version of ArangoDB. 

Please note that the rolling upgrade procedure was introduced with versions 3.3.8 and 3.2.15. Therefore the upgrade version has to be 3.3.8/3.2.15 or above.  

# Upgrade Procedure

The following procedure has to be executed on every ArangoDB Starter instance. It is assumed that a Starter deployment with mode `single`, `activefailover` or `cluster` is running.

## Install the new ArangoDB version binary
Installing the new ArangoDB version binary also includes the latest ArangoDB Starter binary, which is necessary to perform the rolling upgrade.


The first step is to install the new package version. You don't have to stop the Starter processes before doing that.

For example, if you want to upgrade to `3.3.8-1` on Debian or Ubuntu, either call

```
$ apt install arangodb=3.3.8
```

(`apt-get` on older versions) if you've added the ArangoDB repository. Or
install a specific package using

```
$ dpkg -i arangodb3-3.3.8-1_amd64.deb
```

after you've downloaded the file. See https://download.arangodb.com/.

### Stop standalone instance


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

## Shutdown the Starter without stopping the ArangoDB Server process
Now all **arangodb** processes have to be stopped.

Please note that no **arangod** processes should be stopped.


### Have a look at the running arangodb processes

```
ps -C arangodb -fww
```
The output does not only show the PID's of all arangodb processes but also the used commands, which can be useful for the following restart of all arangodb processes.

For example: 
```
ps -C arangodb -fww
UID        PID  PPID  C STIME TTY          TIME CMD
max      29419  3684  0 11:46 pts/1    00:00:00 arangodb --starter.data-dir=./db1
max      29504  3695  0 11:46 pts/2    00:00:00 arangodb --starter.data-dir=./db2 --starter.join 127.0.0.1
max      29513  3898  0 11:46 pts/4    00:00:00 arangodb --starter.data-dir=./db3 --starter.join 127.0.0.1
```


### Shutdown all running arangodb processes

```
kill -9 <pid=of-starter>
```    
    
## Restart the Starter 

When using a supervisor like SystemD, this will happen automatically. In case the Starter was initiated manually, the arangodb processes have to be started manually with the same command that has been used before.

- Send an HTTP `POST` request with empty body to `/database-auto-upgrade` on one of the starters.

The Starter will respond to this request depending on the deployment mode.

If the deployment mode is `single`, the Starter will:

- Restart the single server with an additional `--database.auto-upgrade=true` argument.
  The server will perform the auto-upgrade and then stop.
  After that the Starter will automatically restart it with its normal arguments.

If the deployment mode is `activefailover`, the Starters will:

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

If the deployment mode is `cluster`, the Starters will:

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
