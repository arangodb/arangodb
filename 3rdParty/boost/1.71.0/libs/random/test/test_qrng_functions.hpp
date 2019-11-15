// Copyright Justinas Vygintas Daugmaudis, 2010-2018.
// Use, modification and distribution is subject to the
// Boost Software License, Version 1.0. (See accompanying
// file LICENSE-1.0 or http://www.boost.org/LICENSE-1.0)

#ifndef TEST_QRNG_FUNCTIONS_HPP_INCLUDED
#define TEST_QRNG_FUNCTIONS_HPP_INCLUDED

#include <boost/random/uniform_real.hpp>
#include <boost/test/floating_point_comparison.hpp>

#include <sstream>

namespace test {

// Invokes operator() precisely n times. This is to check that
// Engine::discard(n) actually has the same effect.
template<typename Engine>
inline void trivial_discard(Engine& eng, boost::uintmax_t n)
{
  for( ; n != 0; --n ) eng();
}


template<typename Engine, typename T, std::size_t Dimension>
inline void match_vector(Engine& eng, T (&pt)[Dimension])
{
  BOOST_REQUIRE_EQUAL( eng.dimension(), Dimension ); // paranoid check

  boost::uniform_real<T> dist;

  for( std::size_t i = 0; i != eng.dimension(); ++i )
  {
    T val = dist(eng);
    // We want to check that quasi-random number generator values differ no
    // more than 0.0006% of their value.
    BOOST_CHECK_CLOSE(pt[i], val, 0.0006);
  }
}


template<typename Engine, typename T, std::size_t Dimension, std::size_t N>
inline void expected_values(T (&pt)[N][Dimension], std::size_t skip)
{
  Engine eng(Dimension);

  eng.seed(skip);

  for( std::size_t i = 0; i != N; ++i )
    match_vector(eng, pt[i]);
}

template<typename Engine, typename T>
inline void test_zero_seed(std::size_t dimension)
{
  Engine eng(dimension);

  Engine other(dimension);
  other.seed(0);

  // Check that states are equal after zero seed.
  boost::uniform_real<T> dist;
  BOOST_CHECK( eng == other );
  for( std:: size_t i = 0; i != dimension; ++i )
  {
    T q_val = dist(eng);
    T t_val = dist(other);
    BOOST_CHECK_CLOSE(q_val, t_val, 0.0001);
  }
}

template<typename Engine, typename T, std::size_t Dimension, std::size_t N>
inline void seed_function(T (&pt)[N][Dimension], std::size_t skip)
{
  // Test zero seed before doing other tests.
  test_zero_seed<Engine, T>(Dimension);

  Engine eng(Dimension);
  for( std::size_t i = 0; i != N; ++i )
  {
    // For all N seeds an engine
    // and checks if the expected values match.
    eng.seed(skip + i);
    match_vector(eng, pt[i]);
  }
}

template<typename Engine, typename T, std::size_t Dimension, std::size_t N>
inline void discard_function(T (&pt)[N][Dimension], std::size_t skip)
{
  Engine eng(Dimension), trivial(Dimension), streamed(Dimension), initial_state(Dimension);
  boost::uniform_real<T> dist;

  const std::size_t element_count = N * Dimension;
  const T* pt_array = reinterpret_cast<T *>(boost::addressof(pt));

  std::stringstream ss;

  initial_state.seed(skip);
  for (std::size_t step = 0; step != element_count; ++step)
  {
    // Init to the same state
    eng = initial_state;
    trivial = initial_state;

    // Discards have to have the same effect
    eng.discard(step);
    trivial_discard(trivial, step);

    // test serialization to stream
    ss.str(std::string()); // clear stream
    ss << eng;
    ss >> streamed;

    // Therefore, states are equal
    BOOST_CHECK( eng == trivial );
    BOOST_CHECK( eng == streamed );

    // Now, let's check whether they really produce the same sequence
    T q_val = dist(eng);
    T t_val = dist(trivial);
    T s_val = dist(streamed);
    BOOST_CHECK_CLOSE(q_val, t_val, 0.0001);
    BOOST_CHECK_CLOSE(q_val, s_val, 0.0001);
    // ~ BOOST_CHECK(q_val == t_val), but those are floating point values,
    // so strict equality check may fail unnecessarily

    // States remain equal!
    BOOST_CHECK( eng == trivial );
    BOOST_CHECK( eng == streamed );

    // We want to check that quasi-random number generator values differ no
    // more than 0.0006% of their value.
    BOOST_CHECK_CLOSE(pt_array[step], q_val, 0.0006);
  }
}

inline bool accept_all_exceptions(const std::exception& e)
{
  BOOST_TEST_MESSAGE( e.what() );
  return true;
}

template<typename Engine>
void test_max_seed(std::size_t dim)
{
  typedef typename Engine::size_type size_type;
  static const size_type maxseed = Engine::max();

  Engine eng(dim);
  eng.seed(maxseed-1); // must succeed
  eng(); // skip one element
  BOOST_REQUIRE_EXCEPTION( eng.seed(maxseed), std::range_error, accept_all_exceptions );

  Engine other(dim);
  other.seed(maxseed-1); // must succeed
  other(); // skip one element, too

  // States remain the same even after unsuccessful seeding for eng.
  BOOST_CHECK( eng == other );
  BOOST_CHECK( eng() == other() );
}

template<typename Generator>
void test_max_discard(std::size_t dim)
{
  typedef typename Generator::type engine_type;

  static const boost::uintmax_t maxdiscard = dim * engine_type::max();

  // Max discard limit
  {
    engine_type eng(dim);
    eng.discard(maxdiscard-1); // must succeed
    eng(); // the very last element

    BOOST_REQUIRE_EXCEPTION( eng(), std::range_error, accept_all_exceptions );

    engine_type other(dim);

    BOOST_CHECK( eng != other ); // test that comparison does not overflow

    other(); // the very first element
    other.discard(maxdiscard-1); // must succeed

    BOOST_CHECK( eng == other );

    BOOST_REQUIRE_EXCEPTION( other(), std::range_error, accept_all_exceptions );
  }

  // Overdiscarding
  {
    engine_type eng(dim);
    eng.discard(maxdiscard); // must succeed, since it's maxdiscard operator() invocations

    // must fail because after discarding the whole sequence
    // we can't put the eng to any valid sequence producing state
    BOOST_REQUIRE_EXCEPTION( eng(), std::range_error, accept_all_exceptions );

    // Plain overdiscarding by 1
    engine_type other(dim);
    BOOST_REQUIRE_EXCEPTION( other.discard(maxdiscard+1), std::range_error, accept_all_exceptions );
  }

  // Test wraparound
  {
    engine_type eng(dim);

    // must fail either because seq_count overflow check is triggered,
    // or because this discard violates seeding bitcount constraint
    BOOST_REQUIRE_EXCEPTION( eng.discard(maxdiscard*2), std::range_error, accept_all_exceptions );
   }
}

} // namespace test


