#!/bin/bash

DISTROS="openSUSE_Tumbleweed openSUSE_Factory openSUSE_12.2 openSUSE_12.1 openSUSE_11.4 SLE_11_SP2 SLE_11_SP1 SLE_11"

test -d stable || mkdir stable || exit 1
test -d stable/ymp || mkdir stable/ymp || exit 1

for distro in $DISTROS;  do
  tag=`echo $distro | sed -e 's/openSUSE_/openSUSE:/' | sed -e 's/SLE_11/SUSE:SLE_11/' | sed -e 's/SLE_11_/SLE_11:/'`
  test -d stable/ymp/$distro || mkdir stable/ymp/$distro || exit 1
  
  echo "<metapackage xmlns:os=\"http://opensuse.org/Standards/One_Click_Install\" xmlns=\"http://opensuse.org/Standards/One_Click_Install\">
  <group>
    <repositories>
      <repository recommended=\"true\">
        <name>ArangoDB</name>
        <summary>ArangoDB Project</summary>
        <description>ArangoDB Repository for $tag</description>
        <url>http://www.arangodb.org/repositories/stable/repositories/$distro/</url>
      </repository>
    </repositories>
    <software>
      <item>
        <name>arangodb</name>
        <summary>An open-source, multi-model NoSQL database</summary>
        <description>ArangoDB is a durable, reliable, transactional multi-model database. It's key-features are: Schema-free schemata, an integrated application server, flexible data modelling, free index choice, and configurable durability.

The ArangoDB consists of a server, a separate shell, which allows you to administrate the server, and a set of client API for various languages.

It is written in C/C++.</description>
      </item>
    </software>
  </group>
</metapackage>" > stable/ymp/$distro/arangodb.ymp
 
done
