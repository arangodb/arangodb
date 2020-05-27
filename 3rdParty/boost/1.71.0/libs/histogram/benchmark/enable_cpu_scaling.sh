#/bin/bash
for cpu in /sys/devices/system/cpu/cpu? ; do echo powersave > $cpu/cpufreq/scaling_governor; done
