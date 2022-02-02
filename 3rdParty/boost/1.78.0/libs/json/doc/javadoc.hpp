//
// Copyright (c) 2020 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

----------------------------- DOCUMENTATION STYLE GUIDE -------------------------------

Javadoc documentation shall be structured following the template below.

--------------------------------- GENERAL CONVENTIONS ---------------------------------

Except for a brief element, lines shall not exceed 60 characters in length.

Sentences shall be correctly punctuated. If a line ends in a code element, this
requirement still applies unless the code is part of a Exceptions, Parameters, or
Returns element.

In a code block, the unary negation operator shall be written using the alternative
token "not" when expression following it is not parenthesized and the negation
expression is not the operand of an operator.

With respect to iterators, when referring to an iterator that represents an object
(i.e. not past the end), the convention for doing so is to say the iterator "refers"
to the object. When referring to an iterator that is past-the-end of a range or
an object, the convention for doing so is "iterator one past the end of *X*",
where *X* is the range or object.

----------------------------------------- KEY -----------------------------------------

Names surrounded with braces corrospond to elements defined in the ELEMENTS section.

Bracket enclosed elements are optional. Certain elements have restrictions as to which
contexts they may appear in; if the requirement are not met, the element shall not
be present.

----------------------------------- GENERAL TEMPLATE ----------------------------------

Elements shall appear in the following order:

/** {Brief}

    {Description}


    [@par Complexity

    {Complexity}]


    @par Preconditions

    {Preconditions}


    @par Exception Safety

    {Exception Safety}


    [@par Thread Safety

    {Thread Safety}]


    [@note

    {Remarks}]


    {Template Parameters}

    {Constraints}

    {Return Value}

    {Function Parameters}

    {Exceptions}


    [@par Satisfies

    {Satisfies}]


    [@see

    {See Also}]

*/
---------------------------------------------------------------------------------------




--------------------------------------- ELEMENTS --------------------------------------

All of the following elements shall be present if the contextual requirements for
their appearance is met, and otherwise shall not appear. Optional elements
need not be present.

---------------------------------------------------------------------------------------

The following elements shall only be present for both types and functions.

                          --------------- BOTH ---------------

---------------------------------------------------------------------------------------
Brief
Appearance: Shall be present for functions and types that are visible to the user.

Description: Always written in imperative voice, punctuate with a period. A short
sentence describing the functionality or use of the type or function, without
going into much depth. For functions with no side effects, preconditions,
thrown exceptions and parameters, this should state what the function returns;
further description can be done as a remark. For functions with side effects, it
should be a synopsis of what the function does. For class types, it should describe
what the class does, and for type aliases it should describe what the type represents.

Length requirement: Length should be no more than 200 characters.
---------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------
Description
Appearance: This should be included if the brief and remarks are not sufficient
to describe what the entity does. This shall not be present for type aliases,
or function that have no side effects.

Description: Sufficiently describes the function including all its effects upon its
parameters (if of reference or pointer type), the object it is called upon, or an
object that the called upon object holds a reference or pointer to (in short, any
side-effects that are immediately obvious from the call site).
Does not specify exceptions.
---------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------
Template Parameters
Appearance: Shall be present for templates. Shall not be present
for template parameters whose sole purpose is SFINAE.

Description: Describes what the template parameter represents.
Default arguments shall be specified.
---------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------
Constraints
Appearance: Shall be present for templated entities with constrained
template parameters

Description: For a templated entity that is a function or a function template,
this specifies under which conditions an overload participates in overload
resolution. For a templated entity that is a class or a class template,
this specified under which conditions an template-id referring to a
specialization is well-formed.
---------------------------------------------------------------------------------------

                          ------------- OPTIONAL -------------

---------------------------------------------------------------------------------------
Remarks
Appearance: Optionally present if there are remarks to make outside of the description.
If no description or return value element exists, the content of this shall be
used as a description instead.

Description: For functions, this describes any side effects not covered by the
description (for example, iterator invalidation), or calls to function that can
potentially throw. For types, this is miscellaneous information not covered in the
description that does specify its functionality, but specifies extra
information or restrictions.
---------------------------------------------------------------------------------------


---------------------------------------------------------------------------------------
See Also
Appearance: Optionally present for functions and class types

Description: Specifies function and class types that relate to the
function or class type, or other external resources relating to the entity
---------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------

The following elements shall only be present for functions.

                          ------------- FUNCTIONS -------------

---------------------------------------------------------------------------------------
Preconditions
Appearance: Shall be present for functions with preconditions

Description: Specifies the preconditions of the function. When specifying the validity
of a range, this should be of the form "[x, y] is a valid range.", and otherwise should
take the form of a C++ expression.
---------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------
Exception Safety
Appearance: Shall be present for functions that are potentially throwing

