package LWP::Protocol::http;

use strict;

require LWP::Debug;
require HTTP::Response;
require HTTP::Status;
require Net::HTTP;

use vars qw(@ISA @EXTRA_SOCK_OPTS);

require LWP::Protocol;
@ISA = qw(LWP::Protocol);

my $CRLF = "\015\012";

sub _new_socket
{
    my($self, $host, $port, $timeout) = @_;
    my $conn_cache = $self->{ua}{conn_cache};
    if ($conn_cache) {
	if (my $sock = $conn_cache->withdraw("http", "$host:$port")) {
	    return $sock if $sock && !$sock->can_read(0);
	    # if the socket is readable, then either the peer has closed the
	    # connection or there are some garbage bytes on it.  In either
	    # case we abandon it.
	    $sock->close;
	}
    }

    local($^W) = 0;  # IO::Socket::INET can be noisy
    my $sock = $self->socket_class->new(PeerAddr => $host,
					PeerPort => $port,
					Proto    => 'tcp',
					Timeout  => $timeout,
					KeepAlive => !!$conn_cache,
					SendTE    => 1,
					$self->_extra_sock_opts($host, $port),
				       );

    unless ($sock) {
	# IO::Socket::INET leaves additional error messages in $@
	$@ =~ s/^.*?: //;
	die "Can't connect to $host:$port ($@)";
    }

    # perl 5.005's IO::Socket does not have the blocking method.
    eval { $sock->blocking(0); };

    $sock;
}

sub socket_class
{
    my $self = shift;
    (ref($self) || $self) . "::Socket";
}

sub _extra_sock_opts  # to be overridden by subclass
{
    return @EXTRA_SOCK_OPTS;
}

sub _check_sock
{
    #my($self, $req, $sock) = @_;
}

sub _get_sock_info
{
    my($self, $res, $sock) = @_;
    if (defined(my $peerhost = $sock->peerhost)) {
        $res->header("Client-Peer" => "$peerhost:" . $sock->peerport);
    }
}

sub _fixup_header
{
    my($self, $h, $url, $proxy) = @_;

    # Extract 'Host' header
    my $hhost = $url->authority;
    if ($hhost =~ s/^([^\@]*)\@//) {  # get rid of potential "user:pass@"
	# add authorization header if we need them.  HTTP URLs do
	# not really support specification of user and password, but
	# we allow it.
	if (defined($1) && not $h->header('Authorization')) {
	    require URI::Escape;
	    $h->authorization_basic(map URI::Escape::uri_unescape($_),
				    split(":", $1, 2));
	}
    }
    $h->init_header('Host' => $hhost);

    if ($proxy) {
	# Check the proxy URI's userinfo() for proxy credentials
	# export http_proxy="http://proxyuser:proxypass@proxyhost:port"
	my $p_auth = $proxy->userinfo();
	if(defined $p_auth) {
	    require URI::Escape;
	    $h->proxy_authorization_basic(map URI::Escape::uri_unescape($_),
					  split(":", $p_auth, 2))
	}
    }
}

sub hlist_remove {
    my($hlist, $k) = @_;
    $k = lc $k;
    for (my $i = @$hlist - 2; $i >= 0; $i -= 2) {
	next unless lc($hlist->[$i]) eq $k;
	splice(@$hlist, $i, 2);
    }
}

