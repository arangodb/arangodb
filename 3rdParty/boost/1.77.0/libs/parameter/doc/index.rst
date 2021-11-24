++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
The Boost Parameter Library
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

|(logo)|__

.. |(logo)| image:: ../../../../boost.png
    :alt: Boost

__ ../../../../index.htm

-------------------------------------

:Abstract: Use this library to write functions and class templates that can
    accept arguments by name:

.. parsed-literal::

    new_window(
        "alert"
      , **_width=10**
      , **_titlebar=false**
    );

    smart_ptr<
        Foo
      , **deleter<Deallocate<Foo> >**
      , **copy_policy<DeepCopy>**
    > p(new Foo);

Since named arguments can be passed in any order, they are especially useful
when a function or template has more than one parameter with a useful default
value.  The library also supports *deduced* parameters: that is to say,
parameters whose identity can be deduced from their types.

.. @jam_prefix.append('''
    project test
        : requirements <include>. <implicit-dependency>/boost//headers ;
    ''')

.. @example.prepend('''
    #include <boost/parameter.hpp>

    namespace test {

        BOOST_PARAMETER_NAME(title)
        BOOST_PARAMETER_NAME(width)
        BOOST_PARAMETER_NAME(titlebar)

        BOOST_PARAMETER_FUNCTION(
            (int), new_window, tag, (required (title,*)(width,*)(titlebar,*))
        )
        {
            return 0;
        }

        BOOST_PARAMETER_TEMPLATE_KEYWORD(deleter)
        BOOST_PARAMETER_TEMPLATE_KEYWORD(copy_policy)

        template <typename T>
        struct Deallocate
        {
        };

        struct DeepCopy
        {
        };

        namespace parameter = boost::parameter;

        struct Foo
        {
        };

        template <typename T, typename A0, typename A1>
        struct smart_ptr
        {
            smart_ptr(Foo*);
        };
    }
    using namespace test;
    int x =
    ''');

.. @test('compile')


-------------------------------------

:Authors:       David Abrahams, Daniel Wallin
:Contact:       dave@boost-consulting.com, daniel@boostpro.com
:organization:  `BoostPro Computing`_
:date:          $Date: 2005/07/17 19:53:01 $

