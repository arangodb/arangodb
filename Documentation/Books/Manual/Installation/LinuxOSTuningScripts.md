Linux OS Tuning Script Examples
===============================

The most important suggestions listed in the section
[Linux Operating System Configuration](LinuxOSConfiguration.md)
can be easily applied by making use of a script and _init.d_.

This page includes script examples that can be used to tune the
operating system (OS) in case you are using _Debian_ or _CentOS_,
along with instructions on how to install the scripts.

{% hint 'warning' %}
It is important that the script is set up in a way that it gets
executed even after the machine reboots. Instructions on how to
configure your system so that the script executes during the
boot process can be found below.
{% endhint %}

Debian
------

**Script:**

```bash
#!/bin/bash

### BEGIN INIT INFO
# Provides: arangodb-memory-configuration
# Required-Start:
# Required-Stop:
# Default-Start: 2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: Set arangodb kernel parameters
# Description: Set arangodb kernel parameters
### END INIT INFO

# 1 - Raise the vm map count value
sudo sysctl -w "vm.max_map_count=2048000"

# 2 - Disable Transparent Huge Pages
sudo bash -c "echo madvise > /sys/kernel/mm/transparent_hugepage/enabled"
sudo bash -c "echo madvise > /sys/kernel/mm/transparent_hugepage/defrag"
 
# 3 - Set the virtual memory accounting mode
sudo bash -c "echo 0 > /proc/sys/vm/overcommit_memory"
```

**Installation Instructions:**

1. Create the file inside the `/etc/init.d/` directory, e.g.

   `/etc/init.d/arangodb-os-optimization`

2. Set correct permission, to mark the file executable:

   `sudo chmod 755 /etc/init.d/arangodb-os-optimization`

3. On Ubuntu, use the following command to configure your system
   to execute the script during the boot process:

   `sudo update-rc.d arangodb-os-optimization defaults`

**Note:**

You might need the package _sysfsutils_. If this is the case,
please install it via:

`sudo apt install sysfsutils`

**Important:**

To optimize the OS "now", without having to restart the system,
the script **must** also be directly executed once.

CentOS
------

**Script:**

```bash
#!/bin/bash

### BEGIN INIT INFO
# Provides: arangodb-memory-configuration
# Required-Start:
# Required-Stop:
# Default-Start: 2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: Set arangodb kernel parameters
# Description: Set arangodb kernel parameters
### END INIT INFO

# 1 - Raise the vm map count value
sysctl -w "vm.max_map_count=2048000"

# 2 - Disable Transparent Huge Pages
bash -c "echo madvise > /sys/kernel/mm/transparent_hugepage/enabled"
bash -c "echo madvise > /sys/kernel/mm/transparent_hugepage/defrag"
 
# 3 - Set the virtual memory accounting mode
bash -c "echo 0 > /proc/sys/vm/overcommit_memory"
```

**Installation Instructions:**

1. Create the file inside the `/etc/init.d/` directory. e.g.

   `/etc/init.d/arangodb-os-optimization`

2. Set correct permission, to mark the file executable. As _root_:

   `chmod 755 /etc/init.d/arangodb-os-optimization`

3. On CentOS/RedHat, use the following commands to configure your system
   to execute the script during the boot process. As _root_:

   ```
   chkconfig --add /etc/init.d/arangodb-os-optimization`
   chkconfig arangodb-os-optimization on
   ```

**Note:**

You might need the package _sysfsutils_. If this is the case,
please install it, as _root_, via:

`yum install sysfsutils`

**Important:**

To optimize the OS "now", without having to restart the system,
the script **must** also be directly executed once.
