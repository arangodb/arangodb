#!/bin/bash
apt-get update
apt-get upgrade -y
apt-get install -y git curl python3 xz-utils pkg-config
cd /opt
mkdir v8
cd v8
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=/opt/v8/depot_tools:$PATH
fetch v8
cd v8
git checkout 12.1.165
tools/dev/v8gen.py x64.release.sample
cat <<EOF > out.gn/x64.release.sample/args.gn
dcheck_always_on = true
is_component_build = false
is_debug = false
target_cpu = "x64"
use_custom_libcxx = false
v8_monolithic = true
v8_use_external_startup_data = false
v8_enable_v8_checks = true
v8_optimized_debug = true
v8_enable_webassembly = false
EOF
gn gen out.gn/x64.release.sample
ninja -C out.gn/x64.release.sample v8_monolith