sub request
{
    my($self, $request, $proxy, $arg, $size, $timeout) = @_;
    LWP::Debug::trace('()');

    $size ||= 4096;

    # check method
    my $method = $request->method;
    unless ($method =~ /^[A-Za-z0-9_!\#\$%&\'*+\-.^\`|~]+$/) {  # HTTP token
	return new HTTP::Response &HTTP::Status::RC_BAD_REQUEST,
				  'Library does not allow method ' .
				  "$method for 'http:' URLs";
    }

    my $url = $request->url;
    my($host, $port, $fullpath);

    # Check if we're proxy'ing
    if (defined $proxy) {
	# $proxy is an URL to an HTTP server which will proxy this request
	$host = $proxy->host;
	$port = $proxy->port;
	$fullpath = $method eq "CONNECT" ?
                       ($url->host . ":" . $url->port) :
                       $url->as_string;
    }
    else {
	$host = $url->host;
	$port = $url->port;
	$fullpath = $url->path_query;
	$fullpath = "/$fullpath" unless $fullpath =~ m,^/,;
    }

    # connect to remote site
    my $socket = $self->_new_socket($host, $port, $timeout);
    $self->_check_sock($request, $socket);

    my @h;
    my $request_headers = $request->headers->clone;
    $self->_fixup_header($request_headers, $url, $proxy);

    $request_headers->scan(sub {
			       my($k, $v) = @_;
			       $k =~ s/^://;
			       $v =~ s/\n/ /g;
			       push(@h, $k, $v);
			   });

    my $content_ref = $request->content_ref;
    $content_ref = $$content_ref if ref($$content_ref);
    my $chunked;
    my $has_content;

    if (ref($content_ref) eq 'CODE') {
	my $clen = $request_headers->header('Content-Length');
	$has_content++ if $clen;
	unless (defined $clen) {
	    push(@h, "Transfer-Encoding" => "chunked");
	    $has_content++;
	    $chunked++;
	}
    }
    else {
	# Set (or override) Content-Length header
	my $clen = $request_headers->header('Content-Length');
	if (defined($$content_ref) && length($$content_ref)) {
	    $has_content = length($$content_ref);
	    if (!defined($clen) || $clen ne $has_content) {
		if (defined $clen) {
		    warn "Content-Length header value was wrong, fixed";
		    hlist_remove(\@h, 'Content-Length');
		}
		push(@h, 'Content-Length' => $has_content);
	    }
	}
	elsif ($clen) {
	    warn "Content-Length set when there is no content, fixed";
	    hlist_remove(\@h, 'Content-Length');
	}
    }

    my $write_wait = 0;
    $write_wait = 2
	if ($request_headers->header("Expect") || "") =~ /100-continue/;

    my $req_buf = $socket->format_request($method, $fullpath, @h);
    #print "------\n$req_buf\n------\n";

    if (!$has_content || $write_wait || $has_content > 8*1024) {
        do {
            # Since this just writes out the header block it should almost
            # always succeed to send the whole buffer in a single write call.
            my $n = $socket->syswrite($req_buf, length($req_buf));
            unless (defined $n) {
                redo if $!{EINTR};
                if ($!{EAGAIN}) {
                    select(undef, undef, undef, 0.1);
                    redo;
                }
                die "write failed: $!";
            }
            if ($n) {
                substr($req_buf, 0, $n, "");
            }
            else {
                select(undef, undef, undef, 0.5);
            }
        }
        while (length $req_buf);
    }

    my($code, $mess, @junk);
    my $drop_connection;

    if ($has_content) {
	my $eof;
	my $wbuf;
	my $woffset = 0;
	if (ref($content_ref) eq 'CODE') {
	    my $buf = &$content_ref();
	    $buf = "" unless defined($buf);
	    $buf = sprintf "%x%s%s%s", length($buf), $CRLF, $buf, $CRLF
		if $chunked;
	    substr($buf, 0, 0) = $req_buf if $req_buf;
	    $wbuf = \$buf;
	}
	else {
	    if ($req_buf) {
		my $buf = $req_buf . $$content_ref;
		$wbuf = \$buf;
	    }
	    else {
		$wbuf = $content_ref;
	    }
	    $eof = 1;
	}

	my $fbits = '';
	vec($fbits, fileno($socket), 1) = 1;

      WRITE:
	while ($woffset < length($$wbuf)) {

	    my $sel_timeout = $timeout;
	    if ($write_wait) {
		$sel_timeout = $write_wait if $write_wait < $sel_timeout;
	    }
	    my $time_before;
            $time_before = time if $sel_timeout;

	    my $rbits = $fbits;
	    my $wbits = $write_wait ? undef : $fbits;
            my $sel_timeout_before = $sel_timeout;
          SELECT:
            {
                my $nfound = select($rbits, $wbits, undef, $sel_timeout);
                unless (defined $nfound) {
                    if ($!{EINTR} || $!{EAGAIN}) {
                        if ($time_before) {
                            $sel_timeout = $sel_timeout_before - (time - $time_before);
                            $sel_timeout = 0 if $sel_timeout < 0;
                        }
                        redo SELECT;
                    }
                    die "select failed: $!";
                }
	    }

	    if ($write_wait) {
		$write_wait -= time - $time_before;
		$write_wait = 0 if $write_wait < 0;
	    }

	    if (defined($rbits) && $rbits =~ /[^\0]/) {
		# readable
		my $buf = $socket->_rbuf;
		my $n = $socket->sysread($buf, 1024, length($buf));
                unless (defined $n) {
                    die "read failed: $!" unless  $!{EINTR} || $!{EAGAIN};
                    # if we get here the rest of the block will do nothing
                    # and we will retry the read on the next round
                }
		elsif ($n == 0) {
                    # the server closed the connection before we finished
                    # writing all the request content.  No need to write any more.
                    $drop_connection++;
                    last WRITE;
		}
		$socket->_rbuf($buf);
		if (!$code && $buf =~ /\015?\012\015?\012/) {
		    # a whole response header is present, so we can read it without blocking
		    ($code, $mess, @h) = $socket->read_response_headers(laxed => 1,
									junk_out => \@junk,
								       );
		    if ($code eq "100") {
			$write_wait = 0;
			undef($code);
		    }
		    else {
			$drop_connection++;
			last WRITE;
			# XXX should perhaps try to abort write in a nice way too
		    }
		}
	    }
	    if (defined($wbits) && $wbits =~ /[^\0]/) {
		my $n = $socket->syswrite($$wbuf, length($$wbuf), $woffset);
                unless (defined $n) {
                    die "write failed: $!" unless $!{EINTR} || $!{EAGAIN};
                    $n = 0;  # will retry write on the next round
                }
                elsif ($n == 0) {
		    die "write failed: no bytes written";
		}
		$woffset += $n;

		if (!$eof && $woffset >= length($$wbuf)) {
		    # need to refill buffer from $content_ref code
		    my $buf = &$content_ref();
		    $buf = "" unless defined($buf);
		    $eof++ unless length($buf);
		    $buf = sprintf "%x%s%s%s", length($buf), $CRLF, $buf, $CRLF
			if $chunked;
		    $wbuf = \$buf;
		    $woffset = 0;
		}
	    }
	} # WRITE
    }

    ($code, $mess, @h) = $socket->read_response_headers(laxed => 1, junk_out => \@junk)
	unless $code;
    ($code, $mess, @h) = $socket->read_response_headers(laxed => 1, junk_out => \@junk)
	if $code eq "100";

    my $response = HTTP::Response->new($code, $mess);
    my $peer_http_version = $socket->peer_http_version;
    $response->protocol("HTTP/$peer_http_version");
    while (@h) {
	my($k, $v) = splice(@h, 0, 2);
	$response->push_header($k, $v);
    }
    $response->push_header("Client-Junk" => \@junk) if @junk;

    $response->request($request);
    $self->_get_sock_info($response, $socket);

    if ($method eq "CONNECT") {
	$response->{client_socket} = $socket;  # so it can be picked up
	return $response;
    }

    if (my @te = $response->remove_header('Transfer-Encoding')) {
	$response->push_header('Client-Transfer-Encoding', \@te);
    }
    $response->push_header('Client-Response-Num', $socket->increment_response_count);

    my $complete;
    $response = $self->collect($arg, $response, sub {
	my $buf = ""; #prevent use of uninitialized value in SSLeay.xs
	my $n;
      READ:
	{
	    $n = $socket->read_entity_body($buf, $size);
            unless (defined $n) {
                redo READ if $!{EINTR} || $!{EAGAIN};
                die "read failed: $!";
            }
	    redo READ if $n == -1;
	}
	$complete++ if !$n;
        return \$buf;
    } );
    $drop_connection++ unless $complete;

    @h = $socket->get_trailers;
    while (@h) {
	my($k, $v) = splice(@h, 0, 2);
	$response->push_header($k, $v);
    }

    # keep-alive support
    unless ($drop_connection) {
	if (my $conn_cache = $self->{ua}{conn_cache}) {
	    my %connection = map { (lc($_) => 1) }
		             split(/\s*,\s*/, ($response->header("Connection") || ""));
	    if (($peer_http_version eq "1.1" && !$connection{close}) ||
		$connection{"keep-alive"})
	    {
		LWP::Debug::debug("Keep the http connection to $host:$port");
		$conn_cache->deposit("http", "$host:$port", $socket);
	    }
	}
    }

    $response;
}


#-----------------------------------------------------------
package LWP::Protocol::http::SocketMethods;

sub sysread {
    my $self = shift;
    if (my $timeout = ${*$self}{io_socket_timeout}) {
	die "read timeout" unless $self->can_read($timeout);
    }
    else {
	# since we have made the socket non-blocking we
	# use select to wait for some data to arrive
	$self->can_read(undef) || die "Assert";
    }
    sysread($self, $_[0], $_[1], $_[2] || 0);
}

sub can_read {
    my($self, $timeout) = @_;
    my $fbits = '';
    vec($fbits, fileno($self), 1) = 1;
  SELECT:
    {
        my $before;
        $before = time if $timeout;
        my $nfound = select($fbits, undef, undef, $timeout);
        unless (defined $nfound) {
            if ($!{EINTR} || $!{EAGAIN}) {
                # don't really think EAGAIN can happen here
                if ($timeout) {
                    $timeout -= time - $before;
                    $timeout = 0 if $timeout < 0;
                }
                redo SELECT;
            }
            die "select failed: $!";
        }
        return $nfound > 0;
    }
}

sub ping {
    my $self = shift;
    !$self->can_read(0);
}

sub increment_response_count {
    my $self = shift;
    return ++${*$self}{'myhttp_response_count'};
}

#-----------------------------------------------------------
package LWP::Protocol::http::Socket;
use vars qw(@ISA);
@ISA = qw(LWP::Protocol::http::SocketMethods Net::HTTP);

1;
