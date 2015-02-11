package URI::http;

require URI::_server;
@ISA=qw(URI::_server);

use strict;

sub default_port { 80 }

sub canonical
{
    my $self = shift;
    my $other = $self->SUPER::canonical;

    my $slash_path = defined($other->authority) &&
        !length($other->path) && !defined($other->query);

    if ($slash_path) {
	$other = $other->clone if $other == $self;
	$other->path("/");
    }
    $other;
}

1;