:copyright:     Copyright David Abrahams, Daniel Wallin
                2005-2009. Distributed under the Boost Software License,
                Version 1.0. (See accompanying file LICENSE_1_0.txt
                or copy at http://www.boost.org/LICENSE_1_0.txt)

.. _`BoostPro Computing`: http://www.boostpro.com

.. _concepts: http://www.boost.org/more/generic_programming.html#concept

-------------------------------------

[Note: this tutorial does not cover all details of the library.  Please see
also the `reference documentation`__\ ]

__ reference.html

.. contents:: **Table of Contents**
    :depth: 2

.. role:: concept
    :class: concept

.. role:: vellipsis
    :class: vellipsis

.. section-numbering::

-------------------------------------

==========
Motivation
==========

In C++, arguments_ are normally given meaning by their positions with respect
to a parameter_ list: the first argument passed maps onto the first parameter
in a function's definition, and so on.  That protocol is fine when there is at
most one parameter with a default value, but when there are even a few useful
defaults, the positional interface becomes burdensome:

* .. compound::

    Since an argument's meaning is given by its position, we have to choose an
    (often arbitrary) order for parameters with default values, making some
    combinations of defaults unusable:

    .. parsed-literal::

        window* new_window(
            char const* name
          , **int border_width = default_border_width**
          , bool movable = true
          , bool initially_visible = true
        );

        bool const movability = false;
        window* w = new_window("alert box", movability);

    In the example above we wanted to make an unmoveable window with a default
    ``border_width``, but instead we got a moveable window with a
    ``border_width`` of zero.  To get the desired effect, we'd need to write:

    .. parsed-literal::

        window* w = new_window(
            "alert box", **default_border_width**, movability
        );

* .. compound::

    It can become difficult for readers to understand the meaning of arguments
    at the call site::

        window* w = new_window("alert", 1, true, false);

    Is this window moveable and initially invisible, or unmoveable and
    initially visible?  The reader needs to remember the order of arguments to
    be sure.  

*   The author of the call may not remember the order of the arguments either,
    leading to hard-to-find bugs.

.. @ignore(3)

-------------------------
Named Function Parameters
-------------------------

.. compound::

    This library addresses the problems outlined above by associating each
    parameter name with a keyword object.  Now users can identify arguments by
    name, rather than by position:

    .. parsed-literal::

        window* w = new_window(
            "alert box"
          , **movable_=**\ false
        ); // OK!

.. @ignore()

---------------------------
Deduced Function Parameters
---------------------------

.. compound::

    A **deduced parameter** can be passed in any position *without* supplying
    an explicit parameter name.  It's not uncommon for a function to have
    parameters that can be uniquely identified based on the types of arguments
    passed.  The ``name`` parameter to ``new_window`` is one such
    example.  None of the other arguments, if valid, can reasonably be
    converted to a ``char const*``.  With a deduced parameter interface, we
    could pass the window name in *any* argument position without causing
    ambiguity:

    .. parsed-literal::

        window* w = new_window(
            movable_=false
          , **"alert box"**
        ); // OK!
        window* w = new_window(
            **"alert box"**
          , movable_=false
        ); // OK!

    Appropriately used, a deduced parameter interface can free the user of the
    burden of even remembering the formal parameter names.

.. @ignore()

--------------------------------
Class Template Parameter Support
--------------------------------

.. compound::

    The reasoning we've given for named and deduced parameter interfaces
    applies equally well to class templates as it does to functions.  Using
    the Parameter library, we can create interfaces that allow template
    arguments (in this case ``shared`` and ``Client``) to be explicitly named,
    like this:

    .. parsed-literal::

        smart_ptr<
            **ownership<shared>**
          , **value_type<Client>**
        > p;

    The syntax for passing named template arguments is not quite as natural as
    it is for function arguments (ideally, we'd be able to write
    ``smart_ptr<ownership = shared, …>``).  This small syntactic deficiency
    makes deduced parameters an especially big win when used with class
    templates:

    .. parsed-literal::

        // *p and q could be equivalent, given a deduced*
        // *parameter interface.*
        smart_ptr<**shared**, **Client**> p;
        smart_ptr<**Client**, **shared**> q;

.. @ignore(2)

========
Tutorial
========

This tutorial shows all the basics—how to build both named- and
deduced-parameter interfaces to function templates and class
templates—and several more advanced idioms as well.

---------------------------
Parameter-Enabled Functions
---------------------------

In this section we'll show how the Parameter library can be used to
build an expressive interface to the `Boost Graph library`__\ 's
|dfs|_ algorithm. [#old_interface]_ 

.. Revisit this

    After laying some groundwork and describing the algorithm's abstract
    interface, we'll show you how to build a basic implementation with keyword
    support.  Then we'll add support for default arguments and we'll gradually
    refine the implementation with syntax improvements.  Finally we'll show
    how to streamline the implementation of named parameter interfaces,
    improve their participation in overload resolution, and optimize their
    runtime efficiency.

__ ../../../graph/doc/index.html

.. _dfs: ../../../graph/doc/depth_first_search.html

.. |dfs| replace:: ``depth_first_search``


Headers And Namespaces
======================

Most components of the Parameter library are declared in a header named for
the component.  For example, ::

    #include <boost/parameter/keyword.hpp>

will ensure ``boost::parameter::keyword`` is known to the compiler.  There
is also a combined header, ``boost/parameter.hpp``, that includes most of
the library's components.  For the the rest of this tutorial, unless we
say otherwise, you can use the rule above to figure out which header to
``#include`` to access any given component of the library.

.. @example.append('''
    using boost::parameter::keyword;
    ''')

.. @test('compile')

Also, the examples below will also be written as if the namespace alias ::

    namespace parameter = boost::parameter;

.. @ignore()

has been declared: we'll write ``parameter::xxx`` instead of
``boost::parameter::xxx``.

The Abstract Interface to |dfs|
===============================

The Graph library's |dfs| algorithm is a generic function accepting
from one to four arguments by reference.  If all arguments were
required, its signature might be as follows::

    template <
        typename Graph
      , typename DFSVisitor
      , typename Index
      , typename ColorMap
    >
    void
        depth_first_search(
            Graph const& graph 
          , DFSVisitor visitor
          , typename graph_traits<g>::vertex_descriptor root_vertex
          , IndexMap index_map
          , ColorMap& color
        );

.. @ignore()

However, most of the parameters have a useful default value,
as shown in the table below.

.. _`parameter table`: 
.. _`default expressions`: 

.. table:: ``depth_first_search`` Parameters

    +-----------------+------+-------------------------+------------------------------------+
    | Parameter       | Data | Type                    | Default Value                      |
    | Name            | Flow |                         | (if any)                           |
    +=================+======+=========================+====================================+
    | ``graph``       | in   | Model of                | none - this argument is required.  |
    |                 |      | |IncidenceGraph|_ and   |                                    |
    |                 |      | |VertexListGraph|_      |                                    |
    +-----------------+------+-------------------------+------------------------------------+
    | ``visitor``     | in   | Model of |DFSVisitor|_  | ``boost::dfs_visitor<>()``         |
    +-----------------+------+-------------------------+------------------------------------+
    | ``root_vertex`` | in   | ``graph``'s vertex      | ``*vertices(graph).first``         |
    |                 |      | descriptor type.        |                                    |
    +-----------------+------+-------------------------+------------------------------------+
    | ``index_map``   | in   | Model of                | ``get(boost::vertex_index,graph)`` |
    |                 |      | |ReadablePropertyMap|_  |                                    |
    |                 |      | with key type :=        |                                    |
    |                 |      | ``graph``'s vertex      |                                    |
    |                 |      | descriptor and value    |                                    |
    |                 |      | type an integer type.   |                                    |
    +-----------------+------+-------------------------+------------------------------------+
    | ``color_map``   | in / | Model of                | a ``boost::iterator_property_map`` |
    |                 | out  | |ReadWritePropertyMap|_ | created from a ``std::vector`` of  |
    |                 |      | with key type :=        | ``default_color_type`` of size     |
    |                 |      | ``graph``'s vertex      | ``num_vertices(graph)`` and using  |
    |                 |      | descriptor type.        | ``index_map`` for the index map.   |
    +-----------------+------+-------------------------+------------------------------------+

.. |IncidenceGraph| replace:: :concept:`Incidence Graph`
.. |VertexListGraph| replace:: :concept:`Vertex List Graph`
.. |DFSVisitor| replace:: :concept:`DFS Visitor`
.. |ReadablePropertyMap| replace:: :concept:`Readable Property Map`
.. |ReadWritePropertyMap| replace:: :concept:`Read/Write Property Map`

.. _`IncidenceGraph`: ../../../graph/doc/IncidenceGraph.html
.. _`VertexListGraph`: ../../../graph/doc/VertexListGraph.html
.. _`DFSVisitor`: ../../../graph/doc/DFSVisitor.html
.. _`ReadWritePropertyMap`: ../../../property_map/doc/ReadWritePropertyMap.html
.. _`ReadablePropertyMap`: ../../../property_map/doc/ReadablePropertyMap.html

Don't be intimidated by the information in the second and third columns
above.  For the purposes of this exercise, you don't need to understand
them in detail.

Defining the Keywords
=====================

The point of this exercise is to make it possible to call
``depth_first_search`` with named arguments, leaving out any
arguments for which the default is appropriate:

.. parsed-literal::

    graphs::depth_first_search(g, **color_map_=my_color_map**);

.. @ignore()

To make that syntax legal, there needs to be an object called
“\ ``color_map_``\ ” whose assignment operator can accept a
``my_color_map`` argument.  In this step we'll create one such
**keyword object** for each parameter.  Each keyword object will be
identified by a unique **keyword tag type**.  

.. Revisit this

    We're going to define our interface in namespace ``graphs``.  Since users
    need access to the keyword objects, but not the tag types, we'll define
    the keyword objects so they're accessible through ``graphs``, and we'll
    hide the tag types away in a nested namespace, ``graphs::tag``.  The
    library provides a convenient macro for that purpose.

We're going to define our interface in namespace ``graphs``.  The
library provides a convenient macro for defining keyword objects::

    #include <boost/parameter/name.hpp>

    namespace graphs {

        BOOST_PARAMETER_NAME(graph)    // Note: no semicolon
        BOOST_PARAMETER_NAME(visitor)
        BOOST_PARAMETER_NAME(root_vertex)
        BOOST_PARAMETER_NAME(index_map)
        BOOST_PARAMETER_NAME(color_map)
    }

.. @test('compile')

The declaration of the ``graph`` keyword you see here is equivalent to::

    namespace graphs {
        namespace tag {

            // keyword tag type
            struct graph
            {
                typedef boost::parameter::forward_reference qualifier;
            };
        }

        namespace // unnamed
        {
            // A reference to the keyword object
            boost::parameter::keyword<tag::graph> const& _graph
                = boost::parameter::keyword<tag::graph>::instance;
        }
    }

.. @example.prepend('#include <boost/parameter/keyword.hpp>')
.. @test('compile')

It defines a *keyword tag type* named ``tag::graph`` and a *keyword object*
reference named ``_graph``.

This “fancy dance” involving an unnamed namespace and references is all done
to avoid violating the One Definition Rule (ODR) [#odr]_ when the named
parameter interface is used by function templates that are instantiated in
multiple translation units (MSVC6.x users see `this note`__).

__ `Compiler Can't See References In Unnamed Namespace`_

Writing the Function
====================

Now that we have our keywords defined, the function template definition
follows a simple pattern using the ``BOOST_PARAMETER_FUNCTION`` macro::

    #include <boost/parameter/preprocessor.hpp>

    namespace graphs {

        BOOST_PARAMETER_FUNCTION(
            (void),                 // 1. parenthesized return type
            depth_first_search,     // 2. name of the function template
            tag,                    // 3. namespace of tag types
            (required (graph, *) )  // 4. one required parameter, and
            (optional               //    four optional parameters,
                                    //    with defaults
                (visitor,     *, boost::dfs_visitor<>()) 
                (root_vertex, *, *vertices(graph).first) 
                (index_map,   *, get(boost::vertex_index,graph)) 
                (color_map,   *, 
                    default_color_map(num_vertices(graph), index_map)
                ) 
            )
        )
        {
            // ... body of function goes here...
            // use graph, visitor, index_map, and color_map
        }
    }

.. @example.prepend('''
    #include <boost/parameter/name.hpp>

    BOOST_PARAMETER_NAME(graph)
    BOOST_PARAMETER_NAME(visitor)
    BOOST_PARAMETER_NAME(in(root_vertex))
    BOOST_PARAMETER_NAME(in(index_map))
    BOOST_PARAMETER_NAME(in_out(color_map))

    namespace boost {

        template <typename T = int>
        struct dfs_visitor
        {
        };

        int vertex_index = 0;
    }
    ''')

.. @test('compile')

The arguments to ``BOOST_PARAMETER_FUNCTION`` are:

1.  The return type of the resulting function template.  Parentheses around
    the return type prevent any commas it might contain from confusing the
    preprocessor, and are always required.

2.  The name of the resulting function template.

3.  The name of a namespace where we can find tag types whose names match the
    function's parameter names.

4.  The function signature.  

Function Signatures
===================

Function signatures are described as one or two adjacent parenthesized terms
(a Boost.Preprocessor_ sequence_) describing the function's parameters in the
order in which they'd be expected if passed positionally.  Any required
parameters must come first, but the ``(required … )`` clause can be omitted
when all the parameters are optional.

.. _Boost.Preprocessor: ../../../preprocessor/doc/index.html
.. _sequence: http://boost-consulting.com/mplbook/preprocessor.html#sequences

Required Parameters
-------------------

.. compound::

    Required parameters are given first—nested in a ``(required … )``
    clause—as a series of two-element tuples describing each parameter name
    and any requirements on the argument type.  In this case there is only a
    single required parameter, so there's just a single tuple:

    .. parsed-literal::

        (required **(graph, \*)** )

    Since ``depth_first_search`` doesn't require any particular type for its
    ``graph`` parameter, we use an asterix to indicate that any type is
    allowed.  Required parameters must always precede any optional parameters
    in a signature, but if there are *no* required parameters, the
    ``(required … )`` clause can be omitted entirely.

.. @example.prepend('''
    #include <boost/parameter.hpp>

    BOOST_PARAMETER_NAME(graph)

    BOOST_PARAMETER_FUNCTION((void), f, tag,
    ''')

.. @example.append(') {}')
.. @test('compile')

Optional Parameters
-------------------

.. compound::

    Optional parameters—nested in an ``(optional … )`` clause—are given as a
    series of adjacent *three*\ -element tuples describing the parameter name,
    any requirements on the argument type, *and* and an expression
    representing the parameter's default value:

    .. parsed-literal::

        (optional
            **(visitor,     \*, boost::dfs_visitor<>())
            (root_vertex, \*, \*vertices(graph).first)
            (index_map,   \*, get(boost::vertex_index,graph))
            (color_map,   \*,
                default_color_map(num_vertices(graph), index_map)
            )**
        )

.. @example.prepend('''
    #include <boost/parameter.hpp>

    namespace boost {

        int vertex_index = 0;

        template <typename T = int>
        struct dfs_visitor
        {
        };
    }

    BOOST_PARAMETER_NAME(graph)
    BOOST_PARAMETER_NAME(visitor)
    BOOST_PARAMETER_NAME(in(root_vertex))
    BOOST_PARAMETER_NAME(in(index_map))
    BOOST_PARAMETER_NAME(in_out(color_map))

    BOOST_PARAMETER_FUNCTION((void), f, tag,
        (required (graph, \*))
    ''')

.. @example.append(') {}')
.. @test('compile')

Handling “In”, “Out”, “Consume / Move-From”, and “Forward” Parameters
---------------------------------------------------------------------

.. compound::

    By default, Boost.Parameter treats all parameters as if they were
    *forward* `parameters`_, which functions would take in by rvalue reference
    and only ``std::forward`` or ``boost::forward`` to other functions.  Such
    parameters can be ``const`` lvalues, mutable lvalues, ``const`` rvalues,
    or mutable rvalues.  Therefore, the default configuration grants the most
    flexibility to user code.  However:

    *   Users can configure one or more parameters to be *in* `parameters`_,
        which can fall into the same categories as *forward* `parameters`_ but
        are now passed by ``const`` lvalue reference and so must only be read
        from.  Continuing from the previous example, to indicate that
        ``root_vertex`` and ``index_map`` are read-only, we wrap their names
        in ``in(…)``.

    *   Users can configure one or more parameters to be either *out*
        `parameters`_, which functions would strictly write to, or *in-out*
        `parameters`_, which functions would both read from and write
        to.  Such parameters can only be mutable lvalues.  In the example, to
        indicate that ``color_map`` is read-write, we wrap its name in
        ``in_out(…)``.  Note that Boost.Parameter sees no functional
        difference between ``out(…)`` and ``in_out(…)``, so you may choose
        whichever makes your interfaces more self-documenting.

    *   Users can configure one or more parameters to be *consume* or
        *move-from* `parameters`_, which functions would take in by mutable
        rvalue reference and ``std::move`` or ``boost::move`` as the last
        access step.  Such parameters can only be mutable
        rvalues.  Boost.Parameter supports wrapping the corresponding names in
        ``consume(…)`` or ``move_from(…)``.

    .. parsed-literal::

        BOOST_PARAMETER_NAME(graph)
        BOOST_PARAMETER_NAME(visitor)
        BOOST_PARAMETER_NAME(**in(root_vertex)**)
        BOOST_PARAMETER_NAME(**in(index_map)**)
        BOOST_PARAMETER_NAME(**in_out(color_map)**)

    In order to see what happens when parameters are bound to arguments that
    violate their category constraints, attempt to compile the |compose_cpp|_
    test program with either the ``LIBS_PARAMETER_TEST_COMPILE_FAILURE_0``
    macro or the ``LIBS_PARAMETER_TEST_COMPILE_FAILURE_1`` macro
    ``#defined``.  You should encounter a compiler error caused by a specific
    constraint violation.

.. @example.prepend('''
    #include <boost/parameter.hpp>

    namespace boost {

        int vertex_index = 0;

        template <typename T = int>
        struct dfs_visitor
        {
        };
    }
    ''')

.. @example.append('''
    BOOST_PARAMETER_FUNCTION((void), f, tag,
        (required (graph, \*))
        (optional
            (visitor,     \*, boost::dfs_visitor<>())
            (root_vertex, \*, \*vertices(graph).first)
            (index_map,   \*, get(boost::vertex_index, graph))
            (color_map,   \*,
                default_color_map(num_vertices(graph), index_map)
            )
        )
    )
    {
    }
    ''')

.. @test('compile')

.. _`parameters`: http://www.modernescpp.com/index.php/c-core-guidelines-how-to-pass-function-parameters
.. |compose_cpp| replace:: compose.cpp
.. _compose_cpp: ../../test/compose.cpp

Positional Arguments
--------------------

When arguments are passed positionally (without the use of keywords), they
will be mapped onto parameters in the order the parameters are given in the
signature, so for example in this call ::

    graphs::depth_first_search(x, y);

.. @ignore()

``x`` will always be interpreted as a graph and ``y`` will always be
interpreted as a visitor.

Default Expression Evaluation
-----------------------------

.. compound::

    Note that in our example, the value of the graph parameter is used in the
    default expressions for ``root_vertex``, ``index_map``, and ``color_map``.  

    .. parsed-literal::

        (required (**graph**, \*) )
        (optional
            (visitor,     \*, boost::dfs_visitor<>())
            (root_vertex, \*, \*vertices(**graph**).first)
            (index_map,   \*, get(boost::vertex_index, **graph**))
            (color_map,   \*,
                default_color_map(num_vertices(**graph**), index_map)
            )
        )

.. @ignore()

    A default expression is evaluated in the context of all preceding
    parameters, so you can use any of their values by name.

.. compound::

    A default expression is never evaluated—or even instantiated—if an actual
    argument is passed for that parameter.  We can actually demonstrate that
    with our code so far by replacing the body of ``depth_first_search`` with
    something that prints the arguments:

    .. parsed-literal::

        #include <boost/graph/depth_first_search.hpp>  // for dfs_visitor

        BOOST_PARAMETER_FUNCTION(
            (bool), depth_first_search, tag
            *…signature goes here…*
        )
        {
            std::cout << "graph=" << graph;
            std::cout << std::endl;
            std::cout << "visitor=" << visitor;
            std::cout << std::endl;
            std::cout << "root_vertex=" << root_vertex;
            std::cout << std::endl;
            std::cout << "index_map=" << index_map;
            std::cout << std::endl;
            std::cout << "color_map=" << color_map;
            std::cout << std::endl;
            return true;
        }

        #include <boost/core/lightweight_test.hpp>

        int main()
        {
            char const\* g = "1";
            depth_first_search(1, 2, 3, 4, 5);
            depth_first_search(
                g, '2', _color_map = '5',
                _index_map = "4", _root_vertex = "3"
            );
            return boost::report_errors();
        }

    Despite the fact that default expressions such as
    ``vertices(graph).first`` are ill-formed for the given ``graph``
    arguments, both calls will compile, and each one will print exactly the
    same thing.

.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <iostream>

    BOOST_PARAMETER_NAME(graph)
    BOOST_PARAMETER_NAME(visitor)
    BOOST_PARAMETER_NAME(root_vertex)
    BOOST_PARAMETER_NAME(index_map)
    BOOST_PARAMETER_NAME(color_map)
    ''')

.. @example.replace_emphasis('''
  , (required 
        (graph, \*)
        (visitor, \*)
        (root_vertex, \*)
        (index_map, \*)
        (color_map, \*)
    )
    ''')
.. @test('run')

Signature Matching and Overloading
----------------------------------

In fact, the function signature is so general that any call to
``depth_first_search`` with fewer than five arguments will match our function,
provided we pass *something* for the required ``graph`` parameter.  That might
not seem to be a problem at first; after all, if the arguments don't match the
requirements imposed by the implementation of ``depth_first_search``, a
compilation error will occur later, when its body is instantiated.

There are at least three problems with very general function signatures.  

1.  By the time our ``depth_first_search`` is instantiated, it has been
    selected as the best matching overload.  Some other ``depth_first_search``
    overload might've worked had it been chosen instead.  By the time we see a
    compilation error, there's no chance to change that decision.

2.  Even if there are no overloads, error messages generated at instantiation
    time usually expose users to confusing implementation details.  For
    example, users might see references to names generated by
    ``BOOST_PARAMETER_FUNCTION`` such as
    ``graphs::detail::depth_first_search_with_named_params`` (or worse—think
    of the kinds of errors you get from your STL implementation when you make
    a mistake). [#ConceptsTS]_

3.  The problems with exposing such permissive function template signatures
    have been the subject of much discussion, especially in the presence of
    `unqualified calls`__.  If all we want is to avoid unintentional
    argument-dependent lookup (ADL), we can isolate ``depth_first_search`` in
    a namespace containing no types [#using]_, but suppose we *want* it to
    found via ADL?

__ http://www.open-std.org/jtc1/sc22/wg21/docs/lwg-defects.html#225

It's usually a good idea to prevent functions from being considered for
overload resolution when the passed argument types aren't appropriate.  The
library already does this when the required ``graph`` parameter is not
supplied, but we're not likely to see a depth first search that doesn't take a
graph to operate on.  Suppose, instead, that we found a different depth first
search algorithm that could work on graphs that don't model
|IncidenceGraph|_?  If we just added a simple overload, it would be
ambiguous::

    // new overload
    BOOST_PARAMETER_FUNCTION((void), depth_first_search, (tag),
        (required (graph,*))( … )
    )
    {
        // new algorithm implementation
    }

    …

    // ambiguous!
    depth_first_search(boost::adjacency_list<>(), 2, "hello");

.. @ignore()

Predicate Requirements
......................

We really don't want the compiler to consider the original version of
``depth_first_search`` because the ``root_vertex`` argument, ``"hello"``,
doesn't meet the requirement__ that it match the ``graph`` parameter's vertex
descriptor type.  Instead, this call should just invoke our new overload.  To
take the original ``depth_first_search`` overload out of contention, we first
encode this requirement as follows:

__ `parameter table`_

.. parsed-literal::

    struct vertex_descriptor_predicate
    {
        template <typename T, typename Args>
        struct apply
          : boost::mpl::if_<
                boost::is_convertible<
                    T
                  , typename boost::graph_traits<
                        typename boost::parameter::value_type<
                            Args
                          , graphs::graph
                        >::type
                    >::vertex_descriptor
                >
              , boost::mpl::true\_
              , boost::mpl::false\_
            >
        {
        };
    };

This encoding is an `MPL Binary Metafunction Class`__, a type with a nested
``apply`` metafunction that takes in two template arguments.  For the first
template argument, Boost.Parameter will pass in the type on which we will
impose the requirement.  For the second template argument, Boost.Parameter
will pass in the entire argument pack, making it possible to extract the
type of each of the other ``depth_first_search`` parameters via the
``value_type`` metafunction and the corresponding keyword tag type.  The
result ``type`` of the ``apply`` metafunction will be equivalent to
``boost::mpl::true_`` if ``T`` fulfills our requirement as imposed by the
``boost::is_convertible`` statement; otherwise, the result will be
equivalent to ``boost::mpl::false_``.

__ ../../../mpl/doc/refmanual/metafunction-class.html

At this point, we can append the name of our metafunction class, in
parentheses, to the appropriate ``*`` element of the function signature.

.. parsed-literal::

    (root_vertex
      , \*(**vertex_descriptor_predicate**)
      , \*vertices(graph).first
    )

.. @ignore()

Now the original ``depth_first_search`` will only be called when the
``root_vertex`` argument can be converted to the graph's vertex descriptor
type, and our example that *was* ambiguous will smoothly call the new
overload.

We can encode the requirements on other arguments using the same concept; only
the implementation of the nested ``apply`` metafunction needs to be tweaked
for each argument.  There's no space to give a complete description of graph
library details here, but suffice it to show that the next few metafunction
classes provide the necessary checks.

.. parsed-literal::

    struct graph_predicate
    {
        template <typename T, typename Args>
        struct apply
          : boost::mpl::eval_if<
                boost::is_convertible<
                    typename boost::graph_traits<T>::traversal_category
                  , boost::incidence_graph_tag
                >
              , boost::mpl::if_<
                    boost::is_convertible<
                        typename boost::graph_traits<T>::traversal_category
                      , boost::vertex_list_graph_tag
                    >
                  , boost::mpl::true\_
                  , boost::mpl::false\_
                >
            >
        {
        };
    };

    struct index_map_predicate
    {
        template <typename T, typename Args>
        struct apply
          : boost::mpl::eval_if<
                boost::is_integral<
                    typename boost::property_traits<T>::value_type
                >
              , boost::mpl::if_<
                    boost::is_same<
                        typename boost::property_traits<T>::key_type
                      , typename boost::graph_traits<
                            typename boost::parameter::value_type<
                                Args
                              , graphs::graph
                            >::type
                        >::vertex_descriptor
                    >
                  , boost::mpl::true\_
                  , boost::mpl::false\_
                >
            >
        {
        };
    };

    struct color_map_predicate
    {
        template <typename T, typename Args>
        struct apply
          : boost::mpl::if_<
                boost::is_same<
                    typename boost::property_traits<T>::key_type
                  , typename boost::graph_traits<
                        typename boost::parameter::value_type<
                            Args
                          , graphs::graph
                        >::type
                    >::vertex_descriptor
                >
              , boost::mpl::true\_
              , boost::mpl::false\_
            >
        {
        };
    };

Likewise, computing the default value for the ``color_map`` parameter is no
trivial matter, so it's best to factor the computation out to a separate
function template.

.. parsed-literal::

    template <typename Size, typename IndexMap>
    boost::iterator_property_map<
        std::vector<boost::default_color_type>::iterator
      , IndexMap
      , boost::default_color_type
      , boost::default_color_type&
    >&
        default_color_map(Size num_vertices, IndexMap const& index_map)
    {
        static std::vector<boost::default_color_type> colors(num_vertices);
        static boost::iterator_property_map<
            std::vector<boost::default_color_type>::iterator
          , IndexMap
          , boost::default_color_type
          , boost::default_color_type&
        > m(colors.begin(), index_map);
        return m;
    }

The signature encloses each predicate metafunction in parentheses *preceded
by an asterix*, as follows:

.. parsed-literal::

    BOOST_PARAMETER_FUNCTION((void), depth_first_search, graphs,
    (required
        (graph, \*(**graph_predicate**))
    )
    (optional
        (visitor
          , \*  // not easily checkable
          , boost::dfs_visitor<>()
        )
        (root_vertex
          , (**vertex_descriptor_predicate**)
          , \*vertices(graph).first
        )
        (index_map
          , \*(**index_map_predicate**)
          , get(boost::vertex_index, graph)
        )
        (color_map
          , \*(**color_map_predicate**)
          , default_color_map(num_vertices(graph), index_map)
        )
    )
    )

.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <boost/graph/adjacency_list.hpp>
    #include <boost/graph/depth_first_search.hpp>
    #include <boost/graph/graph_traits.hpp>
    #include <boost/property_map/property_map.hpp>
    #include <boost/mpl/and.hpp>
    #include <boost/type_traits/is_convertible.hpp>
    #include <boost/type_traits/is_integral.hpp>
    #include <boost/type_traits/is_same.hpp>
    #include <vector>
    #include <utility>

    BOOST_PARAMETER_NAME((_graph, graphs) graph)
    BOOST_PARAMETER_NAME((_visitor, graphs) visitor)
    BOOST_PARAMETER_NAME((_root_vertex, graphs) in(root_vertex))
    BOOST_PARAMETER_NAME((_index_map, graphs) in(index_map))
    BOOST_PARAMETER_NAME((_color_map, graphs) in_out(color_map))
    ''')

.. @example.append('''
    {
    }

    #include <boost/core/lightweight_test.hpp>
    #include <boost/graph/adjacency_list.hpp>
    #include <utility>

    int main()
    {
        typedef boost::adjacency_list<
            boost::vecS, boost::vecS, boost::directedS
        > G;
        enum {u, v, w, x, y, z, N};
        typedef std::pair<int, int> E;
        E edges[] = {
            E(u, v), E(u, x), E(x, v), E(y, x),
            E(v, y), E(w, y), E(w,z), E(z, z)
        };
        G g(edges, edges + sizeof(edges) / sizeof(E), N);

        depth_first_search(g);
        depth_first_search(g, _root_vertex = static_cast<int>(x));
        return boost::report_errors();
    }
    ''')

.. @test('run')

It usually isn't necessary to so completely encode the type requirements on
arguments to generic functions.  However, doing so is worth the effort: your
code will be more self-documenting and will often provide a better user
experience.  You'll also have an easier transition to the C++20 standard with
`language support for constraints and concepts`__.

__ `ConceptsTS`_

More on Type Requirements
.........................

Encoding type requirements onto a function's parameters is essential for
enabling the function to have deduced parameter interface.  Let's revisit the
``new_window`` example for a moment:

.. parsed-literal::

    window\* w = new_window(
        movable_=false
      , "alert box"
    );
    window\* w = new_window(
        "alert box"
      , movable_=false
    );

.. @ignore()

The goal this time is to be able to invoke the ``new_window`` function without
specifying the keywords.  For each parameter that has a required type, we can
enclose that type in parentheses, then *replace* the ``*`` element of the
parameter signature:

.. parsed-literal::

    BOOST_PARAMETER_NAME((name\_, keywords) name)
    BOOST_PARAMETER_NAME((movable\_, keywords) movable)

    BOOST_PARAMETER_FUNCTION((window\*), new_window, keywords,
        (deduced
            (required
                (name, *(char const\*)*)
                (movable, *(bool)*)
            )
        )
    )
    {
        // ...
    }

.. @ignore()

The following statements will now work and are equivalent to each other as
well as the previous statements:

.. parsed-literal::

    window\* w = new_window(false, "alert box");
    window\* w = new_window("alert box", false);

.. @ignore()

Deduced Parameters
------------------

To further illustrate deduced parameter support, consider the example of the
|def|_ function from Boost.Python_.  Its signature is roughly as follows:

.. parsed-literal::

    template <
        typename Function
      , typename KeywordExpression
      , typename CallPolicies
    >
    void def(
        // Required parameters
        char const\* name, Function func

        // Optional, deduced parameters
      , char const\* docstring = ""
      , KeywordExpression keywords = no_keywords()
      , CallPolicies policies = default_call_policies()
    );

.. @ignore()

Try not to be too distracted by the use of the term “keywords” in this
example: although it means something analogous in Boost.Python to what
it means in the Parameter library, for the purposes of this exercise
you can think of it as being completely different.

When calling ``def``, only two arguments are required.  The association
between any additional arguments and their parameters can be determined by the
types of the arguments actually passed, so the caller is neither required to
remember argument positions or explicitly specify parameter names for those
arguments.  To generate this interface using ``BOOST_PARAMETER_FUNCTION``, we
need only enclose the deduced parameters in a ``(deduced …)`` clause, as
follows: 

.. parsed-literal::

    char const*& blank_char_ptr()
    {
        static char const* larr = "";
        return larr;
    }

    BOOST_PARAMETER_FUNCTION(
        (bool), def, tag,

        (required (name, (char const\*)) (func,\*) )  // nondeduced

        **(deduced**
            (optional 
                (docstring, (char const\*), "")

                (keywords
                    // see [#is_keyword_expression]_
                  , \*(is_keyword_expression<boost::mpl::_>)
                  , no_keywords()
                )

                (policies
                  , \*(
                        boost::mpl::eval_if<
                            boost::is_convertible<boost::mpl::_,char const\*>
                          , boost::mpl::false\_
                          , boost::mpl::if_<
                                // see [#is_keyword_expression]_
                                is_keyword_expression<boost::mpl::_>
                              , boost::mpl::false\_
                              , boost::mpl::true\_
                            >
                        >
                    )
                  , default_call_policies()
                )
            )
        **)**
    )
    {
        *…*
    }

.. @example.replace_emphasis('return true;')

.. @example.prepend('''
    #include <boost/parameter.hpp>

    BOOST_PARAMETER_NAME(name)
    BOOST_PARAMETER_NAME(func)
    BOOST_PARAMETER_NAME(docstring)
    BOOST_PARAMETER_NAME(keywords)
    BOOST_PARAMETER_NAME(policies)

    struct default_call_policies
    {
    };

    struct no_keywords
    {
    };

    struct keywords
    {
    };

    template <typename T>
    struct is_keyword_expression
      : boost::mpl::false_
    {
    };

    template <>
    struct is_keyword_expression<keywords>
      : boost::mpl::true_
    {
    };

    default_call_policies some_policies;

    void f()
    {
    }

    #include <boost/mpl/placeholders.hpp>
    #include <boost/mpl/if.hpp>
    #include <boost/mpl/eval_if.hpp>
    #include <boost/type_traits/is_convertible.hpp>

    ''')

.. Admonition:: Syntax Note

    A ``(deduced …)`` clause always contains a ``(required …)`` and/or an
    ``(optional …)`` subclause, and must follow any ``(required …)`` or
    ``(optional …)`` clauses indicating nondeduced parameters at the outer
    level.

With the declaration above, the following two calls are equivalent:

.. parsed-literal::

    char const\* f_name = "f";
    def(
        f_name
      , &f
      , **some_policies**
      , **"Documentation for f"**
    );
    def(
        f_name
      , &f
      , **"Documentation for f"**
      , **some_policies**
    );

.. @example.prepend('''
    int main()
    {
    ''')

If the user wants to pass a ``policies`` argument that was also, for some
reason, convertible to ``char const*``, she can always specify the parameter
name explicitly, as follows:

.. parsed-literal::

    def(
        f_name
      , &f
      , **_policies = some_policies**
      , "Documentation for f"
    );

.. @example.append('}')
.. @test('compile', howmany='all')

The |deduced_cpp|_ and |deduced_dependent_predicate|_ test programs
demonstrate additional usage of deduced parameter support.

.. _Boost.Python: ../../../python/doc/index.html
.. |def| replace:: ``def``
.. _def: ../../../python/doc/v2/def.html
.. |deduced_cpp| replace:: deduced.cpp
.. _deduced_cpp: ../../test/deduced.cpp
.. |deduced_dependent_predicate| replace:: deduced_dependent_predicate.cpp
.. _deduced_dependent_predicate: ../../test/deduced_dependent_predicate.cpp

Parameter-Dependent Return Types
--------------------------------

For some algorithms, the return type depends on at least one of the argument
types.  However, there is one gotcha to avoid when encoding this return type
while using ``BOOST_PARAMETER_FUNCTION`` or other code generation macros.  As
an example, given the following definitions::

    BOOST_PARAMETER_NAME(x)
    BOOST_PARAMETER_NAME(y)
    BOOST_PARAMETER_NAME(z)

.. @ignore()

Let our algorithm simply return one of its arguments.  If we naïvely implement
its return type in terms of ``parameter::value_type``::

    BOOST_PARAMETER_FUNCTION(
        (typename parameter::value_type<Args,tag::y>::type), return_y, tag,
        (deduced
            (required
                (x, (std::map<char const*,std::string>))
                (y, (char const*))
            )
            (optional
                (z, (int), 4)
            )
        )
    )
    {
        return y;
    }

.. @ignore()

Then using ``return_y`` in any manner other than with positional arguments
will result in a compiler error::

    std::map<char const*,std::string> k2s;
    assert("foo" == return_y(2, k2s, "foo"));  // error!

.. @ignore()

The problem is that even though ``y`` is a required parameter,
``BOOST_PARAMETER_FUNCTION`` will generate certain overloads for which the
argument pack type ``Args`` does not actually contain the keyword tag type
``tag::y``.  The solution is to use SFINAE to preclude generation of those
overloads.  Since ``parameter::value_type`` is a metafunction, our tool for
the job is ``lazy_enable_if``::

    BOOST_PARAMETER_FUNCTION(
        (
            // Whenever using 'enable_if', 'disable_if', and so on,
            // do not add the 'typename' keyword in front.
            boost::lazy_enable_if<
                typename mpl::has_key<Args,tag::y>::type
              , parameter::value_type<Args,tag::y>
            >
            // Whenever using 'enable_if', 'disable_if', and so on,
            // do not add '::type' here.
        ), return_y, tag,
        (deduced
            (required
                (x, (std::map<char const*,std::string>))
                (y, (char const*))
            )
            (optional
                (z, (int), 4)
            )
        )
    )
    {
        return y;
    }

.. @ignore()

For a working demonstration, see |preprocessor_deduced_cpp|_.

.. |preprocessor_deduced_cpp| replace:: preprocessor_deduced.cpp
.. _preprocessor_deduced_cpp: ../../test/preprocessor_deduced.cpp

----------------------------------
Parameter-Enabled Member Functions
----------------------------------

The ``BOOST_PARAMETER_MEMBER_FUNCTION`` and
``BOOST_PARAMETER_CONST_MEMBER_FUNCTION`` macros accept exactly the same
arguments as ``BOOST_PARAMETER_FUNCTION``, but are designed to be used within
the body of a class::

    BOOST_PARAMETER_NAME(arg1)
    BOOST_PARAMETER_NAME(arg2)

    struct callable2
    {
        BOOST_PARAMETER_CONST_MEMBER_FUNCTION(
            (void), call, tag, (required (arg1,(int))(arg2,(int)))
        )
        {
            std::cout << arg1 << ", " << arg2;
            std::cout << std::endl;
        }
    };

    #include <boost/core/lightweight_test.hpp>

    int main()
    {
        callable2 c2;
        callable2 const& c2_const = c2;
        c2_const.call(1, 2);
        return boost::report_errors();
    }

.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <iostream>
    using namespace boost::parameter;
    ''')

.. @test('run')

These macros don't directly allow a function's interface to be separated from
its implementation, but you can always forward arguments on to a separate
implementation function::

    struct callable2
    {
        BOOST_PARAMETER_CONST_MEMBER_FUNCTION(
            (void), call, tag, (required (arg1,(int))(arg2,(int)))
        )
        {
            call_impl(arg1, arg2);
        }

     private:
        void call_impl(int, int);  // implemented elsewhere.
    };

.. @example.prepend('''
    #include <boost/parameter.hpp>

    BOOST_PARAMETER_NAME(arg1)
    BOOST_PARAMETER_NAME(arg2)
    using namespace boost::parameter;
    ''')

.. @test('compile')

Static Member Functions
=======================

To expose a static member function, simply insert the keyword “``static``”
before the function name:

.. parsed-literal::

    BOOST_PARAMETER_NAME(arg1)

    struct somebody
    {
        BOOST_PARAMETER_MEMBER_FUNCTION(
            (void), **static** f, tag, (optional (arg1,(int),0))
        )
        {
            std::cout << arg1 << std::endl;
        }
    };

    #include <boost/core/lightweight_test.hpp>

    int main()
    {
        somebody::f();
        somebody::f(4);
        return boost::report_errors();
    }

.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <iostream>
    using namespace boost::parameter;
    ''')

.. @test('run')

-----------------------------------------
Parameter-Enabled Function Call Operators
-----------------------------------------

The ``BOOST_PARAMETER_FUNCTION_CALL_OPERATOR`` and
``BOOST_PARAMETER_CONST_FUNCTION_CALL_OPERATOR`` macros accept the same
arguments as the ``BOOST_PARAMETER_MEMBER_FUNCTION`` and
``BOOST_PARAMETER_CONST_MEMBER_FUNCTION`` macros except for the function name,
because these macros allow instances of the enclosing classes to be treated as
function objects::

    BOOST_PARAMETER_NAME(first_arg)
    BOOST_PARAMETER_NAME(second_arg)

    struct callable2
    {
        BOOST_PARAMETER_CONST_FUNCTION_CALL_OPERATOR(
            (void), tag, (required (first_arg,(int))(second_arg,(int)))
        )
        {
            std::cout << first_arg << ", ";
            std::cout << second_arg << std::endl;
        }
    };

    #include <boost/core/lightweight_test.hpp>

    int main()
    {
        callable2 c2;
        callable2 const& c2_const = c2;
        c2_const(1, 2);
        return boost::report_errors();
    }

.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <iostream>
    using namespace boost::parameter;
    ''')

.. @test('run')

------------------------------
Parameter-Enabled Constructors
------------------------------

The lack of a “delegating constructor” feature in C++
(http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n1986.pdf)
limits somewhat the quality of interface this library can provide
for defining parameter-enabled constructors.  The usual workaround
for a lack of constructor delegation applies: one must factor the
common logic into one or more base classes.  

Let's build a parameter-enabled constructor that simply prints its
arguments.  The first step is to write a base class whose
constructor accepts a single argument known as an |ArgumentPack|_:
a bundle of references to the actual arguments, tagged with their
keywords.  The values of the actual arguments are extracted from
the |ArgumentPack| by *indexing* it with keyword objects::

    BOOST_PARAMETER_NAME(name)
    BOOST_PARAMETER_NAME(index)

    struct myclass_impl
    {
        template <typename ArgumentPack>
        myclass_impl(ArgumentPack const& args)
        {
            std::cout << "name = " << args[_name];
            std::cout << "; index = " << args[_index | 42];
            std::cout << std::endl;
        }
    };

.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <iostream>
    ''')

