#!/bin/bash

echo "########################################################"
echo "#                   packetize.sh                       #"
echo "########################################################"
echo

. config/detect_distro.sh

hudson_base="$HOME"
rusr=root
rgrp=root
susr=arango
sgrp=arango
package_type=""
product_name="arangodb"
project_name="arangodb"
runlevels="runlevel(035)"
curl_version="curl -s -o - http://localhost:8529/_api/version"

# name of the epm configuration file
LIST="Installation/${project_name}.list"
# name of a created epm  include file
SUBLIST="Installation/${project_name}.sublist"

START_SCRIPT="";

arangodb_version=`cat VERSION | awk -F"." '{print $1 "." $2}'`
arangodb_release=`cat VERSION | awk -F"." '{print $3}'`  

ostype=`uname -s | tr '[:upper:]' '[:lower:]'`
osvers=`uname -r | awk -F"." '{print $1 "." $2}'`

## directories for epm
prefix=/usr
exec_prefix=${prefix}
sbindir=${exec_prefix}/sbin
bindir=${exec_prefix}/bin
data_dir=/var
static_dir=${prefix}/share
vers_dir=arango-${arangodb_version}
docdir=${prefix}/share/doc/voc/${vers_dir}


echo
echo "########################################################"
echo "Packetize on $TRI_OS_LONG"
echo "########################################################"
echo


case $TRI_OS_LONG in

  Linux-openSUSE*)
    echo "Using configuration for openSuSE"
    package_type="rpm"
    START_SCRIPT="rc.arangodb.OpenSuSE"
    runlevels="runlevel(035)"
    docdir=${prefix}/share/doc/packages/voc/${vers_dir}

    # export "insserv" for the epm configuration file
    export insserv="true"
    ;;

  Linux-Debian*)
    echo "Using configuration for Debian"
    package_type="deb"
    START_SCRIPT="rc.arangodb.Debian"
    runlevels="runlevel(035)"

    if [ ${TRI_MACH} == "x86_64" ] ; then
      TRI_MACH="amd64"
    fi

    # export "insserv" for the epm configuration file
    export insserv="true"
    ;;

  Linux-CentOS-*)
    echo "Using configuration for Centos"
    package_type="rpm"
    START_SCRIPT="rc.arangodb.Centos"
    runlevels="runlevel(0235)"

    # export "insserv" for the epm configuration file
    export insserv="true"
    ;;

  Linux-Ubuntu-*)
    echo "Using configuration for Ubuntu"
    package_type="deb"
    START_SCRIPT="rc.arangodb.Ubuntu"
    runlevels="runlevel(02345)"

    if [ ${TRI_MACH} == "x86_64" ] ; then
      TRI_MACH="amd64"
    fi

    # export "insserv" for the epm configuration file
    export insserv="true"
    ;;

  Linux-LinuxMint-*)
    echo "Using configuration for LinuxMint"
    package_type="deb"
    START_SCRIPT="rc.arangodb.Ubuntu"
    runlevels="runlevel(02345)"

    if [ ${TRI_MACH} == "x86_64" ] ; then
      TRI_MACH="amd64"
    fi

    # export "insserv" for the epm configuration file
    export insserv="true"
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

share_base=${static_dir}/arango
sfolder_name=$(pwd)

