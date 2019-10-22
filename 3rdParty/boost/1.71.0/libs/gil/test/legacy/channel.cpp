//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel.hpp>
#include <boost/gil/channel_algorithm.hpp>
#include <boost/gil/typedefs.hpp>

#include <cstdint>
#include <exception>
#include <iostream>

#if defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#elif BOOST_GCC >= 40700
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#elif BOOST_WORKAROUND(BOOST_MSVC, >= 1400)
#pragma warning(push)
#pragma warning(disable:4512) //assignment operator could not be generated
#endif

using namespace boost::gil;
using namespace std;

void error_if(bool);

auto c8_min   = channel_traits<uint8_t>::min_value();
auto c8_max   = channel_traits<uint8_t>::max_value();
auto c8s_min  = channel_traits<int8_t>::min_value();
auto c8s_max  = channel_traits<int8_t>::max_value();
auto c16_min  = channel_traits<uint16_t>::min_value();
auto c16_max  = channel_traits<uint16_t>::max_value();
auto c16s_min = channel_traits<int16_t>::min_value();
auto c16s_max = channel_traits<int16_t>::max_value();
auto c32_min  = channel_traits<uint32_t>::min_value();
auto c32_max  = channel_traits<uint32_t>::max_value();
auto c32s_min = channel_traits<int32_t>::min_value();
auto c32s_max = channel_traits<int32_t>::max_value();
auto c32f_min = channel_traits<float32_t>::min_value();
auto c32f_max = channel_traits<float32_t>::max_value();


template <typename ChannelTestCore>
struct do_test : public ChannelTestCore {
    using channel_t = typename ChannelTestCore::channel_t;
    using channel_value_t = typename channel_traits<channel_t>::value_type;

    do_test() : ChannelTestCore() {
        error_if(this->_min_v != channel_traits<channel_t>::min_value());
        error_if(this->_max_v != channel_traits<channel_t>::max_value());
    }

    void test_all() {
        test_channel_invert();
        test_channel_convert();
        test_channel_multiply();
        test_channel_math();
    }

    void test_mutable(boost::mpl::false_) {}
    void test_mutable(boost::mpl::true_) {
        channel_value_t mv=this->_min_v;
        ++this->_min_v; this->_min_v++;
        --this->_min_v; this->_min_v--;
        error_if(mv!=this->_min_v);

        this->_min_v+=1;
        this->_min_v-=1;
        error_if(mv!=this->_min_v);

        this->_min_v*=1;
        this->_min_v/=1;
        error_if(mv!=this->_min_v);

         this->_min_v = 1;    // assignable to scalar
         this->_min_v = mv;   // and to value type

         // test swap
         channel_value_t v1=this->_min_v;
         channel_value_t v2=this->_max_v;
         swap(this->_min_v, this->_max_v);

         channel_value_t v3=this->_min_v;
         channel_value_t v4=this->_max_v;
         error_if(v1!=v4 || v2!=v3);
    }

    void test_channel_math() {
        error_if(this->_min_v >= this->_max_v);
        error_if(this->_max_v <= this->_min_v);
        error_if(this->_min_v > this->_max_v);
        error_if(this->_max_v < this->_min_v);
        error_if(this->_max_v == this->_min_v);
        error_if(!(this->_max_v != this->_min_v));

        error_if(this->_min_v * 1 != this->_min_v);
        error_if(this->_min_v / 1 != this->_min_v);

        error_if((this->_min_v + 1) + 1 != (this->_min_v + 2));
        error_if((this->_max_v - 1) - 1 != (this->_max_v - 2));

        error_if(this->_min_v != 1 && this->_min_v==1);  // comparable to integral


        test_mutable(boost::mpl::bool_<channel_traits<channel_t>::is_mutable>());
    }


    void test_channel_invert() {
        error_if(channel_invert(this->_min_v) != this->_max_v);
        error_if(channel_invert(this->_max_v) != this->_min_v);
    }

    void test_channel_multiply() {
        error_if(channel_multiply(this->_min_v, this->_min_v) != this->_min_v);
        error_if(channel_multiply(this->_max_v, this->_max_v) != this->_max_v);
        error_if(channel_multiply(this->_max_v, this->_min_v) != this->_min_v);
    }

