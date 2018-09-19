//
// Boost.Pointer Container
//
//  Copyright Thorsten Ottosen 2003-2005. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/ptr_container/
//

#include <boost/test/unit_test.hpp>
#include "test_data.hpp"
#include <boost/ptr_container/ptr_array.hpp>
#include <boost/ptr_container/detail/ptr_container_disable_deprecated.hpp>
#include <boost/utility.hpp>
#include <boost/array.hpp>
#include <algorithm>
#include <iostream>
#include <cstddef>
#include <string>

using namespace std;
using namespace boost;

template< class Node, size_t N >
class n_ary_tree : boost::noncopyable
{ 
    typedef n_ary_tree<Node,N>       this_type;
    typedef ptr_array<this_type,N>   tree_t;
        
    tree_t         tree;
    Node           data; 

public:
    n_ary_tree()                                 { }
    n_ary_tree( const Node& r ) : data(r)        { }

public: // modifers
    void               set_data( const Node& r ) { data = r; }  
    template< size_t idx >
    void               set_child( this_type* r ) { tree. BOOST_NESTED_TEMPLATE replace<idx>(r); }

public: // accessors
    void               print( std::ostream&, std::string indent = "  " );
    template< size_t idx >
    this_type&         child()                   { return tree. BOOST_NESTED_TEMPLATE at<idx>(); }
    template< size_t idx >
    const this_type&   child() const             { return tree. BOOST_NESTED_TEMPLATE at<idx>(); } 
    
};



template< class Node, size_t N >
void n_ary_tree<Node,N>::print( std::ostream& out, std::string indent )
{
    out << indent << data << "\n";
    indent += "  ";
    for( size_t i = 0; i != N; ++i )
        if( !tree.is_null(i) )
            tree[i].print( out, indent  + "  " );
}


template< class C, class B, class T >
void test_array_interface();

#if defined(BOOST_PTR_CONTAINER_DISABLE_DEPRECATED)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

void test_array()
{
    typedef n_ary_tree<std::string,2> binary_tree;
    binary_tree tree;
    tree.set_data( "root" );
    tree.set_child<0>( new binary_tree( "left subtree" ) );
    tree.set_child<1>( new binary_tree( "right subtree" ) );
    binary_tree& left = tree.child<0>();
    left.set_child<0>( new binary_tree( "left left subtree" ) );
    left.set_child<1>( new binary_tree( "left right subtree" ) );
    binary_tree& right = tree.child<1>();
    right.set_child<0>( new binary_tree( "right left subtree" ) );
    right.set_child<1>( new binary_tree( "right right subtree" ) );

    tree.print( std::cout );

    test_array_interface<ptr_array<Base,10>,Base,Derived_class>();
    test_array_interface<ptr_array<nullable<Base>,10>,Base,Derived_class>();
    test_array_interface<ptr_array<Value,10>,Value,Value>();
    test_array_interface<ptr_array<nullable<Value>,10>,Value,Value>();

    ptr_array<int,10> vec;
    BOOST_CHECK_THROW( vec.at(10), bad_ptr_container_operation );
    BOOST_CHECK_THROW( (vec.replace(10u, new int(0))), bad_ptr_container_operation );
#ifndef BOOST_NO_AUTO_PTR
    BOOST_CHECK_THROW( (vec.replace(10u, std::auto_ptr<int>(new int(0)))), bad_ptr_container_operation ); 
#endif
#ifndef BOOST_NO_CXX11_SMART_PTR
    BOOST_CHECK_THROW( (vec.replace(10u, std::unique_ptr<int>(new int(0)))), bad_ptr_container_operation ); 
#endif
    BOOST_CHECK_THROW( (vec.replace(0u, 0)), bad_ptr_container_operation ); 

    ptr_array<Derived_class,2> derived;
    derived.replace( 0, new Derived_class );
    derived.replace( 1, new Derived_class );
    ptr_array<Base,2> base( derived );
    
    BOOST_TEST_MESSAGE( "finished derived to base test" ); 

    base = derived;   
    ptr_array<Base,2> base2( base );
    base2 = base;
    base  = base;
}

template< class C, class B, class T >
void test_array_interface()
{
    C c;
    c.replace( 0, new T );
    c.replace( 1, new B );
    c.replace( 9, new T );
#ifndef BOOST_NO_AUTO_PTR
    c.replace( 0, std::auto_ptr<T>( new T ) );
#endif
#ifndef BOOST_NO_CXX11_SMART_PTR
    c.replace( 0, std::unique_ptr<T>( new T ) );
#endif
    const C c2( c.clone() );
    
    BOOST_DEDUCED_TYPENAME C::iterator i                  = c.begin();
    hide_warning(i);
    BOOST_DEDUCED_TYPENAME C::const_iterator ci           = c2.begin();
    hide_warning(ci);
    BOOST_DEDUCED_TYPENAME C::iterator i2                 = c.end();
    hide_warning(i2);
    BOOST_DEDUCED_TYPENAME C::const_iterator ci2          = c2.begin();
    hide_warning(ci2);
    BOOST_DEDUCED_TYPENAME C::reverse_iterator ri         = c.rbegin();
    hide_warning(ri);
    BOOST_DEDUCED_TYPENAME C::const_reverse_iterator cri  = c2.rbegin();
    hide_warning(cri);
    BOOST_DEDUCED_TYPENAME C::reverse_iterator rv2        = c.rend();
    hide_warning(rv2);
    BOOST_DEDUCED_TYPENAME C::const_reverse_iterator cvr2 = c2.rend();
    hide_warning(cvr2);

    BOOST_TEST_MESSAGE( "finished iterator test" ); 

    BOOST_CHECK_EQUAL( c.empty(), false );
    BOOST_CHECK_EQUAL( c.size(), c.max_size() );
    
    BOOST_TEST_MESSAGE( "finished capacity test" ); 

    BOOST_CHECK_EQUAL( c.is_null(0), false );
    BOOST_CHECK_EQUAL( c.is_null(1), false );
    BOOST_CHECK_EQUAL( c.is_null(2), true );

    c.front();
    c.back();
    c2.front();
    c2.back();
    C c3;
    c.swap( c3 );
    C c4;
    swap(c4,c3);
    c3.swap(c4);

    BOOST_CHECK_EQUAL( c.is_null(0), true );
    BOOST_CHECK_EQUAL( c3.is_null(0), false );

    c.replace( 5, new T );
    BOOST_CHECK_EQUAL( c.is_null(5), false );
    c = c3.release();
    for( size_t i = 0; i < c3.size(); ++i )
        BOOST_CHECK_EQUAL( c3.is_null(i), true );

    BOOST_TEST_MESSAGE( "finished element access test" ); 
}

#if defined(BOOST_PTR_CONTAINER_DISABLE_DEPRECATED)
#pragma GCC diagnostic pop
#endif

using boost::unit_test::test_suite;

test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    test_suite* test = BOOST_TEST_SUITE( "Pointer Container Test Suite" );

    test->add( BOOST_TEST_CASE( &test_array ) );

    return test;
}