Note that the bitwise or (“\ ``|``\ ”) operator has a special meaning when
applied to keyword objects that are passed to an |ArgumentPack|\ 's indexing
operator: it is used to indicate a default value.  In this case if there is no
``index`` parameter in the |ArgumentPack|, ``42`` will be used instead.

Now we are ready to write the parameter-enabled constructor interface::

    struct myclass : myclass_impl
    {
        BOOST_PARAMETER_CONSTRUCTOR(
            myclass, (myclass_impl), tag
          , (required (name,*)) (optional (index,*))
        ) // no semicolon
    };

Since we have supplied a default value for ``index`` but not for ``name``,
only ``name`` is required.  We can exercise our new interface as follows::

    myclass x("bob", 3);                      // positional
    myclass y(_index = 12, _name = "sally");  // named
    myclass z("june");                        // positional/defaulted

.. @example.wrap('''
    #include <boost/core/lightweight_test.hpp>

    int main() {
    ''', ' return boost::report_errors(); }')
.. @test('run', howmany='all')

For more on |ArgumentPack| manipulation, see the `Advanced Topics`_ section.

---------------------------------
Parameter-Enabled Class Templates
---------------------------------

In this section we'll use Boost.Parameter to build Boost.Python_\
's `class_`_ template, whose “signature” is:

