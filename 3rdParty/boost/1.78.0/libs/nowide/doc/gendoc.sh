#!/bin/bash
#
#  Copyright (c) 2009-2011 Artyom Beilis (Tonkikh)
#
#  Distributed under the Boost Software License, Version 1.0. (See
#  accompanying file LICENSE or copy at
#  http://www.boost.org/LICENSE_1_0.txt)
#

set -euo pipefail

cd "$(dirname "$0")"
rm -f html/*
doxygen
