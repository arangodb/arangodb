package Net::HTTP::Methods;

require 5.005;  # 4-arg substr

use strict;
use vars qw($VERSION);

$VERSION = "5.812";

my $CRLF = "\015\012";   # "\r\n" is not portable

sub new {
    my $class = shift;
    unshift(@_, "Host") if @_ == 1;
    my %cnf = @_;
    require Symbol;
    my $self = bless Symbol::gensym(), $class;
    return $self->http_configure(\%cnf);
}

sub http_configure {
    my($self, $cnf) = @_;

    die "Listen option not allowed" if $cnf->{Listen};
    my $explict_host = (exists $cnf->{Host});
    my $host = delete $cnf->{Host};
    my $peer = $cnf->{PeerAddr} || $cnf->{PeerHost};
    if (!$peer) {
	die "No Host option provided" unless $host;
	$cnf->{PeerAddr} = $peer = $host;
    }

    if ($peer =~ s,:(\d+)$,,) {
	$cnf->{PeerPort} = int($1);  # always override
    }
    if (!$cnf->{PeerPort}) {
	$cnf->{PeerPort} = $self->http_default_port;
    }

    if (!$explict_host) {
	$host = $peer;
	$host =~ s/:.*//;
    }
    if ($host && $host !~ /:/) {
	my $p = $cnf->{PeerPort};
	$host .= ":$p" if $p != $self->http_default_port;
    }

    $cnf->{Proto} = 'tcp';

    my $keep_alive = delete $cnf->{KeepAlive};
    my $http_version = delete $cnf->{HTTPVersion};
    $http_version = "1.1" unless defined $http_version;
    my $peer_http_version = delete $cnf->{PeerHTTPVersion};
    $peer_http_version = "1.0" unless defined $peer_http_version;
    my $send_te = delete $cnf->{SendTE};
    my $max_line_length = delete $cnf->{MaxLineLength};
    $max_line_length = 4*1024 unless defined $max_line_length;
    my $max_header_lines = delete $cnf->{MaxHeaderLines};
    $max_header_lines = 128 unless defined $max_header_lines;

    return undef unless $self->http_connect($cnf);

    $self->host($host);
    $self->keep_alive($keep_alive);
    $self->send_te($send_te);
    $self->http_version($http_version);
    $self->peer_http_version($peer_http_version);
    $self->max_line_length($max_line_length);
    $self->max_header_lines($max_header_lines);

    ${*$self}{'http_buf'} = "";

    return $self;
}

sub http_default_port {
    80;
}

# set up property accessors
for my $method (qw(host keep_alive send_te max_line_length max_header_lines peer_http_version)) {
    my $prop_name = "http_" . $method;
    no strict 'refs';
    *$method = sub {
	my $self = shift;
	my $old = ${*$self}{$prop_name};
	${*$self}{$prop_name} = shift if @_;
	return $old;
    };
}

# we want this one to be a bit smarter
sub http_version {
    my $self = shift;
    my $old = ${*$self}{'http_version'};
    if (@_) {
	my $v = shift;
	$v = "1.0" if $v eq "1";  # float
	unless ($v eq "1.0" or $v eq "1.1") {
	    require Carp;
	    Carp::croak("Unsupported HTTP version '$v'");
	}
	${*$self}{'http_version'} = $v;
    }
    $old;
}

