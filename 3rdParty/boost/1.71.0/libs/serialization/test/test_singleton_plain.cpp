/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_singleton_plain.cpp:
// Test the singleton class for a "plain" singleton (used as singleton<Foo>)
//
// - is_destroyed returns false when singleton is active or uninitialized
// - is_destroyed returns true when singleton is destructed
// - the singleton is eventually destructed (no memory leak)

// (C) Copyright 2018 Alexander Grund
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_tools.hpp"
#include <boost/serialization/singleton.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <stdexcept>

// Can't use BOOST_TEST because:
// a) destructors are called after program exit
// b) This is intended to be used by shared libraries too which would then need their own report_errors call
// We halso have to disable the Wterminate warning as we call this from dtors
// C++ will terminate the program in such cases which is OK here
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wterminate"
#define THROW_ON_FALSE(cond) if(!(cond)) throw std::runtime_error(__FILE__ "(" BOOST_PP_STRINGIZE(__LINE__) ") Assertion failed: " #cond)

// Enum to designate the state of the singletonized instances
enum ConstructionState{CS_UNINIT, CS_INIT, CS_DESTROYED};

// We need another singleton to check for the destruction of the singletons at program exit
// We don't need all the magic for shared library anti-optimization and can keep it very simple
struct controller{
    static controller& getInstance(){
        static controller instance;
        return instance;
    }
    ConstructionState state;
private:
    controller() {
        state = CS_UNINIT;
    }
    ~controller();
};

// A simple class that sets its construction state in the controller singleton
struct Foo{
    Foo(): i(42) {
        // access controller singleton. Therefore controller will be constructed before this
        THROW_ON_FALSE(controller::getInstance().state == CS_UNINIT);
        controller::getInstance().state = CS_INIT;
    }
    ~Foo() {
        // Because controller is constructed before this, it will be destructed AFTER this. Hence controller is still valid
        THROW_ON_FALSE(controller::getInstance().state == CS_INIT);
        controller::getInstance().state = CS_DESTROYED;
    }
    // Volatile to prevent compiler optimization from removing this
    volatile int i;
};

controller::~controller() {
    // If this fails, the singletons were not freed and memory is leaked
    THROW_ON_FALSE(state == CS_DESTROYED);
    // If this fails, then the destroyed flag is not set and one may use a deleted instance if relying on this flag
    THROW_ON_FALSE(boost::serialization::singleton<Foo>::is_destroyed());
}

int
test_main( int /* argc */, char* /* argv */[] )
{
    // Check if the singleton is alive and use it
    BOOST_CHECK(!boost::serialization::singleton<Foo>::is_destroyed());

    BOOST_CHECK(boost::serialization::singleton<Foo>::get_const_instance().i == 42);
    return EXIT_SUCCESS;
}
