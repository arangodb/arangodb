Technicalities
==============

.. contents::
   :local:
   :depth: 2

Creating a reference proxy
--------------------------

Sometimes it is necessary to create a proxy class that represents a
reference to a given object. Examples of these are GIL's reference to
a planar pixel (``planar_pixel_reference``) and GIL's sub-byte channel
references. Writing a reference proxy class can be tricky. One problem
is that the proxy reference is constructed as a temporary object and
returned by value upon dereferencing the iterator:

.. code-block:: cpp

  struct rgb_planar_pixel_iterator
  {
   typedef my_reference_proxy<T> reference;
   reference operator*() const { return reference(red,green,blue); }
  };

The problem arises when an iterator is dereferenced directly into a
function that takes a mutable pixel:

.. code-block:: cpp

  template <typename Pixel>    // Models MutablePixelConcept
  void invert_pixel(Pixel& p);

  rgb_planar_pixel_iterator myIt;
  invert_pixel(*myIt);        // compile error!

C++ does not allow for matching a temporary object against a non-constant
reference. The solution is to:

* Use const qualifier on all members of the reference proxy object:

.. code-block:: cpp

    template <typename T>
    struct my_reference_proxy
    {
      const my_reference_proxy& operator=(const my_reference_proxy& p) const;
      const my_reference_proxy* operator->() const { return this; }
      ...
    };

* Use different classes to denote mutable and constant reference
  (maybe based on the constness of the template parameter)

* Define the reference type of your iterator with const qualifier:

.. code-block:: cpp

    struct iterator_traits<rgb_planar_pixel_iterator>
    {
      typedef const my_reference_proxy<T> reference;
    };

A second important issue is providing an overload for ``swap`` for
your reference class. The default ``std::swap`` will not work
correctly. You must use a real value type as the temporary. A further
complication is that in some implementations of the STL the ``swap``
function is incorrectly called qualified, as ``std::swap``. The only
way for these STL algorithms to use your overload is if you define it
in the ``std`` namespace:

.. code-block:: cpp

  namespace std
  {
   template <typename T>
   void swap(my_reference_proxy<T>& x, my_reference_proxy<T>& y)
   {
      my_value<T> tmp=x;
      x=y;
      y=tmp;
   }
  }

Lastly, remember that constructors and copy-constructors of proxy
references are always shallow and assignment operators are deep.

We are grateful to Dave Abrahams, Sean Parent and Alex Stepanov for
suggesting the above solution.
