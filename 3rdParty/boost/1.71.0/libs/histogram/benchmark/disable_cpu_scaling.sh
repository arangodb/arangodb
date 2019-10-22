#/bin/bash
for cpu in /sys/devices/system/cpu/cpu? ; do echo performance > $cpu/cpufreq/scaling_governor; done
