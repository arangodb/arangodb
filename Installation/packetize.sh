#!/bin/bash

echo "########################################################"
echo "#                   packetize.sh                       #"
echo "########################################################"
echo

. config/detect_distro.sh

hudson_base="$HOME"
rusr=root
rgrp=root
susr=arangodb
sgrp=arangodb
package_type=""
product_name="arangodb"
project_name="arangodb"
runlevels="035"
curl_version="curl -s -o - http://localhost:8529/_api/version"

# name of the epm configuration file
LIST="Installation/epm/${project_name}.list"
# name of a created epm  include file
SUBLIST="Installation/epm/${project_name}.sublist"

START_SCRIPT="";

arangodb_version=`cat VERSION | awk -F"." '{print $1 "." $2}'`
arangodb_release=`cat VERSION | awk -F"." '{print $3}'`  

ostype=`uname -s | tr '[:upper:]' '[:lower:]'`
osvers=`uname -r | awk -F"." '{print $1 "." $2}'`

## directories for epm
prefix=/usr
exec_prefix=${prefix}
sbindir=${exec_prefix}/sbin
initdir=/etc/init.d
bindir=${exec_prefix}/bin
data_dir=/var/lib
static_dir=${prefix}/share
vers_dir=arangodb-${arangodb_version}
docdir=${prefix}/share/doc/${vers_dir}
mandir=${prefix}/share/man
systemddir=/lib/systemd/system


########################################################
# set messages
########################################################
install_message="

ArangoDB (https://www.arangodb.org)
  A universal open-source database with a flexible data model for documents, 
  graphs, and key-values.

First Steps with ArangoDB:
  https:/www.arangodb.org/quickstart

Upgrading ArangoDB:
  https://www.arangodb.org/manuals/current/Upgrading.html

Configuration file:
  /etc/arangodb/arangod.conf

Start ArangoDB shell client:
  > ${bindir}/arangosh
"

# message for systems with systemd
start_systemd_message="
Start ArangoDB service:
  > systemctl start arangodb.service

Enable ArangoDB service:
  > systemctl enable arangodb.service
"

# message for script in /etc/init.d
start_initd_message="
Start ArangoDB service:
  > /etc/init.d/arangodb start
"

echo
echo "########################################################"
echo "Packetize on $TRI_OS_LONG"
echo "########################################################"
echo

case $TRI_OS_LONG in

  Linux-ArchLinux-*)
    echo "Packetize for ArchLinux is not not supported."
    exit 0
    ;;

  Linux-CentOS-*)
    echo "Using configuration for Centos"
    package_type="rpm"
    START_SCRIPT="rc.arangod.Centos"
    runlevels="0235"

    # exports for the epm configuration file
    export chkconf="true"
    install_message="${install_message}${start_initd_message}"
    ;;

  Darwin*)
    echo "Using configuration for DARWIN"
    ostype="macosx"
    osvers=`echo ${RELEASE} | awk -F"." '{print $1 "." $2}'`
    TRI_RELEASE=$osvers
    rusr=root
    rgrp=wheel
    susr=root
    sgrp=wheel
    package_type="osx"

    # export "macosx" for the epm configuration file
    export macosx="true"
    ;;

  Linux-Debian*)
    echo "Using configuration for Debian"
    package_type="deb"
    START_SCRIPT="rc.arangod.Debian"
    runlevels="035"

    if [ ${TRI_MACH} == "x86_64" ] ; then
      TRI_MACH="amd64"
    fi
    install_message="${install_message}${start_initd_message}"

    ;;

  Linux-Fedora*)
    echo "Using configuration for Fedora"
    package_type="rpm"
    START_SCRIPT="rc.arangod.Centos"
    runlevels="035"
    docdir=${prefix}/share/doc/packages/${vers_dir}

    # exports for the epm configuration file
    export use_systemd="true"
    install_message="${install_message}${start_systemd_message}"
    ;;

  Linux-LinuxMint-*)
    echo "Using configuration for LinuxMint"
    package_type="deb"
    START_SCRIPT="rc.arangod.Ubuntu"
    runlevels="02345"

    if [ ${TRI_MACH} == "x86_64" ] ; then
      TRI_MACH="amd64"
    fi
    install_message="${install_message}${start_initd_message}"

    ;;

  Linux-openSUSE-12*)
    echo "Using configuration for openSuSE 12"
    package_type="rpm"
    START_SCRIPT="rc.arangod.OpenSuSE"
    runlevels="035"
    docdir=${prefix}/share/doc/packages/${vers_dir}

    # exports for the epm configuration file
    export use_systemd="true"
    install_message="${install_message}${start_systemd_message}"
    ;;

  Linux-openSUSE*)
    echo "Using configuration for openSuSE"
    package_type="rpm"
    START_SCRIPT="rc.arangod.OpenSuSE"
    runlevels="035"
    docdir=${prefix}/share/doc/packages/${vers_dir}

    # exports for the epm configuration file
    export insserv="true"
    install_message="${install_message}${start_initd_message}"
    ;;

  Linux-Ubuntu-*)
    echo "Using configuration for Ubuntu"
    package_type="deb"
    START_SCRIPT="rc.arangod.Ubuntu"
    runlevels="02345"

    if [ ${TRI_MACH} == "x86_64" ] ; then
      TRI_MACH="amd64"
    fi
    install_message="${install_message}${start_initd_message}"

    ;;

  *)
    echo "Using default configuration"
    ;;

