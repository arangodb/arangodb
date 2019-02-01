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
repoprefix="http://source.icu-project.org/repos/icu/icu/tags/release-"
repo="${repoprefix}${version}"
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

# TODO(jshin): Automatically update BUILD.gn and icu.gypi with the updated
# list of source files.

echo "Done"
