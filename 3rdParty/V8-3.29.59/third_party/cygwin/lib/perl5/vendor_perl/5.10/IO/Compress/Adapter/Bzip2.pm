package IO::Compress::Adapter::Bzip2 ;

use strict;
use warnings;
use bytes;

use IO::Compress::Base::Common  2.011 qw(:Status);

#use Compress::Bzip2 ;
use Compress::Raw::Bzip2  2.011 ;

our ($VERSION);
$VERSION = '2.011';

sub mkCompObject
{
    my $BlockSize100K = shift ;
    my $WorkFactor = shift ;
    my $Verbosity  = shift ;

    my ($def, $status) = new Compress::Raw::Bzip2(1, $BlockSize100K,
                                                 $WorkFactor, $Verbosity);
    #my ($def, $status) = bzdeflateInit();
                        #-BlockSize100K => $params->value('BlockSize100K'),
                        #-WorkFactor    => $params->value('WorkFactor');

    return (undef, "Could not create Deflate object: $status", $status)
        if $status != BZ_OK ;

    return bless {'Def'        => $def,
                  'Error'      => '',
                  'ErrorNo'    => 0,
                 }  ;     
}

sub compr
{
    my $self = shift ;

    my $def   = $self->{Def};

    #my ($out, $status) = $def->bzdeflate(defined ${$_[0]} ? ${$_[0]} : "") ;
    my $status = $def->bzdeflate($_[0], $_[1]) ;
    $self->{ErrorNo} = $status;

    if ($status != BZ_RUN_OK)
    {
        $self->{Error} = "Deflate Error: $status"; 
        return STATUS_ERROR;
    }

    #${ $_[1] } .= $out if defined $out;

    return STATUS_OK;    
}

sub flush
{
    my $self = shift ;

    my $def   = $self->{Def};

    #my ($out, $status) = $def->bzflush($opt);
    #my $status = $def->bzflush($_[0], $opt);
    my $status = $def->bzflush($_[0]);
    $self->{ErrorNo} = $status;

    if ($status != BZ_RUN_OK)
    {
        $self->{Error} = "Deflate Error: $status"; 
        return STATUS_ERROR;
    }

    #${ $_[0] } .= $out if defined $out ;
    return STATUS_OK;    
    
}

sub close
{
    my $self = shift ;

    my $def   = $self->{Def};

    #my ($out, $status) = $def->bzclose();
    my $status = $def->bzclose($_[0]);
    $self->{ErrorNo} = $status;

    if ($status != BZ_STREAM_END)
    {
        $self->{Error} = "Deflate Error: $status"; 
        return STATUS_ERROR;
    }

    #${ $_[0] } .= $out if defined $out ;
    return STATUS_OK;    
    
}


sub reset
{
    my $self = shift ;

    my $outer = $self->{Outer};

    my ($def, $status) = new Compress::Raw::Bzip2();
    $self->{ErrorNo} = ($status == BZ_OK) ? 0 : $status ;

    if ($status != BZ_OK)
    {
        $self->{Error} = "Cannot create Deflate object: $status"; 
        return STATUS_ERROR;
    }

    $self->{Def} = $def;

    return STATUS_OK;    
}

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

#sub total_out
#{
#    my $self = shift ;
#    0;
#}
#

#sub total_in
#{
#    my $self = shift ;
#    $self->{Def}->total_in();
#}
#
#sub crc32
#{
#    my $self = shift ;
#    $self->{Def}->crc32();
#}
#
#sub adler32
#{
#    my $self = shift ;
#    $self->{Def}->adler32();
#}


1;

__END__

