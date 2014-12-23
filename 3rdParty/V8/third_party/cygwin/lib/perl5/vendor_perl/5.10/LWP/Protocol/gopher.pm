package LWP::Protocol::gopher;

# Implementation of the gopher protocol (RFC 1436)
#
# This code is based on 'wwwgopher.pl,v 0.10 1994/10/17 18:12:34 shelden'
# which in turn is a vastly modified version of Oscar's http'get()
# dated 28/3/94 in <ftp://cui.unige.ch/PUBLIC/oscar/scripts/http.pl>
# including contributions from Marc van Heyningen and Martijn Koster.

use strict;
use vars qw(@ISA);

require HTTP::Response;
require HTTP::Status;
require IO::Socket;
require IO::Select;

require LWP::Protocol;
@ISA = qw(LWP::Protocol);


my %gopher2mimetype = (
    '0' => 'text/plain',                # 0 file
    '1' => 'text/html',                 # 1 menu
					# 2 CSO phone-book server
					# 3 Error
    '4' => 'application/mac-binhex40',  # 4 BinHexed Macintosh file
    '5' => 'application/zip',           # 5 DOS binary archive of some sort
    '6' => 'application/octet-stream',  # 6 UNIX uuencoded file.
    '7' => 'text/html',                 # 7 Index-Search server
					# 8 telnet session
    '9' => 'application/octet-stream',  # 9 binary file
    'h' => 'text/html',                 # html
    'g' => 'image/gif',                 # gif
    'I' => 'image/*',                   # some kind of image
);

my %gopher2encoding = (
    '6' => 'x_uuencode',                # 6 UNIX uuencoded file.
);

sub request
{
    my($self, $request, $proxy, $arg, $size, $timeout) = @_;

    LWP::Debug::trace('()');

    $size = 4096 unless $size;

    # check proxy
    if (defined $proxy) {
	return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
				   'You can not proxy through the gopher');
    }

    my $url = $request->url;
    die "bad scheme" if $url->scheme ne 'gopher';


    my $method = $request->method;
    unless ($method eq 'GET' || $method eq 'HEAD') {
	return HTTP::Response->new(&HTTP::Status::RC_BAD_REQUEST,
				   'Library does not allow method ' .
				   "$method for 'gopher:' URLs");
    }

    my $gophertype = $url->gopher_type;
    unless (exists $gopher2mimetype{$gophertype}) {
	return HTTP::Response->new(&HTTP::Status::RC_NOT_IMPLEMENTED,
				   'Library does not support gophertype ' .
				   $gophertype);
    }

    my $response = HTTP::Response->new(&HTTP::Status::RC_OK, "OK");
    $response->header('Content-type' => $gopher2mimetype{$gophertype}
					|| 'text/plain');
    $response->header('Content-Encoding' => $gopher2encoding{$gophertype})
	if exists $gopher2encoding{$gophertype};

    if ($method eq 'HEAD') {
	# XXX: don't even try it so we set this header
	$response->header('Client-Warning' => 'Client answer only');
	return $response;
    }
    
    if ($gophertype eq '7' && ! $url->search) {
      # the url is the prompt for a gopher search; supply boiler-plate
      return $self->collect_once($arg, $response, <<"EOT");
<HEAD>
<TITLE>Gopher Index</TITLE>
<ISINDEX>
</HEAD>
<BODY>
<H1>$url<BR>Gopher Search</H1>
This is a searchable Gopher index.
Use the search function of your browser to enter search terms.
</BODY>
EOT
    }

    my $host = $url->host;
    my $port = $url->port;

    my $requestLine = "";

    my $selector = $url->selector;
    if (defined $selector) {
	$requestLine .= $selector;
	my $search = $url->search;
	if (defined $search) {
	    $requestLine .= "\t$search";
	    my $string = $url->string;
	    if (defined $string) {
		$requestLine .= "\t$string";
	    }
	}
    }
    $requestLine .= "\015\012";

    # potential request headers are just ignored

    # Ok, lets make the request
    my $socket = IO::Socket::INET->new(PeerAddr => $host,
				       PeerPort => $port,
				       Proto    => 'tcp',
				       Timeout  => $timeout);
    die "Can't connect to $host:$port" unless $socket;
    my $sel = IO::Select->new($socket);

    {
	die "write timeout" if $timeout && !$sel->can_write($timeout);
	my $n = syswrite($socket, $requestLine, length($requestLine));
	die $! unless defined($n);
	die "short write" if $n != length($requestLine);
    }

    my $user_arg = $arg;

    # must handle menus in a special way since they are to be
    # converted to HTML.  Undefing $arg ensures that the user does
    # not see the data before we get a change to convert it.
    $arg = undef if $gophertype eq '1' || $gophertype eq '7';

    # collect response
    my $buf = '';
    $response = $self->collect($arg, $response, sub {
	die "read timeout" if $timeout && !$sel->can_read($timeout);
        my $n = sysread($socket, $buf, $size);
	die $! unless defined($n);
	return \$buf;
      } );

    # Convert menu to HTML and return data to user.
    if ($gophertype eq '1' || $gophertype eq '7') {
	my $content = menu2html($response->content);
	if (defined $user_arg) {
	    $response = $self->collect_once($user_arg, $response, $content);
	}
	else {
	    $response->content($content);
	}
    }

    $response;
}


sub gopher2url
{
    my($gophertype, $path, $host, $port) = @_;

    my $url;

    if ($gophertype eq '8' || $gophertype eq 'T') {
	# telnet session
	$url = $HTTP::URI_CLASS->new($gophertype eq '8' ? 'telnet:':'tn3270:');
	$url->user($path) if defined $path;
    }
    else {
	$path = URI::Escape::uri_escape($path);
	$url = $HTTP::URI_CLASS->new("gopher:/$gophertype$path");
    }
    $url->host($host);
    $url->port($port);
    $url;
}

sub menu2html {
    my($menu) = @_;

    $menu =~ s/\015//g;  # remove carriage return
    my $tmp = <<"EOT";
<HTML>
<HEAD>
   <TITLE>Gopher menu</TITLE>
</HEAD>
<BODY>
<H1>Gopher menu</H1>
EOT
    for (split("\n", $menu)) {
	last if /^\./;
	my($pretty, $path, $host, $port) = split("\t");

	$pretty =~ s/^(.)//;
	my $type = $1;

	my $url = gopher2url($type, $path, $host, $port)->as_string;
	$tmp .= qq{<A HREF="$url">$pretty</A><BR>\n};
    }
    $tmp .= "</BODY>\n</HTML>\n";
    $tmp;
}

1;