echo 
echo "########################################################"
echo "Call mkepmlist to create a sublist"

  for dir in js/server/modules js/common/modules js/actions/system js/client js/common/bootstrap; do
      echo "    mkepmlist -u ${susr} -g ${sgrp} --prefix ${share_base}/${dir} ${sfolder_name}/${dir}/*.js >> ${SUBLIST}"
      mkepmlist -u ${susr} -g ${sgrp} --prefix ${share_base}/${dir} ${sfolder_name}/${dir}/*.js >> ${SUBLIST}
  done

  for dir in . css css/images media media/icons media/images js js/modules; do
    for typ in css html js png gif ico;  do
      FILES=${sfolder_name}/html/admin/${dir}/*.${typ}

      if test "${FILES}" != "${sfolder_name}/html/admin/${dir}/\*.${typ}";  then
        echo "    mkepmlist -u ${susr} -g ${sgrp} --prefix ${share_base}/html/admin/${dir} ${sfolder_name}/html/admin/${dir}/*.${typ} >> ${SUBLIST}"
        mkepmlist -u ${susr} -g ${sgrp} --prefix ${share_base}/html/admin/${dir} ${sfolder_name}/html/admin/${dir}/*.${typ} >> ${SUBLIST}
      fi
    done
  done

echo "########################################################"
echo 

cd ${hudson_base}
sudo -E rm -rf ${hudson_base}/${archfolder}
sudo -E mkdir -p ${hudson_base}/${archfolder}
sudo -E mkdir -p ${hudson_base}/${archfolder}/BUILD
sudo -E mkdir -p ${hudson_base}/${archfolder}/buildroot

echo 
echo "########################################################"
echo "Export vars for epm"
echo "   export arangodb_version=$arangodb_version"
echo "   export arangodb_release=$arangodb_release"
echo "   export rusr=$rusr"
echo "   export rgrp=$rgrp"
echo "   export susr=$susr"
echo "   export sgrp=$sgrp"
echo "   export prefix=$prefix"
echo "   export exec_prefix=$exec_prefix"
echo "   export sbindir=$sbindir"
echo "   export bindir=$bindir"
echo "   export data_dir=$data_dir"
echo "   export static_dir=$static_dir"
echo "   export vers_dir=$vers_dir"
echo "   export START_SCRIPT=$START_SCRIPT"
echo "   export runlevels=$runlevels"
echo "   export docdir=$docdir"
echo "   export project_dir=${sfolder_name}"
echo "########################################################"
echo 

export arangodb_version
export arangodb_release
export rusr
export rgrp
export susr
export sgrp
export prefix
export exec_prefix
export sbindir
export bindir
export data_dir
export static_dir
export vers_dir
export START_SCRIPT
export runlevels
export docdir
export project_dir=${sfolder_name}


echo 
echo "########################################################"
echo "Call EPM to build the package."
if [ "$TRI_MACH" == "amd64" ] ; then
  echo "  sudo -E epm -a ${EPM_ARCH} -f ${package_type} ${product_name} ${sfolder_name}/${LIST}"
  sudo -E epm -a ${TRI_MACH} -f ${package_type} ${product_name} ${sfolder_name}/${LIST} || exit 1
else
  echo "  sudo -E epm -f ${package_type} ${product_name} ${sfolder_name}/${LIST}"
  sudo -E epm -f ${package_type} ${product_name} ${sfolder_name}/${LIST} || exit 1
fi
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

  Linux-openSUSE*)
    start_server=""
    stop_server=""

    install_package="sudo rpm -i ${sfolder_name}/${package_name}"
    remove_package="sudo rpm -e $product_name"

    ;;

  Linux-Debian*)
    start_server=""
    stop_server=""

    install_package="sudo dpkg -i ${sfolder_name}/${package_name}"
    remove_package="sudo dpkg --purge $product_name"
    ;;

  Linux-CentOS-*)
    start_server=""
    stop_server=""

    install_package="sudo rpm -i ${sfolder_name}/${package_name}"
    remove_package="sudo rpm -e $product_name"
    ;;

  Linux-Ubuntu-*)
    start_server="sudo /etc/init.d/arango start"
    stop_server="sudo /etc/init.d/arango stop"

    install_package="sudo dpkg -i ${sfolder_name}/${package_name}"
    remove_package="sudo dpkg --purge $product_name"
    ;;

  Linux-LinuxMint-*)
    start_server=""
    stop_server="sudo /etc/init.d/arango stop"

    install_package="sudo dpkg -i ${sfolder_name}/${package_name}"
    remove_package="sudo dpkg --purge $product_name"
    ;;

  Darwin*)
    start_server=""
    stop_server="sudo launchctl unload /Library/LaunchDaemons/org.arangodb.plist"

    install_package="sudo installer -pkg /Volumes/${product_name}/${product_name}.pkg -target / "
    remove_package=""

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

if [ "${start_server}x" != "x" ]; then 
echo "Start"
echo "    ${start_server}"
${start_server} || exit 1
echo "########################################################"
fi

echo "Successfully installed ${package_name}."
echo "########################################################"

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
  exit 1
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
