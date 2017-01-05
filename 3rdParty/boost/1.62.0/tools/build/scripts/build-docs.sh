#!/bin/bash

# Copyright 2014 Vladimir Prus
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

set -e

# Build the documentation
touch doc/jamroot.jam
export BOOST_BUILD_PATH=`pwd`
export BOOST_ROOT=/home/ghost/Sources/boost
./bootstrap.sh
cd doc
../b2

find . -type f -iname "*.html" | while read i; do
    echo "Processing: $i"
    sed -i "s#</body>#\
    <script>\n\
      (function(i,s,o,g,r,a,m){i['GoogleAnalyticsObject']=r;i[r]=i[r]||function(){\n\
      (i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();a=s.createElement(o),\n\
      m=s.getElementsByTagName(o)[0];a.async=1;a.src=g;m.parentNode.insertBefore(a,m)\n\
      })(window,document,'script','//www.google-analytics.com/analytics.js','ga');\n\
\n\
      ga('create', 'UA-2917240-2', 'auto');\n\
      ga('send', 'pageview');\n\
\n\
    </script>\n\
</body>#g" "$i"

done

