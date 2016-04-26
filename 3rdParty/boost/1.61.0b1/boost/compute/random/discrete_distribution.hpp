//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_RANDOM_DISCRETE_DISTRIBUTION_HPP
#define BOOST_COMPUTE_RANDOM_DISCRETE_DISTRIBUTION_HPP

#include <boost/compute/command_queue.hpp>
#include <boost/compute/function.hpp>
#include <boost/compute/algorithm/accumulate.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/detail/literal.hpp>
#include <boost/compute/types/fundamental.hpp>

namespace boost {
namespace compute {

/// \class discrete_distribution
/// \brief Produces random integers on the interval [0, n), where
/// probability of each integer is given by the weight of the ith
/// integer divided by the sum of all weights.
///
/// The following example shows how to setup a discrete distribution to
/// produce 0 and 1 with equal probability
///
/// \snippet test/test_discrete_distribution.cpp generate
///
template<class IntType = uint_>
class discrete_distribution
{
public:
    typedef IntType result_type;

    /// Creates a new discrete distribution with weights given by
    /// the range [\p first, \p last)
    template<class InputIterator>
    discrete_distribution(InputIterator first, InputIterator last)
        : m_n(std::distance(first, last)),
          m_probabilities(std::distance(first, last))
    {
        double sum = 0;

        for(InputIterator iter = first; iter!=last; iter++)
        {
            sum += *iter;
        }

        for(size_t i=0; i<m_n; i++)
        {
            m_probabilities[i] = m_probabilities[i-1] + first[i]/sum;
        }
    }

    /// Destroys the discrete_distribution object.
    ~discrete_distribution()
    {
    }

    /// Returns the value of n
    result_type n() const
    {
        return m_n;
    }

    /// Returns the probabilities
    ::std::vector<double> probabilities() const
    {
        return m_probabilities;
    }

    /// Generates uniformily distributed integers and stores
    /// them to the range [\p first, \p last).
    template<class OutputIterator, class Generator>
    void generate(OutputIterator first,
                  OutputIterator last,
                  Generator &generator,
                  command_queue &queue)
    {
        std::string source = "inline uint scale_random(uint x)\n";

        source = source +
            "{\n" +
            "float rno = convert_float(x) / UINT_MAX;\n";
        for(size_t i=0; i<m_n; i++)
        {
            source = source +
                "if(rno <= " + detail::make_literal<float>(m_probabilities[i]) + ")\n" +
                "   return " + detail::make_literal(i) + ";\n";
        }

        source = source +
            "return " + detail::make_literal(m_n - 1) + ";\n" +
            "}\n";

        BOOST_COMPUTE_FUNCTION(IntType, scale_random, (const uint_ x), {});

        scale_random.set_source(source);

        generator.generate(first, last, scale_random, queue);
    }

private:
    size_t m_n;
    ::std::vector<double> m_probabilities;
};

} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_RANDOM_UNIFORM_INT_DISTRIBUTION_HPP
