#! /usr/bin/env python
# -*- coding: utf-8 -*-
# ===========================================================================
#  Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
#  Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
#  Copyright (c) 2009-2012 Mateusz Loskot (mateusz@loskot.net), London, UK
#  Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland
# 
#  Use, modification and distribution is subject to the Boost Software License,
#  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt)
# ============================================================================

import os, sys, shutil

script_dir = os.path.dirname(__file__)
os.chdir(os.path.abspath(script_dir))
print("Boost.Geometry is making .qbk files in %s" % os.getcwd())

if 'DOXYGEN' in os.environ:
    doxygen_cmd = os.environ['DOXYGEN']
else:
    doxygen_cmd = 'doxygen'

if 'DOXYGEN_XML2QBK' in os.environ:
    doxygen_xml2qbk_cmd = os.environ['DOXYGEN_XML2QBK']
elif '--doxygen-xml2qbk' in sys.argv:
    doxygen_xml2qbk_cmd = sys.argv[sys.argv.index('--doxygen-xml2qbk')+1]
else:
    doxygen_xml2qbk_cmd = 'doxygen_xml2qbk'
os.environ['PATH'] = os.environ['PATH']+os.pathsep+os.path.dirname(doxygen_xml2qbk_cmd)
doxygen_xml2qbk_cmd = os.path.basename(doxygen_xml2qbk_cmd)

cmd = doxygen_xml2qbk_cmd
cmd = cmd + " --xml doxy/doxygen_output/xml/%s.xml"
cmd = cmd + " --start_include boost/geometry/"
cmd = cmd + " --convenience_header_path ../../../boost/geometry/"
cmd = cmd + " --convenience_headers geometry.hpp,geometries/geometries.hpp"
cmd = cmd + " --skip_namespace boost::geometry::"
cmd = cmd + " --copyright src/copyright_block.qbk"
cmd = cmd + " --output_member_variables false"
cmd = cmd + " > generated/%s.qbk"

def run_command(command):
    if os.system(command) != 0:
        raise Exception("Error running %s" % command)

def remove_all_files(dir_relpath):
    if os.path.exists(dir_relpath):
        dir_abspath = os.path.join(os.getcwd(), dir_relpath)
        print("Boost.Geometry is cleaning Doxygen files in %s" % dir_abspath)
        shutil.rmtree(dir_abspath, ignore_errors=True)

def call_doxygen():
    os.chdir("doxy")
    remove_all_files("doxygen_output/xml/")
    run_command(doxygen_cmd)
    os.chdir("..")

def group_to_quickbook(section):
    run_command(cmd % ("group__" + section.replace("_", "__"), section))

def model_to_quickbook(section):
    run_command(cmd % ("classboost_1_1geometry_1_1model_1_1" + section.replace("_", "__"), section))

def model_to_quickbook2(classname, section):
    run_command(cmd % ("classboost_1_1geometry_1_1model_1_1" + classname, section))

def struct_to_quickbook(section):
    run_command(cmd % ("structboost_1_1geometry_1_1" + section.replace("_", "__"), section))

def class_to_quickbook(section):
    run_command(cmd % ("classboost_1_1geometry_1_1" + section.replace("_", "__"), section))

def class_to_quickbook2(classname, section):
    run_command(cmd % ("classboost_1_1geometry_1_1" + classname, section))

def strategy_to_quickbook(section):
    p = section.find("::")
    ns = section[:p]
    strategy = section[p+2:]
    run_command(cmd % ("classboost_1_1geometry_1_1strategy_1_1"
        + ns.replace("_", "__") + "_1_1" + strategy.replace("_", "__"), 
        ns + "_" + strategy))
        
def cs_to_quickbook(section):
    run_command(cmd % ("structboost_1_1geometry_1_1cs_1_1" + section.replace("_", "__"), section))
        

call_doxygen()

