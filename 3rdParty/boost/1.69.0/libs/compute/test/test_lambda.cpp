//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestLambda
#include <boost/test/unit_test.hpp>

#include <boost/tuple/tuple_io.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <boost/compute/function.hpp>
#include <boost/compute/lambda.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/for_each.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/functional/bind.hpp>
#include <boost/compute/iterator/zip_iterator.hpp>
#include <boost/compute/types/pair.hpp>
#include <boost/compute/types/tuple.hpp>

#include "check_macros.hpp"
#include "quirks.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;
namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(squared_plus_one)
{
    bc::vector<int> vector(context);
    vector.push_back(1, queue);
    vector.push_back(2, queue);
    vector.push_back(3, queue);
    vector.push_back(4, queue);
    vector.push_back(5, queue);

    // multiply each value by itself and add one
    bc::transform(vector.begin(),
                  vector.end(),
                  vector.begin(),
                  (bc::_1 * bc::_1) + 1,
                  queue);
    CHECK_RANGE_EQUAL(int, 5, vector, (2, 5, 10, 17, 26));
}

BOOST_AUTO_TEST_CASE(abs_int)
{
    bc::vector<int> vector(context);
    vector.push_back(-1, queue);
    vector.push_back(-2, queue);
    vector.push_back(3, queue);
    vector.push_back(-4, queue);
    vector.push_back(5, queue);

    bc::transform(vector.begin(),
                  vector.end(),
                  vector.begin(),
                  abs(bc::_1),
                  queue);
    CHECK_RANGE_EQUAL(int, 5, vector, (1, 2, 3, 4, 5));
}

template<class Result, class Expr>
void check_lambda_result(const Expr &)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            typename ::boost::compute::lambda::result_of<Expr>::type,
            Result
        >::value
    ));
}

template<class Result, class Expr, class Arg1>
void check_lambda_result(const Expr &, const Arg1 &)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            typename ::boost::compute::lambda::result_of<
                Expr,
                typename boost::tuple<Arg1>
            >::type,
            Result
        >::value
    ));
}

template<class Result, class Expr, class Arg1, class Arg2>
void check_lambda_result(const Expr &, const Arg1 &, const Arg2 &)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            typename ::boost::compute::lambda::result_of<
                Expr,
                typename boost::tuple<Arg1, Arg2>
            >::type,
            Result
        >::value
    ));
}

template<class Result, class Expr, class Arg1, class Arg2, class Arg3>
void check_lambda_result(const Expr &, const Arg1 &, const Arg2 &, const Arg3 &)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            typename ::boost::compute::lambda::result_of<
                Expr,
                typename boost::tuple<Arg1, Arg2, Arg3>
            >::type,
            Result
        >::value
    ));
}