#define QRNG_VALIDATION_TEST_FUNCTIONS(QRNG) \
\
typedef boost::random::QRNG engine_t; \
\
template<typename T, std::size_t Dimension, std::size_t N> \
inline void test_##QRNG##_values(T (&pt)[N][Dimension], std::size_t skip) \
{ \
  test::expected_values<engine_t>(pt, skip); \
} \
\
template<typename T, std::size_t Dimension, std::size_t N> \
inline void test_##QRNG##_seed(T (&pt)[N][Dimension], std::size_t skip) \
{ \
  test::seed_function<engine_t>(pt, skip); \
} \
\
template<typename T, std::size_t Dimension, std::size_t N> \
inline void test_##QRNG##_discard(T (&pt)[N][Dimension], std::size_t skip) \
{ \
  test::discard_function<engine_t>(pt, skip); \
} \
inline void test_##QRNG##_max_seed() \
{ \
  test::test_max_seed<engine_t>(2); \
} \
inline void test_##QRNG##_max_dimension(std::size_t dim) \
{ \
  engine_t eng(dim); /*must succeed*/ \
  BOOST_REQUIRE_EXCEPTION( engine_t(dim+1), std::invalid_argument, test::accept_all_exceptions ); \
} \
\
BOOST_AUTO_TEST_CASE( test_##QRNG##_zero_dimension_fails ) \
{ \
  BOOST_REQUIRE_EXCEPTION( engine_t(0), std::invalid_argument, test::accept_all_exceptions ); \
} \
/**/

#define QRNG_VALIDATION_TEST_DISCARD(QRNG) \
\
template <typename IntType, unsigned w> \
struct gen_engine \
{ \
  typedef boost::random::QRNG##_engine<IntType, w> type; \
}; \
\
inline void test_##QRNG##_max_discard() \
{ \
  static const std::size_t dim = 2;\
   \
   /* test full 8 bits */ \
  test::test_max_discard<gen_engine<boost::uint8_t, 8u> >(dim); \
  \
  /* test 7 bits */ \
  test::test_max_discard<gen_engine<boost::uint8_t, 7u> >(dim); \
  \
  /* test 6 bits for a good measure */ \
  test::test_max_discard<gen_engine<boost::uint8_t, 6u> >(dim); \
} \
/**/

#endif // TEST_QRNG_FUNCTIONS_HPP_INCLUDED