.. parsed-literal::

    template <
        ValueType, BaseList = bases<>
      , HeldType = ValueType, Copyable = void
    >
    class class\_;

.. @ignore()

Only the first argument, ``ValueType``, is required.

.. _class_: http://www.boost.org/libs/python/doc/v2/class.html#class_-spec

Named Template Parameters
=========================

First, we'll build an interface that allows users to pass arguments
positionally or by name:

.. parsed-literal::

    struct B
    {
        virtual ~B() = 0;
    };
    
    struct D : B
    {
        ~D();
    };

    class_<
        **class_type<B>**
      , **copyable<boost::noncopyable>** 
    > …;

    class_<
        **D**
      , **held_type<std::auto_ptr<D> >**
      , **base_list<bases<B> >**
    > …;

.. @ignore()

Template Keywords
-----------------

The first step is to define keywords for each template parameter::

    namespace boost { namespace python {

        BOOST_PARAMETER_TEMPLATE_KEYWORD(class_type)
        BOOST_PARAMETER_TEMPLATE_KEYWORD(base_list)
        BOOST_PARAMETER_TEMPLATE_KEYWORD(held_type)
        BOOST_PARAMETER_TEMPLATE_KEYWORD(copyable)
    }}

.. @example.prepend('#include <boost/parameter.hpp>')
.. @test('compile')

