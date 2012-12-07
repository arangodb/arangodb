#!/bin/bash

DISTROS="CentOS_CentOS-6 Fedora_16 Fedora_17 openSUSE_11.4 openSUSE_12.1 openSUSE_12.2 openSUSE_Factory openSUSE_Tumbleweed RedHat_RHEL-6 SLE_11 SLE_11_SP1 SLE_11_SP2"

test -d stable || mkdir stable || exit 1

for distro in $DISTROS;  do
  tag=`echo $distro | sed -e 's/openSUSE_/openSUSE:/' | sed -e 's/SLE_11/SUSE:SLE_11/' | sed -e 's/SLE_11_/SLE_11:/'`
  test -d stable/$distro || mkdir stable/$distro || exit 1
  
  echo "[arangodb]
name=ArangoDB Project ($tag)
type=rpm-md
baseurl=http://www.arangodb.org/repositories/stable/$distro/
gpgcheck=1
gpgkey=http://www.arangodb.org/repositories/stable/$distro/repodata/repomd.xml.key
enabled=1" > stable/$distro/arangodb.repo
 
done
