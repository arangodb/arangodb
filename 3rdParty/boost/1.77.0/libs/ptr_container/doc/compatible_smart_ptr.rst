++++++++++++++++++++++++++++++++++
 |Boost| Pointer Container Library
++++++++++++++++++++++++++++++++++
 
.. |Boost| image:: boost.png

Compatible Smart Pointer Type
-----------------------------

When specifying parameter or return types in interfaces, the documentation
for this library uses the pseudo-type

.. parsed-literal::
   *compatible-smart-ptr*\ <T>

to indicate that the compiler C++ standard is being used to
selectively provide or remove interfaces with ``std::auto_ptr<T>`` or
``std::unique_ptr<T>``. The exact meaning varies depending on whether
the smart pointer type is a parameter or a return type.

**Parameter Types:**

An interface such as

.. parsed-literal::
   void container::push_back( *compatible-smart-ptr*\ <T> );

indicates that an overload of ``container::push_back`` is present for
one or both of ``std::auto_ptr<T>``, ``std::unique_ptr<T>``:
Boost.Pointer Container provides an overload for each type supported
by the compiler. To be completely explicit, if the compiler provides
``std::auto_ptr``, then 

.. parsed-literal::
   void container::push_back( std::auto_ptr<T> );

is present. If the compiler provides ``std::unique_ptr``, then

.. parsed-literal::
   void container::push_back( std::unique_ptr<T> );

is present. And if the compiler provides both, both overloads are
present.

In practice this means that C++98/03 users have access to
``std::auto_ptr`` overloads, C++11/14 users have access to
overloads taking both ``std::auto_ptr`` and ``std::unique_ptr``, and
users of C++17 and onwards only have access to ``std::unique_ptr``
overloads.

The convention outlined above implies that in certain cases the
documentation will make reference to a single function taking the
compatible smart pointer pseudo parameter, when in fact two distinct
overloaded functions are present. Of course the actual interface
depends on compiler settings, so for clarity the `class hierarchy
reference <reversible_ptr_container.html>`_ will only ever refer to a
single function.

**Return Types:**

The case of return types is handled along the same lines as parameter
types, subject of course to the restriction that C++ functions cannot
be overloaded by return type. Thus, an interface such as

.. parsed-literal::
   *compatible-smart-ptr*\ <T> container::release( );

means that precisely one of ``std::auto_ptr<T>`` or
``std::unique_ptr<T>`` is used as the return type. If the compiler
provides ``std::auto_ptr<T>``, then

.. parsed-literal::
   std::auto_ptr<T> container::release( );

is present, even if the compiler provides ``std::unique_ptr``. For
compilers that only provide ``std::unique_ptr``, the interface above
becomes

.. parsed-literal::
   std::unique_ptr<T> container::release( );

In practice, this means that for users of C++98/03/11/14, such return
types are always ``std::auto_ptr``; for users of C++17 and onwards the
return type is ``std::unique_ptr``.

**Details:**

The ISO C++11 standard saw the addition of the smart pointer class template
``std::unique_ptr``, and with it the formal deprecation of
``std::auto_ptr``. After spending C++11 and C++14 with deprecated
status, ``std::auto_ptr`` has been formally removed as of
the ISO C++17 standard. As such, headers mentioning
``std::auto_ptr`` may be unusable in standard library
implementations which disable ``std::auto_ptr`` when C++17 or later
is used. Boost.Pointer Container predates the existence of
``std::unique_ptr``, and since Boost v. ``1.34`` it has provided
``std::auto_ptr`` overloads for its interfaces. To provide
compatibility across a range of C++ standards, macros are used for
compile-time overloading or replacement of ``std::auto_ptr`` interfaces with
``std::unique_ptr`` interfaces.

`Boost.Config <../../config/index.html>`_ defines the macro
``BOOST_NO_CXX11_SMART_PTR`` for compilers where
``std::unique_ptr`` is not available, and ``BOOST_NO_AUTO_PTR`` for
compilers where ``std::auto_ptr`` is removed (or is defective). These
macros are used for compile-time selection of interfaces depending on
parameter and return type. For interfaces that take smart pointer
parameters, Boost.Pointer Container uses ``BOOST_NO_AUTO_PTR`` and
``BOOST_NO_CXX11_SMART_PTR`` independently of each other to provide
interfaces taking one or both of ``std::auto_ptr``,
``std::unique_ptr`` as parameters. For interfaces with smart pointer
return types, the Boost.Config macros are used first to check if
``std::auto_ptr`` is available, providing ``std::unique_ptr`` as the
return type only for compilers that provide ``std::unique_ptr`` but
not ``std::auto_ptr``. 

Thus, all mentions of

.. parsed-literal::
   *compatible-smart-ptr*\ <T>

shall be understood to mean that `Boost.Config
<../../config/index.html>`_ has been used as outlined above to provide
a smart pointer interface that is compatible with compiler settings.

**Navigate**

- `home <ptr_container.html>`_
- `reference <reference.html>`_

.. raw:: html 

        <hr>

:Copyright:     Thorsten Ottosen 2004-2006. Use, modification and distribution is subject to the Boost Software License, Version 1.0 (see LICENSE_1_0.txt__).

__ http://www.boost.org/LICENSE_1_0.txt