    void test_channel_convert() {
        channel_value_t  v_min, v_max;

        v_min=channel_convert<channel_t>(c8_min);
        v_max=channel_convert<channel_t>(c8_max);
        error_if(v_min!=this->_min_v || v_max!=this->_max_v);

        v_min=channel_convert<channel_t>(c8s_min);
        v_max=channel_convert<channel_t>(c8s_max);
        error_if(v_min!=this->_min_v || v_max!=this->_max_v);

        v_min=channel_convert<channel_t>(c16_min);
        v_max=channel_convert<channel_t>(c16_max);
        error_if(v_min!=this->_min_v || v_max!=this->_max_v);

        v_min=channel_convert<channel_t>(c16s_min);
        v_max=channel_convert<channel_t>(c16s_max);
        error_if(v_min!=this->_min_v || v_max!=this->_max_v);

        v_min=channel_convert<channel_t>(c32_min);
        v_max=channel_convert<channel_t>(c32_max);
        error_if(v_min!=this->_min_v || v_max!=this->_max_v);

        v_min=channel_convert<channel_t>(c32s_min);
        v_max=channel_convert<channel_t>(c32s_max);
        error_if(v_min!=this->_min_v || v_max!=this->_max_v);

        v_min=channel_convert<channel_t>(c32f_min);
        v_max=channel_convert<channel_t>(c32f_max);
        error_if(v_min!=this->_min_v || v_max!=this->_max_v);
    }
};

// Different core classes depending on the different types of channels - channel values, references and subbyte references
// The cores ensure there are two members, _min_v and _max_v initialized with the minimum and maximum channel value.
// The different channel types have different ways to initialize them, thus require different cores

// For channel values simply initialize the value directly
template <typename ChannelValue>
class value_core {
protected:
    using channel_t = ChannelValue;
    channel_t _min_v;
    channel_t _max_v;

    value_core()
        : _min_v(channel_traits<ChannelValue>::min_value())
        , _max_v(channel_traits<ChannelValue>::max_value())
    {
        boost::function_requires<ChannelValueConcept<ChannelValue> >();
    }
};

// For channel references we need to have separate channel values
template <typename ChannelRef>
class reference_core : public value_core<typename channel_traits<ChannelRef>::value_type>
{
    using parent_t = value_core<typename channel_traits<ChannelRef>::value_type>;

protected:
    using channel_t = ChannelRef;
    channel_t _min_v;
    channel_t _max_v;

    reference_core()
        : parent_t()
        , _min_v(parent_t::_min_v)
        , _max_v(parent_t::_max_v)
    {
        boost::function_requires<ChannelConcept<ChannelRef> >();
    }
};

// For subbyte channel references we need to store the bit buffers somewhere
template <typename ChannelSubbyteRef, typename ChannelMutableRef = ChannelSubbyteRef>
class packed_reference_core {
protected:
    using channel_t = ChannelSubbyteRef;
    using integer_t = typename channel_t::integer_t;
    channel_t _min_v, _max_v;

    integer_t _min_buf, _max_buf;

    packed_reference_core() : _min_v(&_min_buf), _max_v(&_max_buf) {
        ChannelMutableRef b1(&_min_buf), b2(&_max_buf);
        b1 = channel_traits<channel_t>::min_value();
        b2 = channel_traits<channel_t>::max_value();

        boost::function_requires<ChannelConcept<ChannelSubbyteRef> >();
    }
};

template <typename ChannelSubbyteRef, typename ChannelMutableRef = ChannelSubbyteRef>
class packed_dynamic_reference_core {
protected:
    using channel_t = ChannelSubbyteRef;
    channel_t _min_v, _max_v;

    typename channel_t::integer_t _min_buf, _max_buf;

    packed_dynamic_reference_core(int first_bit1=1, int first_bit2=2) : _min_v(&_min_buf,first_bit1), _max_v(&_max_buf,first_bit2) {
        ChannelMutableRef b1(&_min_buf,1), b2(&_max_buf,2);
        b1 = channel_traits<channel_t>::min_value();
        b2 = channel_traits<channel_t>::max_value();

        boost::function_requires<ChannelConcept<ChannelSubbyteRef> >();
    }
};


template <typename ChannelValue>
void test_channel_value() {
    do_test<value_core<ChannelValue> >().test_all();
}

template <typename ChannelRef>
void test_channel_reference() {
    do_test<reference_core<ChannelRef> >().test_all();
}

template <typename ChannelSubbyteRef>
void test_packed_channel_reference() {
    do_test<packed_reference_core<ChannelSubbyteRef,ChannelSubbyteRef> >().test_all();
}

template <typename ChannelSubbyteRef, typename MutableRef>
void test_const_packed_channel_reference() {
    do_test<packed_reference_core<ChannelSubbyteRef,MutableRef> >().test_all();
}

template <typename ChannelSubbyteRef>
void test_packed_dynamic_channel_reference() {
    do_test<packed_dynamic_reference_core<ChannelSubbyteRef,ChannelSubbyteRef> >().test_all();
}

template <typename ChannelSubbyteRef, typename MutableRef>
void test_const_packed_dynamic_channel_reference() {
    do_test<packed_dynamic_reference_core<ChannelSubbyteRef,MutableRef> >().test_all();
}

template <typename ChannelValue>
void test_channel_value_impl() {
    test_channel_value<ChannelValue>();
    test_channel_reference<ChannelValue&>();
    test_channel_reference<const ChannelValue&>();
}

