#!/usr/bin/bash

# Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

mkdir -p temp
doxygen source.dox
cd temp
xsltproc combine.xslt index.xml > all.xml
xsltproc ../reference.xsl all.xml > ../reference.qbk

