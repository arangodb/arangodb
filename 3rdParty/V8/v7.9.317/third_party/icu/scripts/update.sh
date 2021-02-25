#!/bin/bash
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is used to prepare for a major version update of ICU (e.g. from
# 54.1 to 56.1). Running this script is step 1 in README.chromium.

if [ $# -lt 1 ];
then
  echo "Usage: "$0" version (e.g. '56-1')" >&2
  exit 1
fi

version="$1"
repoprefix="https://github.com/unicode-org/icu/tags/release-"
repo="${repoprefix}${version}/icu4c"
treeroot="$(dirname "$0")/.."

# Check if the repo for $version is available.
svn ls "${repo}" > /dev/null 2>&1  || \
    { echo "${repo} does not exist." >&2; exit 2; }

echo "Cleaning up source/ ..."
for file in source LICENSE license.html readme.html APIChangeReport.html
do
  rm -rf "${treeroot}/${file}"
done

echo "Download ${version} from the upstream repository ..."
for file in source LICENSE license.html readme.html APIChangeReport.html
do
  svn export --native-eol LF "${repo}/${file}" "${treeroot}/${file}"
done

echo "deleting directories we don't care about ..."
for d in layoutex data/xml test allinone
do
  rm -rf "${treeroot}/source/${d}"
done

echo "deleting Visual Studio build files ..."
find "${treeroot}/source" -name *vcxp* -o -name *sln | xargs rm

echo "restoring local data and configuration files ..."
while read line
do
  # $line is not quoted to expand "*html.ucm".
  git checkout -- "${treeroot}/source/data/"${line}
done < "${treeroot}/scripts/data_files_to_preserve.txt"

echo "Patching configure to work without source/{layoutex,test}  ..."
sed -i.orig -e '/^ac_config_files=/ s:\ layoutex/Makefile::g' \
  -e '/^ac_config_files=/ s: test/.* samples/M: samples/M:' \
  "${treeroot}/source/configure"
rm -f "${treeroot}/source/configure.orig"

echo "git-adding new files"
git status source | sed -n '/^Untracked/,$ p' | grep source | xargs git add

cd "${treeroot}"

echo "Updating BUILD.gn"

find  source/i18n -maxdepth 1  ! -type d  | egrep  '\.(c|cpp|h)$' |sort | \
  sed 's/^\(.*\)$/    "\1",/' > i18n_src.list
ls source/i18n/unicode/*h | sort | sed 's/^\(.*\)$/    "\1",/' > i18n_hdr.list

find  source/common -maxdepth 1  ! -type d  | egrep  '\.(c|cpp|h)$' |sort | \
  sed 's/^\(.*\)$/    "\1",/' > common_src.list
ls source/common/unicode/*h | sort | \
  sed 's/^\(.*\)$/    "\1",/' > common_hdr.list

sed   -i \
  '/I18N_SRC_START/,/I18N_SRC_END/ {
      /I18N_SRC_START/ r i18n_src.list
      /source.i18n/ d
   }
   /I18N_HDR_START/,/I18N_HDR_END/ {
      /I18N_HDR_START/ r i18n_hdr.list
      /source.i18n/ d
   }
   /COMMON_SRC_START/,/COMMON_SRC_END/ {
      /COMMON_SRC_START/ r common_src.list
      /source.common/ d
   }
   /COMMON_HDR_START/,/COMMON_HDR_END/ {
      /COMMON_HDR_START/ r common_hdr.list
      /source.common/ d
   }' BUILD.gn

echo "Updating icu.gyp* files"

ls source/i18n/unicode/*h | sort | \
  sed "s/^.*i18n\/\(.*\)$/              '\1',/" > i18n_hdr.list
ls source/common/unicode/*h | sort | \
  sed "s/^.*common\/\(.*\)$/              '\1',/" > common_hdr.list


find  source/i18n -maxdepth 1  ! -type d  | egrep  '\.(c|cpp)$' | \
  sort | sed "s/^\(.*\)$/      '\1',/" > i18n_src.list
find  source/common -maxdepth 1  ! -type d  | egrep  '\.(c|cpp)$' | \
  sort | sed "s/^\(.*\)$/      '\1',/" > common_src.list

sed   -i \
  '/I18N_HDR_START/,/I18N_HDR_END/ {
      /I18N_HDR_START/ r i18n_hdr.list
      /.unicode.*\.h.,$/ d
   }
   /COMMON_HDR_START/,/COMMON_HDR_END/ {
      /COMMON_HDR_START/ r common_hdr.list
      /.unicode.*\.h.,$/ d
   }' icu.gyp

sed   -i \
  '/I18N_SRC_START/,/I18N_SRC_END/ {
      /I18N_SRC_START/ r i18n_src.list
      /source\/i18n/ d
   }
   /COMMON_SRC_START/,/COMMON_SRC_END/ {
      /COMMON_SRC_START/ r common_src.list
      /source\/common/ d
   }' icu.gypi


echo "Done"
