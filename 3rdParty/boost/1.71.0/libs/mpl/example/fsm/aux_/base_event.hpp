
#ifndef BOOST_FSM_BASE_EVENT_INCLUDED
#define BOOST_FSM_BASE_EVENT_INCLUDED

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

#include <memory>
#include <boost/config.hpp>

namespace fsm { namespace aux {

// represent an abstract base for FSM events

struct base_event
{
 public:
    virtual ~base_event() {};
    
#if defined(BOOST_NO_CXX11_SMART_PTR)

    std::auto_ptr<base_event> clone() const
    
#else

    std::unique_ptr<base_event> clone() const
    
#endif

    {
        return do_clone();
    }
 
 private:

#if defined(BOOST_NO_CXX11_SMART_PTR)

    virtual std::auto_ptr<base_event> do_clone() const = 0;
    
#else

    virtual std::unique_ptr<base_event> do_clone() const = 0;
    
#endif

};

}}

#endif // BOOST_FSM_BASE_EVENT_INCLUDED
