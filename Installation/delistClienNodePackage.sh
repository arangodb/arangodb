#!/usr/bin/env bash

ferr() { echo "$*"; exit 1; }

if [ -z "${1// /}" ]; then
  ferr "Package must be specified!"
else
  package=$1
fi

installDir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

if [[ ! -e $installDir/../js/node/package.json || ! -e $installDir/../js/node/package-lock.json ]]; then
  ferr "No $installDir/../js/node/package.json or $installDir/../js/node/package-lock.json is present!"
fi

if [[ $(type npm >/dev/null 2>&1) ]]; then
  ferr "npm isn't present in PATH or not installed in system!"
elif [[ $(npm --version | cut -d'.' -f 1) < 5 ]]; then
  echo "A" #ferr "Installed npm is $(npm --version). 5+ version is required!"
fi

test -d $installDir/../tmp/node && rm -rf $installDir/../tmp/node ; mkdir -p $installDir/../tmp/node
pushd $installDir/../tmp/node
cp $installDir/../js/node/package{,-lock}.json .
npm un --save $package && $(for i in package{,-lock}.json; do cp $i $installDir/../js/node/$i-no-$package; done)
popd

rm -rf $installDir/../tmp/node

git add $installDir/../js/node/package{,-lock}.json-no-$package
git commit -m "Update package.json-no-$package and package-lock.json-no-$package with delisted $package"
