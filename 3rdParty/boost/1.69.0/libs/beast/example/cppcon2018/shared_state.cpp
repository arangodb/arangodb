//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#include "shared_state.hpp"
#include "websocket_session.hpp"

shared_state::
shared_state(std::string doc_root)
    : doc_root_(std::move(doc_root))
{
}

void
shared_state::
join(websocket_session& session)
{
    sessions_.insert(&session);
}

void
shared_state::
leave(websocket_session& session)
{
    sessions_.erase(&session);
}

void
shared_state::
send(std::string message)
{
    auto const ss = std::make_shared<std::string const>(std::move(message));

    for(auto session : sessions_)
        session->send(ss);
}