algorithms = ["append", "assign", "make", "clear"
    , "area", "buffer", "centroid", "convert", "correct", "covered_by"
    , "convex_hull", "crosses", "difference", "disjoint", "distance" 
    , "envelope", "equals", "expand", "for_each", "is_empty"
    , "is_simple", "is_valid", "intersection", "intersects", "length"
    , "num_geometries", "num_interior_rings", "num_points"
    , "num_segments", "overlaps", "perimeter", "relate", "relation"
    , "reverse", "simplify", "sym_difference", "touches"
    , "transform", "union", "unique", "within"]

access_functions = ["get", "set", "exterior_ring", "interior_rings"
    , "num_points", "num_interior_rings", "num_geometries"]
    
coordinate_systems = ["cartesian", "geographic", "polar", "spherical", "spherical_equatorial"]

core = ["closure", "coordinate_system", "coordinate_type", "cs_tag"
    , "dimension", "exception", "interior_type"
    , "degree", "radian"
    , "is_radian", "point_order"
    , "point_type", "ring_type", "tag", "tag_cast" ]

exceptions = ["exception", "centroid_exception"];

iterators = ["circular_iterator", "closing_iterator"
    , "ever_circling_iterator"]

models = ["point", "linestring", "box"
    , "polygon", "segment", "ring"
    , "multi_linestring", "multi_point", "multi_polygon", "referring_segment"]


strategies = ["distance::pythagoras", "distance::pythagoras_box_box"
    , "distance::pythagoras_point_box", "distance::haversine"
    , "distance::cross_track", "distance::cross_track_point_box"
    , "distance::projected_point"
    , "within::winding", "within::franklin", "within::crossings_multiply"
    , "area::surveyor", "area::spherical"
    #, "area::geographic"
    , "buffer::point_circle", "buffer::point_square"
    , "buffer::join_round", "buffer::join_miter"
    , "buffer::end_round", "buffer::end_flat"
    , "buffer::distance_symmetric", "buffer::distance_asymmetric"
    , "buffer::side_straight"
    , "centroid::bashein_detmer", "centroid::average"
    , "convex_hull::graham_andrew"
    , "simplify::douglas_peucker"
    , "side::side_by_triangle", "side::side_by_cross_track", "side::spherical_side_formula"
    , "transform::inverse_transformer", "transform::map_transformer"
    , "transform::rotate_transformer", "transform::scale_transformer"
    , "transform::translate_transformer", "transform::matrix_transformer"
    ]
    
views = ["box_view", "segment_view"
    , "closeable_view", "reversible_view", "identity_view"]



for i in algorithms:
    group_to_quickbook(i)
    
for i in access_functions:
    group_to_quickbook(i)
    
for i in coordinate_systems:
    cs_to_quickbook(i)

for i in core:
    struct_to_quickbook(i)

for i in exceptions:
    class_to_quickbook(i)

for i in iterators:
    struct_to_quickbook(i)

for i in models:
    model_to_quickbook(i)
   
for i in strategies:
    strategy_to_quickbook(i)

for i in views:
    struct_to_quickbook(i)
    

model_to_quickbook2("d2_1_1point__xy", "point_xy")

group_to_quickbook("arithmetic")
group_to_quickbook("enum")
group_to_quickbook("register")
group_to_quickbook("svg")
class_to_quickbook("svg_mapper")
group_to_quickbook("wkt")

class_to_quickbook2("de9im_1_1matrix", "de9im_matrix")
class_to_quickbook2("de9im_1_1mask", "de9im_mask")
class_to_quickbook2("de9im_1_1static__mask", "de9im_static_mask")

os.chdir("index")
execfile("make_qbk.py")
os.chdir("..")

# Clean up generated intermediate files
if "--release-build" in sys.argv:
    remove_all_files("doxy/doxygen_output/xml/")
    remove_all_files("doxy/doxygen_output/html_by_doxygen/")
    remove_all_files("index/xml/")
    remove_all_files("index/html_by_doxygen/")

# Use either bjam or b2 or ../../../b2 (the last should be done on Release branch)
if "--release-build" not in sys.argv:
    run_command("b2")
