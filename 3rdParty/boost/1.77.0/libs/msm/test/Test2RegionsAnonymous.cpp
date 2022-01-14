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
// functors
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/euml/common.hpp>
// for And_ operator
#include <boost/msm/front/euml/operator.hpp>

#include <boost/test/unit_test.hpp>

using namespace std;
namespace msm = boost::msm;
namespace mpl = boost::mpl;
using namespace msm::front;
// for And_ operator
using namespace msm::front::euml;

namespace
{
    // events
    struct event1 {};
    struct event2 {};

    // front-end: define the FSM structure
    struct my_machine_ : public msm::front::state_machine_def<my_machine_>
    {

        // The list of FSM states
        struct State1 : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        struct State2 : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };

        struct State3 : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };

        struct State1b : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        struct State2b : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };

        struct always_true
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            bool operator()(EVT const&  ,FSM&,SourceState& ,TargetState& )
            {
                return true;
            }
        };
        struct always_false
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            bool operator()(EVT const&  ,FSM&,SourceState& ,TargetState& )
            {
                return false;
            }
        };

        // the initial state of the player SM. Must be defined
        typedef boost::mpl::vector2<State1,State1b> initial_state;

        // Transition table for player
        struct transition_table : boost::mpl::vector<
            //    Start     Event         Next      Action               Guard
            //  +---------+-------------+---------+---------------------+----------------------+
            Row < State1  , event1      , State2  , none                , always_true          >,
            Row < State2  , none        , State3                                               >,
            //  +---------+-------------+---------+---------------------+----------------------+
            Row < State1b , event1      , State2b , none                , always_false         >
            //  +---------+-------------+---------+---------------------+----------------------+
        > {};
        // Replaces the default no-transition response.
        template <class FSM,class Event>
        void no_transition(Event const&, FSM&,int)
        {
            BOOST_FAIL("no_transition called!");
        }
        // init counters
        template <class Event,class FSM>
        void on_entry(Event const&,FSM& fsm)
        {
            fsm.template get_state<my_machine_::State1&>().entry_counter=0;
            fsm.template get_state<my_machine_::State1&>().exit_counter=0;
            fsm.template get_state<my_machine_::State2&>().entry_counter=0;
            fsm.template get_state<my_machine_::State2&>().exit_counter=0;
            fsm.template get_state<my_machine_::State3&>().entry_counter=0;
            fsm.template get_state<my_machine_::State3&>().exit_counter=0;
            fsm.template get_state<my_machine_::State1b&>().entry_counter=0;
            fsm.template get_state<my_machine_::State1b&>().exit_counter=0;
            fsm.template get_state<my_machine_::State2b&>().entry_counter=0;
            fsm.template get_state<my_machine_::State2b&>().exit_counter=0;
        }
    };
    // Pick a back-end
    typedef msm::back::state_machine<my_machine_> my_machine;

    BOOST_AUTO_TEST_CASE( my_test )
    {
        my_machine p;

        p.start();
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 0,"State1 should be active");
        BOOST_CHECK_MESSAGE(p.current_state()[1] == 2,"State1b should be active");

        p.process_event(event1());
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 3,"State3 should be active");
        BOOST_CHECK_MESSAGE(p.current_state()[1] == 2,"State1b should be active");

        BOOST_CHECK_MESSAGE(p.get_state<my_machine_::State1&>().exit_counter == 1,"State1 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<my_machine_::State1&>().entry_counter == 1,"State1 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<my_machine_::State2&>().exit_counter == 1,"State2 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<my_machine_::State2&>().entry_counter == 1,"State2 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<my_machine_::State3&>().exit_counter == 0,"State3 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<my_machine_::State3&>().entry_counter == 1,"State3 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<my_machine_::State1b&>().entry_counter == 1,"State1b entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<my_machine_::State1b&>().exit_counter == 0,"State1b exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<my_machine_::State2b&>().entry_counter == 0,"State2b entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<my_machine_::State2b&>().exit_counter == 0,"State2b exit not called correctly");


    }
}


