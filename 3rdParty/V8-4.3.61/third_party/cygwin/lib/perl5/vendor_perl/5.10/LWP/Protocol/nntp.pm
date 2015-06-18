package LWP::Protocol::nntp;

# Implementation of the Network News Transfer Protocol (RFC 977)

require LWP::Protocol;
@ISA = qw(LWP::Protocol);

require LWP::Debug;
require HTTP::Response;
require HTTP::Status;
require Net::NNTP;

use strict;


sub request
{
    my($self, $request, $proxy, $arg, $size, $timeout) = @_;

    LWP::Debug::trace('()');

    $size = 4096 unless $size;

    # Check for proxy
    if (defined $proxy) {
	return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
				   'You can not proxy through NNTP');
    }

    # Check that the scheme is as expected
    my $url = $request->url;
    my $scheme = $url->scheme;
    unless ($scheme eq 'news' || $scheme eq 'nntp') {
	return HTTP::Response->new(&HTTP::Status::RC_INTERNAL_SERVER_ERROR,
				   "LWP::Protocol::nntp::request called for '$scheme'");
    }

    # check for a valid method
    my $method = $request->method;
    unless ($method eq 'GET' || $method eq 'HEAD' || $method eq 'POST') {
	return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
				   'Library does not allow method ' .
				   "$method for '$scheme:' URLs");
    }

    # extract the identifier and check against posting to an article
    my $groupart = $url->_group;
    my $is_art = $groupart =~ /@/;

    if ($is_art && $method eq 'POST') {
	return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
				   "Can't post to an article <$groupart>");
    }

    my $nntp = Net::NNTP->new($url->host,
			      #Port    => 18574,
			      Timeout => $timeout,
			      #Debug   => 1,
			     );
    die "Can't connect to nntp server" unless $nntp;

    # Check the initial welcome message from the NNTP server
    if ($nntp->status != 2) {
	return HTTP::Response->new(&HTTP::Status::RC_SERVICE_UNAVAILABLE,
				   $nntp->message);
    }
    my $response = HTTP::Response->new(&HTTP::Status::RC_OK, "OK");

    my $mess = $nntp->message;
    LWP::Debug::debug($mess);

    # Try to extract server name from greeting message.
    # Don't know if this works well for a large class of servers, but
    # this works for our server.
    $mess =~ s/\s+ready\b.*//;
    $mess =~ s/^\S+\s+//;
    $response->header(Server => $mess);

    # First we handle posting of articles
    if ($method eq 'POST') {
	$nntp->quit; $nntp = undef;
	$response->code(&HTTP::Status::RC_NOT_IMPLEMENTED);
	$response->message("POST not implemented yet");
	return $response;
    }

    # The method must be "GET" or "HEAD" by now
    if (!$is_art) {
	if (!$nntp->group($groupart)) {
	    $response->code(&HTTP::Status::RC_NOT_FOUND);
	    $response->message($nntp->message);
	}
	$nntp->quit; $nntp = undef;
	# HEAD: just check if the group exists
	if ($method eq 'GET' && $response->is_success) {
	    $response->code(&HTTP::Status::RC_NOT_IMPLEMENTED);
	    $response->message("GET newsgroup not implemented yet");
	}
	return $response;
    }

    # Send command to server to retrieve an article (or just the headers)
    my $get = $method eq 'HEAD' ? "head" : "article";
    my $art = $nntp->$get("<$groupart>");
    unless ($art) {
	$nntp->quit; $nntp = undef;
	$response->code(&HTTP::Status::RC_NOT_FOUND);
	$response->message($nntp->message);
	return $response;
    }
    LWP::Debug::debug($nntp->message);

    # Parse headers
    my($key, $val);
    local $_;
    while ($_ = shift @$art) {
	if (/^\s+$/) {
	    last;  # end of headers
	}
	elsif (/^(\S+):\s*(.*)/) {
	    $response->push_header($key, $val) if $key;
	    ($key, $val) = ($1, $2);
	}
	elsif (/^\s+(.*)/) {
	    next unless $key;
	    $val .= $1;
	}
	else {
	    unshift(@$art, $_);
	    last;
	}
    }
    $response->push_header($key, $val) if $key;

    # Ensure that there is a Content-Type header
    $response->header("Content-Type", "text/plain")
	unless $response->header("Content-Type");

    # Collect the body
    $response = $self->collect_once($arg, $response, join("", @$art))
      if @$art;

    # Say goodbye to the server
    $nntp->quit;
    $nntp = undef;

    $response;
}

1;
