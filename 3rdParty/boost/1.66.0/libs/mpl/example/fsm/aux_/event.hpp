
#ifndef BOOST_FSM_EVENT_INCLUDED
#define BOOST_FSM_EVENT_INCLUDED

// Copyright Aleksey Gurtovoy 2002-2004
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include "base_event.hpp"

namespace fsm { namespace aux {

template< typename Derived >
struct event
    : base_event
{
 public:
    typedef base_event base_t;

 private:

#if defined(BOOST_NO_CXX11_SMART_PTR)

    virtual std::auto_ptr<base_event> do_clone() const
    {
        return std::auto_ptr<base_event>(
              new Derived(static_cast<Derived const&>(*this))
            );
    }
    
#else

    virtual std::unique_ptr<base_event> do_clone() const
    {
        return std::unique_ptr<base_event>(
              new Derived(static_cast<Derived const&>(*this))
            );
    }
    
#endif

};

}}

#endif // BOOST_FSM_EVENT_INCLUDED
