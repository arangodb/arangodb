#!/bin/bash
apt-get update
apt-get upgrade -y
apt-get install -y git curl python3 xz-utils pkg-config python3-distutils python3-httplib2 wget
apt-get install -y build-essential cmake ninja-build

wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
echo deb http://apt.llvm.org/mantic/ llvm-toolchain-mantic-18 main >> /etc/apt/sources.list
apt update
apt-get install -y clang-18 clang-tools-18 clang-18-doc libclang-common-18-dev libclang-18-dev libclang1-18 clang-format-18 python3-clang-18 clangd-18 clang-tidy-18 lld-18
ln -s /usr/bin/clang-18 /usr/bin/clang
ln -s /usr/bin/clang++-18 /usr/bin/clang++
ln -s /usr/bin/lld-18 /usr/bin/lld
ln -s /usr/bin/lld-18 /usr/bin/ld.lld
ln -s /usr/bin/llvm-ar-18 /usr/bin/llvm-ar

cd /opt
mkdir v8
cd v8
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:/opt/v8/depot_tools
export VPYTHON_BYPASS="manually managed python not supported by chrome operations"
fetch v8
cd v8
git checkout 12.1.165
./tools/clang/scripts/build.py --without-android --without-fuchsia --host-cc=gcc --host-cxx=g++ --use-system-cmake --disable-asserts
tools/dev/v8gen.py arm64.release.sample
cat <<EOF > out.gn/arm64.release.sample/args.gn
dcheck_always_on = true
is_component_build = false
is_debug = false
target_cpu = "arm64"
use_custom_libcxx = false
v8_monolithic = true
v8_use_external_startup_data = false
v8_enable_v8_checks = true
v8_optimized_debug = true
v8_enable_webassembly = false
clang_base_path="/usr"
clang_use_chrome_plugins=false
treat_warnings_as_errors=false
EOF
gn gen out.gn/arm64.release.sample
ninja -C out.gn/arm64.release.sample v8_monolith
