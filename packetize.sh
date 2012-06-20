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
LIST="Installation/${project_name}.list"
SUBLIST="Installation/${project_name}.sublist"
START_SCRIPT="";

arangodb_version=`cat VERSION | awk -F"." '{print $1 "." $2}'`
arangodb_release=`cat VERSION | awk -F"." '{print $3}'`  

ostype=`uname -s | tr '[:upper:]' '[:lower:]'`
osvers=`uname -r | awk -F"." '{print $1 "." $2}'`

## directories for epm
prefix=/usr
exec_prefix=${prefix}
bindir=${exec_prefix}/sbin
datadir=${prefix}/share
vers_dir=arango-${arangodb_version}
runlevels="runlevel(035)"
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
    export insserv="true"
    runlevels="runlevel(035)"
    docdir=${prefix}/share/doc/packages/voc/${vers_dir}
    ;;

  Linux-Debian*)
    echo "Using configuration for Debian"
    package_type="deb"
    START_SCRIPT="rc.arangodb.Debian"
    export insserv="true"
    runlevels="runlevel(035)"

    if [ ${TRI_MACH} == "x86_64" ] ; then
      TRI_MACH="amd64"
    fi
    ;;

  Linux-CentOS-*)
    echo "Using configuration for Centos"
    package_type="rpm"
    START_SCRIPT="rc.arangodb.Centos"
    export insserv="true"
    runlevels="runlevel(0235)"

    ;;

  Linux-Ubuntu-*)
    echo "Using configuration for Ubuntu"
    package_type="deb"
    START_SCRIPT="rc.arangodb.Ubuntu"
    export insserv="true"
    runlevels="runlevel(0235)"

    if [ ${TRI_MACH} == "x86_64" ] ; then
      TRI_MACH="amd64"
    fi
    ;;

  Darwin*)
    echo "Using configuration for DARWIN"
    ostype="macosx"
    osvers=${RELEASE}
    rusr=root
    rgrp=wheel
    susr=root
    sgrp=wheel
    package_type="dmg"
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

package_name="$product_name-$arangodb_version-$arangodb_release-$TRI_DIST-$TRI_RELEASE-$TRI_MACH.$package_type"

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
echo "package_name: $package_name"
echo "########################################################"
echo

EPM_RPM_OPTION="--buildroot ${hudson_base}/${archfolder}/buildroot"
export EPM_RPM_OPTION

test -f ${SUBLIST} && rm -f ${SUBLIST}
touch ${SUBLIST}

share_base=/usr/share/voc/arangodb-${arangodb_version}
sfolder_name=$(pwd)

echo 
echo "########################################################"
echo "Call mkepmlist to create a sublist"

  for dir in modules modules/jsunity system; do
    echo "    mkepmlist -u ${susr} -g ${sgrp} --prefix ${share_base}/js ${sfolder_name}/js/${dir}/*.js >> ${SUBLIST}"
    mkepmlist -u ${susr} -g ${sgrp} --prefix ${share_base}/js ${sfolder_name}/js/${dir}/*.js > ${SUBLIST}
  done

echo "Call mkepmlist to create a sublist"
  for dir in . css css/images images script; do
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

################################################################################
## Export vars for epm 
################################################################################
export arangodb_version
export arangodb_release
export rusr
export rgrp
export susr
export sgrp
export prefix
export exec_prefix
export bindir
export datadir
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

# Delete old package in hudson's home folder.
rm -f ${hudson_base}/${package_name} > /dev/null

echo 
echo "########################################################"
echo "Copy package '${product_name}*.${package_type}' to '${package_name}'."
echo "    cp -p ${hudson_base}/${archfolder}/${product_name}*.${package_type} ${hudson_base}/${package_name}"
cp -p ${hudson_base}/${archfolder}/${product_name}*.${package_type} ${hudson_base}/${package_name}
echo "########################################################"
echo 
