#!/bin/bash

cd 3rdParty/tzdata; rm -f * 
URL=https://data.iana.org/time-zones/releases/
curl -q $URL$(curl $URL | grep 'tzdata....a.tar.gz"' |sed 's;.*href="\(.*\)">tzdata.*;\1;' | tail -n 1) --output - | tar -xvzf -

git clone https://github.com/unicode-org/cldr.git
cd cldr
git checkout $(git tag -l --sort=refname release-* |egrep 'release-[0-9]* *$' |tail -n 1)
cp common/supplemental/windowsZones.xml ..
cd ..
rm -rf cldr

git add *
git commit . -m "refresh timezone database"