sub format_request {
    my $self = shift;
    my $method = shift;
    my $uri = shift;

    my $content = (@_ % 2) ? pop : "";

    for ($method, $uri) {
	require Carp;
	Carp::croak("Bad method or uri") if /\s/ || !length;
    }

    push(@{${*$self}{'http_request_method'}}, $method);
    my $ver = ${*$self}{'http_version'};
    my $peer_ver = ${*$self}{'http_peer_http_version'} || "1.0";

    my @h;
    my @connection;
    my %given = (host => 0, "content-length" => 0, "te" => 0);
    while (@_) {
	my($k, $v) = splice(@_, 0, 2);
	my $lc_k = lc($k);
	if ($lc_k eq "connection") {
	    $v =~ s/^\s+//;
	    $v =~ s/\s+$//;
	    push(@connection, split(/\s*,\s*/, $v));
	    next;
	}
	if (exists $given{$lc_k}) {
	    $given{$lc_k}++;
	}
	push(@h, "$k: $v");
    }

    if (length($content) && !$given{'content-length'}) {
	push(@h, "Content-Length: " . length($content));
    }

    my @h2;
    if ($given{te}) {
	push(@connection, "TE") unless grep lc($_) eq "te", @connection;
    }
    elsif ($self->send_te && zlib_ok()) {
	# gzip is less wanted since the Compress::Zlib interface for
	# it does not really allow chunked decoding to take place easily.
	push(@h2, "TE: deflate,gzip;q=0.3");
	push(@connection, "TE");
    }

    unless (grep lc($_) eq "close", @connection) {
	if ($self->keep_alive) {
	    if ($peer_ver eq "1.0") {
		# from looking at Netscape's headers
		push(@h2, "Keep-Alive: 300");
		unshift(@connection, "Keep-Alive");
	    }
	}
	else {
	    push(@connection, "close") if $ver ge "1.1";
	}
    }
    push(@h2, "Connection: " . join(", ", @connection)) if @connection;
    unless ($given{host}) {
	my $h = ${*$self}{'http_host'};
	push(@h2, "Host: $h") if $h;
    }

    return join($CRLF, "$method $uri HTTP/$ver", @h2, @h, "", $content);
}


sub write_request {
    my $self = shift;
    $self->print($self->format_request(@_));
}

sub format_chunk {
    my $self = shift;
    return $_[0] unless defined($_[0]) && length($_[0]);
    return sprintf("%x", length($_[0])) . $CRLF . $_[0] . $CRLF;
}

sub write_chunk {
    my $self = shift;
    return 1 unless defined($_[0]) && length($_[0]);
    $self->print(sprintf("%x", length($_[0])) . $CRLF . $_[0] . $CRLF);
}

sub format_chunk_eof {
    my $self = shift;
    my @h;
    while (@_) {
	push(@h, sprintf "%s: %s$CRLF", splice(@_, 0, 2));
    }
    return join("", "0$CRLF", @h, $CRLF);
}

sub write_chunk_eof {
    my $self = shift;
    $self->print($self->format_chunk_eof(@_));
}


sub my_read {
    die if @_ > 3;
    my $self = shift;
    my $len = $_[1];
    for (${*$self}{'http_buf'}) {
	if (length) {
	    $_[0] = substr($_, 0, $len, "");
	    return length($_[0]);
	}
	else {
	    return $self->sysread($_[0], $len);
	}
    }
}


sub my_readline {
    my $self = shift;
    for (${*$self}{'http_buf'}) {
	my $max_line_length = ${*$self}{'http_max_line_length'};
	my $pos;
	while (1) {
	    # find line ending
	    $pos = index($_, "\012");
	    last if $pos >= 0;
	    die "Line too long (limit is $max_line_length)"
		if $max_line_length && length($_) > $max_line_length;

	    # need to read more data to find a line ending
          READ:
            {
                my $n = $self->sysread($_, 1024, length);
                unless (defined $n) {
                    redo READ if $!{EINTR};
                    if ($!{EAGAIN}) {
                        # Hmm, we must be reading from a non-blocking socket
                        # XXX Should really wait until this socket is readable,...
                        select(undef, undef, undef, 0.1);  # but this will do for now
                        redo READ;
                    }
                    # if we have already accumulated some data let's at least
                    # return that as a line
                    die "read failed: $!" unless length;
                }
                unless ($n) {
                    return undef unless length;
                    return substr($_, 0, length, "");
                }
            }
	}
	die "Line too long ($pos; limit is $max_line_length)"
	    if $max_line_length && $pos > $max_line_length;

	my $line = substr($_, 0, $pos+1, "");
	$line =~ s/(\015?\012)\z// || die "Assert";
	return wantarray ? ($line, $1) : $line;
    }
}


sub _rbuf {
    my $self = shift;
    if (@_) {
	for (${*$self}{'http_buf'}) {
	    my $old;
	    $old = $_ if defined wantarray;
	    $_ = shift;
	    return $old;
	}
    }
    else {
	return ${*$self}{'http_buf'};
    }
}

