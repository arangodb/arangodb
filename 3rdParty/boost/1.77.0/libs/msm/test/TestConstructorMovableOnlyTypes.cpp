// Copyright 2016 Bogumił Chojnowski
// bogumil DOT chojnowski AT gmail DOT com
// This is extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2010 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <memory>
// back-end
#include <boost/msm/back/state_machine.hpp>
//front-end
#include <boost/msm/front/state_machine_def.hpp>
#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE MyTest
#endif
#include <boost/test/unit_test.hpp>

namespace msm = boost::msm;
namespace mpl = boost::mpl;


namespace
{
    struct Lightbulp
    {
        Lightbulp(int c) : current(c) {}
        int current;
    };

    // events
    struct ev_toggle {};

    // front-end: define the FSM structure
    struct bistable_switch_ : public msm::front::state_machine_def<bistable_switch_>
    {
         bistable_switch_(std::unique_ptr<Lightbulp> bulp, int load)
            : bulp_(std::move(bulp))
        {
            BOOST_CHECK_MESSAGE(bulp_->current == 3, "Wrong current value");
            BOOST_CHECK_MESSAGE(load == 5, "Wrong load value");
            bulp_->current = 10;
        }

        std::unique_ptr<Lightbulp> bulp_;

        // The list of FSM states
        struct Off : public msm::front::state<>
        {
            template <typename Event, typename FSM>
            void on_entry(Event const&, FSM& ) { }
            template <typename Event, typename FSM>
            void on_exit(Event const&, FSM&) { }
        };

        struct On : public msm::front::state<>
        {
            template <typename Event, typename FSM>
            void on_entry(Event const&, FSM& ) { }
            template <typename Event, typename FSM>
            void on_exit(Event const&, FSM&) { }
        };

        // the initial state of the player SM. Must be defined
        typedef Off initial_state;

        void turn_on(ev_toggle const&) { bulp_->current = 11; }
        void turn_off(ev_toggle const&) { bulp_->current = 9; }

        typedef bistable_switch_ bs_; // makes transition table cleaner

        // Transition table for player
        struct transition_table : mpl::vector<
            //    Start     Event         Next      Action				 Guard
            //  +---------+-------------+---------+---------------------+----------------------+
          a_row < Off     , ev_toggle   , On      , &bs_::turn_on                              >,
          a_row < On      , ev_toggle   , Off     , &bs_::turn_off                             >
            //  +---------+-------------+---------+---------------------+----------------------+
        > {};
        // Replaces the default no-transition response.

        template <typename Event, typename FSM>
        void no_transition(Event const&, FSM&, int)
        {
            BOOST_FAIL("no_transition called!");
        }
    };

    // Pick a back-end
    typedef msm::back::state_machine<bistable_switch_> bistable_switch;

    BOOST_AUTO_TEST_CASE(my_test)
    {
        auto bulp = std::make_unique<Lightbulp>(3);

        bistable_switch bs(std::move(bulp), 5);
        BOOST_CHECK_MESSAGE(bs.bulp_->current == 10, "Wrong returned current value");

        bs.start();

        bs.process_event(ev_toggle());
        BOOST_CHECK_MESSAGE(bs.bulp_->current == 11, "Wrong returned current value");

        bs.process_event(ev_toggle());
        BOOST_CHECK_MESSAGE(bs.bulp_->current == 9, "Wrong returned current value");

        bs.stop();
    }
}

