
package IO::Compress::Zlib::Constants ;

use strict ;
use warnings;
use bytes;

require Exporter;

our ($VERSION, @ISA, @EXPORT);

$VERSION = '2.011';

@ISA = qw(Exporter);

@EXPORT= qw(

        ZLIB_HEADER_SIZE
        ZLIB_TRAILER_SIZE

        ZLIB_CMF_CM_OFFSET
        ZLIB_CMF_CM_BITS
        ZLIB_CMF_CM_DEFLATED

        ZLIB_CMF_CINFO_OFFSET
        ZLIB_CMF_CINFO_BITS 
        ZLIB_CMF_CINFO_MAX

        ZLIB_FLG_FCHECK_OFFSET
        ZLIB_FLG_FCHECK_BITS

        ZLIB_FLG_FDICT_OFFSET
        ZLIB_FLG_FDICT_BITS

        ZLIB_FLG_LEVEL_OFFSET
        ZLIB_FLG_LEVEL_BITS

        ZLIB_FLG_LEVEL_FASTEST
        ZLIB_FLG_LEVEL_FAST
        ZLIB_FLG_LEVEL_DEFAULT
        ZLIB_FLG_LEVEL_SLOWEST

        ZLIB_FDICT_SIZE

        );

# Constant names derived from RFC1950

use constant ZLIB_HEADER_SIZE       => 2;
use constant ZLIB_TRAILER_SIZE      => 4;

use constant ZLIB_CMF_CM_OFFSET     => 0;
use constant ZLIB_CMF_CM_BITS       => 0xF ; # 0b1111
use constant ZLIB_CMF_CM_DEFLATED   => 8;

use constant ZLIB_CMF_CINFO_OFFSET  => 4;
use constant ZLIB_CMF_CINFO_BITS    => 0xF ; # 0b1111;
use constant ZLIB_CMF_CINFO_MAX     => 7;

use constant ZLIB_FLG_FCHECK_OFFSET => 0;
use constant ZLIB_FLG_FCHECK_BITS   => 0x1F ; # 0b11111;

use constant ZLIB_FLG_FDICT_OFFSET  => 5;
use constant ZLIB_FLG_FDICT_BITS    => 0x1 ; # 0b1;

use constant ZLIB_FLG_LEVEL_OFFSET  => 6;
use constant ZLIB_FLG_LEVEL_BITS    => 0x3 ; # 0b11;

use constant ZLIB_FLG_LEVEL_FASTEST => 0;
use constant ZLIB_FLG_LEVEL_FAST    => 1;
use constant ZLIB_FLG_LEVEL_DEFAULT => 2;
use constant ZLIB_FLG_LEVEL_SLOWEST => 3;

use constant ZLIB_FDICT_SIZE        => 4;


1;
