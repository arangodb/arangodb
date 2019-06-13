//
// Copyright 2007-2012 Christian Henning, Andreas Pokorny
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_DEVICE_HPP
#define BOOST_GIL_IO_DEVICE_HPP

#include <boost/gil/io/base.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/utility/enable_if.hpp>

#include <cstdio>
#include <memory>

namespace boost { namespace gil {

#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400)
#pragma warning(push)
#pragma warning(disable:4512) //assignment operator could not be generated
#endif

namespace detail {

template < typename T > struct buff_item
{
    static const unsigned int size = sizeof( T );
};

template <> struct buff_item< void >
{
    static const unsigned int size = 1;
};

/*!
 * Implements the IODevice concept c.f. to \ref IODevice required by Image libraries like
 * libjpeg and libpng.
 *
 * \todo switch to a sane interface as soon as there is
 * something good in boost. I.E. the IOChains library
 * would fit very well here.
 *
 * This implementation is based on FILE*.
 */
template< typename FormatTag >
class file_stream_device
{
public:

   typedef FormatTag format_tag_t;

public:

    /// Used to overload the constructor.
    struct read_tag {};
    struct write_tag {};

    ///
    /// Constructor
    ///
    file_stream_device( const std::string& file_name
                      , read_tag   = read_tag()
                      )
    {
        FILE* file = NULL;

        io_error_if( ( file = fopen( file_name.c_str(), "rb" )) == NULL
                   , "file_stream_device: failed to open file"
                   );

        _file = file_ptr_t( file
                          , file_deleter
                          );
    }

    ///
    /// Constructor
    ///
    file_stream_device( const char* file_name
                      , read_tag   = read_tag()
                      )
    {
        FILE* file = NULL;

        io_error_if( ( file = fopen( file_name, "rb" )) == NULL
                   , "file_stream_device: failed to open file"
                   );

        _file = file_ptr_t( file
                          , file_deleter
                          );
    }

    ///
    /// Constructor
    ///
    file_stream_device( const std::string& file_name
                      , write_tag
                      )
    {
        FILE* file = NULL;

        io_error_if( ( file = fopen( file_name.c_str(), "wb" )) == NULL
                   , "file_stream_device: failed to open file"
                   );

        _file = file_ptr_t( file
                          , file_deleter
                          );
    }

    ///
    /// Constructor
    ///
    file_stream_device( const char* file_name
                      , write_tag
                      )
    {
        FILE* file = NULL;

        io_error_if( ( file = fopen( file_name, "wb" )) == NULL
                   , "file_stream_device: failed to open file"
                   );

        _file = file_ptr_t( file
                          , file_deleter
                          );
    }

    ///
    /// Constructor
    ///
    file_stream_device( FILE* file )
    : _file( file
           , file_deleter
           )
    {}

    FILE*       get()       { return _file.get(); }
    const FILE* get() const { return _file.get(); }

    int getc_unchecked()
    {
        return std::getc( get() );
    }

    char getc()
    {
        int ch;

        if(( ch = std::getc( get() )) == EOF )
            io_error( "file_stream_device: unexpected EOF" );

        return ( char ) ch;
    }

    ///@todo: change byte_t* to void*
    std::size_t read( byte_t*     data
                    , std::size_t count
                    )
    {
        std::size_t num_elements = fread( data
                                        , 1
                                        , static_cast<int>( count )
                                        , get()
                                        );

        ///@todo: add compiler symbol to turn error checking on and off.
        if(ferror( get() ))
        {
            assert( false );
        }

        //libjpeg sometimes reads blocks in 4096 bytes even when the file is smaller than that.
        //assert( num_elements == count );
        assert( num_elements > 0 );

        return num_elements;
    }

    /// Reads array
    template< typename T
            , int      N
            >
    std::size_t read( T (&buf)[N] )
    {
        return read( buf, N );
    }

    /// Reads byte
    uint8_t read_uint8() throw()
    {
        byte_t m[1];

        read( m );
        return m[0];
    }

    /// Reads 16 bit little endian integer
    uint16_t read_uint16() throw()
    {
        byte_t m[2];

        read( m );
        return (m[1] << 8) | m[0];
    }

    /// Reads 32 bit little endian integer
    uint32_t read_uint32() throw()
    {
        byte_t m[4];

        read( m );
        return (m[3] << 24) | (m[2] << 16) | (m[1] << 8) | m[0];
    }

    /// Writes number of elements from a buffer
    template < typename T >
    std::size_t write( const T*    buf
                     , std::size_t count
                     )
    throw()
    {
        std::size_t num_elements = fwrite( buf
                                         , buff_item<T>::size
                                         , count
                                         , get()
                                         );

        assert( num_elements == count );

        return num_elements;
    }

    /// Writes array
    template < typename    T
             , std::size_t N
             >
    std::size_t write( const T (&buf)[N] ) throw()
    {
        return write( buf, N );
    }

    /// Writes byte
    void write_uint8( uint8_t x ) throw()
    {
        byte_t m[1] = { x };
        write(m);
    }