BOOST_AUTO_TEST_CASE(result_of)
{
    using ::boost::compute::lambda::_1;
    using ::boost::compute::lambda::_2;
    using ::boost::compute::lambda::_3;

    namespace proto = ::boost::proto;

    using boost::compute::int_;

    check_lambda_result<int_>(proto::lit(1));
    check_lambda_result<int_>(proto::lit(1) + 2);
    check_lambda_result<float>(proto::lit(1.2f));
    check_lambda_result<float>(proto::lit(1) + 1.2f);
    check_lambda_result<float>(proto::lit(1) / 2 + 1.2f);

    using boost::compute::float4_;
    using boost::compute::int4_;

    check_lambda_result<int_>(_1, int_(1));
    check_lambda_result<float>(_1, float(1.2f));
    check_lambda_result<float4_>(_1, float4_(1, 2, 3, 4));
    check_lambda_result<float4_>(2.0f * _1, float4_(1, 2, 3, 4));
    check_lambda_result<float4_>(_1 * 2.0f, float4_(1, 2, 3, 4));

    check_lambda_result<float>(dot(_1, _2), float4_(0, 1, 2, 3), float4_(3, 2, 1, 0));
    check_lambda_result<float>(dot(_1, float4_(3, 2, 1, 0)), float4_(0, 1, 2, 3));
    check_lambda_result<float>(distance(_1, _2), float4_(0, 1, 2, 3), float4_(3, 2, 1, 0));
    check_lambda_result<float>(distance(_1, float4_(3, 2, 1, 0)), float4_(0, 1, 2, 3));

    check_lambda_result<float>(length(_1), float4_(3, 2, 1, 0));

    check_lambda_result<float4_>(cross(_1, _2), float4_(0, 1, 2, 3), float4_(3, 2, 1, 0));
    check_lambda_result<float4_>(cross(_1, float4_(3, 2, 1, 0)), float4_(0, 1, 2, 3));

    check_lambda_result<float4_>(max(_1, _2), float4_(3, 2, 1, 0), float4_(0, 1, 2, 3));
    check_lambda_result<float4_>(max(_1, float(1.0f)), float4_(0, 1, 2, 3));
    check_lambda_result<int4_>(max(_1, int4_(3, 2, 1, 0)), int4_(0, 1, 2, 3));
    check_lambda_result<int4_>(max(_1, int_(1)), int4_(0, 1, 2, 3));
    check_lambda_result<float4_>(min(_1, float4_(3, 2, 1, 0)), float4_(0, 1, 2, 3));

    check_lambda_result<float4_>(step(_1, _2), float4_(3, 2, 1, 0), float4_(0, 1, 2, 3));
    check_lambda_result<int4_>(step(_1, _2), float(3.0f), int4_(0, 1, 2, 3));

    check_lambda_result<float4_>(
        smoothstep(_1, _2, _3),
        float4_(3, 2, 1, 0), float4_(3, 2, 1, 0), float4_(0, 1, 2, 3)
    );
    check_lambda_result<int4_>(
        smoothstep(_1, _2, _3),
        float(2.0f), float(3.0f), int4_(0, 1, 2, 3)
    );

    check_lambda_result<int4_>(bc::lambda::isinf(_1), float4_(0, 1, 2, 3));

    check_lambda_result<int>(_1 + 2, int(2));
    check_lambda_result<float>(_1 + 2, float(2.2f));

    check_lambda_result<int>(_1 + _2, int(1), int(2));
    check_lambda_result<float>(_1 + _2, int(1), float(2.2f));

    check_lambda_result<int>(_1 + _1, int(1));
    check_lambda_result<float>(_1 * _1, float(1));

    using boost::compute::lambda::get;

    check_lambda_result<float>(get<0>(_1), float4_(1, 2, 3, 4));
    check_lambda_result<bool>(get<0>(_1) < 1.f, float4_(1, 2, 3, 4));
    check_lambda_result<bool>(_1 < 1.f, float(2));

    using boost::compute::lambda::make_pair;

    check_lambda_result<int>(get<0>(make_pair(_1, _2)), int(1), float(1.2f));
    check_lambda_result<float>(get<1>(make_pair(_1, _2)), int(1), float(1.2f));
    check_lambda_result<std::pair<int, float> >(make_pair(_1, _2), int(1), float(1.2f));

    using boost::compute::lambda::make_tuple;

    check_lambda_result<boost::tuple<int> >(make_tuple(_1), int(1));
    check_lambda_result<boost::tuple<int, float> >(make_tuple(_1, _2), int(1), float(1.2f));
    check_lambda_result<boost::tuple<int, int> >(make_tuple(_1, _1), int(1));
    check_lambda_result<boost::tuple<int, float> >(make_tuple(_1, _2), int(1), float(1.4f));
    check_lambda_result<boost::tuple<char, int, float> >(
        make_tuple(_1, _2, _3), char('a'), int(2), float(3.4f)
    );
    check_lambda_result<boost::tuple<int, int, int> >(
        make_tuple(_1, _1, _1), int(1), float(1.4f)
    );
    check_lambda_result<boost::tuple<int, float, int, float, int> >(
        make_tuple(_1, _2, _1, _2, _1), int(1), float(1.4f)
    );
}

BOOST_AUTO_TEST_CASE(make_function_from_lamdba)
{
    using boost::compute::lambda::_1;

    int data[] = { 2, 4, 6, 8, 10 };
    compute::vector<int> vector(data, data + 5, queue);

    compute::function<int(int)> f = _1 * 2 + 3;

    compute::transform(
        vector.begin(), vector.end(), vector.begin(), f, queue
    );
    CHECK_RANGE_EQUAL(int, 5, vector, (7, 11, 15, 19, 23));
}

BOOST_AUTO_TEST_CASE(make_function_from_binary_lamdba)
{
    using boost::compute::lambda::_1;
    using boost::compute::lambda::_2;
    using boost::compute::lambda::abs;

    int data1[] = { 2, 4, 6, 8, 10 };
    int data2[] = { 10, 8, 6, 4, 2 };
    compute::vector<int> vec1(data1, data1 + 5, queue);
    compute::vector<int> vec2(data2, data2 + 5, queue);
    compute::vector<int> result(5, context);

    compute::function<int(int, int)> f = abs(_1 - _2);

    compute::transform(
        vec1.begin(), vec1.end(), vec2.begin(), result.begin(), f, queue
    );
    CHECK_RANGE_EQUAL(int, 5, result, (8, 4, 0, 4, 8));
}

