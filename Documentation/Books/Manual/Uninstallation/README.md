Uninstallation
==============

Uninstallation depends on the method used to install ArangoDB, and on the Operating System in use, and typically consists of the following _high-level_ steps:

**If an installation package is used:**

- uninstallation of the ArangoDB package (which might or not remove, in addition
  to the ArangoDB _binaries_, also the database directory and the configuration file,
  depending on how the uninstallation process is started and on the Operating
  System in use)
- optionally removal of the leftover files (database directory and/or configuration files)

**If a _tar.gz_ is used:**

- removal of the unpacked archive

{% hint 'info' %} 
If the ArangoDB _Starter_ has been used to deploy ArangoDB, there will be an additional manual step:

- removal of the data directory created by the _Starter_ (the path used for the option 
  _--starter.data-dir_). This is in addition of the removal of the data directory 
  that is created for the _Stand Alone_ instance by the installation package
{% endhint %}
