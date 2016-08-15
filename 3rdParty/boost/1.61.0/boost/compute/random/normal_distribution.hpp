//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_RANDOM_NORMAL_DISTRIBUTION_HPP
#define BOOST_COMPUTE_RANDOM_NORMAL_DISTRIBUTION_HPP

#include <limits>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/function.hpp>
#include <boost/compute/types/fundamental.hpp>
#include <boost/compute/type_traits/make_vector_type.hpp>

namespace boost {
namespace compute {

/// \class normal_distribution
/// \brief Produces random, normally-distributed floating-point numbers.
///
/// The following example shows how to setup a normal distribution to
/// produce random \c float values centered at \c 5:
///
/// \snippet test/test_normal_distribution.cpp generate
///
/// \see default_random_engine, uniform_real_distribution
template<class RealType = float>
class normal_distribution
{
public:
    typedef RealType result_type;

    /// Creates a new normal distribution producing numbers with the given
    /// \p mean and \p stddev.
    normal_distribution(RealType mean = 0.f, RealType stddev = 1.f)
        : m_mean(mean),
          m_stddev(stddev)
    {
    }

    /// Destroys the normal distribution object.
    ~normal_distribution()
    {
    }

    /// Returns the mean value of the distribution.
    result_type mean() const
    {
        return m_mean;
    }

    /// Returns the standard-deviation of the distribution.
    result_type stddev() const
    {
        return m_stddev;
    }

    /// Returns the minimum value of the distribution.
    result_type min BOOST_PREVENT_MACRO_SUBSTITUTION () const
    {
        return -std::numeric_limits<RealType>::infinity();
    }

    /// Returns the maximum value of the distribution.
    result_type max BOOST_PREVENT_MACRO_SUBSTITUTION () const
    {
        return std::numeric_limits<RealType>::infinity();
    }

    /// Generates normally-distributed floating-point numbers and stores
    /// them to the range [\p first, \p last).
    template<class OutputIterator, class Generator>
    void generate(OutputIterator first,
                  OutputIterator last,
                  Generator &generator,
                  command_queue &queue)
    {
        typedef typename make_vector_type<RealType, 2>::type RealType2;

        size_t count = detail::iterator_range_size(first, last);

        vector<uint_> tmp(count, queue.get_context());
        generator.generate(tmp.begin(), tmp.end(), queue);

        BOOST_COMPUTE_FUNCTION(RealType2, box_muller, (const uint2_ x),
        {
            const RealType x1 = x.x / (RealType) (UINT_MAX - 1);
            const RealType x2 = x.y / (RealType) (UINT_MAX - 1);

            const RealType z1 = sqrt(-2.f * log2(x1)) * cos(2.f * M_PI_F * x2);
            const RealType z2 = sqrt(-2.f * log2(x1)) * sin(2.f * M_PI_F * x2);

            return (RealType2)(MEAN, MEAN) + (RealType2)(z1, z2) * (RealType2)(STDDEV, STDDEV);
        });

        box_muller.define("MEAN", boost::lexical_cast<std::string>(m_mean));
        box_muller.define("STDDEV", boost::lexical_cast<std::string>(m_stddev));
        box_muller.define("RealType", type_name<RealType>());
        box_muller.define("RealType2", type_name<RealType2>());

        transform(
            make_buffer_iterator<uint2_>(tmp.get_buffer(), 0),
            make_buffer_iterator<uint2_>(tmp.get_buffer(), count / 2),
            make_buffer_iterator<RealType2>(first.get_buffer(), 0),
            box_muller,
            queue
        );
    }

private:
    RealType m_mean;
    RealType m_stddev;
};

} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_RANDOM_NORMAL_DISTRIBUTION_HPP
