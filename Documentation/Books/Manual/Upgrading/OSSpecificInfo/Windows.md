**TMP**

Upgrading on Windows
====================

As there are different ways to install ArangoDB on Windows, the upgrade
method depends on the installation method that was used.

In general, it will be needed to:

- install the new ArangoDB binaries on the system
- upgrade the database

Some of the above command may be done automatically, depending on your
situation.

Upgrading via the Installer
---------------------------

If you have installed via the Installer, to upgrade:

- download the new Installer and execute it
- the Installer will ask if you want to update your current database

After installing the new package, you will have both packages installed.
You can uninstall the old one.

{% hint 'danger' %} 
When uninstalling the old package, please make sure the option
_"Delete databases with unistallation"_ is **not** checked.
{% endhint %}

{% info 'warning' %} 
Upgrading via the Installer, when the old data is kept, will keep your 
password and choice of storage engine as it is.
{% endhint %}
