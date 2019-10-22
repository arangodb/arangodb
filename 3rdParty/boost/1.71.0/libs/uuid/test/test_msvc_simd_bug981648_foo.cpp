/*
* Copyright 2014 Andrey Semashev
*
* Distributed under the Boost Software License, Version 1.0.
* See accompanying file LICENSE_1_0.txt or copy at
* https://www.boost.org/LICENSE_1_0.txt
*/
/*
* This is a part of the test for a workaround for MSVC 12 (VS2013) optimizer bug
* which causes incorrect SIMD code generation for operator==. See:
*
* https://svn.boost.org/trac/boost/ticket/8509#comment:3
* https://connect.microsoft.com/VisualStudio/feedbackdetail/view/981648#tabs
*
* The file contains the function that actually causes the crash. Reproduces only
* in Release x64 builds.
*/
#include <cstdio>
#include "test_msvc_simd_bug981648.hpp"
void mp_grid_update_marker_parameters(headerProperty* header_prop, my_obj &current_marker)
{
headerProperty *old_header_prop = NULL;
my_obj *p = dynamic_cast<my_obj*>(header_prop);
/*
* This != statement crashes with a GP.
* */
if (p != NULL && (current_marker.get_id() != p->get_marker_id())) {
std::printf("works okay, if it reaches this printf: %p\n", reinterpret_cast<void *>(p));
old_header_prop = header_prop;
if (old_header_prop == 0) { fprintf(stderr, "ouch"); }
}
}
