#!/bin/bash
set -e

# prepare wrapper for gold
mkdir -p $HOME/bin/gold
(echo '#!/bin/bash'; echo 'gold "$@"') > $HOME/bin/gold/ld
chmod a+x $HOME/bin/gold/ld

# prepare CCACHE
(echo '#!/bin/bash'; echo 'ccache /usr/bin/gcc-5 "$@"') > $HOME/bin/gcc
chmod a+x $HOME/bin/gcc
                                                         
(echo '#!/bin/bash'; echo 'ccache /usr/bin/g++-5 "$@"') > $HOME/bin/g++
chmod a+x $HOME/bin/g++

# prepare files for unit test
d='UnitTests/HttpInterface'

echo
echo "$0: switching into ${d}"
cd "${d}"

echo
echo "$0: installing bundler"
gem install bundler

echo
echo "$0: executing bundle"
bundle
