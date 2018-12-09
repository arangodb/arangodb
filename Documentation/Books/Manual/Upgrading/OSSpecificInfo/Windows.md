**TMP**

Upgrading on Windows
====================

As there are different ways to install ArangoDB on Windows, the upgrade
method depends on the installation method that was used.

In general, it will be needed to:

- install the new ArangoDB binaries on the system
- upgrade the database
- optinal (but suggested) to keep the system clean (unless there are specific
  reasons to not do so): remove the old binaries from the system

Some of the above steps may be done automatically, depending on your
specifc situation.

Upgrading via the Installer
---------------------------

If you have installed via the _Installer_, to upgrade:

- Download the new _Installer_ and run it.
- The _Installer_ will ask if you want to update your current database: select
  this option so the database files will be upgraded.

{% hint 'info' %} 
Upgrading via the Installer, when the old data is kept, will keep your 
password and choice of storage engine as it is.
{% endhint %}

- After installing the new package, you will have both packages installed.
You can uninstall the old one manually.

{% hint 'danger' %} 
When uninstalling the old package, please make sure the option
_"Delete databases with unistallation"_ is **not** checked.
{% endhint %}

Manual upgrade of 'ZIP archive' installation
--------------------------------------------

