package LWP::Authen::Ntlm;

use strict;
use vars qw/$VERSION/;

$VERSION = '5.810';

use Authen::NTLM "1.02";
use MIME::Base64 "2.12";

sub authenticate {
	LWP::Debug::debug("authenticate() has been called");
    my($class, $ua, $proxy, $auth_param, $response,
       $request, $arg, $size) = @_;

    my($user, $pass) = $ua->get_basic_credentials($auth_param->{realm},
                                                  $request->url, $proxy);

    unless(defined $user and defined $pass) {
		LWP::Debug::debug("No username and password available from get_basic_credentials().  Returning unmodified response object");
		return $response;
	}

	if (!$ua->conn_cache()) {
		LWP::Debug::debug("Keep alive not enabled, emitting a warning");
		warn "The keep_alive option must be enabled for NTLM authentication to work.  NTLM authentication aborted.\n";
		return $response;
	}

	my($domain, $username) = split(/\\/, $user);

	ntlm_domain($domain);
	ntlm_user($username);
	ntlm_password($pass);

    my $auth_header = $proxy ? "Proxy-Authorization" : "Authorization";

	# my ($challenge) = $response->header('WWW-Authenticate'); 
	my $challenge;
	foreach ($response->header('WWW-Authenticate')) { 
		last if /^NTLM/ && ($challenge=$_);
	}

	if ($challenge eq 'NTLM') {
		# First phase, send handshake
		LWP::Debug::debug("In first phase of NTLM authentication");
	    my $auth_value = "NTLM " . ntlm();
		ntlm_reset();

	    # Need to check this isn't a repeated fail!
	    my $r = $response;
		my $retry_count = 0;
	    while ($r) {
			my $auth = $r->request->header($auth_header);
			++$retry_count if ($auth && $auth eq $auth_value);
			if ($retry_count > 2) {
				    # here we know this failed before
				    $response->header("Client-Warning" =>
						      "Credentials for '$user' failed before");
				    return $response;
			}
			$r = $r->previous;
	    }

	    my $referral = $request->clone;
	    $referral->header($auth_header => $auth_value);
		LWP::Debug::debug("Returning response object with auth header:\n$auth_header $auth_value");
	    return $ua->request($referral, $arg, $size, $response);
	}
	
	else {
		# Second phase, use the response challenge (unless non-401 code
		#  was returned, in which case, we just send back the response
		#  object, as is
		LWP::Debug::debug("In second phase of NTLM authentication");
		my $auth_value;
		if ($response->code ne '401') {
			LWP::Debug::debug("Response from server was not 401 Unauthorized, returning response object without auth headers");
			return $response;
		}
		else {
			my $challenge;
			foreach ($response->header('WWW-Authenticate')) { 
				last if /^NTLM/ && ($challenge=$_);
			}
			$challenge =~ s/^NTLM //;
			ntlm();
			$auth_value = "NTLM " . ntlm($challenge);
			ntlm_reset();
		}

	    my $referral = $request->clone;
	    $referral->header($auth_header => $auth_value);
		LWP::Debug::debug("Returning response object with auth header:\n$auth_header $auth_value");
	    my $response2 = $ua->request($referral, $arg, $size, $response);
		return $response2;
	}
}

1;


=head1 NAME

LWP::Authen::Ntlm - Library for enabling NTLM authentication (Microsoft) in LWP

=head1 SYNOPSIS

 use LWP::UserAgent;
 use HTTP::Request::Common;
 my $url = 'http://www.company.com/protected_page.html';

 # Set up the ntlm client and then the base64 encoded ntlm handshake message
 my $ua = new LWP::UserAgent(keep_alive=>1);
 $ua->credentials('www.company.com:80', '', "MyDomain\\MyUserCode", 'MyPassword');

 $request = GET $url;
 print "--Performing request now...-----------\n";
 $response = $ua->request($request);
 print "--Done with request-------------------\n";

 if ($response->is_success) {print "It worked!->" . $response->code . "\n"}
 else {print "It didn't work!->" . $response->code . "\n"}

=head1 DESCRIPTION

C<LWP::Authen::Ntlm> allows LWP to authenticate against servers that are using the 
NTLM authentication scheme popularized by Microsoft.  This type of authentication is 
common on intranets of Microsoft-centric organizations.

The module takes advantage of the Authen::NTLM module by Mark Bush.  Since there 
is also another Authen::NTLM module available from CPAN by Yee Man Chan with an 
entirely different interface, it is necessary to ensure that you have the correct 
NTLM module.

In addition, there have been problems with incompatibilities between different 
versions of Mime::Base64, which Bush's Authen::NTLM makes use of.  Therefore, it is 
necessary to ensure that your Mime::Base64 module supports exporting of the 
encode_base64 and decode_base64 functions.

=head1 USAGE

The module is used indirectly through LWP, rather than including it directly in your 
code.  The LWP system will invoke the NTLM authentication when it encounters the 
authentication scheme while attempting to retrieve a URL from a server.  In order 
for the NTLM authentication to work, you must have a few things set up in your 
code prior to attempting to retrieve the URL:

=over 4

=item *

Enable persistent HTTP connections

To do this, pass the "keep_alive=>1" option to the LWP::UserAgent when creating it, like this:

    my $ua = new LWP::UserAgent(keep_alive=>1);

=item *

Set the credentials on the UserAgent object

The credentials must be set like this:

   $ua->credentials('www.company.com:80', '', "MyDomain\\MyUserCode", 'MyPassword');

Note that you cannot use the HTTP::Request object's authorization_basic() method to set 
the credentials.  Note, too, that the 'www.company.com:80' portion only sets credentials 
on the specified port AND it is case-sensitive (this is due to the way LWP is coded, and 
has nothing to do with LWP::Authen::Ntlm)

=back

If you run into trouble and need help troubleshooting your problems, try enabling LWP 
debugging by putting this line at the top of your code:

    use LWP::Debug qw(+);

You should get copious debugging output, including messages from LWP::Authen::Ntlm itself.

=head1 AVAILABILITY

General queries regarding LWP should be made to the LWP Mailing List.

Questions specific to LWP::Authen::Ntlm can be forwarded to jtillman@bigfoot.com

=head1 COPYRIGHT

Copyright (c) 2002 James Tillman. All rights reserved. This
program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head1 SEE ALSO

L<LWP>, L<LWP::UserAgent>, L<lwpcook>.
