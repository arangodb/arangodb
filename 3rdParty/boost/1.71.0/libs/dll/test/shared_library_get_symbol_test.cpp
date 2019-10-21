// Copyright 2011-2012 Renato Tegon Forti.
// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2019 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include "../example/b2_workarounds.hpp"
#include <boost/dll.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/function.hpp>
#include <boost/fusion/container.hpp>
// lib functions

typedef float (lib_version_func)();
typedef void  (say_hello_func)  ();
typedef int   (increment)       (int);

typedef boost::fusion::vector<std::vector<int>, std::vector<int>, std::vector<int>, const std::vector<int>*, std::vector<int>* > do_share_res_t;
typedef boost::shared_ptr<do_share_res_t> (do_share_t)(
            std::vector<int> v1,
            std::vector<int>& v2,
            const std::vector<int>& v3,
            const std::vector<int>* v4,
            std::vector<int>* v5
        );

void refcountable_test(boost::dll::fs::path shared_library_path) {
    using namespace boost::dll;
    using namespace boost::fusion;

    std::vector<int> v(1000);

    {
        boost::function<say_hello_func> sz2
            = import<say_hello_func>(shared_library_path, "say_hello");

        sz2();
        sz2();
        sz2();
    }

    {
        boost::function<std::size_t(const std::vector<int>&)> sz
            = import_alias<std::size_t(const std::vector<int>&)>(shared_library_path, "foo_bar");
        BOOST_TEST(sz(v) == 1000);
    }


    {
        boost::function<do_share_t> f;

        {
            boost::function<do_share_t> f2 = import_alias<do_share_t>(shared_library_path, "do_share");
            f = f2;
        }

        std::vector<int> v1(1, 1), v2(2, 2), v3(3, 3), v4(4, 4), v5(1000, 5);
        boost::shared_ptr<do_share_res_t> res = f(v1, v2, v3, &v4, &v5);

        BOOST_TEST(at_c<0>(*res).size() == 1); BOOST_TEST(at_c<0>(*res).front() == 1);
        BOOST_TEST(at_c<1>(*res).size() == 2); BOOST_TEST(at_c<1>(*res).front() == 2);
        BOOST_TEST(at_c<2>(*res).size() == 3); BOOST_TEST(at_c<2>(*res).front() == 3);
        BOOST_TEST(at_c<3>(*res)->size() == 4); BOOST_TEST(at_c<3>(*res)->front() == 4);
        BOOST_TEST(at_c<4>(*res)->size() == 1000); BOOST_TEST(at_c<4>(*res)->front() == 5);

        BOOST_TEST(at_c<3>(*res) == &v4);
        BOOST_TEST(at_c<4>(*res) == &v5);
        BOOST_TEST(at_c<1>(*res).back() == 777);
        BOOST_TEST(v5.back() == 9990);
    }

    {
        boost::shared_ptr<int> i = import<int>(shared_library_path, "integer_g");
        BOOST_TEST(*i == 100);

        boost::shared_ptr<int> i2;
        i.swap(i2);
        BOOST_TEST(*i2 == 100);
    }

    {
        boost::function<int&()> f = import_alias<int&()>(shared_library_path, "ref_returning_function");
        BOOST_TEST(f() == 0);

        f() = 10;
        BOOST_TEST(f() == 10);
        
        boost::function<int&()> f1 = import_alias<int&()>(shared_library_path, "ref_returning_function");
        BOOST_TEST(f1() == 10);

        f1() += 10;
        BOOST_TEST(f() == 20);
    }

    {
        boost::shared_ptr<const int> i = import<const int>(shared_library_path, "const_integer_g");
        BOOST_TEST(*i == 777);

        boost::shared_ptr<const int> i2 = i;
        i.reset();
        BOOST_TEST(*i2 == 777);
    }

    {
        boost::shared_ptr<std::string> s = import_alias<std::string>(shared_library_path, "info");
        BOOST_TEST(*s == "I am a std::string from the test_library (Think of me as of 'Hello world'. Long 'Hello world').");

        boost::shared_ptr<std::string> s2;
        s.swap(s2);
        BOOST_TEST(*s2 == "I am a std::string from the test_library (Think of me as of 'Hello world'. Long 'Hello world').");
    }
}

// exe function
extern "C" int BOOST_SYMBOL_EXPORT exef() {
    return 15;
}

// Unit Tests
int main(int argc, char* argv[]) {
    using namespace boost::dll;

    boost::dll::fs::path shared_library_path = b2_workarounds::first_lib_from_argv(argc, argv);
    BOOST_TEST(shared_library_path.string().find("test_library") != std::string::npos);
    BOOST_TEST(b2_workarounds::is_shared_library(shared_library_path));

    refcountable_test(shared_library_path);

    shared_library sl(shared_library_path);

    BOOST_TEST(sl.get<int>("integer_g") == 100);

    sl.get<int>("integer_g") = 10;
    BOOST_TEST(sl.get<int>("integer_g") == 10);
    BOOST_TEST(sl.get<int>(std::string("integer_g")) == 10);

    BOOST_TEST(sl.get<say_hello_func>("say_hello"));
    sl.get<say_hello_func>("say_hello")();

    float ver = sl.get<lib_version_func>("lib_version")();
    BOOST_TEST(ver == 1.0);

    int n = sl.get<increment>("increment")(1);
    BOOST_TEST(n == 2);

    BOOST_TEST(sl.get<const int>("const_integer_g") == 777);

    boost::function<int(int)> inc = sl.get<int(int)>("increment");
    BOOST_TEST(inc(1) == 2);
    BOOST_TEST(inc(2) == 3);
    BOOST_TEST(inc(3) == 4);

    // Checking that symbols are still available, after another load+unload of the library
    { shared_library sl2(shared_library_path); }

    BOOST_TEST(inc(1) == 2);
    BOOST_TEST(sl.get<int>("integer_g") == 10);


    // Checking aliases
    boost::function<std::size_t(const std::vector<int>&)> sz 
        = sl.get_alias<std::size_t(const std::vector<int>&)>("foo_bar");

    std::vector<int> v(10);
    BOOST_TEST(sz(v) == 10);
    BOOST_TEST(sl.get_alias<std::size_t>("foo_variable") == 42);


    sz = sl.get<std::size_t(*)(const std::vector<int>&)>("foo_bar");
    BOOST_TEST(sz(v) == 10);
    BOOST_TEST(*sl.get<std::size_t*>("foo_variable") == 42);
    
    { // self
        shared_library sl(program_location());
        int val = sl.get<int(void)>("exef")();
        BOOST_TEST(val == 15);   
    }

    int& reference_to_internal_integer = sl.get<int&>("reference_to_internal_integer");
    BOOST_TEST(reference_to_internal_integer == 0xFF0000);

#ifndef BOOST_NO_RVALUE_REFERENCES
    int&& rvalue_reference_to_internal_integer = sl.get<int&&>("rvalue_reference_to_internal_integer");
    BOOST_TEST(rvalue_reference_to_internal_integer == 0xFF0000);
#endif

    return boost::report_errors();
}

