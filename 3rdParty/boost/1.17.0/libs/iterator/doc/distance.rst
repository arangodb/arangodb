.. Copyright (C) 2017 Michel Morin.
   Distributed under the Boost Software License, Version 1.0.
   (See accompanying file LICENSE_1_0.txt or copy at
   http://www.boost.org/LICENSE_1_0.txt)

========
distance
========

``boost::iterators::distance`` is an adapted version of ``std::distance`` for 
the Boost iterator traversal concepts.


Header
------

``<boost/iterator/distance.hpp>``


Synopsis
--------

::

    template <typename Iterator>
    constexpr typename iterator_difference<Iterator>::type
    distance(Iterator first, Iterator last);


Description
-----------

Computes the (signed) distance from ``first`` to ``last``.


Requirements
------------

``Iterator`` should model Single Pass Iterator.


Preconditions
-------------

If ``Iterator`` models Random Access Traversal Iterator, 
``[first, last)`` or ``[last, first)`` should be valid;
otherwise ``[first, last)`` should be valid.


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
