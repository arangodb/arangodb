
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[cline90_calendar
#include <boost/contract.hpp>
#include <cassert>

class calendar {
    friend class boost::contract::access;

    void invariant() const {
        BOOST_CONTRACT_ASSERT(month() >= 1);
        BOOST_CONTRACT_ASSERT(month() <= 12);
        BOOST_CONTRACT_ASSERT(date() >= 1);
        BOOST_CONTRACT_ASSERT(date() <= days_in(month()));
    }

public:
    calendar() : month_(1), date_(31) {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(month() == 1);
                BOOST_CONTRACT_ASSERT(date() == 31);
            })
        ;
    }

    virtual ~calendar() {
        // Check invariants.
        boost::contract::check c = boost::contract::destructor(this);
    }

    int month() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return month_;
    }

    int date() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return date_;
    }

    void reset(int new_month) {
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(new_month >= 1);
                BOOST_CONTRACT_ASSERT(new_month <= 12);
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(month() == new_month);
            })
        ;

        month_ = new_month;
    }

private:
    static int days_in(int month) {
        int result;
        boost::contract::check c = boost::contract::function()
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(month >= 1);
                BOOST_CONTRACT_ASSERT(month <= 12);
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(result >= 1);
                BOOST_CONTRACT_ASSERT(result <= 31);
            })
        ;

        return result = 31; // For simplicity, assume all months have 31 days.
    }

    int month_, date_;
};

int main() {
    calendar cal;
    assert(cal.date() == 31);
    assert(cal.month() == 1);

    cal.reset(8); // Set month 
    assert(cal.month() == 8);

    return 0;
}
//]

