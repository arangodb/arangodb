Uninstallation
==============

Uninstallation depends on the method used to install ArangoDB, and on the
operating system in use, and typically consists of the following high-level steps:

**If an installation package is used:**

- Uninstallation of the ArangoDB package, which may or may not remove the
  database directory and the configuration files in addition to the ArangoDB
  _binaries_, depending on how the uninstallation process is started and on
  the operating system in use.
- Optional removal of the leftover files (database directory and/or
  configuration files)

**If a _tar.gz_ or _.zip_ archive is used:**

- Removal of the files and folders that were unpacked from the archive.
  A data directory might exist in a subdirectory of the root folder.
- Optional removal of the leftover files (database directory and/or
  configuration files) stored in a different place

{% hint 'info' %} 
If the ArangoDB _Starter_ was used to deploy ArangoDB, there will be an
additional manual step:

- Removal of the data directory created by the _Starter_
  (the path used for the option `--starter.data-dir`).
  This is different and in addition to the removal of the data directory 
  that is created for the _Single Instance_ by the installation package.
{% endhint %}
