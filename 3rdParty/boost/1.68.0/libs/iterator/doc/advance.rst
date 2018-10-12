.. Copyright (C) 2017 Michel Morin.
   Distributed under the Boost Software License, Version 1.0.
   (See accompanying file LICENSE_1_0.txt or copy at
   http://www.boost.org/LICENSE_1_0.txt)

=======
advance
=======

``boost::iterators::advance`` is an adapted version of ``std::advance`` for
the Boost iterator traversal concepts.


Header
------

``<boost/iterator/advance.hpp>``


Synopsis
--------

::

    template <typename Iterator, typename Distance>
    constexpr void advance(Iterator& it, Distance n);


Description
-----------

Moves ``it`` forward by ``n`` increments
(or backward by ``|n|`` decrements if ``n`` is negative).


Requirements
------------

``Iterator`` should model Incrementable Iterator.


Preconditions
-------------

Let ``it``\ :sub:`i` be the iterator obtained by incrementing
(or decrementing if ``n`` is negative) ``it`` by *i*. All the iterators
``it``\ :sub:`i` for *i* = 0, 1, 2, ..., ``|n|`` should be valid.

If ``Iterator`` does not model Bidirectional Traversal Iterator,
``n`` should be non-negative.


Complexity
----------

If ``Iterator`` models Random Access Traversal Iterator, it takes constant time;
otherwise it takes linear time.


Notes
-----

- This function is not a customization point and is protected against
  being found by argument-dependent lookup (ADL).
- This function is ``constexpr`` only in C++14 or later.


--------------------------------------------------------------------------------

| Author: Michel Morin
| Copyright |C| 2017 Michel Morin
| Distributed under the `Boost Software License, Version 1.0
  <http://www.boost.org/LICENSE_1_0.txt>`_.

.. |C| unicode:: U+00A9 .. COPYRIGHT SIGN