Description: Specifies the exception safety guarantee of the function. Shall be
one of: "Strong guarantee", "Basic guarantee", or "No guarantee".
---------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------
Return Value
Appearance: Shall be present for functions with a return type other than void
whose return value has not already been specified fully in the brief.
If no description element exists, the content of this shall be used as a description
instead (this will replace the description before the Remarks will). Shall not be
present for a constructor or destructor.

Description: Specifies the return value of the function
---------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------
Function Parameters
Appearance: Shall be present for functions that accept one or more parameters

Description: Describes the significance of each of the non-discarded function
parameters (i.e. not used to enact constraints), and specifies default
arguments, if any.
---------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------
Exceptions
Appearance: Shall be present for functions that are potentially throwing

Description: Specifies which exceptions can potentially be directly
thrown by that function and under which conditions they are thrown.
---------------------------------------------------------------------------------------

                          ------------- OPTIONAL -------------

---------------------------------------------------------------------------------------
Complexity
Appearance: Optionally present for functions

Description: Specifies the complexity of the function.
This is done in non-big O notation.
---------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------

The following elements shall only be present for class types.

                       ------------- TYPES (OPTIONAL) -------------

---------------------------------------------------------------------------------------
Thread Safety
Appearance: Optionally present for class types

Description: Specifies when the member function of a class may be called concurrently,
or if they cannot be called concurrently for the same object
---------------------------------------------------------------------------------------

---------------------------------------------------------------------------------------
Satisfies
Appearance: Optionally present for class types

Description: For standard library related types, this specifies
a list of relevant named requirements that this type meets.
---------------------------------------------------------------------------------------





------------------------------ SPECIALIZED TEMPLATES ----------------------------------

                          ------------- CLASS -------------

/** {Brief}

    {Description}


    [@par Thread Safety

    {Thread Safety}]


    [@note

    {Remarks}]


    {Template Parameters}

    {Constraints}


    [@par Satisfies

    {Satisfies}]


    [@see

    {See Also}]

*/

                 ------------- NON-THROWING FUNCTION -------------

  /** {Brief}

    {Description}


    [@par Complexity

    {Complexity}]


    @par Preconditions

    {Preconditions}


    [@note

    {Remarks}]


    {Return Value}

    {Function Parameters}


    [@see

    {See Also}]

*/

                --------------- THROWING FUNCTION ---------------

/** {Brief}

    {Description}


    [@par Complexity

    {Complexity}]


    @par Preconditions

    {Preconditions}


    @par Exception Safety

    {Exception Safety}


    [@note

    {Remarks}]


    {Return Value}

    {Function Parameters}

    {Exceptions}


    [@see

    {See Also}]

*/

            ------------- NON-THROWING FUNCTION TEMPLATE -------------

  /** {Brief}

    {Description}


    [@par Complexity

    {Complexity}]


    @par Preconditions

    {Preconditions}


    [@note

    {Remarks}]


    {Template Parameters}

    {Constraints}

    {Return Value}

    {Function Parameters}


    [@see

    {See Also}]

*/

           --------------- THROWING FUNCTION TEMPLATE ---------------

/** {Brief}

    {Description}


    [@par Complexity

    {Complexity}]


    @par Preconditions

    {Preconditions}


    @par Exception Safety

    {Exception Safety}


    [@note

    {Remarks}]


    {Template Parameters}

    {Constraints}

    {Return Value}

    {Function Parameters}

    {Exceptions}


    [@see

    {See Also}]

*/

        --------------- THROWING FUNCTION NO PRECONDITIONS---------------

/** {Brief}

    {Description}


    [@par Complexity

    {Complexity}]


    @par Exception Safety

    {Exception Safety}


    [@note

    {Remarks}]


    {Return Value}

    {Function Parameters}

    {Exceptions}


    [@see

    {See Also}]

*/

    --------------- THROWING FUNCTION TEMPLATE NO PRECONDITIONS ---------------

/** {Brief}

    {Description}


    [@par Complexity

    {Complexity}]


    @par Exception Safety

    {Exception Safety}


    [@note

    {Remarks}]


    {Template Parameters}

    {Constraints}

    {Return Value}

    {Function Parameters}

    {Exceptions}


    [@see

    {See Also}]

*/


        --------------- NON-THROWING FUNCTION NO PRECONDITIONS---------------

/** {Brief}

    {Description}


    [@par Complexity

    {Complexity}]


    [@note

    {Remarks}]


    {Return Value}

    {Function Parameters}


    [@see

    {See Also}]

*/

    --------------- NON-THROWING FUNCTION TEMPLATE NO PRECONDITIONS ---------------

/** {Brief}

    {Description}


    [@par Complexity

    {Complexity}]


    [@note

    {Remarks}]


    {Template Parameters}

    {Constraints}

    {Return Value}

    {Function Parameters}


    [@see

    {See Also}]

*/