The declaration of the ``class_type`` keyword you see here is equivalent to::

    namespace boost { namespace python {
        namespace tag {

            struct class_type;  // keyword tag type
        }

        template <typename T>
        struct class_type
          : parameter::template_keyword<tag::class_type,T>
        {
        };
    }}

.. @example.prepend('#include <boost/parameter.hpp>')
.. @test('compile')

It defines a keyword tag type named ``tag::class_type`` and a
*parameter passing template* named ``class_type``.

Class Template Skeleton
-----------------------

The next step is to define the skeleton of our class template, which has three
optional parameters.  Because the user may pass arguments in any order, we
don't know the actual identities of these parameters, so it would be premature
to use descriptive names or write out the actual default values for any of
them.  Instead, we'll give them generic names and use the special type
``boost::parameter::void_`` as a default:

.. parsed-literal::

    namespace boost { namespace python {

        template <
            typename A0
          , typename A1 = boost::parameter::void\_
          , typename A2 = boost::parameter::void\_
          , typename A3 = boost::parameter::void\_
        >
        struct class\_
        {
            *…*
        };
    }}

.. @example.prepend('#include <boost/parameter.hpp>')
.. @example.replace_emphasis('')
.. @test('compile')

Class Template Signatures
-------------------------

Next, we need to build a type, known as a |ParameterSpec|_, describing the
“signature” of ``boost::python::class_``.  A |ParameterSpec|_ enumerates the
required and optional parameters in their positional order, along with any
type requirements (note that it does *not* specify defaults -- those will be
dealt with separately)::

    namespace boost { namespace python {

        using boost::mpl::_;

        typedef parameter::parameters<
            required<tag::class_type, boost::is_class<_> >
          , parameter::optional<tag::base_list, mpl::is_sequence<_> >
          , parameter::optional<tag::held_type>
          , parameter::optional<tag::copyable>
        > class_signature;
    }}

