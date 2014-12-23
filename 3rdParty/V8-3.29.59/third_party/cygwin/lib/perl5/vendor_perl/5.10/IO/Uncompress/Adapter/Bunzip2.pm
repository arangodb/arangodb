package IO::Uncompress::Adapter::Bunzip2;

use strict;
use warnings;
use bytes;

use IO::Compress::Base::Common 2.011 qw(:Status);

#use Compress::Bzip2 ;
use Compress::Raw::Bzip2 2.011 ;

our ($VERSION, @ISA);
$VERSION = '2.011';

#@ISA = qw( Compress::Raw::Bunzip2 );


sub mkUncompObject
{
    my $small     = shift || 0;
    my $verbosity = shift || 0;

    #my ($inflate, $status) = bzinflateInit;
                                #Small        => $params->value('Small');
    my ($inflate, $status) = new Compress::Raw::Bunzip2(1, 1, $small, $verbosity);

    return (undef, "Could not create Inflation object: $status", $status)
        if $status != BZ_OK ;

    return bless {'Inf'        => $inflate,
                  'CompSize'   => 0,
                  'UnCompSize' => 0,
                  'Error'      => '',
                 }  ;     
    
}

sub uncompr
{
    my $self = shift ;
    my $from = shift ;
    my $to   = shift ;
    my $eof  = shift ;

    my $inf   = $self->{Inf};

    my $status = $inf->bzinflate($from, $to);
    $self->{ErrorNo} = $status;

    if ($status != BZ_STREAM_END && $eof)
    {
        $self->{Error} = "unexpected end of file";
        return STATUS_ERROR;
    }

    if ($status != BZ_OK && $status != BZ_STREAM_END )
    {
        $self->{Error} = "Inflation Error: $status";
        return STATUS_ERROR;
    }

    
    return STATUS_OK        if $status == BZ_OK ;
    return STATUS_ENDSTREAM if $status == BZ_STREAM_END ;
    return STATUS_ERROR ;
}


#sub uncompr
#{
#    my $self = shift ;
#
#    my $inf = $self->{Inf};
#    my $eof = $_[2];
#
#    #my ($out, $status) = $inf->bzinflate(${ $_[0] });
#    my $status = $inf->bzinflate($_[0], $_[1]);
#    $self->{ErrorNo} = $status;
#
#    if (! defined $out)
#    {
#        my $err = $inf->error();
#        $self->{Error} = "Inflation Error: $err";
#        return STATUS_ERROR;
#    }
#
#    #${ $_[1] } .= $out if defined $out;
#    
#    if ($eof)
#    {
#        #my ($out, $status) = $inf->bzclose();
#        $status = $inf->bzclose($_[1]);
#        $self->{ErrorNo} = $status;
#
#        if (! defined $out)
#        {
#            my $err = $inf->error();
#            $self->{Error} = "Inflation Error: $err";
#            return STATUS_ERROR;
#        }
#
#        #${ $_[1] } .= $out if defined $out;
#        return STATUS_ENDSTREAM ;
#    }
#
#    return STATUS_OK ;
#}

#sub uncompr
#{
#    my $self = shift ;
#
#    my $inf   = $self->{Inf};
#    my $eof = $_[2];
#
#    my ($out, $status) = $inf->bzinflate(${ $_[0] });
#    $self->{ErrorNo} = $status;
#
#    if ($status != BZ_STREAM_END && $eof)
#    {
#        $self->{Error} = "unexpected end of file";
#        return STATUS_ERROR;
#    }
#
#    if ($status != BZ_OK && $status != BZ_STREAM_END )
#    {
#        my $err = $inf->error();
#        $self->{Error} = "Inflation Error: $err";
#        return STATUS_ERROR;
#    }
#
#    ${ $_[1] } .= $out ;
#    
#    return STATUS_OK        if $status == BZ_OK ;
#    return STATUS_ENDSTREAM if $status == BZ_STREAM_END ;
#    return STATUS_ERROR ;
#}

sub reset
{
    my $self = shift ;

    my ($inf, $status) = new Compress::Raw::Bunzip2();
    $self->{ErrorNo} = ($status == BZ_OK) ? 0 : $status ;

    if ($status != BZ_OK)
    {
        $self->{Error} = "Cannot create Inflate object: $status"; 
        return STATUS_ERROR;
    }

    $self->{Inf} = $inf;

    return STATUS_OK ;
}

#sub count
#{
#    my $self = shift ;
#    $self->{Inf}->inflateCount();
#}

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

sub crc32
{
    my $self = shift ;
    #$self->{Inf}->crc32();
}

sub adler32
{
    my $self = shift ;
    #$self->{Inf}->adler32();
}

sub sync
{
    my $self = shift ;
    #( $self->{Inf}->inflateSync(@_) == BZ_OK) 
    #        ? STATUS_OK 
    #        : STATUS_ERROR ;
}


1;

__END__

