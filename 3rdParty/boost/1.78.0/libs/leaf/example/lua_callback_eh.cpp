// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This is a simple program that shows how to report error objects out of a
// C-callback (which may not throw exceptions).

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

struct e_lua_exception { std::exception_ptr value; };

// A noexcept wrapper for a lua_CFunction pointer. We capture the
// std::current_exception and wrap it in a e_lua_exception object.
template <int (*F)( lua_State * L )>
int wrap_lua_CFunction( lua_State * L ) noexcept
{
    return leaf::try_catch(
        [&]
        {
            return F(L);
        },
        [&]( leaf::error_info const & ei )
        {
            ei.error().load( e_lua_exception{std::current_exception()} );
            return luaL_error(L, "C++ Exception"); // luaL_error does not return (longjmp).
        } );
}


// This is a C callback with a specific signature, callable from programs
// written in Lua. If it succeeds, it returns an int answer, by pushing it onto
// the Lua stack. But "sometimes" it fails, in which case it throws an
// exception, which will be processed by wrap_lua_CFunction (above).
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
        throw leaf::exception(ec1);
    }
}


std::shared_ptr<lua_State> init_lua_state()
{
    // Create a new lua_State, we'll use std::shared_ptr for automatic cleanup.
    std::shared_ptr<lua_State> L(lua_open(), &lua_close);

    // Register the do_work function (above) as a C callback, under the global
    // Lua name "do_work". With this, calls from Lua programs to do_work will
    // land in the do_work C function we've registered.
    lua_register( &*L, "do_work", &wrap_lua_CFunction<&do_work> );

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
int call_lua( lua_State * L )
{
    return leaf::try_catch(
        [&]
        {
            leaf::error_monitor cur_err;

            // Ask the Lua interpreter to call the global Lua function call_do_work.
            lua_getfield( L, LUA_GLOBALSINDEX, "call_do_work" );
            if( int err = lua_pcall(L, 0, 1, 0) )
            {
                std::string msg = lua_tostring(L, 1);
                lua_pop(L,1);

                // We got a Lua error which may be the error we're reporting
                // from do_work, or some other error. If it is another error,
                // cur_err.assigned_error_id() will return a new leaf::error_id,
                // otherwise we'll be working with the original error reported
                // by a C++ exception out of do_work.
                throw leaf::exception( cur_err.assigned_error_id().load( e_lua_pcall_error{err}, e_lua_error_message{std::move(msg)} ) );
            }
            else
            {
                // Success! Just return the int answer.
                int answer = lua_tonumber(L, -1);
                lua_pop(L, 1);
                return answer;
            }
        },

        []( e_lua_exception e ) -> int
        {
            // This is the exception communicated out of wrap_lua_CFunction.
            std::rethrow_exception( e.value );
        } );
}

int main()
{
    std::shared_ptr<lua_State> L=init_lua_state();

    for( int i=0; i!=10; ++i )
    {
        leaf::try_catch(

            [&]
            {
                int answer = call_lua(&*L);
                std::cout << "do_work succeeded, answer=" << answer << '\n';

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