    /// Writes 16 bit little endian integer
    void write_uint16( uint16_t x ) throw()
    {
        byte_t m[2];

        m[0] = byte_t( x >> 0 );
        m[1] = byte_t( x >> 8 );

        write( m );
    }

    /// Writes 32 bit little endian integer
    void write_uint32( uint32_t x ) throw()
    {
        byte_t m[4];

        m[0] = byte_t( x >>  0 );
        m[1] = byte_t( x >>  8 );
        m[2] = byte_t( x >> 16 );
        m[3] = byte_t( x >> 24 );

        write( m );
    }

    void seek( long count, int whence = SEEK_SET )
    {
        io_error_if( fseek( get()
                          , count
                          , whence
                          ) != 0
                   , "file read error"
                   );
    }

    long int tell()
    {
        long int pos = ftell( get() );

        io_error_if( pos == -1L
                   , "file read error"
                   );

        return pos;
    }

    void flush()
    {
        fflush( get() );
    }

    /// Prints formatted ASCII text
    void print_line( const std::string& line )
    {
        std::size_t num_elements = fwrite( line.c_str()
                                         , sizeof( char )
                                         , line.size()
                                         , get()
                                         );

        assert( num_elements == line.size() );
        boost::ignore_unused(num_elements);
    }

    int error()
    {
        return ferror( get() );
    }

private:

    static void file_deleter( FILE* file )
    {
        if( file )
        {
            fclose( file );
        }
    }

private:

    using file_ptr_t = std::shared_ptr<FILE> ;
    file_ptr_t _file;
};

/**
 * Input stream device
 */
template< typename FormatTag >
class istream_device
{
public:
   istream_device( std::istream& in )
   : _in( in )
   {
       if (!in)
       {
           // does the file exists?
           io_error("Stream is not valid.");
       }
   }

    int getc_unchecked()
    {
        return _in.get();
    }

    char getc()
    {
        int ch;

        if(( ch = _in.get() ) == EOF )
            io_error( "file_stream_device: unexpected EOF" );

        return ( char ) ch;
    }

    std::size_t read( byte_t*     data
                    , std::size_t count )
    {
        std::streamsize cr = 0;

        do
        {
            _in.peek();
            std::streamsize c = _in.readsome( reinterpret_cast< char* >( data )
                                            , static_cast< std::streamsize >( count ));

            count -= static_cast< std::size_t >( c );
            data += c;
            cr += c;

        } while( count && _in );

        return static_cast< std::size_t >( cr );
    }

    /// Reads array
    template< typename T
            , int      N
            >
    size_t read( T (&buf)[N] )
    {
        return read( buf, N );
    }

    /// Reads byte
    uint8_t read_uint8() throw()
    {
        byte_t m[1];

        read( m );
        return m[0];
    }

    /// Reads 16 bit little endian integer
    uint16_t read_uint16() throw()
    {
        byte_t m[2];

        read( m );
        return (m[1] << 8) | m[0];
    }

    /// Reads 32 bit little endian integer
    uint32_t read_uint32() throw()
    {
        byte_t m[4];

        read( m );
        return (m[3] << 24) | (m[2] << 16) | (m[1] << 8) | m[0];
    }

    void seek( long count, int whence = SEEK_SET )
    {
        _in.seekg( count
                 , whence == SEEK_SET ? std::ios::beg
                                      :( whence == SEEK_CUR ? std::ios::cur
                                                            : std::ios::end )
                 );
    }

    void write(const byte_t*, std::size_t)
    {
        io_error( "Bad io error." );
    }

    void flush() {}

private:

    std::istream& _in;
};

/**
 * Output stream device
 */
template< typename FormatTag >
class ostream_device
{
public:
    ostream_device( std::ostream & out )
        : _out( out )
    {
    }

    std::size_t read(byte_t *, std::size_t)
    {
        io_error( "Bad io error." );
        return 0;
    }

    void seek( long count, int whence )
    {
        _out.seekp( count
                  , whence == SEEK_SET
                    ? std::ios::beg
                    : ( whence == SEEK_CUR
                        ?std::ios::cur
                        :std::ios::end )
                  );
    }

    void write( const byte_t* data
              , std::size_t   count )
    {
        _out.write( reinterpret_cast<char const*>( data )
                 , static_cast<std::streamsize>( count )
                 );
    }

    /// Writes array
    template < typename    T
             , std::size_t N
             >
    void write( const T (&buf)[N] ) throw()
    {
        write( buf, N );
    }

    /// Writes byte
    void write_uint8( uint8_t x ) throw()
    {
        byte_t m[1] = { x };
        write(m);
    }

    /// Writes 16 bit little endian integer
    void write_uint16( uint16_t x ) throw()
    {
        byte_t m[2];

        m[0] = byte_t( x >> 0 );
        m[1] = byte_t( x >> 8 );

        write( m );
    }

    /// Writes 32 bit little endian integer
    void write_uint32( uint32_t x ) throw()
    {
        byte_t m[4];

        m[0] = byte_t( x >>  0 );
        m[1] = byte_t( x >>  8 );
        m[2] = byte_t( x >> 16 );
        m[3] = byte_t( x >> 24 );

        write( m );
    }

