// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This is a simple program that shows how to report error objects out of a
// C-callback, converting them to leaf::result<T> as soon as controlreaches C++.

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}
#include <boost/leaf.hpp>
#include <iostream>
#include <stdlib.h>

namespace leaf = boost::leaf;

enum do_work_error_code
{
    ec1=1,
    ec2
};

struct e_lua_pcall_error
{
    int value;

    friend std::ostream & operator<<( std::ostream & os, e_lua_pcall_error const & x )
    {
        os << "Lua error code = " << x.value;
        switch( x.value )
        {
            case LUA_ERRRUN: return os << " (LUA_ERRRUN)";
            case LUA_ERRMEM: return os << " (LUA_ERRMEM)";
            case LUA_ERRERR: return os << " (LUA_ERRERR)";
            default: return os << " (unknown)";
        }
    }
};

struct e_lua_error_message { std::string value; };


// This is a C callback with a specific signature, callable from programs
// written in Lua. If it succeeds, it returns an int answer, by pushing it onto
// the Lua stack. But "sometimes" it fails, in which case it calls luaL_error.
// This causes the Lua interpreter to abort and pop back into the C++ code which
// called it (see call_lua below).
int do_work( lua_State * L )
{
    bool success = rand() % 2; // "Sometimes" do_work fails.
    if( success )
    {
        lua_pushnumber(L, 42); // Success, push the result on the Lua stack, return to Lua.
        return 1;
    }
    else
    {
        return leaf::new_error(ec1), luaL_error(L,"do_work_error"); // luaL_error does not return (longjmp).
    }
}


std::shared_ptr<lua_State> init_lua_state()
{
    // Create a new lua_State, we'll use std::shared_ptr for automatic cleanup.
    std::shared_ptr<lua_State> L(lua_open(), &lua_close);

    // Register the do_work function (above) as a C callback, under the global
    // Lua name "do_work". With this, calls from Lua programs to do_work will
    // land in the do_work C function we've registered.
    lua_register( &*L, "do_work", &do_work );

    // Pass some Lua code as a C string literal to Lua. This creates a global
    // Lua function called "call_do_work", which we will later ask Lua to
    // execute.
    luaL_dostring( &*L, "\
\n      function call_do_work()\
\n          return do_work()\
\n      end" );

    return L;
}


// Here we will ask Lua to execute the function call_do_work, which is written
// in Lua, and returns the value from do_work, which is written in C++ and
// registered with the Lua interpreter as a C callback.

// If do_work succeeds, we return the resulting int answer. If it fails, we'll
// communicate that failure to our caller.
leaf::result<int> call_lua( lua_State * L )
{
    leaf::error_monitor cur_err;

    // Ask the Lua interpreter to call the global Lua function call_do_work.
    lua_getfield( L, LUA_GLOBALSINDEX, "call_do_work" );
    if( int err = lua_pcall(L, 0, 1, 0) ) // Ask Lua to call the global function call_do_work.
    {
        std::string msg = lua_tostring(L, 1);
        lua_pop(L, 1);

        // We got a Lua error which may be the error we're reporting from
        // do_work, or some other error. If it is another error,
        // cur_err.assigned_error_id() will return a new leaf::error_id,
        // otherwise we'll be working with the original value returned by
        // leaf::new_error in do_work.
        return cur_err.assigned_error_id().load( e_lua_pcall_error{err}, e_lua_error_message{std::move(msg)} );
    }
    else
    {
        // Success! Just return the int answer.
        int answer = lua_tonumber(L, -1);
        lua_pop(L, 1);
        return answer;
    }
}

int main()
{
    std::shared_ptr<lua_State> L=init_lua_state();

    for( int i=0; i!=10; ++i )
    {
        leaf::try_handle_all(

            [&]() -> leaf::result<void>
            {
                BOOST_LEAF_AUTO(answer, call_lua(&*L));
                std::cout << "do_work succeeded, answer=" << answer << '\n';
                return { };
            },

            []( do_work_error_code e, e_lua_error_message const & msg )
            {
                std::cout << "Got do_work_error_code = " << e << ", " << msg.value << "\n";
            },

            []( e_lua_pcall_error const & err, e_lua_error_message const & msg )
            {
                std::cout << "Got e_lua_pcall_error, " << err << ", " << msg.value << "\n";
            },

            []( leaf::error_info const & unmatched )
            {
                std::cerr <<
                    "Unknown failure detected" << std::endl <<
                    "Cryptic diagnostic information follows" << std::endl <<
                    unmatched;
            } );
    }

    return 0;
}

#ifdef BOOST_LEAF_NO_EXCEPTIONS

namespace boost
{
    BOOST_LEAF_NORETURN void throw_exception( std::exception const & e )
    {
        std::cerr << "Terminating due to a C++ exception under BOOST_LEAF_NO_EXCEPTIONS: " << e.what();
        std::terminate();
    }

    struct source_location;
    BOOST_LEAF_NORETURN void throw_exception( std::exception const & e, boost::source_location const & )
    {
        throw_exception(e);
    }
}

#endif
