package IO::Uncompress::Adapter::Identity;

use warnings;
use strict;
use bytes;

use IO::Compress::Base::Common  2.011 qw(:Status);

our ($VERSION);

$VERSION = '2.011';

use Compress::Raw::Zlib  2.011 ();

sub mkUncompObject
{
    my $crc32 = 1; #shift ;
    my $adler32 = shift;

    bless { 'CompSize'   => 0,
            'UnCompSize' => 0,
            'wantCRC32'  => $crc32,
            'CRC32'      => Compress::Raw::Zlib::crc32(''),
            'wantADLER32'=> $adler32,
            'ADLER32'    => Compress::Raw::Zlib::adler32(''),
          } ;
}

sub uncompr
{
    my $self = shift;
    my $eof = $_[2];

    if (defined ${ $_[0] } && length ${ $_[0] }) {
        $self->{CompSize} += length ${ $_[0] } ;
        $self->{UnCompSize} = $self->{CompSize} ;

        $self->{CRC32} = Compress::Raw::Zlib::crc32($_[0],  $self->{CRC32})
            if $self->{wantCRC32};

        $self->{ADLER32} = Compress::Zlib::adler32($_[0],  $self->{ADLER32})
            if $self->{wantADLER32};

        ${ $_[1] } .= ${ $_[0] };
        ${ $_[0] } = "";
    }

    return STATUS_ENDSTREAM if $eof;
    return STATUS_OK ;
}

sub reset
{
    my $self = shift;

    $self->{CompSize}   = 0;
    $self->{UnCompSize} = 0;
    $self->{CRC32}      = Compress::Raw::Zlib::crc32('');
    $self->{ADLER32}    = Compress::Raw::Zlib::adler32('');      

    return STATUS_OK ;
}


#sub count
#{
#    my $self = shift ;
#    return $self->{UnCompSize} ;
#}

sub compressedBytes
{
    my $self = shift ;
    return $self->{UnCompSize} ;
}

sub uncompressedBytes
{
    my $self = shift ;
    return $self->{UnCompSize} ;
}

sub sync
{
    return STATUS_OK ;
}

sub crc32
{
    my $self = shift ;
    return $self->{CRC32};
}

sub adler32
{
    my $self = shift ;
    return $self->{ADLER32};
}

1;

__END__
