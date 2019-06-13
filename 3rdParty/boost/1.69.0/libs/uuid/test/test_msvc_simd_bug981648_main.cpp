/*
* Copyright 2014 Andrey Semashev
*
* Distributed under the Boost Software License, Version 1.0.
* See accompanying file LICENSE_1_0.txt or copy at
* http://www.boost.org/LICENSE_1_0.txt
*/
/*
* This is a part of the test for a workaround for MSVC 12 (VS2013) optimizer bug
* which causes incorrect SIMD code generation for operator==. See:
*
* https://svn.boost.org/trac/boost/ticket/8509#comment:3
* https://connect.microsoft.com/VisualStudio/feedbackdetail/view/981648#tabs
*
* This file contains the main entry point.
*/
#include <cstdio>
#include "test_msvc_simd_bug981648.hpp"
extern void mp_grid_update_marker_parameters(headerProperty* header_prop, my_obj &current_marker);
static my_obj g_my_obj;
int main(void)
{
my_obj *p = &g_my_obj;
p->m_uuid = uuid();
uuid one, two;
one.data[0] = 0; two.data[0] = 1;
//*****************************************
// This != statement generates two movdqu statements or pcmpeqd with a memory operand which crashes
if (one != two) {
std::printf("The first != operator works okay if it reaches this printf.\n");
}
my_obj a;
a.m_uuid.data[0] = 1;
std::printf("There should be a another printf coming next.\n");
//*****************************************
// The != statement in this function generates a movups and a movdqu statement.
// It also crashes because the optimizer also creates a pcmpeqd for a non-aligned memory location.
mp_grid_update_marker_parameters(p, a);
return 0;
}