.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <boost/mpl/is_sequence.hpp>
    #include <boost/noncopyable.hpp>
    #include <boost/type_traits/is_class.hpp>
    #include <memory>

    using namespace boost::parameter;

    namespace boost { namespace python {

        BOOST_PARAMETER_TEMPLATE_KEYWORD(class_type)
        BOOST_PARAMETER_TEMPLATE_KEYWORD(base_list)
        BOOST_PARAMETER_TEMPLATE_KEYWORD(held_type)
        BOOST_PARAMETER_TEMPLATE_KEYWORD(copyable)

        template <typename B = int>
        struct bases
        {
        };
    }}
    ''')

.. |ParameterSpec| replace:: :concept:`ParameterSpec`

.. _ParameterSpec: reference.html#parameterspec

.. _binding_intro:

Argument Packs and Parameter Extraction
---------------------------------------

Next, within the body of ``class_`` , we use the |ParameterSpec|\ 's
nested ``::bind< … >`` template to bundle the actual arguments into an
|ArgumentPack|_ type, and then use the library's ``value_type< … >``
metafunction to extract “logical parameters”.  ``value_type< … >`` is
a lot like ``binding< … >``, but no reference is added to the actual
argument type.  Note that defaults are specified by passing it an
optional third argument::

    namespace boost { namespace python {

        template <
            typename A0
          , typename A1 = boost::parameter::void_
          , typename A2 = boost::parameter::void_
          , typename A3 = boost::parameter::void_
        >
        struct class_
        {
            // Create ArgumentPack
            typedef typename class_signature::template bind<
                A0, A1, A2, A3
            >::type args;

            // Extract first logical parameter.
            typedef typename parameter::value_type<
                args, tag::class_type
            >::type class_type;

            typedef typename parameter::value_type<
                args, tag::base_list, bases<>
            >::type base_list;

            typedef typename parameter::value_type<
                args, tag::held_type, class_type
            >::type held_type;

            typedef typename parameter::value_type<
                args, tag::copyable, void
            >::type copyable;
        };
    }}

.. |ArgumentPack| replace:: :concept:`ArgumentPack`
.. _ArgumentPack: reference.html#argumentpack

Exercising the Code So Far
==========================

.. compound::

    Revisiting our original examples, ::

        typedef boost::python::class_<
            class_type<B>, copyable<boost::noncopyable>
        > c1;

        typedef boost::python::class_<
            D
          , held_type<std::auto_ptr<D> >
          , base_list<bases<B> >
        > c2;

    .. @example.prepend('''
        using boost::python::class_type;
        using boost::python::copyable;
        using boost::python::held_type;
        using boost::python::base_list;
        using boost::python::bases;

        struct B
        {
        };

        struct D
        {
        };
        ''')

    we can now examine the intended parameters::

        BOOST_MPL_ASSERT((boost::is_same<c1::class_type, B>));
        BOOST_MPL_ASSERT((boost::is_same<c1::base_list, bases<> >));
        BOOST_MPL_ASSERT((boost::is_same<c1::held_type, B>));
        BOOST_MPL_ASSERT((
            boost::is_same<c1::copyable, boost::noncopyable>
        ));

        BOOST_MPL_ASSERT((boost::is_same<c2::class_type, D>));
        BOOST_MPL_ASSERT((boost::is_same<c2::base_list, bases<B> >));
        BOOST_MPL_ASSERT((
            boost::is_same<c2::held_type, std::auto_ptr<D> >
        ));
        BOOST_MPL_ASSERT((boost::is_same<c2::copyable, void>));

.. @test('compile', howmany='all')

Deduced Template Parameters
===========================

To apply a deduced parameter interface here, we need only make the type
requirements a bit tighter so the ``held_type`` and ``copyable`` parameters
can be crisply distinguished from the others.  Boost.Python_ does this by
requiring that ``base_list`` be a specialization of its ``bases< … >``
template (as opposed to being any old MPL sequence) and by requiring that
``copyable``, if explicitly supplied, be ``boost::noncopyable``.  One easy way
of identifying specializations of ``bases< … >`` is to derive them all from
the same class, as an implementation detail:

.. parsed-literal::

    namespace boost { namespace python {
        namespace detail {

            struct bases_base
            {
            };
        }

        template <
            typename A0 = void, typename A1 = void, typename A2 = void *…*
        >
        struct bases **: detail::bases_base**
        {
        };
    }}  

.. @example.replace_emphasis('')
.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <boost/mpl/is_sequence.hpp>
    #include <boost/noncopyable.hpp>
    #include <memory>

    using namespace boost::parameter;
    using boost::mpl::_;

    namespace boost { namespace python {

        BOOST_PARAMETER_TEMPLATE_KEYWORD(class_type)
        BOOST_PARAMETER_TEMPLATE_KEYWORD(base_list)
        BOOST_PARAMETER_TEMPLATE_KEYWORD(held_type)
        BOOST_PARAMETER_TEMPLATE_KEYWORD(copyable)
    }}
    ''')

Now we can rewrite our signature to make all three optional parameters
deducible::

    typedef parameter::parameters<
        required<tag::class_type, is_class<_> >

      , parameter::optional<
            deduced<tag::base_list>
          , is_base_and_derived<detail::bases_base,_>
        >

      , parameter::optional<
            deduced<tag::held_type>
          , mpl::not_<
                mpl::or_<
                    is_base_and_derived<detail::bases_base,_>
                  , is_same<noncopyable,_>
                >
            >
        >

      , parameter::optional<
            deduced<tag::copyable>
          , is_same<noncopyable,_>
        >

    > class_signature;

.. @example.prepend('''
    #include <boost/type_traits/is_class.hpp>
    namespace boost { namespace python {
    ''')

.. @example.append('''
        template <
            typename A0
          , typename A1 = boost::parameter::void_
          , typename A2 = boost::parameter::void_
          , typename A3 = boost::parameter::void_
        >
        struct class_
        {
            // Create ArgumentPack
            typedef typename class_signature::bind<
                A0, A1, A2, A3
            >::type args;

            // Extract first logical parameter.
            typedef typename parameter::value_type<
                args, tag::class_type
            >::type class_type;

            typedef typename parameter::value_type<
                args, tag::base_list, bases<>
            >::type base_list;

            typedef typename parameter::value_type<
                args, tag::held_type, class_type
            >::type held_type;

            typedef typename parameter::value_type<
                args, tag::copyable, void
            >::type copyable;
        };
    }}
    ''')

It may seem like we've added a great deal of complexity, but the benefits to
our users are greater.  Our original examples can now be written without
explicit parameter names:

.. parsed-literal::

    typedef boost::python::class_<**B**, **boost::noncopyable**> c1;

    typedef boost::python::class_<
        **D**, **std::auto_ptr<D>**, **bases<B>**
    > c2;

.. @example.prepend('''
    struct B
    {
    };

    struct D
    {
    };

    using boost::python::bases;
    ''')

.. @example.append('''
    BOOST_MPL_ASSERT((boost::is_same<c1::class_type, B>));
    BOOST_MPL_ASSERT((boost::is_same<c1::base_list, bases<> >));
    BOOST_MPL_ASSERT((boost::is_same<c1::held_type, B>));
    BOOST_MPL_ASSERT((
        boost::is_same<c1::copyable, boost::noncopyable>
    ));

    BOOST_MPL_ASSERT((boost::is_same<c2::class_type, D>));
    BOOST_MPL_ASSERT((boost::is_same<c2::base_list, bases<B> >));
    BOOST_MPL_ASSERT((
        boost::is_same<c2::held_type, std::auto_ptr<D> >
    ));
    BOOST_MPL_ASSERT((boost::is_same<c2::copyable, void>));
    ''')

.. @test('compile', howmany='all')

===============
Advanced Topics
===============

At this point, you should have a good grasp of the basics.  In this section
we'll cover some more esoteric uses of the library.

-------------------------
Fine-Grained Name Control
-------------------------

If you don't like the leading-underscore naming convention used to refer to
keyword objects, or you need the name ``tag`` for something other than the
keyword type namespace, there's another way to use ``BOOST_PARAMETER_NAME``:

.. parsed-literal::

    BOOST_PARAMETER_NAME(
        **(**
            *object-name*
          **,** *tag-namespace*
        **)** *parameter-name*
    )

.. @ignore()

Here is a usage example:

.. parsed-literal::

    BOOST_PARAMETER_NAME(
        (
            **pass_foo**, **keywords**
        ) **foo**
    )

    BOOST_PARAMETER_FUNCTION(
        (int), f, 
        **keywords**, (required (**foo**, \*))
    )
    {
        return **foo** + 1;
    }

    int x = f(**pass_foo** = 41);

.. @example.prepend('#include <boost/parameter.hpp>')
.. @example.append('''
    int main()
    {
        return 0;
    }
    ''')
.. @test('run')

Before you use this more verbose form, however, please read the section on
`best practices for keyword object naming`__.

__ `Keyword Naming`_

----------------------
More |ArgumentPack|\ s
----------------------

We've already seen |ArgumentPack|\ s when we looked at
`parameter-enabled constructors`_ and `class templates`__.  As you
might have guessed, |ArgumentPack|\ s actually lie at the heart of
everything this library does; in this section we'll examine ways to
build and manipulate them more effectively.

__ binding_intro_

Building |ArgumentPack|\ s
==========================

The simplest |ArgumentPack| is the result of assigning into a keyword object::

    BOOST_PARAMETER_NAME(index)

    template <typename ArgumentPack>
    int print_index(ArgumentPack const& args)
    {
        std::cout << "index = " << args[_index];
        std::cout << std::endl;
        return 0;
    }

    int x = print_index(_index = 3);  // prints "index = 3"

