// Copyright 2013-2014 Antony Polukhin

// Distributed under the Boost Software License, Version 1.0.
// (See the accompanying file LICENSE_1_0.txt
// or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)


//[type_index_names_table
/*`
    The following example shows how different type names look when we explicitly use classes for RTTI and RTT off.
    
    This example requires RTTI. For a more portable example see 'Getting human readable and mangled type names':
*/


#include <boost/type_index/stl_type_index.hpp>
#include <boost/type_index/ctti_type_index.hpp>
#include <iostream>

template <class T>
void print(const char* name) {
    boost::typeindex::stl_type_index sti = boost::typeindex::stl_type_index::type_id<T>();
    boost::typeindex::ctti_type_index cti = boost::typeindex::ctti_type_index::type_id<T>();
    std::cout << "\t[" /* start of the row */
        << "[" << name << "]"
        << "[`" << sti.raw_name() << "`] "
        << "[`" << sti.pretty_name() << "`] "
        << "[`" << cti.raw_name() << "`] "
    << "]\n" /* end of the row */ ;
}

struct user_defined_type{};

namespace ns1 { namespace ns2 { 
    struct user_defined_type{};
}} // namespace ns1::ns2

namespace {
    struct in_anon_type{};
} // anonymous namespace

namespace ns3 { namespace { namespace ns4 {
    struct in_anon_type{};
}}} // namespace ns3::{anonymous}::ns4


template <class T0, class T1>
class templ {};

template <>
class templ<int, int> {};

int main() {
    std::cout << "[table:id Table of names\n";
    std::cout << "\t[[Type] [RTTI & raw_name] [RTTI & pretty_name] [noRTTI & raw_name]]\n";

    print<user_defined_type>("User defined type");
    print<in_anon_type>("In anonymous namespace");
    print<ns3::ns4::in_anon_type>("In ns3::{anonymous}::ns4 namespace");
    print<templ<short, int> >("Template class");
    print<templ<int, int> >("Template class (full specialization)");
    print<templ<
        templ<char, signed char>, 
        templ<int, user_defined_type> 
    > >("Template class with templae classes");


    std::cout << "]\n";
}

/*`
    Code from the example will produce the following table:

    [table:id Table of names
        [[Type] [RTTI & raw_name] [RTTI & pretty_name] [noRTTI & raw_name]]
        [[User defined type][`17user_defined_type`] [`user_defined_type`] [`user_defined_type]`] ]
        [[In anonymous namespace][`N12_GLOBAL__N_112in_anon_typeE`] [`(anonymous namespace)::in_anon_type`] [`{anonymous}::in_anon_type]`] ]
        [[In ns3::{anonymous}::ns4 namespace][`N3ns312_GLOBAL__N_13ns412in_anon_typeE`] [`ns3::(anonymous namespace)::ns4::in_anon_type`] [`ns3::{anonymous}::ns4::in_anon_type]`] ]
        [[Template class][`5templIsiE`] [`templ<short, int>`] [`templ<short int, int>]`] ]
        [[Template class (full specialization)][`5templIiiE`] [`templ<int, int>`] [`templ<int, int>]`] ]
        [[Template class with template classes][`5templIS_IcaES_Ii17user_defined_typeEE`] [`templ<templ<char, signed char>, templ<int, user_defined_type> >`] [`templ<templ<char, signed char>, templ<int, user_defined_type> >]`] ]
    ]

    We have not show the "noRTTI & pretty_name" column in the table because it is almost equal
    to "noRTTI & raw_name" column.

    [warning With RTTI off different classes with same names in anonymous namespace may collapse. See 'RTTI emulation limitations'. ] 
*/

//] [/type_index_names_table]
