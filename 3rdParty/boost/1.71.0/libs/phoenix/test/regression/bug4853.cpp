/*==============================================================================
    Copyright (c) 2005-2010 Joel de Guzman
    Copyright (c) 2010 Thomas Heller
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <utility> // for std::forward used by boost/range in some cases.
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/bind.hpp>
#include <boost/range.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/uniqued.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>

#include <vector>
#include <string>
#include <iostream>


namespace phoenix = boost::phoenix;

struct Foo {
    Foo(const std::string& name, int value)
        : name_(name)
        , value_(value)
    { }
    
    std::string name_; int value_;
};

typedef boost::shared_ptr<Foo> FooPtr;

int range_test_complex() {
    typedef std::vector<FooPtr> V;
    
    V source;
    
    source.push_back(boost::make_shared<Foo>("Foo", 10));
    source.push_back(boost::make_shared<Foo>("Bar", 20));
    source.push_back(boost::make_shared<Foo>("Baz", 30));
    source.push_back(boost::make_shared<Foo>("Baz", 30)); //duplicate is here
    
    std::vector<std::string> result1;
    std::vector<int> result2;
    
    using namespace boost::adaptors;
    using phoenix::arg_names::arg1;
    
    // This is failing for gcc 4.4 and 4.5 - reason not identified.
#if ((BOOST_GCC_VERSION < 40400) || (BOOST_GCC_VERSION >= 40600))
    boost::push_back(result1, source | transformed(phoenix::bind(&Foo::name_, *arg1)) | uniqued);

    for(unsigned i = 0; i < result1.size(); ++i)
        std::cout << result1[i] << "\n";
#endif

// This is failing for gcc 4.4 and 4.5 - reason not identified.
#if !(BOOST_GCC_VERSION < 40600)
    boost::push_back(result2, source | transformed(phoenix::bind(&Foo::value_, *arg1)) | uniqued);

    for(unsigned i = 0; i < result2.size(); ++i)
        std::cout << result2[i] << "\n";
#endif
   
    return 0;
    
}

int main()
{
    return range_test_complex();
}