BOOST_AUTO_TEST_CASE(lambda_binary_function_with_pointer_modf)
{
    using boost::compute::lambda::_1;
    using boost::compute::lambda::_2;
    using boost::compute::lambda::abs;

    bc::float_ data1[] = { 2.2f, 4.2f, 6.3f, 8.3f, 10.2f };
    compute::vector<bc::float_> vec1(data1, data1 + 5, queue);
    compute::vector<bc::float_> vec2(size_t(5), context);
    compute::vector<bc::float_> result(5, context);

    compute::transform(
        bc::make_transform_iterator(vec1.begin(), _1 + 0.01f),
        bc::make_transform_iterator(vec1.end(), _1 + 0.01f),
        vec2.begin(),
        result.begin(),
        bc::lambda::modf(_1, _2),
        queue
    );
    CHECK_RANGE_CLOSE(bc::float_, 5, result, (0.21f, 0.21f, 0.31f, 0.31f, 0.21f), 0.01f);
    CHECK_RANGE_CLOSE(bc::float_, 5, vec2, (2, 4, 6, 8, 10), 0.01f);
}

BOOST_AUTO_TEST_CASE(lambda_tenary_function_with_pointer_remquo)
{
    if(!has_remquo_func(device))
    {
        return;
    }

    using boost::compute::lambda::_1;
    using boost::compute::lambda::_2;
    using boost::compute::lambda::get;

    bc::float_ data1[] = { 2.2f, 4.2f, 6.3f, 8.3f, 10.2f };
    bc::float_ data2[] = { 4.4f, 4.2f, 6.3f, 16.6f, 10.2f };
    compute::vector<bc::float_> vec1(data1, data1 + 5, queue);
    compute::vector<bc::float_> vec2(data2, data2 + 5, queue);
    compute::vector<bc::int_> vec3(size_t(5), context);
    compute::vector<bc::float_> result(5, context);

    compute::transform(
        compute::make_zip_iterator(
            boost::make_tuple(vec1.begin(), vec2.begin(), vec3.begin())
        ),
        compute::make_zip_iterator(
            boost::make_tuple(vec1.end(), vec2.end(), vec3.end())
        ),
        result.begin(),
        bc::lambda::remquo(get<0>(_1), get<1>(_1), get<2>(_1)),
        queue
    );
    CHECK_RANGE_CLOSE(bc::float_, 5, result, (2.2f, 0.0f, 0.0f, 8.3f, 0.0f), 0.01f);
    CHECK_RANGE_EQUAL(bc::int_, 5, vec3, (0, 1, 1, 0, 1));
}

BOOST_AUTO_TEST_CASE(lambda_get_vector)
{
    using boost::compute::_1;
    using boost::compute::int2_;
    using boost::compute::lambda::get;

    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    compute::vector<int2_> vector(4, context);

    compute::copy(
        reinterpret_cast<int2_ *>(data),
        reinterpret_cast<int2_ *>(data) + 4,
        vector.begin(),
        queue
    );

    // extract first component of each vector
    compute::vector<int> first_component(4, context);
    compute::transform(
        vector.begin(),
        vector.end(),
        first_component.begin(),
        get<0>(_1),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, first_component, (1, 3, 5, 7));

    // extract second component of each vector
    compute::vector<int> second_component(4, context);
    compute::transform(
        vector.begin(),
        vector.end(),
        first_component.begin(),
        get<1>(_1),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, first_component, (2, 4, 6, 8));
}

BOOST_AUTO_TEST_CASE(lambda_get_pair)
{
    using boost::compute::_1;
    using boost::compute::lambda::get;

    compute::vector<std::pair<int, float> > vector(context);
    vector.push_back(std::make_pair(1, 1.2f), queue);
    vector.push_back(std::make_pair(3, 3.4f), queue);
    vector.push_back(std::make_pair(5, 5.6f), queue);
    vector.push_back(std::make_pair(7, 7.8f), queue);

    // extract first compoenent of each pair
    compute::vector<int> first_component(4, context);
    compute::transform(
        vector.begin(),
        vector.end(),
        first_component.begin(),
        get<0>(_1),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, first_component, (1, 3, 5, 7));

    // extract second compoenent of each pair
    compute::vector<float> second_component(4, context);
    compute::transform(
        vector.begin(),
        vector.end(),
        second_component.begin(),
        get<1>(_1),
        queue
    );
    CHECK_RANGE_EQUAL(float, 4, second_component, (1.2f, 3.4f, 5.6f, 7.8f));
}

