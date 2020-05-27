
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#if !defined(BOOST_VMD_TEST_DOC_EXAMPLE_SWITCH_HPP)
#define BOOST_VMD_TEST_DOC_EXAMPLE_SWITCH_HPP

//[ example_switch

#include <boost/vmd/detail/setup.hpp>

#if BOOST_PP_VARIADICS

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/control/expr_iif.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/control/while.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/facilities/expand.hpp>
#include <boost/preprocessor/tuple/replace.hpp>
#include <boost/preprocessor/tuple/size.hpp>
#include <boost/preprocessor/variadic/to_tuple.hpp>
#include <boost/preprocessor/variadic/size.hpp>
#include <boost/vmd/equal.hpp>
#include <boost/vmd/identity.hpp>
#include <boost/vmd/is_empty.hpp>

/*

  State index into state values

*/

#define BOOST_VMD_SWITCH_STATE_ELEM_INDEX 2
#define BOOST_VMD_SWITCH_STATE_ELEM_DEFAULT 4
#define BOOST_VMD_SWITCH_STATE_ELEM_RESULT 5

/*

  Retrieve the state value, never changes

*/

#define BOOST_VMD_SWITCH_STATE_GET_VALUE(state) \
    BOOST_PP_TUPLE_ELEM(0,state) \
/**/

/*

  Retrieve the state tuple of values, never changes

*/

#define BOOST_VMD_SWITCH_STATE_GET_CHOICES(state) \
    BOOST_PP_TUPLE_ELEM(1,state) \
/**/

/*

  Retrieve the state index

*/

#define BOOST_VMD_SWITCH_STATE_GET_INDEX(state) \
    BOOST_PP_TUPLE_ELEM(2,state) \
/**/

/*

  Retrieve the state tuple of values size, never changes

*/

#define BOOST_VMD_SWITCH_STATE_GET_SIZE(state) \
    BOOST_PP_TUPLE_ELEM(3,state) \
/**/

/*

  Retrieve the state default tuple

*/

#define BOOST_VMD_SWITCH_STATE_GET_DEFAULT(state) \
    BOOST_PP_TUPLE_ELEM(4,state) \
/**/

/*

  Retrieve the state result tuple

*/

#define BOOST_VMD_SWITCH_STATE_GET_RESULT(state) \
    BOOST_PP_TUPLE_ELEM(5,state) \
/**/

/*

  Retrieve the current value tuple

*/

#define BOOST_VMD_SWITCH_STATE_GET_CURRENT_CHOICE(state) \
    BOOST_PP_TUPLE_ELEM \
        ( \
        BOOST_VMD_SWITCH_STATE_GET_INDEX(state), \
        BOOST_VMD_SWITCH_STATE_GET_CHOICES(state) \
        ) \
/**/

/*

  Expands to the state
  
  value = value to compare against
  tuple = choices as a tuple of values
  size = size of tuple of values
  
  None of these ever change in the WHILE state

*/

#define BOOST_VMD_SWITCH_STATE_EXPAND(value,tuple,size) \
    (value,tuple,0,size,(0,),(,)) \
/**/

/*

  Expands to the WHILE state

  The state to our WHILE consists of a tuple of elements:
  
  1: value to compare against
  2: tuple of values. Each value is a value/macro pair or if the default just a macro
  3: index into the values
  4: tuple for default macro. 0 means no default macro, 1 means default macro and then second value is the default macro.
  5: tuple of result matched. Emptiness means no result yet specified, 0 means no match, 1 means match and second value is the matching macro.

*/

#define BOOST_VMD_SWITCH_STATE(value,...) \
    BOOST_VMD_SWITCH_STATE_EXPAND \
        ( \
        value, \
        BOOST_PP_VARIADIC_TO_TUPLE(__VA_ARGS__), \
        BOOST_PP_VARIADIC_SIZE(__VA_ARGS__) \
        ) \
/**/

/*

  Sets the state upon a successful match.
  
  macro = is the matching macro found

*/

#define BOOST_VMD_SWITCH_OP_SUCCESS(d,state,macro) \
    BOOST_PP_TUPLE_REPLACE_D \
        ( \
        d, \
        state, \
        BOOST_VMD_SWITCH_STATE_ELEM_RESULT, \
        (1,macro) \
        ) \
/**/

/*

  Sets the state upon final failure to find a match.
  
  def = default tuple macro, ignored

*/

#define BOOST_VMD_SWITCH_OP_FAILURE(d,state,def) \
    BOOST_PP_TUPLE_REPLACE_D \
        ( \
        d, \
        state, \
        BOOST_VMD_SWITCH_STATE_ELEM_RESULT, \
        (0,) \
        ) \
/**/

/*

  Increments the state index into the tuple values

*/

