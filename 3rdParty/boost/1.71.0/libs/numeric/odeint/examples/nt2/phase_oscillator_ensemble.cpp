//==============================================================================
//         Copyright 2011-2014     Karsten Ahnert
//         Copyright 2011-2014     Mario Mulansky
//         Copyright 2014          LRI    UMR 8623 CNRS/Univ Paris Sud XI
//         Copyright 2014          NumScale SAS
//
//          Distributed under the Boost Software License, Version 1.0.
//                 See accompanying file LICENSE.txt or copy at
//                     http://www.boost.org/LICENSE_1_0.txt
//==============================================================================

#include <iostream>
#include <utility>

#include <boost/numeric/odeint.hpp>

#ifndef M_PI //not there on windows
#define M_PI 3.141592653589793 //...
#endif

#include <boost/random.hpp>
#include <boost/dispatch/meta/as_integer.hpp>

#include <nt2/include/functions/cos.hpp>
#include <nt2/include/functions/sin.hpp>
#include <nt2/include/functions/atan2.hpp>
#include <nt2/table.hpp>
#include <nt2/include/functions/zeros.hpp>
#include <nt2/include/functions/sum.hpp>
#include <nt2/include/functions/mean.hpp>
#include <nt2/arithmetic/include/functions/hypot.hpp>
#include <nt2/include/functions/tie.hpp>

#include <boost/numeric/odeint/external/nt2/nt2_algebra_dispatcher.hpp>


using namespace std;
using namespace boost::numeric::odeint;

template <typename container_type, typename T>
pair< T, T > calc_mean_field( const container_type &x )

{
  T cos_sum = 0.0 , sin_sum = 0.0;

  nt2::tie(cos_sum,sin_sum) = nt2::tie(nt2::mean( nt2::cos(x) ), nt2::mean( nt2::sin(x) ));

  T K = nt2::hypot(sin_sum,cos_sum);
  T Theta = nt2::atan2( sin_sum , cos_sum );

  return make_pair( K , Theta );
}

template <typename container_type, typename T>
struct phase_ensemble
{
  typedef typename boost::dispatch::meta::as_integer<T,unsigned>::type int_type;
  container_type m_omega;
  T m_epsilon;

  phase_ensemble( const int_type n , T g = 1.0 , T epsilon = 1.0 )
  : m_epsilon( epsilon )
  {
    m_omega = nt2::zeros(nt2::of_size(n), nt2::meta::as_<T>());
    create_frequencies( g );
  }

  void create_frequencies( T g )
  {
    boost::mt19937 rng;
    boost::cauchy_distribution<> cauchy( 0.0 , g );
    boost::variate_generator< boost::mt19937&, boost::cauchy_distribution<> > gen( rng , cauchy );
    generate( m_omega.begin() , m_omega.end() , gen );
}

  void set_epsilon( T epsilon ) { m_epsilon = epsilon; }

  T get_epsilon( void ) const { return m_epsilon; }

  void operator()( const container_type &x , container_type &dxdt , T ) const
  {
    pair< T, T > mean = calc_mean_field<container_type,T>( x );
    dxdt = m_omega +  m_epsilon * mean.first * nt2::sin( mean.second - x );
  }
};

template<typename T>
struct statistics_observer
{
    typedef typename boost::dispatch::meta::as_integer<T,unsigned>::type int_type;
    T m_K_mean;
    int_type m_count;

    statistics_observer( void )
    : m_K_mean( 0.0 ) , m_count( 0 ) { }

    template< class State >
    void operator()( const State &x , T t )
    {
        pair< T, T > mean = calc_mean_field<State,T>( x );
        m_K_mean += mean.first;
        ++m_count;
    }

    T get_K_mean( void ) const { return ( m_count != 0 ) ? m_K_mean / T( m_count ) : 0.0 ; }

    void reset( void ) { m_K_mean = 0.0; m_count = 0; }
};

template<typename T>
struct test_ode_table
{
  typedef nt2::table<T> array_type;
  typedef void experiment_is_immutable;

  typedef typename boost::dispatch::meta::as_integer<T,unsigned>::type int_type;

  test_ode_table ( )
                 : size_(16384), ensemble( size_ , 1.0 ), unif( 0.0 , 2.0 * M_PI ), gen( rng , unif ), obs()
  {
    x.resize(nt2::of_size(size_));
  }

  void operator()()
  {
    for( T epsilon = 0.0 ; epsilon < 5.0 ; epsilon += 0.1 )
    {
      ensemble.set_epsilon( epsilon );
      obs.reset();

      // start with random initial conditions
      generate( x.begin() , x.end() , gen );
      // calculate some transients steps
      integrate_const( runge_kutta4< array_type, T >() , boost::ref( ensemble ) , x , T(0.0) , T(10.0) , dt );

      // integrate and compute the statistics
      integrate_const( runge_kutta4< array_type, T >() , boost::ref( ensemble ) , x , T(0.0) , T(100.0) , dt , boost::ref( obs ) );
      cout << epsilon << "\t" << obs.get_K_mean() << endl;
    }
  }

  friend std::ostream& operator<<(std::ostream& os, test_ode_table<T> const& p)
  {
    return os << "(" << p.size() << ")";
  }

  std::size_t size() const { return size_; }

  private:
    std::size_t size_;
    phase_ensemble<array_type,T> ensemble;
    boost::uniform_real<> unif;
    array_type x;
    boost::mt19937 rng;
    boost::variate_generator< boost::mt19937&, boost::uniform_real<> > gen;
    statistics_observer<T> obs;

    static const T dt = 0.1;
};

int main()
{
  std::cout<< " With T = [double] \n";
  test_ode_table<double> test_double;
  test_double();

  std::cout<< " With T = [float] \n";
  test_ode_table<float> test_float;
  test_float();
}
