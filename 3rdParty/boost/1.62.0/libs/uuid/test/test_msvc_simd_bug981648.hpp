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
* The header contains common definitions for the two source files.
*/
#include <boost/uuid/uuid.hpp>
using boost::uuids::uuid;
class headerProperty
{
public:
virtual ~headerProperty() {}
};
class my_obj:
public headerProperty
{
public:
// This char tmp[8] forces the following uuid to be misaligned.
char tmp[8];
// This m_uuid is misaligned (not 16-byte aligned) and causes the != operator to crash.
uuid m_uuid;
const uuid &get_marker_id() const { return m_uuid; }
uuid get_id() const { return m_uuid; }
};
