Channel
=======

.. contents::
   :local:
   :depth: 2

Overview
--------

A channel indicates the intensity of a color component (for example, the red
channel in an RGB pixel). Typical channel operations are getting, comparing
and setting the channel values. Channels have associated minimum and maximum
value. GIL channels model the following concept:

.. code-block:: cpp

  concept ChannelConcept<typename T> : EqualityComparable<T>
  {
      typename value_type      = T;        // use channel_traits<T>::value_type to access it
      where ChannelValueConcept<value_type>;
      typename reference       = T&;       // use channel_traits<T>::reference to access it
      typename pointer         = T*;       // use channel_traits<T>::pointer to access it
      typename const_reference = const T&; // use channel_traits<T>::const_reference to access it
      typename const_pointer   = const T*; // use channel_traits<T>::const_pointer to access it
      static const bool is_mutable;        // use channel_traits<T>::is_mutable to access it

      static T min_value();                // use channel_traits<T>::min_value to access it
      static T max_value();                // use channel_traits<T>::min_value to access it
  };

  concept MutableChannelConcept<ChannelConcept T> : Swappable<T>, Assignable<T> {};

  concept ChannelValueConcept<ChannelConcept T> : Regular<T> {};

GIL allows built-in integral and floating point types to be channels.
Therefore the associated types and range information are defined in
``channel_traits`` with the following default implementation:

.. code-block:: cpp

  template <typename T>
  struct channel_traits
  {
      typedef T         value_type;
      typedef T&        reference;
      typedef T*        pointer;
      typedef T& const  const_reference;
      typedef T* const  const_pointer;

      static value_type min_value() { return std::numeric_limits<T>::min(); }
      static value_type max_value() { return std::numeric_limits<T>::max(); }
  };

Two channel types are *compatible* if they have the same value type:

.. code-block:: cpp

  concept ChannelsCompatibleConcept<ChannelConcept T1, ChannelConcept T2>
  {
      where SameType<T1::value_type, T2::value_type>;
  };

A channel may be *convertible* to another channel:

.. code-block:: cpp

  template <ChannelConcept Src, ChannelValueConcept Dst>
  concept ChannelConvertibleConcept
  {
      Dst channel_convert(Src);
  };

Note that ``ChannelConcept`` and ``MutableChannelConcept`` do not require a
default constructor. Channels that also support default construction (and thus
are regular types) model ``ChannelValueConcept``.
To understand the motivation for this distinction, consider a 16-bit RGB pixel
in a "565" bit pattern. Its channels correspond to bit ranges. To support such
channels, we need to create a custom proxy class corresponding to a reference
to a sub-byte channel.
Such a proxy reference class models only ``ChannelConcept``, because, similar
to native C++ references, it may not have a default constructor.

Note also that algorithms may impose additional requirements on channels,
such as support for arithmetic operations.

.. seealso::

  - `ChannelConcept<T> <reference/structboost_1_1gil_1_1_channel_concept.html>`_
  - `ChannelValueConcept<T> <reference/structboost_1_1gil_1_1_channel_value_concept.html>`_
  - `MutableChannelConcept<T> <reference/structboost_1_1gil_1_1_mutable_channel_concept.html>`_
  - `ChannelsCompatibleConcept<T1,T2> <reference/structboost_1_1gil_1_1_channels_compatible_concept.html>`_
  - `ChannelConvertibleConcept<SrcChannel,DstChannel> <reference/structboost_1_1gil_1_1_channel_convertible_concept.html>`_

Models
------

All C++11 fundamental integer and float point types are valid channels.

The minimum and maximum values of a channel modeled by a built-in type
correspond to the minimum and maximum physical range of the built-in type, as
specified by its ``std::numeric_limits``. Sometimes the physical range is not
appropriate. GIL provides ``scoped_channel_value``, a model for a channel
adapter that allows for specifying a custom range.
We use it to define a ``[0..1]`` floating point channel type as follows:

.. code-block:: cpp

  struct float_zero { static float apply() { return 0.0f; } };
  struct float_one  { static float apply() { return 1.0f; } };
  typedef scoped_channel_value<float,float_zero,float_one> bits32f;

GIL also provides models for channels corresponding to ranges of bits:

.. code-block:: cpp

  // Value of a channel defined over NumBits bits. Models ChannelValueConcept
  template <int NumBits> class packed_channel_value;

  // Reference to a channel defined over NumBits bits. Models ChannelConcept
  template <int FirstBit,
          int NumBits,       // Defines the sequence of bits in the data value that contain the channel
          bool Mutable>      // true if the reference is mutable
  class packed_channel_reference;

  // Reference to a channel defined over NumBits bits. Its FirstBit is a run-time parameter. Models ChannelConcept
  template <int NumBits,       // Defines the sequence of bits in the data value that contain the channel
          bool Mutable>      // true if the reference is mutable
  class packed_dynamic_channel_reference;

Note that there are two models of a reference proxy which differ based on
whether the offset of the channel range is specified as a template or a
run-time parameter. The first model is faster and more compact while the
second model is more flexible. For example, the second model allows us to
construct an iterator over bit range channels.

Algorithms
----------

Here is how to construct the three channels of a 16-bit "565" pixel and set
them to their maximum value:

.. code-block:: cpp

  using channel16_0_5_reference_t  = packed_channel_reference<0, 5, true>;
  using channel16_5_6_reference_t  = packed_channel_reference<5, 6, true>;
  using channel16_11_5_reference_t = packed_channel_reference<11, 5, true>;

  std::uint16_t data=0;
  channel16_0_5_reference_t  channel1(&data);
  channel16_5_6_reference_t  channel2(&data);
  channel16_11_5_reference_t channel3(&data);

  channel1 = channel_traits<channel16_0_5_reference_t>::max_value();
  channel2 = channel_traits<channel16_5_6_reference_t>::max_value();
  channel3 = channel_traits<channel16_11_5_reference_t>::max_value();
  assert(data == 65535);

Assignment, equality comparison and copy construction are defined only between
compatible channels:

.. code-block:: cpp

  packed_channel_value<5> channel_6bit = channel1;
  channel_6bit = channel3;

  // compile error: Assignment between incompatible channels
  //channel_6bit = channel2;

All channel models provided by GIL are pairwise convertible:

.. code-block:: cpp

  channel1 = channel_traits<channel16_0_5_reference_t>::max_value();
  assert(channel1 == 31);

  bits16 chan16 = channel_convert<bits16>(channel1);
  assert(chan16 == 65535);

Channel conversion is a lossy operation. GIL's channel conversion is a linear
transformation between the ranges of the source and destination channel.
It maps precisely the minimum to the minimum and the maximum to the maximum.
(For example, to convert from uint8_t to uint16_t GIL does not do a bit shift
because it will not properly match the maximum values. Instead GIL multiplies
the source by 257).

All channel models that GIL provides are convertible from/to an integral or
floating point type. Thus they support arithmetic operations. Here are the
channel-level algorithms that GIL provides:

.. code-block:: cpp

  // Converts a source channel value into a destination channel.
  // Linearly maps the value of the source into the range of the destination.
  template <typename DstChannel, typename SrcChannel>
  typename channel_traits<DstChannel>::value_type channel_convert(SrcChannel src);

  // returns max_value - x + min_value
  template <typename Channel>
  typename channel_traits<Channel>::value_type channel_invert(Channel x);

  // returns a * b / max_value
  template <typename Channel>
  typename channel_traits<Channel>::value_type channel_multiply(Channel a, Channel b);
