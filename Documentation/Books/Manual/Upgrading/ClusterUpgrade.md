Upgrading a cluster
===================

Preparations
------------

* Make sure the cluster-instances of arangod and the usual standalone-instance
  from the package are independent, especially:
  * The data directory
    - if necessary, change the standalone instance's directory in
    `/etc/arangodb3/arangod.conf`
      ```
      [database]
      directory = /var/lib/arangodb3
      ```
      to something else.
  * The port/interface combination
    - if necessary, change the standalone instance's port in
        `/etc/arangodb3/arangod.conf`
          ```
          [server]
          endpoint = tcp://127.0.0.1:8529
          ```
          to something else.
  * The init script
    - The script `/etc/init.d/arangodb3` should be the default one
      installed by the package. A custom script should be named differently.


Install the new version
-----------------------

* Install the package (especially, replace the files)


Stop standalone instance
------------------------

* (optional) Stop the standalone instance
* (optional) Remove the standalone init-script from the runlevels


Restart arangod processes
-------------------------

* Restart the agency process (e.g. kill it and let the starter restart it)
* Restart the DBServer process
  * If necessary, upgrade the database before/when starting the process
* Restart the coordinator process
