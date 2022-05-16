//  Boost CRC library documentation examples file  ---------------------------//

//  Copyright 2012 Daryle Walker.
//  Distributed under the Boost Software License, Version 1.0.  (See the
//  accompanying file LICENSE_1_0.txt or a copy at
//  <http://www.boost.org/LICENSE_1_0.txt>.)

//  See <http://www.boost.org/libs/crc/> for the library's home page.

#include "boost/crc.hpp"
#include <cstdarg>
#include <cstddef>
#include <utility>

//[ crc_basic_reuse
//` Here's an example of reuse:
std::pair<unsigned, unsigned> crc_16_and_xmodem( void const *b, std::size_t l )
{
    std::pair<unsigned, unsigned>  result;
    /*<< The parameters are based on `boost::crc_16_type`. >>*/
    boost::crc_basic<16>           crc1( 0x8005u, 0u, 0u, true, true );

    crc1.process_bytes( b, l );
    result.first = crc1.checksum();
    /*<< Change the parameters to match `boost::crc_xmodem_type`. >>*/
    crc1 = boost::crc_basic<16>( 0x8408u, crc1.get_initial_remainder(),
     crc1.get_final_xor_value(), crc1.get_reflect_input(),
     crc1.get_reflect_remainder() );
    crc1.process_bytes( b, l );
    result.second = crc1.checksum();

    return result;
}
/*`
For now, most __RMCA__ parameters can only be changed through assignment to the
entire object.
*/
//]

//[ crc_piecemeal_run
//` Persistent objects mean that the data doesn't have to be in one block:
unsigned  combined_crc_16( unsigned block_count, ... )
{
    /*<< C-style variable-argument routines are or may be macros. >>*/
    using namespace std;

    /*<< The parameters are based on `boost::crc_16_type`. >>*/
    boost::crc_basic<16>  crc1( 0x8005u, 0u, 0u, true, true );
    va_list               ap;

    va_start( ap, block_count );
    while ( block_count-- )
    {
        void const * const  bs = va_arg( ap, void const * );
        size_t const        bl = va_arg( ap, size_t );

        /*<< The `va_arg` calls were within the `process_bytes` call, but I
         remembered that calling order is not guaranteed among function
         arguments, so I need explicit object declarations to enforce the
         extraction order. >>*/
        crc1.process_bytes( bs, bl );
    }
    va_end( ap );

    return crc1.checksum();
}
/*` No CRC operation throws, so there is no need for extra protection between
    the varargs macro calls.
*/
//]

//[ acrc_piecemeal_run
//` The `augmented_crc` function can compute CRCs from distributed data, too:
unsigned  combined_acrc_16( int block_count, ... )
{
    /*<< C-style variable-argument routines are or may be macros. >>*/
    using namespace std;

    va_list   ap;
    unsigned  result = 0u;

    va_start( ap, block_count );
    if ( block_count <= 0 )
        goto finish;

    void const *  bs = va_arg( ap, void const * );
    size_t        bl = va_arg( ap, size_t );

    /*<< The parameters are based on `boost::crc_xmodem_t`. >>*/
    result = boost::augmented_crc<16, 0x1021u>( bs, bl );
    while ( --block_count )
    {
        bs = va_arg( ap, void const * );
        bl = va_arg( ap, size_t );
        result = boost::augmented_crc<16, 0x1021u>( bs, bl, result );
    }

finish:
    va_end( ap );
    return result;
}
/*` No CRC operation throws, so there is no need for extra protection between
    the varargs macro calls.  Feeding the result from the previous run as the
    initial remainder for the next run works easily because there's no output
    reflection or XOR mask.
*/
//]

/*<-*/ int  main()  {} /*->*/
