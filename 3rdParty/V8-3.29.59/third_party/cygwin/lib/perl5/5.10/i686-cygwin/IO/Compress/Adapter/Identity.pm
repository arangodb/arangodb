package IO::Compress::Adapter::Identity ;

use strict;
use warnings;
use bytes;

use IO::Compress::Base::Common  2.011 qw(:Status);
our ($VERSION);

$VERSION = '2.011';

sub mkCompObject
{
    my $level    = shift ;
    my $strategy = shift ;

    return bless {
                  'CompSize'   => 0,
                  'UnCompSize' => 0,
                  'Error'      => '',
                  'ErrorNo'    => 0,
                 } ;     
}

sub compr
{
    my $self = shift ;

    if (defined ${ $_[0] } && length ${ $_[0] }) {
        $self->{CompSize} += length ${ $_[0] } ;
        $self->{UnCompSize} = $self->{CompSize} ;

        if ( ref $_[1] ) 
          { ${ $_[1] } .= ${ $_[0] } }
        else
          { $_[1] .= ${ $_[0] } }
    }

    return STATUS_OK ;
}

sub flush
{
    my $self = shift ;

    return STATUS_OK;    
}

sub close
{
    my $self = shift ;

    return STATUS_OK;    
}

sub reset
{
    my $self = shift ;

    $self->{CompSize}   = 0;
    $self->{UnCompSize} = 0;

    return STATUS_OK;    
}

sub deflateParams 
{
    my $self = shift ;

    return STATUS_OK;   
}

#sub total_out
#{
#    my $self = shift ;
#    return $self->{UnCompSize} ;
#}
#
#sub total_in
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

1;


__END__