esac

if [ ${TRI_MACH} == "i686" ] ; then
  archfolder="${ostype}-${osvers}-intel"
else
  archfolder="${ostype}-${osvers}-${TRI_MACH}"
fi

echo
echo "########################################################"
echo "arangodb_version: $arangodb_version"
echo "arangodb_release: $arangodb_release"
echo "hudson_base: $hudson_base"
echo "rusr: $rusr"
echo "rgrp: $rgrp"
echo "susr: $susr"
echo "sgrp: $sgrp"
echo "package_type: $package_type"
echo "product_name: $product_name"
echo "project_name: $project_name"
echo "archfolder: $archfolder"
echo "########################################################"
echo

EPM_RPM_OPTION="--buildroot ${hudson_base}/${archfolder}/buildroot"
export EPM_RPM_OPTION

test -f ${SUBLIST} && rm -f ${SUBLIST}
touch ${SUBLIST}

share_base=${static_dir}/arangodb
sfolder_name=$(pwd)

echo 
echo "########################################################"
echo "Call mkepmlist to create a sublist"

  mkepmlist -u ${susr} -g ${sgrp} --prefix ${mandir}/man1 ${sfolder_name}/Doxygen/man/man1/*.1 >> ${SUBLIST}
  mkepmlist -u ${susr} -g ${sgrp} --prefix ${mandir}/man8 ${sfolder_name}/Doxygen/man/man8/*.8 >> ${SUBLIST}

  for dir in `find js -type d`; do
    # echo "    mkepmlist -u ${susr} -g ${sgrp} --prefix ${share_base}/${dir} ${sfolder_name}/${dir}/*.js >> ${SUBLIST}"
    mkepmlist -u ${susr} -g ${sgrp} --prefix ${share_base}/${dir} ${sfolder_name}/${dir}/*.js >> ${SUBLIST}
  done

  for dir in . css css/images media media/icons media/images js js/modules; do
    for typ in css html js png gif ico;  do
      FILES=${sfolder_name}/html/admin/${dir}/*.${typ}

      if test "${FILES}" != "${sfolder_name}/html/admin/${dir}/\*.${typ}";  then
        # echo "    mkepmlist -u ${susr} -g ${sgrp} --prefix ${share_base}/html/admin/${dir} ${sfolder_name}/html/admin/${dir}/*.${typ} >> ${SUBLIST}"
        mkepmlist -u ${susr} -g ${sgrp} --prefix ${share_base}/html/admin/${dir} ${sfolder_name}/html/admin/${dir}/*.${typ} >> ${SUBLIST}
      fi
    done
  done

echo "########################################################"
echo 

##
## build install/help message
##

cd ${hudson_base}
sudo -E rm -rf ${hudson_base}/${archfolder}
sudo -E mkdir -p ${hudson_base}/${archfolder}
sudo -E mkdir -p ${hudson_base}/${archfolder}/BUILD
sudo -E mkdir -p ${hudson_base}/${archfolder}/buildroot

echo 
echo "########################################################"
echo "Export vars for epm"
echo "   export arangodb_release=$arangodb_release"
echo "   export arangodb_version=$arangodb_version"
echo "   export bindir=$bindir"
echo "   export chkconf=$chkconf"
echo "   export data_dir=$data_dir"
echo "   export docdir=$docdir"
echo "   export exec_prefix=$exec_prefix"
echo "   export initdir=$initdir"
echo "   export insserv=$insserv"
echo "   export prefix=$prefix"
echo "   export project_dir=${sfolder_name}"
echo "   export rgrp=$rgrp"
echo "   export runlevels=$runlevels"
echo "   export rusr=$rusr"
echo "   export sbindir=$sbindir"
echo "   export sgrp=$sgrp"
echo "   export static_dir=$static_dir"
echo "   export mandir=$mandir"
echo "   export susr=$susr"
echo "   export vers_dir=$vers_dir"
echo "   export START_SCRIPT=$START_SCRIPT"
echo "   export systemddir=$systemddir"
echo "########################################################"
echo 

export arangodb_release
export arangodb_version
export bindir
export data_dir
export docdir
export exec_prefix
export initdir
export prefix
export project_dir=${sfolder_name}
export rgrp
export runlevels
export rusr
export sbindir
export sgrp
export static_dir
export susr
export vers_dir
export START_SCRIPT
export install_message
export mandir
export systemddir

echo 
echo "########################################################"
echo "Call EPM to build the package."
echo "  sudo -E epm -a ${TRI_MACH} -f ${package_type} ${product_name} ${sfolder_name}/${LIST}"
sudo -E epm -a ${TRI_MACH} -f ${package_type} ${product_name} ${sfolder_name}/${LIST} || exit 1
echo "########################################################"
echo 

if [ ${package_type} == "osx" ] ; then
  package_type="dmg"
fi

package_name="$product_name-$arangodb_version-$arangodb_release-$TRI_DIST-$TRI_RELEASE-$TRI_MACH.$package_type"

# Delete old package in hudson's home folder.
rm -f ${hudson_base}/${package_name} > /dev/null

echo 
echo "########################################################"
echo "Copy package '${product_name}*.${package_type}' to '${package_name}'."
echo "    cp -p ${hudson_base}/${archfolder}/${product_name}*.${package_type} ${sfolder_name}/${package_name}"
cp -p ${hudson_base}/${archfolder}/${product_name}*.${package_type} ${sfolder_name}/${package_name}
echo "########################################################"
echo 

start_server=
stop_server=
install_package=
remove_package=
mount_install_package=
unmount_install_package=

case $TRI_OS_LONG in

  Linux-openSUSE-12*)
    start_server="sudo systemctl start arangodb.service"
    stop_server="sudo systemctl stop arangodb.service"

    install_package="sudo rpm -i ${sfolder_name}/${package_name}"
    remove_package="sudo rpm -e $product_name"

    ;;

  Linux-openSUSE*)
    start_server="sudo /etc/init.d/arangodb start"
    stop_server="sudo /etc/init.d/arangodb stop"

    install_package="sudo rpm -i ${sfolder_name}/${package_name}"
    remove_package="sudo rpm -e $product_name"

    ;;

  Linux-Debian*)
    start_server="sudo /etc/init.d/arangodb start"
    stop_server="sudo /etc/init.d/arangodb stop"

    install_package="sudo dpkg -i ${sfolder_name}/${package_name}"
    remove_package="sudo dpkg --purge $product_name"
    ;;

  Linux-CentOS-*)
    start_server="sudo /etc/init.d/arangodb start"
    stop_server="sudo /etc/init.d/arangodb stop"

    install_package="sudo rpm -i ${sfolder_name}/${package_name}"
    remove_package="sudo rpm -e $product_name"
    ;;

  Linux-Ubuntu-*)
    start_server="sudo /etc/init.d/arangodb start"
    stop_server="sudo /etc/init.d/arangodb stop"

    install_package="sudo dpkg -i ${sfolder_name}/${package_name}"
    remove_package="sudo dpkg --purge $product_name"
    ;;

  Linux-LinuxMint-*)
    start_server="sudo /etc/init.d/arangodb start"
    stop_server="sudo /etc/init.d/arangodb stop"

    install_package="sudo dpkg -i ${sfolder_name}/${package_name}"
    remove_package="sudo dpkg --purge $product_name"
    ;;

  Darwin*)
    start_server=""
    stop_server="sudo launchctl unload /Library/LaunchDaemons/org.arangodb.plist"

    install_package="sudo installer -pkg /Volumes/${product_name}/${product_name}.pkg -target / "
    remove_package="sudo rm /Library/LaunchDaemons/org.arangodb.plist*"

    mount_install_package="hdiutil attach ${sfolder_name}/${package_name}"
    unmount_install_package="hdiutil detach /Volumes/${product_name}"
    ;;

  *)
    ;;

esac

echo 
echo "########################################################"
echo "                     INSTALL TEST "
echo "########################################################"
echo "Stop and uninstall server"

# stop and uninstall server
if [ "${stop_server}x" != "x" ]; then 
  echo "     $stop_server"
  $stop_server
fi

if [ "${remove_package}x" != "x" ]; then 
  echo "     $remove_package"
  $remove_package
fi

if [ "${unmount_install_package}x" != "x" ]; then 
  echo "     $unmount_install_package"
  $unmount_install_package
fi

if [ "${mount_install_package}x" != "x" ]; then 
  echo "     $mount_install_package"
  $mount_install_package
fi

echo 
echo "########################################################"
echo "Install"
echo "    ${install_package}"
${install_package} || exit 1
echo "########################################################"

echo "Successfully installed ${package_name}."
echo "########################################################"

if [ "${start_server}x" != "x" ]; then 
echo "Start"
echo "    ${start_server}"
${start_server}
echo "########################################################"
fi

echo "Check process"
process=$(ps aux | grep arangod | grep -v grep)
echo "$process"

echo "Wait for server..."
echo "    sleep 10"
sleep 10

echo 
echo "########################################################"
echo "Request version number "
echo "   $curl_version"
answer=$( $curl_version )
expect='{"server":"arango","version":"'$arangodb_version.$arangodb_release'"}'
if [ "x$answer" == "x$expect" ]; then 
  echo "ok: $answer"
else
  echo "error: $answer != $expect"
  sudo tail -50 /var/log/rangodb/arangod.log
fi
echo "########################################################"
echo 

echo 
echo "########################################################"
if [ "${stop_server}x" != "x" ]; then 
echo "Stop"
echo "    $stop_server"
$stop_server || exit 1
echo "########################################################"
fi
if [ "${remove_package}x" != "x" ]; then 
echo "Uninstall"
echo "    $remove_package"
$remove_package || exit 1
echo "########################################################"
echo "Successfully uninstalled ${product_name}."
echo "########################################################"
fi
echo 

if [ "${unmount_install_package}x" != "x" ]; then 
  $unmount_install_package > /dev/null 2>&1 
fi

if [ "x$answer" == "x$expect" ]; then 
  exit 0
else
  exit 1
fi
