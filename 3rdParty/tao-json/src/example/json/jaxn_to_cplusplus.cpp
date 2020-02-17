// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#include <iostream>

#include <tao/json.hpp>

namespace tao::json::cplusplus::events
{
   struct to_pretty_stream
   {
   protected:
      std::ostream& os;
      char buffer[ 32 ];
      const std::size_t indent;
      const std::string eol;

      std::size_t current_indent = 0;

      bool first = true;
      bool after_key = true;

      void next_line()
      {
         os << eol;
         std::size_t len = current_indent;
         while( len != 0 ) {
            const auto chunk = ( std::min )( indent, sizeof( buffer ) );
            os.write( buffer, chunk );
            len -= chunk;
         }
      }

      void next()
      {
         if( !first ) {
            os.put( ',' );
         }
         if( after_key ) {
            after_key = false;
         }
         else {
            next_line();
         }
      }

   public:
      to_pretty_stream( std::ostream& in_os, const std::size_t in_indent )
         : os( in_os ),
           buffer(),
           indent( in_indent ),
           eol( "\n" )
      {
         std::memset( buffer, os.fill(), sizeof( buffer ) );
      }

      void null()
      {
         next();
         os << "tao::json::null";
      }

      void boolean( const bool v )
      {
         next();
         if( v ) {
            os.write( "true", 4 );
         }
         else {
            os.write( "false", 5 );
         }
      }

      void number( const std::int64_t v )
      {
         next();
         itoa::i64tos( os, v );
      }

      void number( const std::uint64_t v )
      {
         next();
         itoa::u64tos( os, v );
      }

      void number( const double v )
      {
         next();
         if( !std::isfinite( v ) ) {
            if( std::isnan( v ) ) {
               os.write( "NAN", 3 );
            }
            else if( v < 0 ) {
               os.write( "-INFINITY", 9 );
            }
            else {
               os.write( "INFINITY", 8 );
            }
         }
         else {
            ryu::d2s_stream( os, v );
         }
      }

      void string( const std::string_view v )
      {
         next();
         string_impl( v );
      }

      void binary( const tao::binary_view v )
      {
         next();
         os << "tao::binary({ ";
         if( !v.empty() ) {
            os << "std::byte(" << int( v[ 0 ] ) << ")";
            for( std::size_t j = 1; j < v.size(); ++j ) {
               os << ", std::byte(" << int( v[ j ] ) << ")";
            }
         }
         os.write( " })", 3 );
      }

      void begin_array( const std::size_t size = 1 )
      {
         next();
         if( size == 0 ) {
            os << "tao::json::empty_array";
            return;
         }
         os << "tao::json::value::array({";
         current_indent += indent;
         first = true;
      }

      void element() noexcept
      {
         first = false;
      }

      void end_array( const std::size_t size = 1 )
      {
         if( size > 0 ) {
            current_indent -= indent;
            if( !first ) {
               next_line();
            }
            os.write( "})", 2 );
         }
      }

      void begin_object( const std::size_t size = 1 )
      {
         next();
         if( size == 0 ) {
            os << "tao::json::empty_object";
            return;
         }
         os << "tao::json::value::object({";
         current_indent += indent;
         first = true;
      }

      void key( const std::string_view v )
      {
         next();
         os.write( "{ ", 2 );
         string_impl( v );
         os.write( ", ", 2 );
         first = true;
         after_key = true;
      }

      void member()
      {
         first = false;
         os.write( " }", 2 );
      }

      void end_object( const std::size_t size = 1 )
      {
         if( size > 0 ) {
            current_indent -= indent;
            if( !first ) {
               next_line();
            }
            os.write( "})", 2 );
         }
      }

   private:
      void string_impl( const std::string_view v )
      {
         const auto t = make_delimiter( v );
         os.write( "R\"", 2 );
         os << t;
         os.put( '(' );
         os << v;
         os.put( ')' );
         os << t;
         os.put( '"' );
      }

      std::string make_delimiter( const std::string_view v )
      {
         for( std::string r; r.size() <= 16; r += '$' ) {
            if( v.find( ')' + r + '"' ) == std::string::npos ) {
               return r;
            }
         }
         for( std::string r; r.size() <= 16; r += '%' ) {
            if( v.find( ')' + r + '"' ) == std::string::npos ) {
               return r;
            }
         }
         for( std::string r; r.size() <= 16; r += '&' ) {
            if( v.find( ')' + r + '"' ) == std::string::npos ) {
               return r;
            }
         }
         for( std::string r; r.size() <= 16; r += '#' ) {
            if( v.find( ')' + r + '"' ) == std::string::npos ) {
               return r;
            }
         }
         for( std::string r; r.size() <= 16; r += '@' ) {
            if( v.find( ')' + r + '"' ) == std::string::npos ) {
               return r;
            }
         }
         throw std::runtime_error( "That's one weird string!" );
      }
   };

}  // namespace tao::json::cplusplus::events

int main( int argc, char** argv )
{
   if( argc != 2 ) {
      std::cerr << "usage: " << argv[ 0 ] << " file.jaxn" << std::endl;
      std::cerr << "  parses the jaxn file and writes it to stdout as c++" << std::endl;
      return 1;
   }
   std::cout << "const tao::json::value change_me_please = ";
   tao::json::cplusplus::events::to_pretty_stream consumer( std::cout, 3 );
   tao::json::jaxn::events::from_file( consumer, argv[ 1 ] );
   std::cout << ";" << std::endl;
   return 0;
}