/////////////////////////////////////////////////////////
///
/// A channel archetype - to test the minimum requirements of the concept
///
/////////////////////////////////////////////////////////

struct channel_value_archetype;
struct channel_archetype {
    // equality comparable
    friend bool operator==(const channel_archetype&,const channel_archetype&) { return true; }
    friend bool operator!=(const channel_archetype&,const channel_archetype&) { return false; }
    // less-than comparable
    friend bool operator<(const channel_archetype&,const channel_archetype&) { return false; }
    // convertible to a scalar
    operator std::uint8_t() const { return 0; }


    channel_archetype& operator++() { return *this; }
    channel_archetype& operator--() { return *this; }
    channel_archetype  operator++(int) { return *this; }
    channel_archetype  operator--(int) { return *this; }

    template <typename Scalar> channel_archetype operator+=(Scalar) { return *this; }
    template <typename Scalar> channel_archetype operator-=(Scalar) { return *this; }
    template <typename Scalar> channel_archetype operator*=(Scalar) { return *this; }
    template <typename Scalar> channel_archetype operator/=(Scalar) { return *this; }

    using value_type = channel_value_archetype;
    using reference = channel_archetype;
    using const_reference = channel_archetype const;
    using pointer = channel_value_archetype *;
    using const_pointer = channel_value_archetype const*;
    static constexpr bool is_mutable=true;

    static value_type min_value();
    static value_type max_value();
};


struct channel_value_archetype : public channel_archetype {
    channel_value_archetype() {}                                        // default constructible
    channel_value_archetype(const channel_value_archetype&) {}          // copy constructible
    channel_value_archetype& operator=(const channel_value_archetype&){return *this;} // assignable
    channel_value_archetype(std::uint8_t) {}
};

channel_value_archetype channel_archetype::min_value() { return channel_value_archetype(); }
channel_value_archetype channel_archetype::max_value() { return channel_value_archetype(); }


void test_packed_channel_reference()
{
    using channel16_0_5_reference_t = packed_channel_reference<std::uint16_t, 0, 5, true>;
    using channel16_5_6_reference_t = packed_channel_reference<std::uint16_t, 5, 6, true>;
    using channel16_11_5_reference_t = packed_channel_reference<std::uint16_t, 11, 5, true>;

    std::uint16_t data=0;
    channel16_0_5_reference_t   channel1(&data);
    channel16_5_6_reference_t   channel2(&data);
    channel16_11_5_reference_t  channel3(&data);

    channel1=channel_traits<channel16_0_5_reference_t>::max_value();
    channel2=channel_traits<channel16_5_6_reference_t>::max_value();
    channel3=channel_traits<channel16_11_5_reference_t>::max_value();
    error_if(data!=65535);

    test_packed_channel_reference<channel16_0_5_reference_t>();
    test_packed_channel_reference<channel16_5_6_reference_t>();
    test_packed_channel_reference<channel16_11_5_reference_t>();
}

void test_packed_dynamic_channel_reference()
{
    using channel16_5_reference_t = packed_dynamic_channel_reference<std::uint16_t, 5, true>;
    using channel16_6_reference_t = packed_dynamic_channel_reference<std::uint16_t, 6, true>;

    std::uint16_t data=0;
    channel16_5_reference_t  channel1(&data,0);
    channel16_6_reference_t  channel2(&data,5);
    channel16_5_reference_t  channel3(&data,11);

    channel1=channel_traits<channel16_5_reference_t>::max_value();
    channel2=channel_traits<channel16_6_reference_t>::max_value();
    channel3=channel_traits<channel16_5_reference_t>::max_value();
    error_if(data!=65535);

    test_packed_dynamic_channel_reference<channel16_5_reference_t>();
}

void test_channel() {
    test_channel_value_impl<uint8_t>();
    test_channel_value_impl<int8_t>();
    test_channel_value_impl<uint16_t>();
    test_channel_value_impl<int16_t>();
    test_channel_value_impl<uint32_t>();
    test_channel_value_impl<int16_t>();

    test_channel_value_impl<float32_t>();

    test_packed_channel_reference();
    test_packed_dynamic_channel_reference();

    // Do only compile-time tests for the archetype (because asserts like val1<val2 fail)
    boost::function_requires<MutableChannelConcept<channel_archetype> >();

    do_test<value_core<channel_value_archetype> >();
    do_test<reference_core<channel_archetype> >();
    do_test<reference_core<const channel_archetype&> >();
}

int main()
{
    try
    {
        test_channel();

        return EXIT_SUCCESS;
    }
    catch (std::exception const& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...)
    {
        return EXIT_FAILURE;
    }
}

// TODO:
// - provide algorithm performance overloads for scoped channel and packed channels
// - Update concepts and documentation
// - What to do about pointer types?!
// - Performance!!
//      - is channel_convert the same as native?
//      - is operator++ on float32_t the same as native? How about if operator++ is defined in scoped_channel to do _value++?