BOOST_AUTO_TEST_CASE(lambda_get_tuple)
{
    using boost::compute::_1;
    using boost::compute::lambda::get;

    compute::vector<boost::tuple<int, char, float> > vector(context);

    vector.push_back(boost::make_tuple(1, 'a', 1.2f), queue);
    vector.push_back(boost::make_tuple(3, 'b', 3.4f), queue);
    vector.push_back(boost::make_tuple(5, 'c', 5.6f), queue);
    vector.push_back(boost::make_tuple(7, 'd', 7.8f), queue);

    // extract first component of each tuple
    compute::vector<int> first_component(4, context);
    compute::transform(
        vector.begin(),
        vector.end(),
        first_component.begin(),
        get<0>(_1),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, first_component, (1, 3, 5, 7));

    // extract second component of each tuple
    compute::vector<char> second_component(4, context);
    compute::transform(
        vector.begin(),
        vector.end(),
        second_component.begin(),
        get<1>(_1),
        queue
    );
    CHECK_RANGE_EQUAL(char, 4, second_component, ('a', 'b', 'c', 'd'));

    // extract third component of each tuple
    compute::vector<float> third_component(4, context);
    compute::transform(
        vector.begin(),
        vector.end(),
        third_component.begin(),
        get<2>(_1),
        queue
    );
    CHECK_RANGE_EQUAL(float, 4, third_component, (1.2f, 3.4f, 5.6f, 7.8f));
}

BOOST_AUTO_TEST_CASE(lambda_get_zip_iterator)
{
    using boost::compute::_1;
    using boost::compute::lambda::get;

    float data[] = { 1.2f, 2.3f, 3.4f, 4.5f, 5.6f, 6.7f, 7.8f, 9.0f };
    compute::vector<float> input(8, context);
    compute::copy(data, data + 8, input.begin(), queue);

    compute::vector<float> output(8, context);

    compute::for_each(
        compute::make_zip_iterator(
            boost::make_tuple(input.begin(), output.begin())
        ),
        compute::make_zip_iterator(
            boost::make_tuple(input.end(), output.end())
        ),
        get<1>(_1) = get<0>(_1),
        queue
    );
    CHECK_RANGE_EQUAL(float, 8, output,
        (1.2f, 2.3f, 3.4f, 4.5f, 5.6f, 6.7f, 7.8f, 9.0f)
    );
}

BOOST_AUTO_TEST_CASE(lambda_make_pair)
{
    using boost::compute::_1;
    using boost::compute::_2;
    using boost::compute::lambda::make_pair;

    int int_data[] = { 1, 3, 5, 7 };
    float float_data[] = { 1.2f, 2.3f, 3.4f, 4.5f };

    compute::vector<int> int_vector(int_data, int_data + 4, queue);
    compute::vector<float> float_vector(float_data, float_data + 4, queue);
    compute::vector<std::pair<int, float> > output_vector(4, context);

    compute::transform(
        int_vector.begin(),
        int_vector.end(),
        float_vector.begin(),
        output_vector.begin(),
        make_pair(_1 - 1, 0 - _2),
        queue
    );

    std::vector<std::pair<int, float> > host_vector(4);
    compute::copy_n(output_vector.begin(), 4, host_vector.begin(), queue);
    BOOST_CHECK(host_vector[0] == std::make_pair(0, -1.2f));
    BOOST_CHECK(host_vector[1] == std::make_pair(2, -2.3f));
    BOOST_CHECK(host_vector[2] == std::make_pair(4, -3.4f));
    BOOST_CHECK(host_vector[3] == std::make_pair(6, -4.5f));
}

