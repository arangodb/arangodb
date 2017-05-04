Linux
=====

- Visit the official [ArangoDB install page](https://www.arangodb.com/install)
  and download the correct package for your Linux distribution. You can find
  binary packages for the most common distributions there.
- Follow the instructions to use your favorite package manager for the
  major distributions. After setting up the ArangoDB repository you can
  easily install ArangoDB using yum, aptitude, urpmi or zypper.
- Debian based packages will ask for a password during installation. For an
  unattended installation for Debian, see [below](#unattended-installation).
  Red-Hat based packages will set a random password during installation.
  For other distributions or to change the password, run
  `arango-secure-installation` to set a root password. 
- Alternatively, see [Compiling](Compiling.md) if you want to build ArangoDB
  yourself.
- Start up the database server.

Normally, this is done by executing the following command:

    unix> /etc/init.d/arangod start
 
It will start the server, and do that as well at system boot time.

To stop the server you can use the following command:

    unix> /etc/init.d/arangod stop

The exact commands depend on your Linux distribution.
You may require root privileges to execute these commands.

Linux Mint
----------

Please use the corresponding Ubuntu or Debian packages.

Unattended Installation
-----------------------

Debian based package will ask for a password during installation.
For unattended installation, you can set the password using the
[debconf helpers](http://www.microhowto.info/howto/perform_an_unattended_installation_of_a_debian_package.html).

```
echo arangodb3 arangodb3/password password NEWPASSWORD | debconf-set-selections
echo arangodb3 arangodb3/password_again password NEWPASSWORD | debconf-set-selections
```

The commands should be executed prior to the installation.

Red-Hat based packages will set a random password during installation.
If you want to force a password, execute

```
ARANGODB_DEFAULT_ROOT_PASSWORD=NEWPASSWORD arango-secure-installation
```

The command should be executed after the installation.

Non-Standard Installation
-------------------------

If you compiled ArangoDB from source and did not use any installation
package – or using non-default locations and/or multiple ArangoDB
instances on the same host – you may want to start the server process 
manually. You can do so by invoking the arangod binary from the command
line as shown below:

```
unix> /usr/local/sbin/arangod /tmp/vocbase
20ZZ-XX-YYT12:37:08Z [8145] INFO using built-in JavaScript startup files
20ZZ-XX-YYT12:37:08Z [8145] INFO ArangoDB (version 1.x.y) is ready for business
20ZZ-XX-YYT12:37:08Z [8145] INFO Have Fun!
```

To stop the database server gracefully, you can
either press CTRL-C or by send the SIGINT signal to the server process. 
On many systems this can be achieved with the following command:

    unix> kill -2 `pidof arangod`


Once you started the server, there should be a running instance of *_arangod_* -
the ArangoDB database server.

    unix> ps auxw | fgrep arangod
    arangodb 14536 0.1 0.6 5307264 23464 s002 S 1:21pm 0:00.18 /usr/local/sbin/arangod

If there is no such process, check the log file
*/var/log/arangodb/arangod.log* for errors. If you see a log message like

    2012-12-03T11:35:29Z [12882] ERROR Database directory version (1) is lower than server version (1.2).
    2012-12-03T11:35:29Z [12882] ERROR It seems like you have upgraded the ArangoDB binary. If this is what you wanted to do, please restart with the --database.auto-upgrade option to upgrade the data in the database directory.
    2012-12-03T11:35:29Z [12882] FATAL Database version check failed. Please start the server with the --database.auto-upgrade option

make sure to start the server once with the *--database.auto-upgrade* option.

Note that you may have to enable logging first. If you start the server
in a shell, you should see errors logged there as well.
