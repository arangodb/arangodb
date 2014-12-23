package LWP::Authen::Basic;
use strict;

require MIME::Base64;

sub authenticate
{
    my($class, $ua, $proxy, $auth_param, $response,
       $request, $arg, $size) = @_;

    my($user, $pass) = $ua->get_basic_credentials($auth_param->{realm},
                                                  $request->url, $proxy);
    return $response unless defined $user and defined $pass;

    my $auth_header = $proxy ? "Proxy-Authorization" : "Authorization";
    my $auth_value = "Basic " . MIME::Base64::encode("$user:$pass", "");

    # Need to check this isn't a repeated fail!
    my $r = $response;
    while ($r) {
	my $auth = $r->request->header($auth_header);
	if ($auth && $auth eq $auth_value) {
	    # here we know this failed before
	    $response->header("Client-Warning" =>
			      "Credentials for '$user' failed before");
	    return $response;
	}
	$r = $r->previous;
    }

    my $referral = $request->clone;
    $referral->header($auth_header => $auth_value);
    return $ua->request($referral, $arg, $size, $response);
}

1;