BOOST_AUTO_TEST_CASE(lambda_make_tuple)
{
    using boost::compute::_1;
    using boost::compute::lambda::get;
    using boost::compute::lambda::make_tuple;

    std::vector<boost::tuple<int, float> > data;
    data.push_back(boost::make_tuple(2, 1.2f));
    data.push_back(boost::make_tuple(4, 2.4f));
    data.push_back(boost::make_tuple(6, 4.6f));
    data.push_back(boost::make_tuple(8, 6.8f));

    compute::vector<boost::tuple<int, float> > input_vector(4, context);
    compute::copy(data.begin(), data.end(), input_vector.begin(), queue);

    // reverse the elements in the tuple
    compute::vector<boost::tuple<float, int> > output_vector(4, context);

    compute::transform(
        input_vector.begin(),
        input_vector.end(),
        output_vector.begin(),
        make_tuple(get<1>(_1), get<0>(_1)),
        queue
    );

    std::vector<boost::tuple<float, int> > host_vector(4);
    compute::copy_n(output_vector.begin(), 4, host_vector.begin(), queue);
    BOOST_CHECK_EQUAL(host_vector[0], boost::make_tuple(1.2f, 2));
    BOOST_CHECK_EQUAL(host_vector[1], boost::make_tuple(2.4f, 4));
    BOOST_CHECK_EQUAL(host_vector[2], boost::make_tuple(4.6f, 6));
    BOOST_CHECK_EQUAL(host_vector[3], boost::make_tuple(6.8f, 8));

    // duplicate each element in the tuple
    compute::vector<boost::tuple<int, int, float, float> > doubled_vector(4, context);
    compute::transform(
        input_vector.begin(),
        input_vector.end(),
        doubled_vector.begin(),
        make_tuple(get<0>(_1), get<0>(_1), get<1>(_1), get<1>(_1)),
        queue
    );

    std::vector<boost::tuple<int, int, float, float> > doubled_host_vector(4);
    compute::copy_n(doubled_vector.begin(), 4, doubled_host_vector.begin(), queue);
    BOOST_CHECK_EQUAL(doubled_host_vector[0], boost::make_tuple(2, 2, 1.2f, 1.2f));
    BOOST_CHECK_EQUAL(doubled_host_vector[1], boost::make_tuple(4, 4, 2.4f, 2.4f));
    BOOST_CHECK_EQUAL(doubled_host_vector[2], boost::make_tuple(6, 6, 4.6f, 4.6f));
    BOOST_CHECK_EQUAL(doubled_host_vector[3], boost::make_tuple(8, 8, 6.8f, 6.8f));
}

BOOST_AUTO_TEST_CASE(bind_lambda_function)
{
    using compute::placeholders::_1;
    namespace lambda = compute::lambda;

    int data[] = { 1, 2, 3, 4 };
    compute::vector<int> vector(data, data + 4, queue);

    compute::transform(
        vector.begin(), vector.end(), vector.begin(),
        compute::bind(lambda::_1 * lambda::_2, _1, 2),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, vector, (2, 4, 6, 8));
}

BOOST_AUTO_TEST_CASE(lambda_function_with_uint_args)
{
  compute::uint_ host_data[] = { 1, 3, 5, 7, 9 };
  compute::vector<compute::uint_> device_vector(host_data, host_data + 5, queue);

  using boost::compute::lambda::clamp;
  using compute::lambda::_1;

  compute::transform(
    device_vector.begin(), device_vector.end(),
    device_vector.begin(),
    clamp(_1, compute::uint_(4), compute::uint_(6)),
    queue
  );
  CHECK_RANGE_EQUAL(compute::uint_, 5, device_vector, (4, 4, 5, 6, 6));
}

BOOST_AUTO_TEST_CASE(lambda_function_with_short_args)
{
  compute::short_ host_data[] = { 1, 3, 5, 7, 9 };
  compute::vector<compute::short_> device_vector(host_data, host_data + 5, queue);

  using boost::compute::lambda::clamp;
  using compute::lambda::_1;

  compute::transform(
    device_vector.begin(), device_vector.end(),
    device_vector.begin(),
    clamp(_1, compute::short_(4), compute::short_(6)),
    queue
  );
  CHECK_RANGE_EQUAL(compute::short_, 5, device_vector, (4, 4, 5, 6, 6));
}

BOOST_AUTO_TEST_CASE(lambda_function_with_uchar_args)
{
  compute::uchar_ host_data[] = { 1, 3, 5, 7, 9 };
  compute::vector<compute::uchar_> device_vector(host_data, host_data + 5, queue);

  using boost::compute::lambda::clamp;
  using compute::lambda::_1;

  compute::transform(
    device_vector.begin(), device_vector.end(),
    device_vector.begin(),
    clamp(_1, compute::uchar_(4), compute::uchar_(6)),
    queue
  );
  CHECK_RANGE_EQUAL(compute::uchar_, 5, device_vector, (4, 4, 5, 6, 6));
}

BOOST_AUTO_TEST_CASE(lambda_function_with_char_args)
{
  compute::char_ host_data[] = { 1, 3, 5, 7, 9 };
  compute::vector<compute::char_> device_vector(host_data, host_data + 5, queue);

  using boost::compute::lambda::clamp;
  using compute::lambda::_1;

  compute::transform(
    device_vector.begin(), device_vector.end(),
    device_vector.begin(),
    clamp(_1, compute::char_(4), compute::char_(6)),
    queue
  );
  CHECK_RANGE_EQUAL(compute::char_, 5, device_vector, (4, 4, 5, 6, 6));
}

BOOST_AUTO_TEST_SUITE_END()
