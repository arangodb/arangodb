// Copyright 2013-2015 Antony Polukhin

// Distributed under the Boost Software License, Version 1.0.
// (See the accompanying file LICENSE_1_0.txt
// or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)


//[type_index_my_type_index_worldwide_macro
/*`
    There is an easy way to force `boost::typeindex::type_id` to use your own type_index class.

    All we need to do is just define `BOOST_TYPE_INDEX_USER_TYPEINDEX` to the full path to header file
    of your type index class:
*/

// BOOST_TYPE_INDEX_USER_TYPEINDEX must be defined *BEFORE* first inclusion of <boost/type_index.hpp>
#define BOOST_TYPE_INDEX_USER_TYPEINDEX <boost/../libs/type_index/examples/user_defined_typeinfo.hpp>
#include <boost/type_index.hpp>
//] [/type_index_my_type_index_worldwide_macro]

#include <boost/core/lightweight_test.hpp>
#ifdef assert
#   undef assert
#endif
#define assert(X) BOOST_TEST(X)


using namespace my_namespace;

int main() {
//[type_index_my_type_index_usage
/*`
    Finally we can use the my_type_index class for getting type indexes:
*/

    my_type_index
        cl1 = my_type_index::type_id<my_class>(),
        st1 = my_type_index::type_id<my_struct>(),
        st2 = my_type_index::type_id<my_struct>(),
        vec = my_type_index::type_id<my_classes>()
    ;

    assert(cl1 != st1);
    assert(st2 == st1);
    assert(vec.pretty_name() == "my_classes");
    assert(cl1.pretty_name() == "my_class");

//] [/type_index_my_type_index_usage]

//[type_index_my_type_index_type_id_runtime_test
/*`
    Now the following example will compile and work.
*/
    my_struct str;
    my_class& reference = str;
    assert(my_type_index::type_id<my_struct>() == my_type_index::type_id_runtime(reference));
//][/type_index_my_type_index_type_id_runtime_test]

//[type_index_my_type_index_worldwide_usage
/*`
    That's it! Now all TypeIndex global methods and typedefs will be using your class:
*/
    boost::typeindex::type_index worldwide = boost::typeindex::type_id<my_classes>();
    assert(worldwide.pretty_name() == "my_classes");
    assert(worldwide == my_type_index::type_id<my_classes>());
//][/type_index_my_type_index_worldwide_usage]
}


