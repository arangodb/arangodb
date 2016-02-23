package LWP::Protocol::loopback;

use strict;
use vars qw(@ISA);
require HTTP::Response;

require LWP::Protocol;
@ISA = qw(LWP::Protocol);

sub request {
    my($self, $request, $proxy, $arg, $size, $timeout) = @_;

    my $response = HTTP::Response->new(200, "OK");
    $response->content_type("message/http; msgtype=request");

    $response->header("Via", "loopback/1.0 $proxy")
	if $proxy;

    $response->header("X-Arg", $arg);
    $response->header("X-Read-Size", $size);
    $response->header("X-Timeout", $timeout);

    return $self->collect_once($arg, $response, $request->as_string);
}

1;
