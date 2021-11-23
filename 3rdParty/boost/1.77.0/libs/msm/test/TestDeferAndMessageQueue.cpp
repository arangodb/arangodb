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
    struct eventResolve {};
    struct eventConnect {};
    struct eventResolved {};
    struct eventRead {};
    struct eventd {};


    // front-end: define the FSM structure
    struct player_ : public msm::front::state_machine_def<player_>
    {
        player_()
            :expected_action_counter(0)
        {}

        struct enqueue_action1
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& fsm,SourceState& ,TargetState& )
            {
                fsm.template process_event(eventResolve());
            }
        };
        struct enqueue_action2
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& fsm,SourceState& ,TargetState& )
            {
                fsm.template process_event(eventConnect());
            }
        };
        struct expected_action
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& fsm,SourceState& ,TargetState& )
            {
                ++fsm.expected_action_counter;
                //std::cout << "expected action called" << std::endl;
            }
        };
        struct unexpected_action
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& fsm,SourceState& ,TargetState& )
            {
                std::cout << "unexpected action called" << std::endl;
            }
        };

        // The list of FSM states
        struct Unresolved : public msm::front::state<>
        {
            typedef mpl::vector<eventRead > deferred_events;
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
            // Transition table for Empty
            struct internal_transition_table : mpl::vector<
                //    Start     Event         Next      Action               Guard
           Internal <           eventConnect          , msm::front::ActionSequence_<mpl::vector<enqueue_action1,enqueue_action2>>    >
                //  +---------+-------------+---------+---------------------+----------------------+
            > {};
        };
        struct Resolving : public msm::front::state<>
        {
            typedef mpl::vector<eventConnect > deferred_events;
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        struct Resolved : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        struct Connecting : public msm::front::state<>
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
        typedef mpl::vector<Unresolved,State22> initial_state;


        // Transition table for player
        struct transition_table : mpl::vector<
            //      Start     Event         Next      Action               Guard
            //    +---------+-------------+---------+---------------------+----------------------+
            Row < Unresolved , eventResolve         , Resolving                                  >,
            Row < Resolving  , eventResolved        , Resolved                                   >,
            Row < Resolved   , eventConnect         , Connecting , expected_action               >,
            Row < State22    , eventd               , State22                                    >
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
            fsm.template get_state<player_::Unresolved&>().entry_counter=0;
            fsm.template get_state<player_::Unresolved&>().exit_counter=0;
            fsm.template get_state<player_::Resolving&>().entry_counter=0;
            fsm.template get_state<player_::Resolving&>().exit_counter=0;
            fsm.template get_state<player_::Resolved&>().entry_counter=0;
            fsm.template get_state<player_::Resolved&>().exit_counter=0;
            fsm.template get_state<player_::Connecting&>().entry_counter=0;
            fsm.template get_state<player_::Connecting&>().exit_counter=0;
        }
        int expected_action_counter;
    };
    // Pick a back-end
    typedef msm::back::state_machine<player_> player;

    BOOST_AUTO_TEST_CASE( TestDeferAndMessageQueue )
    {
        player p;
        // needed to start the highest-level SM. This will call on_entry and mark the start of the SM
        p.start();

        p.process_event(eventConnect());
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 1,"Resolving should be active");
        BOOST_CHECK_MESSAGE(p.current_state()[1] == 3,"State22 should be active");
        BOOST_CHECK_MESSAGE(p.get_state<player_::Unresolved&>().exit_counter == 1,"Unresolved exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::Unresolved&>().entry_counter == 1,"Unresolved entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::Resolving&>().entry_counter == 1,"Resolving entry not called correctly");

        p.process_event(eventResolved());
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 4,"Connecting should be active");
        BOOST_CHECK_MESSAGE(p.current_state()[1] == 3,"State22 should be active");
        BOOST_CHECK_MESSAGE(p.get_state<player_::Resolved&>().exit_counter == 1,"Resolved exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::Resolved&>().entry_counter == 1,"Resolved entry not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::Resolving&>().exit_counter == 1,"Resolving exit not called correctly");
        BOOST_CHECK_MESSAGE(p.get_state<player_::Connecting&>().entry_counter == 1,"Connecting entry not called correctly");

        BOOST_CHECK_MESSAGE(p.expected_action_counter == 1,"expected_action should have been called");

    }
}


