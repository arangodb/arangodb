#!/bin/bash

# Copyright Hans Dembinski 2019
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

for cpu in /sys/devices/system/cpu/cpu? ; do echo powersave > $cpu/cpufreq/scaling_governor; done
