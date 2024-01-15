#!/bin/sh
sed -i.bu -e 's/^# deb-src/deb-src/' /etc/apt/sources.list
apt-get update
apt-get upgrade -y
apt-get install -y dpkg-dev autoconf devscripts
apt-get build-dep -y glibc
