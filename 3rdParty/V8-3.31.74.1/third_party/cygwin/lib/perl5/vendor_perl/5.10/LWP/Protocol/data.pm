package LWP::Protocol::data;

# Implements access to data:-URLs as specified in RFC 2397

use strict;
use vars qw(@ISA);

require HTTP::Response;
require HTTP::Status;

require LWP::Protocol;
@ISA = qw(LWP::Protocol);

use HTTP::Date qw(time2str);
require LWP;  # needs version number

sub request
{
    my($self, $request, $proxy, $arg, $size) = @_;

    # check proxy
    if (defined $proxy)
    {
	return new HTTP::Response &HTTP::Status::RC_BAD_REQUEST,
				  'You can not proxy with data';
    }

    # check method
    my $method = $request->method;
    unless ($method eq 'GET' || $method eq 'HEAD') {
	return new HTTP::Response &HTTP::Status::RC_BAD_REQUEST,
				  'Library does not allow method ' .
				  "$method for 'data:' URLs";
    }

    my $url = $request->url;
    my $response = new HTTP::Response &HTTP::Status::RC_OK, "Document follows";

    my $media_type = $url->media_type;

    my $data = $url->data;
    $response->header('Content-Type'   => $media_type,
		      'Content-Length' => length($data),
		      'Date'           => time2str(time),
		      'Server'         => "libwww-perl-internal/$LWP::VERSION"
		     );
    $response->content($data) if $method ne "HEAD";

    return $response;
}

1;