sub _rbuf_length {
    my $self = shift;
    return length ${*$self}{'http_buf'};
}


sub _read_header_lines {
    my $self = shift;
    my $junk_out = shift;

    my @headers;
    my $line_count = 0;
    my $max_header_lines = ${*$self}{'http_max_header_lines'};
    while (my $line = my_readline($self)) {
	if ($line =~ /^(\S+)\s*:\s*(.*)/s) {
	    push(@headers, $1, $2);
	}
	elsif (@headers && $line =~ s/^\s+//) {
	    $headers[-1] .= " " . $line;
	}
	elsif ($junk_out) {
	    push(@$junk_out, $line);
	}
	else {
	    die "Bad header: '$line'\n";
	}
	if ($max_header_lines) {
	    $line_count++;
	    if ($line_count >= $max_header_lines) {
		die "Too many header lines (limit is $max_header_lines)";
	    }
	}
    }
    return @headers;
}


sub read_response_headers {
    my($self, %opt) = @_;
    my $laxed = $opt{laxed};

    my($status, $eol) = my_readline($self);
    unless (defined $status) {
	die "Server closed connection without sending any data back";
    }

    my($peer_ver, $code, $message) = split(/\s+/, $status, 3);
    if (!$peer_ver || $peer_ver !~ s,^HTTP/,, || $code !~ /^[1-5]\d\d$/) {
	die "Bad response status line: '$status'" unless $laxed;
	# assume HTTP/0.9
	${*$self}{'http_peer_http_version'} = "0.9";
	${*$self}{'http_status'} = "200";
	substr(${*$self}{'http_buf'}, 0, 0) = $status . ($eol || "");
	return 200 unless wantarray;
	return (200, "Assumed OK");
    };

    ${*$self}{'http_peer_http_version'} = $peer_ver;
    ${*$self}{'http_status'} = $code;

    my $junk_out;
    if ($laxed) {
	$junk_out = $opt{junk_out} || [];
    }
    my @headers = $self->_read_header_lines($junk_out);

    # pick out headers that read_entity_body might need
    my @te;
    my $content_length;
    for (my $i = 0; $i < @headers; $i += 2) {
	my $h = lc($headers[$i]);
	if ($h eq 'transfer-encoding') {
	    my $te = $headers[$i+1];
	    $te =~ s/^\s+//;
	    $te =~ s/\s+$//;
	    push(@te, $te) if length($te);
	}
	elsif ($h eq 'content-length') {
	    # ignore bogus and overflow values
	    if ($headers[$i+1] =~ /^\s*(\d{1,15})(?:\s|$)/) {
		$content_length = $1;
	    }
	}
    }
    ${*$self}{'http_te'} = join(",", @te);
    ${*$self}{'http_content_length'} = $content_length;
    ${*$self}{'http_first_body'}++;
    delete ${*$self}{'http_trailers'};
    return $code unless wantarray;
    return ($code, $message, @headers);
}


