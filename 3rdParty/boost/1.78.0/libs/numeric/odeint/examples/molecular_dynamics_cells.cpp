/*
 [auto_generated]
 libs/numeric/odeint/examples/molecular_dynamics_cells.cpp

 [begin_description]
 Molecular dynamics example with cells.
 [end_description]

 Copyright 2009-2012 Karsten Ahnert
 Copyright 2009-2012 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/numeric/odeint.hpp>

#include <cstddef>
#include <vector>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <iostream>
#include <random>

#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm/sort.hpp>
#include <boost/range/algorithm/unique_copy.hpp>
#include <boost/range/algorithm_ext/iota.hpp>
#include <boost/iterator/zip_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/permutation_iterator.hpp>
#include <boost/iterator/counting_iterator.hpp>

#include "point_type.hpp"








struct local_force
{
    double m_gamma;        // friction
    local_force( double gamma = 0.0 ) : m_gamma( gamma ) { }
    template< typename Point >
    Point operator()( Point& x , Point& v ) const
    {
        return - m_gamma * v;
    }
};


struct lennard_jones
{
    double m_sigma;
    double m_eps;
    lennard_jones( double sigma = 1.0 , double eps = 0.1 ) : m_sigma( sigma ) , m_eps( eps ) { }
    double operator()( double r ) const
    {
        double c = m_sigma / r;
        double c3 = c * c * c;
        double c6 = c3 * c3;
        return 4.0 * m_eps * ( -12.0 * c6 * c6 / r + 6.0 * c6 / r );
    }
};

template< typename F >
struct conservative_interaction
{
    F m_f;
    conservative_interaction( F const &f = F() ) : m_f( f ) { }
    template< typename Point >
    Point operator()( Point const& x1 , Point const& x2 ) const
    {
        Point diff = x1 - x2;
        double r = abs( diff );
        double f = m_f( r );
        return - diff / r * f; 
    }
};

template< typename F >
conservative_interaction< F > make_conservative_interaction( F const &f )
{
    return conservative_interaction< F >( f );
}







// force = interaction( x1 , x2 )
// force = local_force( x , v )
template< typename LocalForce , typename Interaction >
class md_system_bs
{
public:
    
    typedef std::vector< double > vector_type;
    typedef point< double , 2 > point_type;
    typedef point< int , 2 > index_type;
    typedef std::vector< point_type > point_vector;
    typedef std::vector< index_type > index_vector;
    typedef std::vector< size_t > hash_vector;
    typedef LocalForce local_force_type;
    typedef Interaction interaction_type;


    struct params
    {
        size_t n;
        size_t n_cell_x , n_cell_y , n_cells;
        double x_max , y_max , cell_size;
        double eps , sigma;    // interaction strength, interaction radius
        interaction_type interaction;
        local_force_type local_force;
    };


    struct cell_functor
    {
        params const &m_p;

        cell_functor( params const& p ) : m_p( p ) { }
        
        template< typename Tuple >
        void operator()( Tuple const& t ) const
        {
            auto point = boost::get< 0 >( t );
            size_t i1 = size_t( point[0] / m_p.cell_size ) , i2 = size_t( point[1] / m_p.cell_size );
            boost::get< 1 >( t ) = index_type( i1 , i2 );
            boost::get< 2 >( t ) = hash_func( boost::get< 1 >( t ) , m_p );
        }
    };



    struct transform_functor
    {
        typedef size_t argument_type;
        typedef size_t result_type;
        hash_vector const* m_index;
        transform_functor( hash_vector const& index ) : m_index( &index ) { }
        size_t operator()( size_t i ) const { return (*m_index)[i]; }
    };



    struct interaction_functor
    {
        hash_vector const &m_cells_begin;
        hash_vector const &m_cells_end;
        hash_vector const &m_order;
        point_vector const &m_x;
        point_vector const &m_v;
        params const &m_p;
        size_t m_ncellx , m_ncelly;
        
        interaction_functor( hash_vector const& cells_begin , hash_vector const& cells_end , hash_vector pos_order ,
                            point_vector const&x , point_vector const& v , params const &p )
        : m_cells_begin( cells_begin ) , m_cells_end( cells_end ) , m_order( pos_order ) , m_x( x ) , m_v( v ) ,
        m_p( p ) { }
        
        template< typename Tuple >
        void operator()( Tuple const &t ) const
        {
            point_type x = periodic_bc( boost::get< 0 >( t ) , m_p ) , v = boost::get< 1 >( t );
            index_type index = boost::get< 3 >( t );
            size_t pos_hash = boost::get< 4 >( t );

            point_type a = m_p.local_force( x , v );

            for( int i=-1 ; i<=1 ; ++i )
            {
                for( int j=-1 ; j<=1 ; ++j )
                {
                    index_type cell_index = index + index_type( i , j );
                    size_t cell_hash = hash_func( cell_index , m_p );
                    for( size_t ii = m_cells_begin[ cell_hash ] ; ii < m_cells_end[ cell_hash ] ; ++ii )
                    {
                        if( m_order[ ii ] == pos_hash ) continue;
                        point_type x2 = periodic_bc( m_x[ m_order[ ii ] ] , m_p );

                        if( cell_index[0] >= m_p.n_cell_x ) x2[0] += m_p.x_max;
                        if( cell_index[0] < 0 ) x2[0] -= m_p.x_max;
                        if( cell_index[1] >= m_p.n_cell_y ) x2[1] += m_p.y_max;
                        if( cell_index[1] < 0 ) x2[1] -= m_p.y_max;

                        a += m_p.interaction( x , x2 );
                    }
                }
            }
            boost::get< 2 >( t ) = a;
        }
    };




    md_system_bs( size_t n ,
                  local_force_type const& local_force = local_force_type() ,                  
                  interaction_type const& interaction = interaction_type() ,
                  double xmax = 100.0 , double ymax = 100.0 , double cell_size = 2.0 )
    : m_p() 
    {
        m_p.n = n;
        m_p.x_max = xmax;
        m_p.y_max = ymax;
        m_p.interaction = interaction;
        m_p.local_force = local_force;
        m_p.n_cell_x = size_t( xmax / cell_size );
        m_p.n_cell_y = size_t( ymax / cell_size );
        m_p.n_cells = m_p.n_cell_x * m_p.n_cell_y;
        m_p.cell_size = cell_size;
    }
    
    void init_point_vector( point_vector &x ) const { x.resize( m_p.n ); }
    
    void operator()( point_vector const& x , point_vector const& v , point_vector &a , double t ) const
    {
        // init
        hash_vector pos_hash( m_p.n , 0 );
        index_vector pos_index( m_p.n );
        hash_vector pos_order( m_p.n , 0 );
        hash_vector cells_begin( m_p.n_cells ) , cells_end( m_p.n_cells ) , cell_order( m_p.n_cells );

        boost::iota( pos_order , 0 );
        boost::iota( cell_order , 0 );

        // calculate grid hash
        // calcHash( m_dGridParticleHash, m_dGridParticleIndex, dPos, m_numParticles);
        std::for_each(
            boost::make_zip_iterator( boost::make_tuple( x.begin() , pos_index.begin() , pos_hash.begin() ) ) ,
            boost::make_zip_iterator( boost::make_tuple( x.end() , pos_index.end() , pos_hash.end() ) ) ,
            cell_functor( m_p ) );

//         // sort particles based on hash
//         // sortParticles(m_dGridParticleHash, m_dGridParticleIndex, m_numParticles);        
        boost::sort( pos_order , [&]( size_t i1 , size_t i2 ) -> bool {
            return pos_hash[i1] < pos_hash[i2]; } );


        
        // reorder particle arrays into sorted order and find start and end of each cell
        std::for_each( cell_order.begin() , cell_order.end() , [&]( size_t i ) {
            auto pos_begin = boost::make_transform_iterator( pos_order.begin() , transform_functor( pos_hash ) );
            auto pos_end = boost::make_transform_iterator( pos_order.end() , transform_functor( pos_hash ) );
            cells_begin[ i ] = std::distance( pos_begin , std::lower_bound( pos_begin , pos_end , i ) );
            cells_end[ i ] = std::distance( pos_begin , std::upper_bound( pos_begin , pos_end , i ) );
        } );
        
        std::for_each(
            boost::make_zip_iterator( boost::make_tuple(
                x.begin() ,
                v.begin() ,
                a.begin() ,
                pos_index.begin() ,
                boost::counting_iterator< size_t >( 0 )
            ) ) ,
            boost::make_zip_iterator( boost::make_tuple(
                x.end() ,
                v.end() ,
                a.end() ,
                pos_index.end() ,
                boost::counting_iterator< size_t >( m_p.n )
            ) ) ,
            interaction_functor( cells_begin , cells_end , pos_order , x , v , m_p ) );
    }

    void bc( point_vector &x )
    {
        for( size_t i=0 ; i<m_p.n ; ++i )
        {
            x[i][0] = periodic_bc( x[ i ][0] , m_p.x_max );
            x[i][1] = periodic_bc( x[ i ][1] , m_p.y_max );
        }
    }
    
    static inline double periodic_bc( double x , double xmax )
    {
        double tmp = x - xmax * int( x / xmax );
        return tmp >= 0.0 ? tmp : tmp + xmax;
    }


    static inline point_type periodic_bc( point_type const& x , params const& p ) 
    {
        return point_type( periodic_bc( x[0] , p.x_max ) , periodic_bc( x[1] , p.y_max ) );
    }


    static inline int check_interval( int i , int max )
    {
        int tmp = i % max;
        return tmp >= 0 ? tmp : tmp + max;
    }


    static inline size_t hash_func( index_type index , params const & p )
    {
        size_t i1 = check_interval( index[0] , p.n_cell_x );
        size_t i2 = check_interval( index[1] , p.n_cell_y );
        return i1 * p.n_cell_y + i2;
    }
    
    params m_p;
};


template< typename LocalForce , typename Interaction >
md_system_bs< LocalForce , Interaction > make_md_system_bs( size_t n , LocalForce const &f1 , Interaction const &f2 ,
    double xmax = 100.0 , double ymax = 100.0 , double cell_size = 2.0 )
{
    return md_system_bs< LocalForce , Interaction >( n , f1 , f2 , xmax , ymax , cell_size );
}






using namespace boost::numeric::odeint;



int main( int argc , char *argv[] )
{
    const size_t n1 = 32;
    const size_t n2 = 32;
    const size_t n = n1 * n2;
    auto sys = make_md_system_bs( n , local_force() , make_conservative_interaction( lennard_jones() ) , 100.0 , 100.0 , 2.0 );
    typedef decltype( sys ) system_type;
    typedef system_type::point_vector point_vector;
    
    std::mt19937 rng;
    std::normal_distribution<> dist( 0.0 , 1.0 );
    
    point_vector x , v;
    sys.init_point_vector( x );
    sys.init_point_vector( v );
    
    for( size_t i=0 ; i<n1 ; ++i )
    {
        for( size_t j=0 ; j<n2 ; ++j )
        {
            size_t index = i * n2 + j; 
            x[index][0] = 10.0 + i * 2.0 ;
            x[index][1] = 10.0 + j * 2.0 ;
            v[index][0] = dist( rng ) ;
            v[index][1] = dist( rng ) ;
        }
    }
    
    velocity_verlet< point_vector > stepper;
    const double dt = 0.025;
    double t = 0.0;
    // std::cout << "set term x11" << endl;
    for( size_t oi=0 ; oi<10000 ; ++oi )
    {
        for( size_t ii=0 ; ii<50 ; ++ii,t+=dt )
            stepper.do_step( sys , std::make_pair( std::ref( x ) , std::ref( v ) ) , t , dt );
        sys.bc( x );
        
        std::cout << "set size square" << "\n";
        std::cout << "unset key" << "\n";
        std::cout << "p [0:" << sys.m_p.x_max << "][0:" << sys.m_p.y_max << "] '-' pt 7 ps 0.5" << "\n";
        for( size_t i=0 ; i<n ; ++i )
            std::cout << x[i][0] << " " << x[i][1] << " " << v[i][0] << " " << v[i][1] << "\n";
        std::cout << "e" << std::endl;
    }

    return 0;
}
