#!/bin/bash

# set -x

ICUROOT="$(dirname "$0")/.."

if [ $# -lt 2 ];
then
  echo "Usage: "$0" icubuilddir1 icubuilddir2" >&2
  echo "$0 compare data files of all builds inside two icu build directories." >&2
  echo "These files were previously archived by backup_outdir in scripts/copy_data.sh." >&2
  exit 1
fi

DIR1=$1
DIR2=$2

echo "#######################################################"
echo "             ICUDT*L.DAT FILE SIZE REPORT"
echo "#######################################################"
for build in "chromeos" "common" "cast" "android" "android_small" "android_extra" "ios" "flutter"
do
  ICUDT_L_DAT1=`ls ${DIR1}/dataout/${build}/data/out/tmp/icudt*l.dat`
  ICUDT_L_DAT2=`ls ${DIR2}/dataout/${build}/data/out/tmp/icudt*l.dat`
  STAT1=`stat --printf="%s" ${ICUDT_L_DAT1}`
  STAT2=`stat --printf="%s" ${ICUDT_L_DAT2}`
  SIZEDIFF=`expr $STAT2 - $STAT1`
  echo $build $STAT1 $STAT2 $SIZEDIFF
done

echo "#######################################################"
echo "             PER BUILD REPORT"
echo "#######################################################"
for build in "chromeos" "common" "cast" "android" "android_small" "ios" "flutter"
do
  $ICUROOT/scripts/diff_data.sh $build ${DIR1} ${DIR2}
done
