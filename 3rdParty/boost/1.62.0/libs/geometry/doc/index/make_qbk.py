#! /usr/bin/env python
# -*- coding: utf-8 -*-
# ===========================================================================
#  Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.
#  Copyright (c) 2011-2013 Adam Wulkiewicz, Lodz, Poland.
# 
#  Use, modification and distribution is subject to the Boost Software License,
#  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt)9
# ============================================================================

import os, sys

cmd = "doxygen_xml2qbk"
cmd = cmd + " --xml xml/%s.xml"
cmd = cmd + " --start_include boost/"
cmd = cmd + " --output_style alt"
cmd = cmd + " > generated/%s.qbk"

def run_command(command):
    if os.system(command) != 0:
        raise Exception("Error running %s" % command)

def remove_all_files(dir):
    if os.path.exists(dir):
        for f in os.listdir(dir):
            os.remove(dir+f)

remove_all_files("xml/")

run_command("doxygen Doxyfile")
run_command(cmd % ("classboost_1_1geometry_1_1index_1_1rtree", "rtree"))
run_command(cmd % ("group__rtree__functions", "rtree_functions"))

run_command(cmd % ("structboost_1_1geometry_1_1index_1_1linear", "rtree_linear"))
run_command(cmd % ("structboost_1_1geometry_1_1index_1_1quadratic", "rtree_quadratic"))
run_command(cmd % ("structboost_1_1geometry_1_1index_1_1rstar", "rtree_rstar"))
run_command(cmd % ("classboost_1_1geometry_1_1index_1_1dynamic__linear", "rtree_dynamic_linear"))
run_command(cmd % ("classboost_1_1geometry_1_1index_1_1dynamic__quadratic", "rtree_dynamic_quadratic"))
run_command(cmd % ("classboost_1_1geometry_1_1index_1_1dynamic__rstar", "rtree_dynamic_rstar"))

run_command(cmd % ("structboost_1_1geometry_1_1index_1_1indexable", "indexable"))
run_command(cmd % ("structboost_1_1geometry_1_1index_1_1equal__to", "equal_to"))

run_command(cmd % ("group__predicates", "predicates"))
#run_command(cmd % ("group__nearest__relations", "nearest_relations"))
run_command(cmd % ("group__adaptors", "adaptors"))
run_command(cmd % ("group__inserters", "inserters"))

#run_command("b2")