.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <iostream>
    ''')

Also, |ArgumentPack|\ s can be composed using the comma operator.  The extra
parentheses below are used to prevent the compiler from seeing two separate
arguments to ``print_name_and_index``::

    BOOST_PARAMETER_NAME(name)

    template <typename ArgumentPack>
    int print_name_and_index(ArgumentPack const& args)
    {
        std::cout << "name = " << args[_name];
        std::cout << "; ";
        return print_index(args);
    }

    int y = print_name_and_index((_index = 3, _name = "jones"));

The |compose_cpp|_ test program shows more examples of this feature.

To build an |ArgumentPack| with positional arguments, we can use a
|ParameterSpec|_.  As introduced described in the section on `Class Template
Signatures`_, a |ParameterSpec| describes the positional order of parameters
and any associated type requirements.  Just as we can build an |ArgumentPack|
*type* with its nested ``::bind< … >`` template, we can build an
|ArgumentPack| *object* by invoking its function call operator:

.. parsed-literal::

    parameter::parameters<
        required<tag::\ name, is_convertible<_,char const*> >
      , optional<tag::\ index, is_convertible<_,int> >
    > spec;

    char const sam[] = "sam";
    int twelve = 12;

    int z0 = print_name_and_index(
        **spec(** sam, twelve **)**
    );

    int z1 = print_name_and_index( 
        **spec(** _index=12, _name="sam" **)** 
    );

.. @example.prepend('''
    namespace parameter = boost::parameter;
    using parameter::required;
    using parameter::optional;
    using boost::is_convertible;
    using boost::mpl::_;
    ''')

.. @example.append('''
    int main()
    {
        return 0;
    }
    ''')

.. @test('run', howmany='all')

Extracting Parameter Types
==========================

If we want to know the types of the arguments passed to
``print_name_and_index``, we have a couple of options.  The
simplest and least error-prone approach is to forward them to a
function template and allow *it* to do type deduction::

    BOOST_PARAMETER_NAME(name)
    BOOST_PARAMETER_NAME(index)

    template <typename Name, typename Index>
    int deduce_arg_types_impl(Name&& name, Index&& index)
    {
        // we know the types
        Name&& n2 = boost::forward<Name>(name);
        Index&& i2 = boost::forward<Index>(index);
        return index;
    }

    template <typename ArgumentPack>
    int deduce_arg_types(ArgumentPack const& args)
    {
        return deduce_arg_types_impl(args[_name], args[_index | 42]);
    }

.. @example.prepend('''
    #include <boost/parameter.hpp>
    ''')

.. @example.append('''
    #include <boost/core/lightweight_test.hpp>

    int main()
    {
        int a1 = deduce_arg_types((_name = "foo"));
        int a2 = deduce_arg_types((_name = "foo", _index = 3));
        BOOST_TEST_EQ(a1, 42);
        BOOST_TEST_EQ(a2, 3);
        return boost::report_errors();
    }
    ''')

.. @test('run')

Occasionally one needs to deduce argument types without an extra layer of
function call.  For example, suppose we wanted to return twice the value of
the ``index`` parameter?  In that case we can use the ``value_type< … >``
metafunction introduced `earlier`__::

    BOOST_PARAMETER_NAME(index)

    template <typename ArgumentPack>
    typename boost::parameter::value_type<ArgumentPack,tag::index,int>::type
        twice_index(ArgumentPack const& args)
    {
        return 2 * args[_index | 42];
    }

.. @example.prepend('''
    #include <boost/parameter.hpp>
    ''')

.. @example.append('''
    #include <boost/core/lightweight_test.hpp>

    int main()
    {
        int six = twice_index(_index = 3);
        BOOST_TEST_EQ(six, 6);
        return boost::report_errors();
    }
    ''')

.. @test('run', howmany='all')

Note that if we had used ``binding< … >`` rather than ``value_type< … >``, we
would end up returning a reference to the temporary created in the ``2 * …``
expression.

__ binding_intro_

Lazy Default Computation
========================

When a default value is expensive to compute, it would be preferable to avoid
it until we're sure it's absolutely necessary.  ``BOOST_PARAMETER_FUNCTION``
takes care of that problem for us, but when using |ArgumentPack|\ s
explicitly, we need a tool other than ``operator|``::

    BOOST_PARAMETER_NAME(s1)
    BOOST_PARAMETER_NAME(s2)
    BOOST_PARAMETER_NAME(s3)

    template <typename ArgumentPack>
    std::string f(ArgumentPack const& args)
    {
        std::string const& s1 = args[_s1];
        std::string const& s2 = args[_s2];
        typename parameter::binding<
            ArgumentPack,tag::s3,std::string
        >::type s3 = args[_s3 | (s1 + s2)];  // always constructs s1 + s2
        return s3;
    }

    std::string x = f((
        _s1="hello,", _s2=" world", _s3="hi world"
    ));

.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <string>

    namespace parameter = boost::parameter;
    ''')

.. @example.append('''
    int main()
    {
        return 0;
    }
    ''')

.. @test('run')

In the example above, the string ``"hello, world"`` is constructed despite the
fact that the user passed us a value for ``s3``.  To remedy that, we can
compute the default value *lazily* (that is, only on demand), by using
``boost::bind()`` to create a function object.

.. danielw: I'm leaving the text below in the source, because we might
.. want to change back to it after 1.34, and if I remove it now we
.. might forget about it.

.. by combining the logical-or (“``||``”) operator
.. with a function object built by the Boost Lambda_ library: [#bind]_

.. parsed-literal::

    typename parameter::binding<
        ArgumentPack,tag::s3,std::string
    >::type s3 = args[
        _s3 **|| boost::bind(
            std::plus<std::string>(), boost::ref(s1), boost::ref(s2)
        )**
    ];

.. @example.prepend('''
    #include <boost/bind.hpp>
    #include <boost/ref.hpp>
    #include <boost/parameter.hpp>
    #include <string>
    #include <functional>

    namespace parameter = boost::parameter;

    BOOST_PARAMETER_NAME(s1)
    BOOST_PARAMETER_NAME(s2)
    BOOST_PARAMETER_NAME(s3)

    template <typename ArgumentPack>
    std::string f(ArgumentPack const& args)
    {
        std::string const& s1 = args[_s1];
        std::string const& s2 = args[_s2];
    ''')

.. @example.append('''
        return s3;
    }

    std::string x = f((_s1="hello,", _s2=" world", _s3="hi world"));

    int main()
    {
        return 0;
    }
    ''')

.. @test('run')

.. .. _Lambda: ../../../lambda/index.html

.. sidebar:: Mnemonics

    To remember the difference between ``|`` and ``||``, recall that ``||``
    normally uses short-circuit evaluation: its second argument is only
    evaluated if its first argument is ``false``.  Similarly, in
    ``color_map[param || f]``, ``f`` is only invoked if no ``color_map``
    argument was supplied.

The expression ``bind(std::plus<std::string>(), ref(s1), ref(s2))`` yields a
*function object* that, when invoked, adds the two strings together.  That
function will only be invoked if no ``s3`` argument is supplied by the caller.

.. The expression ``lambda::var(s1) + lambda::var(s2)`` yields a
.. *function object* that, when invoked, adds the two strings
.. together.  That function will only be invoked if no ``s3`` argument
.. is supplied by the caller.

==============
Best Practices
==============

By now you should have a fairly good idea of how to use the Parameter
library.  This section points out a few more-marginal issues that will help
you use the library more effectively.

--------------
Keyword Naming
--------------

``BOOST_PARAMETER_NAME`` prepends a leading underscore to the names of all our
keyword objects in order to avoid the following usually-silent bug:

.. parsed-literal::

    namespace people
    {
        namespace tag
        {
            struct name
            {
                typedef boost::parameter::forward_reference qualifier;
            };

            struct age
            {
                typedef boost::parameter::forward_reference qualifier;
            };
        }

        namespace // unnamed
        {
            boost::parameter::keyword<tag::name>& **name**
                = boost::parameter::keyword<tag::name>::instance;
            boost::parameter::keyword<tag::age>& **age**
                = boost::parameter::keyword<tag::age>::instance;
        }

        BOOST_PARAMETER_FUNCTION(
            (void), g, tag, (optional (name, \*, "bob")(age, \*, 42))
        )
        {
            std::cout << name << ":" << age;
        }

        void f(int age)
        {
            :vellipsis:`\ 
            .
            .
            .
            ` 
            g(**age** = 3);  // whoops!
        }
    }

.. @ignore()

Although in the case above, the user was trying to pass the value ``3`` as the
``age`` parameter to ``g``, what happened instead was that ``f``\ 's ``age``
argument got reassigned the value 3, and was then passed as a positional
argument to ``g``.  Since ``g``'s first positional parameter is ``name``, the
default value for ``age`` is used, and g prints ``3:42``.  Our leading
underscore naming convention makes this problem less likely to occur.

In this particular case, the problem could have been detected if f's ``age``
parameter had been made ``const``, which is always a good idea whenever
possible.  Finally, we recommend that you use an enclosing namespace for all
your code, but particularly for names with leading underscores.  If we were to
leave out the ``people`` namespace above, names in the global namespace
beginning with leading underscores—which are reserved to your C++
compiler—might become irretrievably ambiguous with those in our
unnamed namespace.

----------
Namespaces
----------

In our examples we've always declared keyword objects in (an unnamed namespace
within) the same namespace as the Boost.Parameter-enabled functions using
those keywords:

.. parsed-literal::

    namespace lib {

        **BOOST_PARAMETER_NAME(name)
        BOOST_PARAMETER_NAME(index)**

        BOOST_PARAMETER_FUNCTION(
            (int), f, tag, 
            (optional (name,*,"bob")(index,(int),1))
        )
        {
            std::cout << name << ":" << index;
            std::cout << std::endl;
            return index;
        }
    }

