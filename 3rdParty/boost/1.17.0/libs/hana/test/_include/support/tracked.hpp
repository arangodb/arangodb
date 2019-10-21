// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef TEST_SUPPORT_TRACKED_HPP
#define TEST_SUPPORT_TRACKED_HPP

// Define this if you want Tracked objects to print information to stderr.
// #define TRACKED_PRINT_STUFF

#include <boost/hana/assert.hpp>

#ifdef TRACKED_PRINT_STUFF
#   include <iostream>
#endif

#include <iosfwd>


struct Tracked {
    enum class State { CONSTRUCTED, MOVED_FROM, DESTROYED };

    int value;
    State state;

    explicit Tracked(int k) : value{k}, state{State::CONSTRUCTED} {
#ifdef TRACKED_PRINT_STUFF
        std::cerr << "constructing " << *this << '\n';
#endif
    }

    Tracked(Tracked const& t) : value{t.value}, state{State::CONSTRUCTED} {
        BOOST_HANA_RUNTIME_CHECK(t.state != State::MOVED_FROM &&
            "copying a moved-from object");

        BOOST_HANA_RUNTIME_CHECK(t.state != State::DESTROYED &&
            "copying a destroyed object");

#ifdef TRACKED_PRINT_STUFF
        std::cerr << "copying " << *this << '\n';
#endif
    }

    Tracked(Tracked&& t) : value{t.value}, state{State::CONSTRUCTED} {
        BOOST_HANA_RUNTIME_CHECK(t.state != State::MOVED_FROM &&
            "double moving from an object");

        BOOST_HANA_RUNTIME_CHECK(t.state != State::DESTROYED &&
            "moving from a destroyed object");

#ifdef TRACKED_PRINT_STUFF
        std::cerr << "moving " << t << '\n';
#endif
        t.state = State::MOVED_FROM;
    }

    Tracked& operator=(Tracked const& other) {
        BOOST_HANA_RUNTIME_CHECK(this->state != State::DESTROYED &&
            "assigning to a destroyed object");

        BOOST_HANA_RUNTIME_CHECK(other.state != State::MOVED_FROM &&
            "assigning a moved-from object");

        BOOST_HANA_RUNTIME_CHECK(other.state != State::DESTROYED &&
            "assigning a destroyed object");

#ifdef TRACKED_PRINT_STUFF
        std::cerr << "assigning " << other << " to " << *this << '\n';
#endif
        this->value = other.value;
        return *this;
    }

    Tracked& operator=(Tracked&& other) {
        BOOST_HANA_RUNTIME_CHECK(this->state != State::DESTROYED &&
            "assigning to a destroyed object");

        BOOST_HANA_RUNTIME_CHECK(other.state != State::MOVED_FROM &&
            "double-moving from an object");

        BOOST_HANA_RUNTIME_CHECK(other.state != State::DESTROYED &&
            "assigning a destroyed object");

#ifdef TRACKED_PRINT_STUFF
        std::cerr << "assigning " << other << " to " << *this << '\n';
#endif
        this->value = other.value;
        other.state = State::MOVED_FROM;
        return *this;
    }

    ~Tracked() {
        BOOST_HANA_RUNTIME_CHECK(state != State::DESTROYED &&
            "double-destroying an object");

#ifdef TRACKED_PRINT_STUFF
        std::cerr << "destructing " << *this << '\n';
#endif
        state = State::DESTROYED;
    }

    template <typename CharT, typename Traits>
    friend std::basic_ostream<CharT, Traits>&
    operator<<(std::basic_ostream<CharT, Traits>& os, Tracked const& t) {
        os << "Tracked{" << t.value << "}";
        switch (t.state) {
        case State::CONSTRUCTED:
            os << "[ok]"; break;
        case State::MOVED_FROM:
            os << "[moved from]"; break;
        case State::DESTROYED:
            os << "[destroyed]"; break;
        default:
            BOOST_HANA_RUNTIME_CHECK(false && "never reached");
        }
        return os;
    }
};

#endif // !TEST_SUPPORT_TRACKED_HPP
