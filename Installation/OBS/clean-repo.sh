#!/bin/bash

rm -f 'download/robots.txt'

test -d stable || mkdir stable || exit 1

mv download/repositories/home:/fceller/* stable || exit 1

rm -rf 'download'
rm -rf 'stable/CentOS_CentOS-5'
rm -rf 'stable/RedHat_RHEL-5'
rm -rf 'stable/SLE_10_SDK'

find stable -name "index.html*" -exec 'rm' '{}' ';'
find stable -name "home:fceller.repo*" -exec 'rm' '{}' ';'
find stable -name "*.meta4" -exec 'rm' '{}' ';'
find stable -name "*.metalink" -exec 'rm' '{}' ';'
find stable -name "*.mirrorlist" -exec 'rm' '{}' ';'
find stable -name "repocache" -prune -exec 'rmdir' '{}' ';' 
