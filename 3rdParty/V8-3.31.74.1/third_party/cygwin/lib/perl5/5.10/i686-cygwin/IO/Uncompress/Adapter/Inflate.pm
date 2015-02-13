package IO::Uncompress::Adapter::Inflate;

use strict;
use warnings;
use bytes;

use IO::Compress::Base::Common  2.011 qw(:Status);
use Compress::Raw::Zlib  2.011 qw(Z_OK Z_DATA_ERROR Z_STREAM_END Z_FINISH MAX_WBITS);

our ($VERSION);
$VERSION = '2.011';



sub mkUncompObject
{
    my $crc32   = shift || 1;
    my $adler32 = shift || 1;
    my $scan    = shift || 0;

    my $inflate ;
    my $status ;

    if ($scan)
    {
        ($inflate, $status) = new Compress::Raw::Zlib::InflateScan
                                    CRC32        => $crc32,
                                    ADLER32      => $adler32,
                                    WindowBits   => - MAX_WBITS ;
    }
    else
    {
        ($inflate, $status) = new Compress::Raw::Zlib::Inflate
                                    AppendOutput => 1,
                                    CRC32        => $crc32,
                                    ADLER32      => $adler32,
                                    WindowBits   => - MAX_WBITS ;
    }

    return (undef, "Could not create Inflation object: $status", $status) 
        if $status != Z_OK ;

    return bless {'Inf'        => $inflate,
                  'CompSize'   => 0,
                  'UnCompSize' => 0,
                  'Error'      => '',
                 } ;     
    
}

sub uncompr
{
    my $self = shift ;
    my $from = shift ;
    my $to   = shift ;
    my $eof  = shift ;

    my $inf   = $self->{Inf};

    my $status = $inf->inflate($from, $to, $eof);
    $self->{ErrorNo} = $status;

    if ($status != Z_STREAM_END && $eof)
    {
        $self->{Error} = "unexpected end of file";
        return STATUS_ERROR;
    }

    if ($status != Z_OK && $status != Z_STREAM_END )
    {
        $self->{Error} = "Inflation Error: $status";
        return STATUS_ERROR;
    }

    
    return STATUS_OK        if $status == Z_OK ;
    return STATUS_ENDSTREAM if $status == Z_STREAM_END ;
    return STATUS_ERROR ;
}

sub reset
{
    my $self = shift ;
    $self->{Inf}->inflateReset();

    return STATUS_OK ;
}

#sub count
#{
#    my $self = shift ;
#    $self->{Inf}->inflateCount();
#}

sub crc32
{
    my $self = shift ;
    $self->{Inf}->crc32();
}

sub compressedBytes
{
    my $self = shift ;
    $self->{Inf}->compressedBytes();
}

sub uncompressedBytes
{
    my $self = shift ;
    $self->{Inf}->uncompressedBytes();
}

sub adler32
{
    my $self = shift ;
    $self->{Inf}->adler32();
}

sub sync
{
    my $self = shift ;
    ( $self->{Inf}->inflateSync(@_) == Z_OK) 
            ? STATUS_OK 
            : STATUS_ERROR ;
}


sub getLastBlockOffset
{
    my $self = shift ;
    $self->{Inf}->getLastBlockOffset();
}

sub getEndOffset
{
    my $self = shift ;
    $self->{Inf}->getEndOffset();
}

sub resetLastBlockByte
{
    my $self = shift ;
    $self->{Inf}->resetLastBlockByte(@_);
}

sub createDeflateStream
{
    my $self = shift ;
    my $deflate = $self->{Inf}->createDeflateStream(@_);
    return bless {'Def'        => $deflate,
                  'CompSize'   => 0,
                  'UnCompSize' => 0,
                  'Error'      => '',
                 }, 'IO::Compress::Adapter::Deflate';
}

1;


__END__

