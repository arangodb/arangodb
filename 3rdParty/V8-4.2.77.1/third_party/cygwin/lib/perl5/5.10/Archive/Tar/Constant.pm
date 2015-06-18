package Archive::Tar::Constant;

BEGIN {
    require Exporter;
    $VERSION= '0.02';
    @ISA    = qw[Exporter];
    @EXPORT = qw[
                FILE HARDLINK SYMLINK CHARDEV BLOCKDEV DIR FIFO SOCKET UNKNOWN
                BUFFER HEAD READ_ONLY WRITE_ONLY UNPACK PACK TIME_OFFSET ZLIB
                BLOCK_SIZE TAR_PAD TAR_END ON_UNIX BLOCK CAN_READLINK MAGIC 
                TAR_VERSION UNAME GNAME CAN_CHOWN MODE CHECK_SUM UID GID 
                GZIP_MAGIC_NUM MODE_READ LONGLINK LONGLINK_NAME PREFIX_LENGTH
                LABEL NAME_LENGTH STRIP_MODE ON_VMS
            ];

    require Time::Local if $^O eq "MacOS";
}

use constant FILE           => 0;
use constant HARDLINK       => 1;
use constant SYMLINK        => 2;
use constant CHARDEV        => 3;
use constant BLOCKDEV       => 4;
use constant DIR            => 5;
use constant FIFO           => 6;
use constant SOCKET         => 8;
use constant UNKNOWN        => 9;
use constant LONGLINK       => 'L';
use constant LABEL          => 'V';

use constant BUFFER         => 4096;
use constant HEAD           => 512;
use constant BLOCK          => 512;

use constant BLOCK_SIZE     => sub { my $n = int($_[0]/BLOCK); $n++ if $_[0] % BLOCK; $n * BLOCK };
use constant TAR_PAD        => sub { my $x = shift || return; return "\0" x (BLOCK - ($x % BLOCK) ) };
use constant TAR_END        => "\0" x BLOCK;

use constant READ_ONLY      => sub { shift() ? 'rb' : 'r' };
use constant WRITE_ONLY     => sub { $_[0] ? 'wb' . shift : 'w' };
use constant MODE_READ      => sub { $_[0] =~ /^r/ ? 1 : 0 };

# Pointless assignment to make -w shut up
my $getpwuid; $getpwuid = 'unknown' unless eval { my $f = getpwuid (0); };
my $getgrgid; $getgrgid = 'unknown' unless eval { my $f = getgrgid (0); };
use constant UNAME          => sub { $getpwuid || scalar getpwuid( shift() ) || '' };
use constant GNAME          => sub { $getgrgid || scalar getgrgid( shift() ) || '' };
use constant UID            => $>;
use constant GID            => (split ' ', $) )[0];

use constant MODE           => do { 0666 & (0777 & ~umask) };
use constant STRIP_MODE     => sub { shift() & 0777 };
use constant CHECK_SUM      => "      ";

use constant UNPACK         => 'A100 A8 A8 A8 A12 A12 A8 A1 A100 A6 A2 A32 A32 A8 A8 A155 x12';
use constant PACK           => 'a100 a8 a8 a8 a12 a12 A8 a1 a100 a6 a2 a32 a32 a8 a8 a155 x12';
use constant NAME_LENGTH    => 100;
use constant PREFIX_LENGTH  => 155;

use constant TIME_OFFSET    => ($^O eq "MacOS") ? Time::Local::timelocal(0,0,0,1,0,70) : 0;    
use constant MAGIC          => "ustar";
use constant TAR_VERSION    => "00";
use constant LONGLINK_NAME  => '././@LongLink';

                            ### allow ZLIB to be turned off using ENV
                            ### DEBUG only
use constant ZLIB           => do { !$ENV{'PERL5_AT_NO_ZLIB'} and
                                        eval { require IO::Zlib };
                                    $ENV{'PERL5_AT_NO_ZLIB'} || $@ ? 0 : 1 };
                                    
use constant GZIP_MAGIC_NUM => qr/^(?:\037\213|\037\235)/;

use constant CAN_CHOWN      => do { ($> == 0 and $^O ne "MacOS" and $^O ne "MSWin32") };
use constant CAN_READLINK   => ($^O ne 'MSWin32' and $^O !~ /RISC(?:[ _])?OS/i and $^O ne 'VMS');
use constant ON_UNIX        => ($^O ne 'MSWin32' and $^O ne 'MacOS' and $^O ne 'VMS');
use constant ON_VMS         => $^O eq 'VMS'; 

1;
