package LWP::Protocol::cpan;

use strict;
use vars qw(@ISA);

require LWP::Protocol;
@ISA = qw(LWP::Protocol);

require URI;
require HTTP::Status;
require HTTP::Response;

our $CPAN;

unless ($CPAN) {
    # Try to find local CPAN mirror via $CPAN::Config
    eval {
	require CPAN::Config;
	if($CPAN::Config) {
	    my $urls = $CPAN::Config->{urllist};
	    if (ref($urls) eq "ARRAY") {
		my $file;
		for (@$urls) {
		    if (/^file:/) {
			$file = $_;
			last;
		    }
		}

		if ($file) {
		    $CPAN = $file;
		}
		else {
		    $CPAN = $urls->[0];
		}
	    }
	}
    };

    $CPAN ||= "http://cpan.org/";  # last resort
}

# ensure that we don't chop of last part
$CPAN .= "/" unless $CPAN =~ m,/$,;


sub request {
    my($self, $request, $proxy, $arg, $size) = @_;
    # check proxy
    if (defined $proxy)
    {
	return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
				   'You can not proxy with cpan');
    }

    # check method
    my $method = $request->method;
    unless ($method eq 'GET' || $method eq 'HEAD') {
	return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
				   'Library does not allow method ' .
				   "$method for 'cpan:' URLs");
    }

    my $path = $request->uri->path;
    $path =~ s,^/,,;

    my $response = HTTP::Response->new(&HTTP::Status::RC_FOUND);
    $response->header("Location" => URI->new_abs($path, $CPAN));
    $response;
}

1;
