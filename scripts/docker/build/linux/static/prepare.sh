#!/bin/sh
sed -i -e 's/Types: deb/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources
apt-get update
apt-get upgrade -y
apt-get install -y dpkg-dev autoconf devscripts
apt-get build-dep -y glibc
