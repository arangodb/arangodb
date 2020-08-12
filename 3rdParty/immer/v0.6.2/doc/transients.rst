Transients
==========

Rationale
---------

*Transients* is a concept borrowed `from Clojure
<clojure-transients>`_, with some twists to turn make more idiomatic in
C++.  Essentially, they are a mutable interface built on top of the
same data structures that support the immutable containers.  They
serve these two use-cases.

.. _clojure-transients: https://clojure.org/reference/transients

Efficient batch manipulations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sometimes you may write a function that needs to do multiple changes
to a container.  Like most code you write with this library, this
function is *pure*: it takes one container value in, and produces a
new container value out, no side-effects.

Let's say we want to write a function that inserts all integers in the
range :math:`[first, last)` into an immutable vector:

.. literalinclude:: ../example/vector/iota-slow.cpp
   :language: c++
   :start-after: include:myiota/start
   :end-before:  include:myiota/end

This function works as expected, but it is slower than necessary.
On every loop iteration, a new value is produced, just to be
forgotten in the next iteration.

Instead, we can grab a mutable view on the value, a *transient*.
Then, we manipulate it *in-place*.  When we are done with it, we
extract back an immutable value from it.  The code now looks like
this:

.. _iota-transient:

.. literalinclude:: ../example/vector/iota-transient.cpp
   :language: c++
   :start-after: include:myiota/start
   :end-before:  include:myiota/end

Both conversions are :math:`O(1)`.  Note that calling ``transient()``
does not break the immutability of the variable it is called on.  The
new mutable object will adopt its contents, but when a mutation is
performed, it will copy the data necessary using *copy on write*.
Subsequent manipulations may hit parts that have already been copied,
and these changes are done in-place.  Because of this, it does not
make sense to use transients to do only one change.

.. tip::

   Note that :ref:`move semantics<move-semantics>` can be used instead to
   support a similar use-case.  However, transients optimise updates
   even when reference counting is disabled.

Standard library compatibility
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Transients try to provide an interface as similar as possible to
similar standard library containers.  Thus, they may be used to
interoperate with standard library components.

For example the :ref:`myiota() function above<iota-transient>` may as
well be written using standard library tools:

.. literalinclude:: ../example/vector/iota-transient-std.cpp
   :language: c++
   :start-after: include:myiota/start
   :end-before:  include:myiota/end

array_transient
---------------

.. doxygenclass:: immer::array_transient
    :members:
    :undoc-members:

vector_transient
----------------

.. doxygenclass:: immer::vector_transient
    :members:
    :undoc-members:

flex_vector_transient
---------------------

.. doxygenclass:: immer::flex_vector_transient
    :members:
    :undoc-members:

set_transient
-------------

.. doxygenclass:: immer::set_transient
    :members:
    :undoc-members:

map_transient
-------------

.. doxygenclass:: immer::map_transient
    :members:
    :undoc-members:
