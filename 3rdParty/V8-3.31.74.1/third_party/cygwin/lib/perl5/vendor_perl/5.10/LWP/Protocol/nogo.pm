package LWP::Protocol::nogo;
# If you want to disable access to a particular scheme, use this
# class and then call
#   LWP::Protocol::implementor(that_scheme, 'LWP::Protocol::nogo');
# For then on, attempts to access URLs with that scheme will generate
# a 500 error.

use strict;
use vars qw(@ISA);
require HTTP::Response;
require HTTP::Status;
require LWP::Protocol;
@ISA = qw(LWP::Protocol);

sub request {
    my($self, $request) = @_;
    my $scheme = $request->url->scheme;
    
    return HTTP::Response->new(
      &HTTP::Status::RC_INTERNAL_SERVER_ERROR,
      "Access to \'$scheme\' URIs has been disabled"
    );
}
1;