#define BOOST_VMD_SWITCH_OP_UPDATE_INDEX(d,state) \
    BOOST_PP_TUPLE_REPLACE_D \
        ( \
        d, \
        state, \
        BOOST_VMD_SWITCH_STATE_ELEM_INDEX, \
        BOOST_PP_INC(BOOST_VMD_SWITCH_STATE_GET_INDEX(state)) \
        ) \
/**/

/*

  Choose our current value's macro as our successful match

  tuple = current tuple to test
  
*/

#define BOOST_VMD_SWITCH_OP_TEST_CURRENT_VALUE_MATCH(d,state,tuple) \
    BOOST_VMD_SWITCH_OP_SUCCESS(d,state,BOOST_PP_TUPLE_ELEM(1,tuple)) \
/**/

/*

  Update our state index

  tuple = current tuple to test, ignored
  
*/

#define BOOST_VMD_SWITCH_OP_TEST_CURRENT_VALUE_UPDATE_INDEX(d,state,tuple) \
    BOOST_VMD_SWITCH_OP_UPDATE_INDEX(d,state) \
/**/

/*

  Test our current value against our value to compare against

  tuple = current tuple to test
  
*/

#define BOOST_VMD_SWITCH_OP_TEST_CURRENT_VALUE(d,state,tuple) \
    BOOST_PP_IIF \
        ( \
        BOOST_VMD_EQUAL_D \
            ( \
            d, \
            BOOST_VMD_SWITCH_STATE_GET_VALUE(state), \
            BOOST_PP_TUPLE_ELEM(0,tuple) \
            ), \
        BOOST_VMD_SWITCH_OP_TEST_CURRENT_VALUE_MATCH, \
        BOOST_VMD_SWITCH_OP_TEST_CURRENT_VALUE_UPDATE_INDEX \
        ) \
    (d,state,tuple)    \
/**/

/*

  Set our default macro and update the index in our WHILE state

  tuple = current tuple to test
  
*/

#if BOOST_VMD_MSVC

#define BOOST_VMD_SWITCH_OP_TEST_CURRENT_CREATE_DEFAULT_NN(number,name) \
    (number,name) \
/**/

#define BOOST_VMD_SWITCH_OP_TEST_CURRENT_CREATE_DEFAULT(d,state,tuple) \
    BOOST_VMD_SWITCH_OP_UPDATE_INDEX \
        ( \
        d, \
        BOOST_PP_TUPLE_REPLACE_D \
            ( \
            d, \
            state, \
            BOOST_VMD_SWITCH_STATE_ELEM_DEFAULT, \
            BOOST_VMD_SWITCH_OP_TEST_CURRENT_CREATE_DEFAULT_NN(1,BOOST_PP_TUPLE_ENUM(tuple)) \
            ) \
        ) \
/**/

#else

#define BOOST_VMD_SWITCH_OP_TEST_CURRENT_CREATE_DEFAULT(d,state,tuple) \
    BOOST_VMD_SWITCH_OP_UPDATE_INDEX \
        ( \
        d, \
        BOOST_PP_TUPLE_REPLACE_D \
            ( \
            d, \
            state, \
            BOOST_VMD_SWITCH_STATE_ELEM_DEFAULT, \
            (1,BOOST_PP_TUPLE_ENUM(tuple)) \
            ) \
        ) \
/**/

#endif

/*

  If our current value is a default macro, just set the default macro,
  else test our current value.
  
  tuple = current tuple to test

*/

#define BOOST_VMD_SWITCH_OP_TEST_CURRENT_TUPLE(d,state,tuple) \
    BOOST_PP_IIF \
        ( \
        BOOST_PP_EQUAL_D \
            ( \
            d, \
            BOOST_PP_TUPLE_SIZE(tuple), \
            1 \
            ), \
        BOOST_VMD_SWITCH_OP_TEST_CURRENT_CREATE_DEFAULT, \
        BOOST_VMD_SWITCH_OP_TEST_CURRENT_VALUE \
        ) \
    (d,state,tuple) \
/**/

/*

  Test the current value in our tuple of values

*/

#define BOOST_VMD_SWITCH_OP_TEST_CURRENT(d,state) \
    BOOST_VMD_SWITCH_OP_TEST_CURRENT_TUPLE \
        ( \
        d, \
        state, \
        BOOST_VMD_SWITCH_STATE_GET_CURRENT_CHOICE(state) \
        ) \
/**/

/*

  Choose the default macro as our successful match
  
  def = default tuple consisting of just the default macro name

*/

#define BOOST_VMD_SWITCH_OP_DEFAULT_RET_CHOSEN(d,state,def) \
    BOOST_VMD_SWITCH_OP_SUCCESS \
        ( \
        d, \
        state, \
        BOOST_PP_TUPLE_ELEM(1,def) \
        ) \
