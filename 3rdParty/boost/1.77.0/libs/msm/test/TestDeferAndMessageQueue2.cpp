// Copyright 2017 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
// back-end
#include <boost/msm/back/state_machine.hpp>
//front-end
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#include <boost/test/unit_test.hpp>

namespace msm = boost::msm;
namespace mpl = boost::mpl;
using namespace boost::msm::front;

namespace
{
    // events
    struct event1 {};
    struct event2 {};
    struct event3 {};
    struct eventd {};


    // front-end: define the FSM structure
    struct player_ : public msm::front::state_machine_def<player_>
    {
        player_()
            :expected_action_counter(0),expected_action2_counter(0)
        {}
        // The list of FSM states
        struct State11 : public msm::front::state<>
        {
            typedef mpl::vector<eventd> deferred_events;

            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        struct State12 : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        struct State13 : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        struct enqueue_action
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& fsm,SourceState& ,TargetState& )
            {
                fsm.template process_event(event2());
            }
        };
        struct expected_action
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& fsm,SourceState& ,TargetState& )
            {
                ++fsm.expected_action_counter;
            }
        };
        struct expected_action2
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& fsm,SourceState& ,TargetState& )
            {
                ++fsm.expected_action2_counter;
            }
        };
        // the initial state of the player SM. Must be defined
        typedef mpl::vector<State11> initial_state;


        // Transition table for player
        struct transition_table : mpl::vector<
            //      Start     Event         Next      Action               Guard
            //    +---------+-------------+---------+---------------------+----------------------+
            Row < State11   , event1      , State12 , enqueue_action                              >,
            Row < State12   , event2      , State13 , expected_action2                            >,
            Row < State12   , eventd      , State13 , expected_action                             >,
            Row < State13   , event2      , State11                                               >,
            Row < State13   , eventd      , State11                                               >
            //    +---------+-------------+---------+---------------------+----------------------+
        > {};

        // Replaces the default no-transition response.
        template <class FSM,class Event>
        void no_transition(Event const& , FSM&,int )
        {
            BOOST_FAIL("no_transition called!");
        }
        // init counters
        template <class Event,class FSM>
        void on_entry(Event const&,FSM& fsm)
        {
            fsm.template get_state<player_::State11&>().entry_counter=0;
            fsm.template get_state<player_::State11&>().exit_counter=0;
            fsm.template get_state<player_::State12&>().entry_counter=0;
            fsm.template get_state<player_::State12&>().exit_counter=0;
            fsm.template get_state<player_::State13&>().entry_counter=0;
            fsm.template get_state<player_::State13&>().exit_counter=0;
        }
        int expected_action_counter;
        int expected_action2_counter;
    };
    // Pick a back-end
    typedef msm::back::state_machine<player_> player;

    BOOST_AUTO_TEST_CASE( TestDeferAndMessageQueue2 )
    {
        player p;
        // needed to start the highest-level SM. This will call on_entry and mark the start of the SM
        p.start();

        p.process_event(eventd());
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 0,"State11 should be active");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State11&>().entry_counter == 1,"State11 entry not called correctly");

        p.process_event(event1());
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 0,"State11 should be active");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State11&>().exit_counter == 1,"State11 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State12&>().entry_counter == 1,"State12 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State12&>().exit_counter == 1,"State12 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State11&>().entry_counter == 2,"State11 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State13&>().exit_counter == 1,"State13 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State13&>().entry_counter == 1,"State13 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.expected_action_counter == 1,"expected_action should have been called");
        BOOST_CHECK_MESSAGE(p.expected_action2_counter == 0,"expected_action2 should not have been called");
    }
}


