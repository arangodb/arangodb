//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef CPPCON2018_SHARED_STATE_HPP
#define CPPCON2018_SHARED_STATE_HPP

#include <memory>
#include <string>
#include <unordered_set>

// Forward declaration
class websocket_session;

// Represents the shared server state
class shared_state
{
    std::string doc_root_;

    // This simple method of tracking
    // sessions only works with an implicit
    // strand (i.e. a single-threaded server)
    std::unordered_set<websocket_session*> sessions_;

public:
    explicit
    shared_state(std::string doc_root);

    std::string const&
    doc_root() const noexcept
    {
        return doc_root_;
    }

    void join  (websocket_session& session);
    void leave (websocket_session& session);
    void send  (std::string message);
};

#endif
