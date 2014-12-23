package IO::Compress::Adapter::Deflate ;

use strict;
use warnings;
use bytes;

use IO::Compress::Base::Common  2.011 qw(:Status);

use Compress::Raw::Zlib  2.011 qw(Z_OK Z_FINISH MAX_WBITS) ;
our ($VERSION);

$VERSION = '2.011';

sub mkCompObject
{
    my $crc32    = shift ;
    my $adler32  = shift ;
    my $level    = shift ;
    my $strategy = shift ;

    my ($def, $status) = new Compress::Raw::Zlib::Deflate
                                -AppendOutput   => 1,
                                -CRC32          => $crc32,
                                -ADLER32        => $adler32,
                                -Level          => $level,
                                -Strategy       => $strategy,
                                -WindowBits     => - MAX_WBITS;

    return (undef, "Cannot create Deflate object: $status", $status) 
        if $status != Z_OK;    

    return bless {'Def'        => $def,
                  'Error'      => '',
                 } ;     
}

sub compr
{
    my $self = shift ;

    my $def   = $self->{Def};

    my $status = $def->deflate($_[0], $_[1]) ;
    $self->{ErrorNo} = $status;

    if ($status != Z_OK)
    {
        $self->{Error} = "Deflate Error: $status"; 
        return STATUS_ERROR;
    }

    return STATUS_OK;    
}

sub flush
{
    my $self = shift ;

    my $def   = $self->{Def};

    my $opt = $_[1] || Z_FINISH;
    my $status = $def->flush($_[0], $opt);
    $self->{ErrorNo} = $status;

    if ($status != Z_OK)
    {
        $self->{Error} = "Deflate Error: $status"; 
        return STATUS_ERROR;
    }

    return STATUS_OK;    
    
}

sub close
{
    my $self = shift ;

    my $def   = $self->{Def};

    $def->flush($_[0], Z_FINISH)
        if defined $def ;
}

sub reset
{
    my $self = shift ;

    my $def   = $self->{Def};

    my $status = $def->deflateReset() ;
    $self->{ErrorNo} = $status;
    if ($status != Z_OK)
    {
        $self->{Error} = "Deflate Error: $status"; 
        return STATUS_ERROR;
    }

    return STATUS_OK;    
}

sub deflateParams 
{
    my $self = shift ;

    my $def   = $self->{Def};

    my $status = $def->deflateParams(@_);
    $self->{ErrorNo} = $status;
    if ($status != Z_OK)
    {
        $self->{Error} = "deflateParams Error: $status"; 
        return STATUS_ERROR;
    }

    return STATUS_OK;   
}



#sub total_out
#{
#    my $self = shift ;
#    $self->{Def}->total_out();
#}
#
#sub total_in
#{
#    my $self = shift ;
#    $self->{Def}->total_in();
#}

sub compressedBytes
{
    my $self = shift ;

    $self->{Def}->compressedBytes();
}

sub uncompressedBytes
{
    my $self = shift ;
    $self->{Def}->uncompressedBytes();
}




sub crc32
{
    my $self = shift ;
    $self->{Def}->crc32();
}

sub adler32
{
    my $self = shift ;
    $self->{Def}->adler32();
}


1;

__END__