/**/

/*

  If the default macro exists, choose it else indicate no macro was found

  def = default tuple consisting of just the default macro name
  
*/

#define BOOST_VMD_SWITCH_OP_DEFAULT_RET(d,state,def) \
    BOOST_PP_IIF \
        ( \
        BOOST_PP_TUPLE_ELEM(0,def), \
        BOOST_VMD_SWITCH_OP_DEFAULT_RET_CHOSEN, \
        BOOST_VMD_SWITCH_OP_FAILURE \
        ) \
    (d,state,def) \
/**/

/*

 Try to choose the default macro if it exists

*/

#define BOOST_VMD_SWITCH_OP_DEFAULT(d,state) \
    BOOST_VMD_SWITCH_OP_DEFAULT_RET \
        ( \
        d, \
        state, \
        BOOST_VMD_SWITCH_STATE_GET_DEFAULT(state) \
        ) \
/**/

/*

  WHILE loop operation
  
  Check for the next value match or try to choose the default if all matches have been checked

*/

#define BOOST_VMD_SWITCH_OP(d,state) \
    BOOST_PP_IIF \
        ( \
        BOOST_PP_EQUAL_D \
            (  \
            d, \
            BOOST_VMD_SWITCH_STATE_GET_INDEX(state), \
            BOOST_VMD_SWITCH_STATE_GET_SIZE(state) \
            ), \
        BOOST_VMD_SWITCH_OP_DEFAULT, \
        BOOST_VMD_SWITCH_OP_TEST_CURRENT \
        ) \
    (d,state) \
/**/

/*

  WHILE loop predicate
  
  Continue the WHILE loop if a result has not yet been specified

*/

#define BOOST_VMD_SWITCH_PRED(d,state) \
    BOOST_VMD_IS_EMPTY \
        ( \
        BOOST_PP_TUPLE_ELEM \
            ( \
            0, \
            BOOST_VMD_SWITCH_STATE_GET_RESULT(state) \
            ) \
        ) \
/**/

/*

  Invokes the function-like macro
  
  macro = function-like macro name
  tparams = tuple of macro parameters

*/

#define BOOST_VMD_SWITCH_PROCESS_INVOKE_MACRO(macro,tparams) \
    BOOST_PP_EXPAND(macro tparams) \
/**/

/*

  Processes our WHILE loop result
  
  callp = tuple of parameters for the called macro
  result = tuple. The first tuple element is 0
           if no macro has been found or 1 if a macro
           has been found. If 1 the second element is
           the name of a function-like macro

*/

#define BOOST_VMD_SWITCH_PROCESS(callp,result) \
    BOOST_PP_EXPR_IIF \
        ( \
        BOOST_PP_TUPLE_ELEM(0,result), \
        BOOST_VMD_SWITCH_PROCESS_INVOKE_MACRO \
            ( \
            BOOST_PP_TUPLE_ELEM(1,result), \
            callp \
            ) \
        ) \
/**/

/*

  Use BOOST_VMD_SWITCH_IDENTITY to pass a fixed value instead
  of a function-like macro as the second element of
  any tuple of the variadic parameters, or as the default
  value, to BOOST_VMD_SWITCH.
  
*/

#if BOOST_VMD_MSVC
#define BOOST_VMD_SWITCH_IDENTITY(item) BOOST_PP_CAT(BOOST_VMD_IDENTITY(item),)
#else
#define BOOST_VMD_SWITCH_IDENTITY BOOST_VMD_IDENTITY
#endif

/*

  Switch macro
  
  Parameters are:
  
  value = value to compare against. May be any VMD data value.
  callp = tuple of parameters for the called macro
  variadic parameters = each parameter must be a tuple.
    Each tuple consists of a two-element tuple. The first element is
    a value, which may be any VMD data value, and the second element
    is the name of a function-like macro to be called if the value
    is equal to the value to compare against. For a default value
    the tuple is a single-element tuple which contains the name of 
    a function-like macro to be called if no other value matches.

*/

#define BOOST_VMD_SWITCH(value,callp,...) \
    BOOST_VMD_SWITCH_PROCESS \
        ( \
        callp, \
        BOOST_VMD_SWITCH_STATE_GET_RESULT \
            ( \
            BOOST_PP_WHILE \
                ( \
                BOOST_VMD_SWITCH_PRED, \
                BOOST_VMD_SWITCH_OP, \
                BOOST_VMD_SWITCH_STATE(value,__VA_ARGS__) \
                ) \
            ) \
        ) \
/**/

#endif /* BOOST_PP_VARIADICS */

//]
 
#endif /* BOOST_VMD_TEST_DOC_EXAMPLE_SWITCH_HPP */