.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <iostream>
    ''')
.. @namespace_setup = str(example)
.. @ignore()

Users of these functions have a few choices:

1.  Full qualification:

    .. parsed-literal::

        int x = **lib::**\ f(
            **lib::**\ _name = "jill"
          , **lib::**\ _index = 1
        );

    This approach is more verbose than many users would like.

.. @example.prepend(namespace_setup)
.. @example.append('int main() { return 0; }')
.. @test('run')

2.  Make keyword objects available through *using-declarations*:

    .. parsed-literal::

        **using lib::_name;
        using lib::_index;**

        int x = lib::f(_name = "jill", _index = 1);

    This version is much better at the actual call site, but the
    *using-declarations* themselves can be verbose and hard to manage.

.. @example.prepend(namespace_setup)
.. @example.append('int main() { return 0; }')
.. @test('run')

3.  Bring in the entire namespace with a *using-directive*:

    .. parsed-literal::

        **using namespace lib;**
        int x = **f**\ (_name = "jill", _index = 3);

    This option is convenient, but it indiscriminately makes the *entire*
    contents of ``lib`` available without qualification.

.. @example.prepend(namespace_setup)
.. @example.append('int main() { return 0; }')
.. @test('run')

If we add an additional namespace around keyword declarations, though, we can
give users more control:

.. parsed-literal::

    namespace lib {
        **namespace keywords {**

            BOOST_PARAMETER_NAME(name)
            BOOST_PARAMETER_NAME(index)
        **}**

        BOOST_PARAMETER_FUNCTION(
            (int), f, **keywords::**\ tag,
            (optional (name,*,"bob")(index,(int),1))
        )
        {
            std::cout << name << ":" << index;
            std::cout << std::endl;
            return index;
        }
    }

.. @example.prepend('''
    #include <boost/parameter.hpp>
    #include <iostream>
    ''')

Now users need only a single *using-directive* to bring in just the names of
all keywords associated with ``lib``:

.. parsed-literal::

    **using namespace lib::keywords;**
    int y = lib::f(_name = "bob", _index = 2);

.. @example.append('int main() { return 0; }')
.. @test('run', howmany='all')

-------------
Documentation
-------------

The interface idioms enabled by Boost.Parameter are completely new (to C++),
and as such are not served by pre-existing documentation conventions.  

.. Note:: This space is empty because we haven't settled on any best practices
    yet.  We'd be very pleased to link to your documentation if you've got a
    style that you think is worth sharing.

==========================
Portability Considerations
==========================

Use the `regression test results`_ for the latest Boost release of
the Parameter library to see how it fares on your favorite
compiler.  Additionally, you may need to be aware of the following
issues and workarounds for particular compilers.

.. _`regression test results`: http\://www.boost.org/regression/release/user/parameter.html

--------------------------
Perfect Forwarding Support
--------------------------

If your compiler supports `perfect forwarding`_, then the Parameter library
will ``#define`` the macro ``BOOST_PARAMETER_HAS_PERFECT_FORWARDING`` unless
you disable it manually.  If your compiler does not provide this support, then
``parameter::parameters::operator()`` will treat rvalue references as lvalue
``const`` references to work around the `forwarding problem`_, so in certain
cases you must wrap |boost_ref|_ or |std_ref|_ around any arguments that will
be bound to out parameters.  The |evaluate_category|_ and
|preprocessor_eval_category|_ test programs demonstrate this support.

.. _`perfect forwarding`: http\://www.justsoftwaresolutions.co.uk/cplusplus/rvalue_references_and_perfect_forwarding.html
.. _`forwarding problem`: http\://www.open-std.org/jtc1/sc22/wg21/docs/papers/2002/n1385.htm
.. |boost_ref| replace:: ``boost::ref``
.. _boost_ref: ../../../core/doc/html/core/ref.html
.. |std_ref| replace:: ``std::ref``
.. _std_ref: http://en.cppreference.com/w/cpp/utility/functional/ref
.. |evaluate_category| replace:: evaluate_category.cpp
.. _evaluate_category: ../../test/evaluate_category.cpp
.. |preprocessor_eval_category| replace:: preprocessor_eval_category.cpp
.. _preprocessor_eval_category: ../../test/preprocessor_eval_category.cpp

------------------
Boost.MP11 Support
------------------

If your compiler is sufficiently compliant with the C++11 standard, then the
Parameter library will ``#define`` the macro ``BOOST_PARAMETER_CAN_USE_MP11``
unless you disable it manually.  The |singular_cpp|_, |compose_cpp|_,
|optional_deduced_sfinae_cpp|_, and |deduced_dep_pred_cpp|_ test programs
demonstrate support for `Boost.MP11`_.

.. _`Boost.MP11`: ../../../mp11/doc/html/mp11.html
.. |singular_cpp| replace:: singular.cpp
.. _singular_cpp: ../../test/singular.cpp
.. |optional_deduced_sfinae_cpp| replace:: optional_deduced_sfinae.cpp
.. _optional_deduced_sfinae_cpp: ../../test/optional_deduced_sfinae.cpp
.. |deduced_dep_pred_cpp| replace:: deduced_dependent_predicate.cpp
.. _deduced_dep_pred_cpp: ../../test/deduced_dependent_predicate.cpp

-----------------
No SFINAE Support
-----------------

Some older compilers don't support SFINAE.  If your compiler meets that
criterion, then Boost headers will ``#define`` the preprocessor symbol
``BOOST_NO_SFINAE``, and parameter-enabled functions won't be removed
from the overload set based on their signatures.  The |sfinae_cpp|_ and
|optional_deduced_sfinae|_ test programs demonstrate SFINAE support.

.. |sfinae_cpp| replace:: sfinae.cpp
.. _sfinae_cpp: ../../test/sfinae.cpp
.. |optional_deduced_sfinae| replace:: optional_deduced_sfinae.cpp
.. _optional_deduced_sfinae: ../../test/optional_deduced_sfinae.cpp

---------------------------
No Support for |result_of|_
---------------------------

.. |result_of| replace:: ``result_of``

.. _result_of: ../../../utility/utility.htm#result_of

`Lazy default computation`_ relies on the |result_of| class template to
compute the types of default arguments given the type of the function object
that constructs them.  On compilers that don't support |result_of|,
``BOOST_NO_RESULT_OF`` will be ``#define``\ d, and the compiler will expect
the function object to contain a nested type name, ``result_type``, that
indicates its return type when invoked without arguments.  To use an ordinary
function as a default generator on those compilers, you'll need to wrap it in
a class that provides ``result_type`` as a ``typedef`` and invokes the
function via its ``operator()``.

..
    Can't Declare |ParameterSpec| via ``typedef``
    =============================================

    In principle you can declare a |ParameterSpec| as a ``typedef`` for a
    specialization of ``parameters<…>``, but Microsoft Visual C++ 6.x has been
    seen to choke on that usage.  The workaround is to use inheritance and
    declare your |ParameterSpec| as a class:

    .. parsed-literal::

        **struct dfs_parameters
          :** parameter::parameters<
                tag::graph, tag::visitor, tag::root_vertex
              , tag::index_map, tag::color_map
            >
        **{
        };**

    Default Arguments Unsupported on Nested Templates
    =============================================

    As of this writing, Borland compilers don't support the use of default
    template arguments on member class templates.  As a result, you have to
    supply ``BOOST_PARAMETER_MAX_ARITY`` arguments to every use of
    ``parameters<…>::match``.  Since the actual defaults used are unspecified,
    the workaround is to use |BOOST_PARAMETER_MATCH|_ to declare default
    arguments for SFINAE.

    .. |BOOST_PARAMETER_MATCH| replace:: ``BOOST_PARAMETER_MATCH``

--------------------------------------------------
Compiler Can't See References In Unnamed Namespace
--------------------------------------------------

If you use Microsoft Visual C++ 6.x, you may find that the compiler has
trouble finding your keyword objects.  This problem has been observed, but
only on this one compiler, and it disappeared as the test code evolved, so
we suggest you use it only as a last resort rather than as a preventative
measure.  The solution is to add *using-declarations* to force the names
to be available in the enclosing namespace without qualification::

    namespace graphs {

        using graphs::graph;
        using graphs::visitor;
        using graphs::root_vertex;
        using graphs::index_map;
        using graphs::color_map;
    }

==============
Python Binding
==============

.. _python: python.html

Follow `this link`__ for documentation on how to expose
Boost.Parameter-enabled functions to Python with `Boost.Python`_.

__ ../../../parameter_python/doc/html/index.html

=========
Reference
=========

.. _reference: reference.html

Follow `this link`__ to the Boost.Parameter reference documentation.  

__ reference.html

========
Glossary
========

.. _arguments:

-------------------------------
Argument (or “actual argument”)
-------------------------------

the value actually passed to a function or class template.

.. _parameter:

---------------------------------
Parameter (or “formal parameter”)
---------------------------------

the name used to refer to an argument within a function or class
template.  For example, the value of ``f``'s *parameter* ``x`` is given by the
*argument* ``3``:

.. parsed-literal::

    int f(int x) { return x + 1; }
    int y = f(3);

================
Acknowledgements
================

The authors would like to thank all the Boosters who participated in the
review of this library and its documentation, most especially our review
manager, Doug Gregor.

--------------------------

.. [#old_interface] As of Boost 1.33.0 the Graph library was still using an
    `older named parameter mechanism`__, but there are plans to change it to
    use Boost.Parameter (this library) in an upcoming release, while keeping
    the old interface available for backward-compatibility.  

__ ../../../graph/doc/bgl_named_params.html

.. [#odr] The **One Definition Rule** says that any given entity in a C++
    program must have the same definition in all translation units (object
    files) that make up a program.

.. [#vertex_descriptor] If you're not familiar with the Boost Graph Library,
    don't worry about the meaning of any Graph-library-specific details you
    encounter.  In this case you could replace all mentions of vertex
    descriptor types with ``int`` in the text, and your understanding of the
    Parameter library wouldn't suffer.

.. [#ConceptsTS] This is a major motivation behind `C++20 constraints`_.

.. _`C++20 constraints`: http://en.cppreference.com/w/cpp/language/constraints

.. .. [#bind] The Lambda library is known not to work on `some
.. less-conformant compilers`__.  When using one of those you could
.. use `Boost.Bind`_ to generate the function object\:\:

..     boost\:\:bind(std\:\:plus<std\:\:string>(),s1,s2)

.. [#is_keyword_expression] Here we're assuming there's a predicate
    metafunction ``is_keyword_expression`` that can be used to identify
    models of Boost.Python's KeywordExpression concept.

.. .. __ http://www.boost.org/regression/release/user/lambda.html
.. _Boost.Bind: ../../../bind/index.html

.. [#using] You can always give the illusion that the function
    lives in an outer namespace by applying a *using-declaration*::

        namespace foo_overloads {

            // foo declarations here
            void foo() { ... }
            ...
        }
        using foo_overloads::foo;

    This technique for avoiding unintentional argument-dependent lookup is due
    to Herb Sutter.

.. [#sfinae] This capability depends on your compiler's support for
    SFINAE.  **SFINAE**: **S**\ ubstitution **F**\ ailure **I**\ s **N**\ ot
    **A**\ n **E**\ rror.  If type substitution during the instantiation of a
    function template results in an invalid type, no compilation error is
    emitted; instead the overload is removed from the overload set.  By
    producing an invalid type in the function signature depending on the
    result of some condition, we can decide whether or not an overload is
    considered during overload resolution.  The technique is formalized in the
    |enable_if|_ utility.  Most recent compilers support SFINAE; on compilers
    that don't support it, the Boost config library will ``#define`` the
    symbol ``BOOST_NO_SFINAE``.  See
    http://www.semantics.org/once_weakly/w02_SFINAE.pdf for more information
    on SFINAE.

.. |enable_if| replace:: ``enable_if``
.. _enable_if: ../../../core/doc/html/core/enable_if.html