sub read_entity_body {
    my $self = shift;
    my $buf_ref = \$_[0];
    my $size = $_[1];
    die "Offset not supported yet" if $_[2];

    my $chunked;
    my $bytes;

    if (${*$self}{'http_first_body'}) {
	${*$self}{'http_first_body'} = 0;
	delete ${*$self}{'http_chunked'};
	delete ${*$self}{'http_bytes'};
	my $method = shift(@{${*$self}{'http_request_method'}});
	my $status = ${*$self}{'http_status'};
	if ($method eq "HEAD") {
	    # this response is always empty regardless of other headers
	    $bytes = 0;
	}
	elsif (my $te = ${*$self}{'http_te'}) {
	    my @te = split(/\s*,\s*/, lc($te));
	    die "Chunked must be last Transfer-Encoding '$te'"
		unless pop(@te) eq "chunked";

	    for (@te) {
		if ($_ eq "deflate" && zlib_ok()) {
		    #require Compress::Zlib;
		    my $i = Compress::Zlib::inflateInit();
		    die "Can't make inflator" unless $i;
		    $_ = sub { scalar($i->inflate($_[0])) }
		}
		elsif ($_ eq "gzip" && zlib_ok()) {
		    #require Compress::Zlib;
		    my @buf;
		    $_ = sub {
			push(@buf, $_[0]);
			return Compress::Zlib::memGunzip(join("", @buf)) if $_[1];
			return "";
		    };
		}
		elsif ($_ eq "identity") {
		    $_ = sub { $_[0] };
		}
		else {
		    die "Can't handle transfer encoding '$te'";
		}
	    }

	    @te = reverse(@te);

	    ${*$self}{'http_te2'} = @te ? \@te : "";
	    $chunked = -1;
	}
	elsif (defined(my $content_length = ${*$self}{'http_content_length'})) {
	    $bytes = $content_length;
	}
        elsif ($status =~ /^(?:1|[23]04)/) {
            # RFC 2616 says that these responses should always be empty
            # but that does not appear to be true in practice [RT#17907]
            $bytes = 0;
        }
	else {
	    # XXX Multi-Part types are self delimiting, but RFC 2616 says we
	    # only has to deal with 'multipart/byteranges'

	    # Read until EOF
	}
    }
    else {
	$chunked = ${*$self}{'http_chunked'};
	$bytes   = ${*$self}{'http_bytes'};
    }

    if (defined $chunked) {
	# The state encoded in $chunked is:
	#   $chunked == 0:   read CRLF after chunk, then chunk header
        #   $chunked == -1:  read chunk header
	#   $chunked > 0:    bytes left in current chunk to read

	if ($chunked <= 0) {
	    my $line = my_readline($self);
	    if ($chunked == 0) {
		die "Missing newline after chunk data: '$line'"
		    if !defined($line) || $line ne "";
		$line = my_readline($self);
	    }
	    die "EOF when chunk header expected" unless defined($line);
	    my $chunk_len = $line;
	    $chunk_len =~ s/;.*//;  # ignore potential chunk parameters
	    unless ($chunk_len =~ /^([\da-fA-F]+)\s*$/) {
		die "Bad chunk-size in HTTP response: $line";
	    }
	    $chunked = hex($1);
	    if ($chunked == 0) {
		${*$self}{'http_trailers'} = [$self->_read_header_lines];
		$$buf_ref = "";

		my $n = 0;
		if (my $transforms = delete ${*$self}{'http_te2'}) {
		    for (@$transforms) {
			$$buf_ref = &$_($$buf_ref, 1);
		    }
		    $n = length($$buf_ref);
		}

		# in case somebody tries to read more, make sure we continue
		# to return EOF
		delete ${*$self}{'http_chunked'};
		${*$self}{'http_bytes'} = 0;

		return $n;
	    }
	}

	my $n = $chunked;
	$n = $size if $size && $size < $n;
	$n = my_read($self, $$buf_ref, $n);
	return undef unless defined $n;

	${*$self}{'http_chunked'} = $chunked - $n;

	if ($n > 0) {
	    if (my $transforms = ${*$self}{'http_te2'}) {
		for (@$transforms) {
		    $$buf_ref = &$_($$buf_ref, 0);
		}
		$n = length($$buf_ref);
		$n = -1 if $n == 0;
	    }
	}
	return $n;
    }
    elsif (defined $bytes) {
	unless ($bytes) {
	    $$buf_ref = "";
	    return 0;
	}
	my $n = $bytes;
	$n = $size if $size && $size < $n;
	$n = my_read($self, $$buf_ref, $n);
	return undef unless defined $n;
	${*$self}{'http_bytes'} = $bytes - $n;
	return $n;
    }
    else {
	# read until eof
	$size ||= 8*1024;
	return my_read($self, $$buf_ref, $size);
    }
}

sub get_trailers {
    my $self = shift;
    @{${*$self}{'http_trailers'} || []};
}

BEGIN {
my $zlib_ok;

sub zlib_ok {
    return $zlib_ok if defined $zlib_ok;

    # Try to load Compress::Zlib.
    local $@;
    local $SIG{__DIE__};
    $zlib_ok = 0;

    eval {
	require Compress::Zlib;
	Compress::Zlib->VERSION(1.10);
	$zlib_ok++;
    };

    return $zlib_ok;
}

} # BEGIN

1;
