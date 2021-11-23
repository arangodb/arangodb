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
        // The list of FSM states
        struct State11 : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        struct State12 : public msm::front::state<>
        {
            typedef mpl::vector<eventd> deferred_events;
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
        struct State21 : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        struct State22 : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        // the initial state of the player SM. Must be defined
        typedef mpl::vector<State11,State21> initial_state;


        // Transition table for player
        struct transition_table : mpl::vector<
            //      Start     Event         Next      Action               Guard
            //    +---------+-------------+---------+---------------------+----------------------+
            Row < State11   , event1      , State12                                               >,
            Row < State12   , event2      , State13                                               >,

            Row < State21   , event3      , State22                                               >,
            Row < State22   , eventd      , State21                                               >
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
            fsm.template get_state<player_::State21&>().entry_counter=0;
            fsm.template get_state<player_::State22&>().exit_counter=0;
        }
    };
    // Pick a back-end
    typedef msm::back::state_machine<player_> player;

    BOOST_AUTO_TEST_CASE( TestDeferIn2Regions )
    {
        player p;
        // needed to start the highest-level SM. This will call on_entry and mark the start of the SM
        p.start();

        p.process_event(event1());
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 1,"State12 should be active");
        BOOST_CHECK_MESSAGE(p.current_state()[1] == 2,"State21 should be active");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State11&>().exit_counter == 1,"State11 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State11&>().entry_counter == 1,"State11 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State12&>().entry_counter == 1,"State12 entry not called correctly");

        // deferred
        p.process_event(eventd());
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 1,"State12 should be active");
        BOOST_CHECK_MESSAGE(p.current_state()[1] == 2,"State21 should be active");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State11&>().exit_counter == 1,"State11 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State11&>().entry_counter == 1,"State11 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State12&>().entry_counter == 1,"State12 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State12&>().exit_counter == 0,"State12 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State21&>().exit_counter == 0,"State21 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State21&>().entry_counter == 1,"State21 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State22&>().exit_counter == 0,"State22 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State22&>().entry_counter == 0,"State22 entry not called correctly");

        p.process_event(event3());
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 1,"State12 should be active");
        BOOST_CHECK_MESSAGE(p.current_state()[1] == 2,"State21 should be active");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State21&>().exit_counter == 1,"State21 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State21&>().entry_counter == 2,"State21 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State22&>().exit_counter == 1,"State22 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State22&>().entry_counter == 1,"State22 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_deferred_queue().size() == 1,"Deferred queue should have one element");
        p.clear_deferred_queue();

        p.process_event(event2());
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 4,"State13 should be active");
        BOOST_CHECK_MESSAGE(p.current_state()[1] == 2,"State21 should be active");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State21&>().exit_counter == 1,"State21 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State21&>().entry_counter == 2,"State21 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State22&>().exit_counter == 1,"State22 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::State22&>().entry_counter == 1,"State22 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_deferred_queue().size() == 0,"Deferred queue should have no element");
    }
}