    void flush()
    {
        _out << std::flush;
    }

    /// Prints formatted ASCII text
    void print_line( const std::string& line )
    {
        _out << line;
    }



private:

    std::ostream& _out;
};


/**
 * Metafunction to detect input devices.
 * Should be replaced by an external facility in the future.
 */
template< typename IODevice  > struct is_input_device : mpl::false_{};
template< typename FormatTag > struct is_input_device< file_stream_device< FormatTag > > : mpl::true_{};
template< typename FormatTag > struct is_input_device<     istream_device< FormatTag > > : mpl::true_{};

template< typename FormatTag
        , typename T
        , typename D = void
        >
struct is_adaptable_input_device : mpl::false_{};

template< typename FormatTag
        , typename T
        >
struct is_adaptable_input_device< FormatTag
                                , T
                                , typename enable_if< mpl::or_< is_base_and_derived< std::istream, T >
                                                              , is_same            < std::istream, T >
                                                              >
                                                    >::type
                                > : mpl::true_
{
    typedef istream_device< FormatTag > device_type;
};

template< typename FormatTag >
struct is_adaptable_input_device< FormatTag
                                , FILE*
                                , void
                                >
    : mpl::true_
{
    typedef file_stream_device< FormatTag > device_type;
};

///
/// Metafunction to decide if a given type is an acceptable read device type.
///
template< typename FormatTag
        , typename T
        , typename D = void
        >
struct is_read_device : mpl::false_
{};

template< typename FormatTag
        , typename T
        >
struct is_read_device< FormatTag
                     , T
                     , typename enable_if< mpl::or_< is_input_device< FormatTag >
                                                   , is_adaptable_input_device< FormatTag
                                                                              , T
                                                                              >
                                                   >
                                         >::type
                     > : mpl::true_
{};


/**
 * Metafunction to detect output devices.
 * Should be replaced by an external facility in the future.
 */
template<typename IODevice> struct is_output_device : mpl::false_{};

template< typename FormatTag > struct is_output_device< file_stream_device< FormatTag > > : mpl::true_{};
template< typename FormatTag > struct is_output_device< ostream_device    < FormatTag > > : mpl::true_{};

template< typename FormatTag
        , typename IODevice
        , typename D = void
        >
struct is_adaptable_output_device : mpl::false_ {};

template< typename FormatTag
        , typename T
        > struct is_adaptable_output_device< FormatTag
                                           , T
                                           , typename enable_if< mpl::or_< is_base_and_derived< std::ostream, T >
                                                                         , is_same            < std::ostream, T >
                                                                         >
                                           >::type
        > : mpl::true_
{
    typedef ostream_device< FormatTag > device_type;
};

template<typename FormatTag> struct is_adaptable_output_device<FormatTag,FILE*,void>
  : mpl::true_
{
    typedef file_stream_device< FormatTag > device_type;
};


///
/// Metafunction to decide if a given type is an acceptable read device type.
///
template< typename FormatTag
        , typename T
        , typename D = void
        >
struct is_write_device : mpl::false_
{};

template< typename FormatTag
        , typename T
        >
struct is_write_device< FormatTag
                      , T
                      , typename enable_if< mpl::or_< is_output_device< FormatTag >
                                                    , is_adaptable_output_device< FormatTag
                                                                                , T
                                                                                >
                                                    >
                                          >::type
                      > : mpl::true_
{};

} // namespace detail

template< typename Device, typename FormatTag > class scanline_reader;
template< typename Device, typename FormatTag, typename ConversionPolicy > class reader;

template< typename Device, typename FormatTag, typename Log = no_log > class writer;

template< typename Device, typename FormatTag > class dynamic_image_reader;
template< typename Device, typename FormatTag, typename Log = no_log > class dynamic_image_writer;


namespace detail {

template< typename T >
struct is_reader : mpl::false_
{};

template< typename Device
        , typename FormatTag
        , typename ConversionPolicy
        >
struct is_reader< reader< Device
                        , FormatTag
                        , ConversionPolicy
                        >
                > : mpl::true_
{};

template< typename T >
struct is_dynamic_image_reader : mpl::false_
{};

template< typename Device
        , typename FormatTag
        >
struct is_dynamic_image_reader< dynamic_image_reader< Device
                                                    , FormatTag
                                                    >
                              > : mpl::true_
{};

template< typename T >
struct is_writer : mpl::false_
{};

template< typename Device
        , typename FormatTag
        >
struct is_writer< writer< Device
                        , FormatTag
                        >
                > : mpl::true_
{};

template< typename T >
struct is_dynamic_image_writer : mpl::false_
{};

template< typename Device
        , typename FormatTag
        >
struct is_dynamic_image_writer< dynamic_image_writer< Device
                                                    , FormatTag
                                                    >
                > : mpl::true_
{};

} // namespace detail

#if BOOST_WORKAROUND(BOOST_MSVC, >= 1400)
#pragma warning(pop)
#endif

} // namespace gil
} // namespace boost

#endif
