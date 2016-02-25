package HTTP::Message;

use strict;
use vars qw($VERSION $AUTOLOAD);
$VERSION = "5.812";

require HTTP::Headers;
require Carp;

my $CRLF = "\015\012";   # "\r\n" is not portable
$HTTP::URI_CLASS ||= $ENV{PERL_HTTP_URI_CLASS} || "URI";
eval "require $HTTP::URI_CLASS"; die $@ if $@;

*_utf8_downgrade = defined(&utf8::downgrade) ?
    sub {
        utf8::downgrade($_[0], 1) or
            Carp::croak("HTTP::Message content must be bytes")
    }
    :
    sub {
    };

sub new
{
    my($class, $header, $content) = @_;
    if (defined $header) {
	Carp::croak("Bad header argument") unless ref $header;
        if (ref($header) eq "ARRAY") {
	    $header = HTTP::Headers->new(@$header);
	}
	else {
	    $header = $header->clone;
	}
    }
    else {
	$header = HTTP::Headers->new;
    }
    if (defined $content) {
        _utf8_downgrade($content);
    }
    else {
        $content = '';
    }

    bless {
	'_headers' => $header,
	'_content' => $content,
    }, $class;
}


sub parse
{
    my($class, $str) = @_;

    my @hdr;
    while (1) {
	if ($str =~ s/^([^\s:]+)[ \t]*: ?(.*)\n?//) {
	    push(@hdr, $1, $2);
	    $hdr[-1] =~ s/\r\z//;
	}
	elsif (@hdr && $str =~ s/^([ \t].*)\n?//) {
	    $hdr[-1] .= "\n$1";
	    $hdr[-1] =~ s/\r\z//;
	}
	else {
	    $str =~ s/^\r?\n//;
	    last;
	}
    }

    new($class, \@hdr, $str);
}


sub clone
{
    my $self  = shift;
    my $clone = HTTP::Message->new($self->headers,
				   $self->content);
    $clone->protocol($self->protocol);
    $clone;
}


sub clear {
    my $self = shift;
    $self->{_headers}->clear;
    $self->content("");
    delete $self->{_parts};
    return;
}


sub protocol { shift->_elem('_protocol',  @_); }

sub content  {

    my $self = $_[0];
    if (defined(wantarray)) {
	$self->_content unless exists $self->{_content};
	my $old = $self->{_content};
	$old = $$old if ref($old) eq "SCALAR";
	&_set_content if @_ > 1;
	return $old;
    }

    if (@_ > 1) {
	&_set_content;
    }
    else {
	Carp::carp("Useless content call in void context") if $^W;
    }
}

sub _set_content {
    my $self = $_[0];
    _utf8_downgrade($_[1]);
    if (!ref($_[1]) && ref($self->{_content}) eq "SCALAR") {
	${$self->{_content}} = $_[1];
    }
    else {
	die "Can't set content to be a scalar reference" if ref($_[1]) eq "SCALAR";
	$self->{_content} = $_[1];
	delete $self->{_content_ref};
    }
    delete $self->{_parts} unless $_[2];
}


sub add_content
{
    my $self = shift;
    $self->_content unless exists $self->{_content};
    my $chunkref = \$_[0];
    $chunkref = $$chunkref if ref($$chunkref);  # legacy

    _utf8_downgrade($$chunkref);

    my $ref = ref($self->{_content});
    if (!$ref) {
	$self->{_content} .= $$chunkref;
    }
    elsif ($ref eq "SCALAR") {
	${$self->{_content}} .= $$chunkref;
    }
    else {
	Carp::croak("Can't append to $ref content");
    }
    delete $self->{_parts};
}

sub add_content_utf8 {
    my($self, $buf)  = @_;
    utf8::upgrade($buf);
    utf8::encode($buf);
    $self->add_content($buf);
}

sub content_ref
{
    my $self = shift;
    $self->_content unless exists $self->{_content};
    delete $self->{_parts};
    my $old = \$self->{_content};
    my $old_cref = $self->{_content_ref};
    if (@_) {
	my $new = shift;
	Carp::croak("Setting content_ref to a non-ref") unless ref($new);
	delete $self->{_content};  # avoid modifying $$old
	$self->{_content} = $new;
	$self->{_content_ref}++;
    }
    $old = $$old if $old_cref;
    return $old;
}


sub decoded_content
{
    my($self, %opt) = @_;
    my $content_ref;
    my $content_ref_iscopy;

    eval {

	require HTTP::Headers::Util;
	my($ct, %ct_param);
	if (my @ct = HTTP::Headers::Util::split_header_words($self->header("Content-Type"))) {
	    ($ct, undef, %ct_param) = @{$ct[-1]};
	    $ct = lc($ct);

	    die "Can't decode multipart content" if $ct =~ m,^multipart/,;
	}

	$content_ref = $self->content_ref;
	die "Can't decode ref content" if ref($content_ref) ne "SCALAR";

	if (my $h = $self->header("Content-Encoding")) {
	    $h =~ s/^\s+//;
	    $h =~ s/\s+$//;
	    for my $ce (reverse split(/\s*,\s*/, lc($h))) {
		next unless $ce || $ce eq "identity";
		if ($ce eq "gzip" || $ce eq "x-gzip") {
		    require Compress::Zlib;
		    unless ($content_ref_iscopy) {
			# memGunzip is documented to destroy its buffer argument
			my $copy = $$content_ref;
			$content_ref = \$copy;
			$content_ref_iscopy++;
		    }
		    $content_ref = \Compress::Zlib::memGunzip($$content_ref);
		    die "Can't gunzip content" unless defined $$content_ref;
		}
		elsif ($ce eq "x-bzip2") {
		    require Compress::Bzip2;
		    $content_ref = Compress::Bzip2::decompress($$content_ref);
		    die "Can't bunzip content" unless defined $$content_ref;
		    $content_ref_iscopy++;
		}
		elsif ($ce eq "deflate") {
		    require Compress::Zlib;
		    my $out = Compress::Zlib::uncompress($$content_ref);
		    unless (defined $out) {
			# "Content-Encoding: deflate" is supposed to mean the "zlib"
                        # format of RFC 1950, but Microsoft got that wrong, so some
                        # servers sends the raw compressed "deflate" data.  This
                        # tries to inflate this format.
			unless ($content_ref_iscopy) {
			    # the $i->inflate method is documented to destroy its
			    # buffer argument
			    my $copy = $$content_ref;
			    $content_ref = \$copy;
			    $content_ref_iscopy++;
			}

			my($i, $status) = Compress::Zlib::inflateInit(
			    WindowBits => -Compress::Zlib::MAX_WBITS(),
                        );
			my $OK = Compress::Zlib::Z_OK();
			die "Can't init inflate object" unless $i && $status == $OK;
			($out, $status) = $i->inflate($content_ref);
			if ($status != Compress::Zlib::Z_STREAM_END()) {
			    if ($status == $OK) {
				$self->push_header("Client-Warning" =>
				    "Content might be truncated; incomplete deflate stream");
			    }
			    else {
				# something went bad, can't trust $out any more
				$out = undef;
			    }
			}
		    }
		    die "Can't inflate content" unless defined $out;
		    $content_ref = \$out;
		    $content_ref_iscopy++;
		}
		elsif ($ce eq "compress" || $ce eq "x-compress") {
		    die "Can't uncompress content";
		}
		elsif ($ce eq "base64") {  # not really C-T-E, but should be harmless
		    require MIME::Base64;
		    $content_ref = \MIME::Base64::decode($$content_ref);
		    $content_ref_iscopy++;
		}
		elsif ($ce eq "quoted-printable") { # not really C-T-E, but should be harmless
		    require MIME::QuotedPrint;
		    $content_ref = \MIME::QuotedPrint::decode($$content_ref);
		    $content_ref_iscopy++;
		}
		else {
		    die "Don't know how to decode Content-Encoding '$ce'";
		}
	    }
	}

	if ($ct && $ct =~ m,^text/,,) {
	    my $charset = $opt{charset} || $ct_param{charset} || $opt{default_charset} || "ISO-8859-1";
	    $charset = lc($charset);
	    if ($charset ne "none") {
		require Encode;
		if (do{my $v = $Encode::VERSION; $v =~ s/_//g; $v} < 2.0901 &&
		    !$content_ref_iscopy)
		{
		    # LEAVE_SRC did not work before Encode-2.0901
		    my $copy = $$content_ref;
		    $content_ref = \$copy;
		    $content_ref_iscopy++;
		}
		$content_ref = \Encode::decode($charset, $$content_ref,
		     ($opt{charset_strict} ? Encode::FB_CROAK() : 0) | Encode::LEAVE_SRC());
	    }
	}
    };
    if ($@) {
	Carp::croak($@) if $opt{raise_error};
	return undef;
    }

    return $opt{ref} ? $content_ref : $$content_ref;
}


sub as_string
{
    my($self, $eol) = @_;
    $eol = "\n" unless defined $eol;

    # The calculation of content might update the headers
    # so we need to do that first.
    my $content = $self->content;

    return join("", $self->{'_headers'}->as_string($eol),
		    $eol,
		    $content,
		    (@_ == 1 && length($content) &&
		     $content !~ /\n\z/) ? "\n" : "",
		);
}


sub headers            { shift->{'_headers'};                }
sub headers_as_string  { shift->{'_headers'}->as_string(@_); }


sub parts {
    my $self = shift;
    if (defined(wantarray) && (!exists $self->{_parts} || ref($self->{_content}) eq "SCALAR")) {
	$self->_parts;
    }
    my $old = $self->{_parts};
    if (@_) {
	my @parts = map { ref($_) eq 'ARRAY' ? @$_ : $_ } @_;
	my $ct = $self->content_type || "";
	if ($ct =~ m,^message/,) {
	    Carp::croak("Only one part allowed for $ct content")
		if @parts > 1;
	}
	elsif ($ct !~ m,^multipart/,) {
	    $self->remove_content_headers;
	    $self->content_type("multipart/mixed");
	}
	$self->{_parts} = \@parts;
	_stale_content($self);
    }
    return @$old if wantarray;
    return $old->[0];
}

sub add_part {
    my $self = shift;
    if (($self->content_type || "") !~ m,^multipart/,) {
	my $p = HTTP::Message->new($self->remove_content_headers,
				   $self->content(""));
	$self->content_type("multipart/mixed");
	$self->{_parts} = [$p];
    }
    elsif (!exists $self->{_parts} || ref($self->{_content}) eq "SCALAR") {
	$self->_parts;
    }

    push(@{$self->{_parts}}, @_);
    _stale_content($self);
    return;
}

sub _stale_content {
    my $self = shift;
    if (ref($self->{_content}) eq "SCALAR") {
	# must recalculate now
	$self->_content;
    }
    else {
	# just invalidate cache
	delete $self->{_content};
	delete $self->{_content_ref};
    }
}


# delegate all other method calls the the _headers object.
sub AUTOLOAD
{
    my $method = substr($AUTOLOAD, rindex($AUTOLOAD, '::')+2);
    return if $method eq "DESTROY";

    # We create the function here so that it will not need to be
    # autoloaded the next time.
    no strict 'refs';
    *$method = eval "sub { shift->{'_headers'}->$method(\@_) }";
    goto &$method;
}


# Private method to access members in %$self
sub _elem
{
    my $self = shift;
    my $elem = shift;
    my $old = $self->{$elem};
    $self->{$elem} = $_[0] if @_;
    return $old;
}


# Create private _parts attribute from current _content
sub _parts {
    my $self = shift;
    my $ct = $self->content_type;
    if ($ct =~ m,^multipart/,) {
	require HTTP::Headers::Util;
	my @h = HTTP::Headers::Util::split_header_words($self->header("Content-Type"));
	die "Assert" unless @h;
	my %h = @{$h[0]};
	if (defined(my $b = $h{boundary})) {
	    my $str = $self->content;
	    $str =~ s/\r?\n--\Q$b\E--\r?\n.*//s;
	    if ($str =~ s/(^|.*?\r?\n)--\Q$b\E\r?\n//s) {
		$self->{_parts} = [map HTTP::Message->parse($_),
				   split(/\r?\n--\Q$b\E\r?\n/, $str)]
	    }
	}
    }
    elsif ($ct eq "message/http") {
	require HTTP::Request;
	require HTTP::Response;
	my $content = $self->content;
	my $class = ($content =~ m,^(HTTP/.*)\n,) ?
	    "HTTP::Response" : "HTTP::Request";
	$self->{_parts} = [$class->parse($content)];
    }
    elsif ($ct =~ m,^message/,) {
	$self->{_parts} = [ HTTP::Message->parse($self->content) ];
    }

    $self->{_parts} ||= [];
}


# Create private _content attribute from current _parts
sub _content {
    my $self = shift;
    my $ct = $self->header("Content-Type") || "multipart/mixed";
    if ($ct =~ m,^\s*message/,i) {
	_set_content($self, $self->{_parts}[0]->as_string($CRLF), 1);
	return;
    }

    require HTTP::Headers::Util;
    my @v = HTTP::Headers::Util::split_header_words($ct);
    Carp::carp("Multiple Content-Type headers") if @v > 1;
    @v = @{$v[0]};

    my $boundary;
    my $boundary_index;
    for (my @tmp = @v; @tmp;) {
	my($k, $v) = splice(@tmp, 0, 2);
	if (lc($k) eq "boundary") {
	    $boundary = $v;
	    $boundary_index = @v - @tmp - 1;
	    last;
	}
    }

    my @parts = map $_->as_string($CRLF), @{$self->{_parts}};

    my $bno = 0;
    $boundary = _boundary() unless defined $boundary;
 CHECK_BOUNDARY:
    {
	for (@parts) {
	    if (index($_, $boundary) >= 0) {
		# must have a better boundary
		$boundary = _boundary(++$bno);
		redo CHECK_BOUNDARY;
	    }
	}
    }

    if ($boundary_index) {
	$v[$boundary_index] = $boundary;
    }
    else {
	push(@v, boundary => $boundary);
    }

    $ct = HTTP::Headers::Util::join_header_words(@v);
    $self->header("Content-Type", $ct);

    _set_content($self, "--$boundary$CRLF" .
	                join("$CRLF--$boundary$CRLF", @parts) .
			"$CRLF--$boundary--$CRLF",
                        1);
}


sub _boundary
{
    my $size = shift || return "xYzZY";
    require MIME::Base64;
    my $b = MIME::Base64::encode(join("", map chr(rand(256)), 1..$size*3), "");
    $b =~ s/[\W]/X/g;  # ensure alnum only
    $b;
}


1;


__END__

=head1 NAME

HTTP::Message - HTTP style message (base class)

=head1 SYNOPSIS

 use base 'HTTP::Message';

=head1 DESCRIPTION

An C<HTTP::Message> object contains some headers and a content body.
The following methods are available:

=over 4

=item $mess = HTTP::Message->new

=item $mess = HTTP::Message->new( $headers )

=item $mess = HTTP::Message->new( $headers, $content )

This constructs a new message object.  Normally you would want
construct C<HTTP::Request> or C<HTTP::Response> objects instead.

The optional $header argument should be a reference to an
C<HTTP::Headers> object or a plain array reference of key/value pairs.
If an C<HTTP::Headers> object is provided then a copy of it will be
embedded into the constructed message, i.e. it will not be owned and
can be modified afterwards without affecting the message.

The optional $content argument should be a string of bytes.

=item $mess = HTTP::Message->parse( $str )

This constructs a new message object by parsing the given string.

=item $mess->headers

Returns the embedded C<HTTP::Headers> object.

=item $mess->headers_as_string

=item $mess->headers_as_string( $eol )

Call the as_string() method for the headers in the
message.  This will be the same as

    $mess->headers->as_string

but it will make your program a whole character shorter :-)

=item $mess->content

=item $mess->content( $bytes )

The content() method sets the raw content if an argument is given.  If no
argument is given the content is not touched.  In either case the
original raw content is returned.

Note that the content should be a string of bytes.  Strings in perl
can contain characters outside the range of a byte.  The C<Encode>
module can be used to turn such strings into a string of bytes.

=item $mess->add_content( $bytes )

The add_content() methods appends more data bytes to the end of the
current content buffer.

=item $mess->add_content_utf8( $string )

The add_content_utf8() method appends the UTF-8 bytes representing the
string to the end of the current content buffer.

=item $mess->content_ref

=item $mess->content_ref( \$bytes )

The content_ref() method will return a reference to content buffer string.
It can be more efficient to access the content this way if the content
is huge, and it can even be used for direct manipulation of the content,
for instance:

  ${$res->content_ref} =~ s/\bfoo\b/bar/g;

This example would modify the content buffer in-place.

If an argument is passed it will setup the content to reference some
external source.  The content() and add_content() methods
will automatically dereference scalar references passed this way.  For
other references content() will return the reference itself and
add_content() will refuse to do anything.

=item $mess->decoded_content( %options )

Returns the content with any C<Content-Encoding> undone and the raw
content encoded to perl's Unicode strings.  If the C<Content-Encoding>
or C<charset> of the message is unknown this method will fail by
returning C<undef>.

The following options can be specified.

=over

=item C<charset>

This override the charset parameter for text content.  The value
C<none> can used to suppress decoding of the charset.

=item C<default_charset>

This override the default charset of "ISO-8859-1".

=item C<charset_strict>

Abort decoding if malformed characters is found in the content.  By
default you get the substitution character ("\x{FFFD}") in place of
malformed characters.

=item C<raise_error>

If TRUE then raise an exception if not able to decode content.  Reason
might be that the specified C<Content-Encoding> or C<charset> is not
supported.  If this option is FALSE, then decoded_content() will return
C<undef> on errors, but will still set $@.

=item C<ref>

If TRUE then a reference to decoded content is returned.  This might
be more efficient in cases where the decoded content is identical to
the raw content as no data copying is required in this case.

=back

=item $mess->parts

=item $mess->parts( @parts )

=item $mess->parts( \@parts )

Messages can be composite, i.e. contain other messages.  The composite
messages have a content type of C<multipart/*> or C<message/*>.  This
method give access to the contained messages.

The argumentless form will return a list of C<HTTP::Message> objects.
If the content type of $msg is not C<multipart/*> or C<message/*> then
this will return the empty list.  In scalar context only the first
object is returned.  The returned message parts should be regarded as
are read only (future versions of this library might make it possible
to modify the parent by modifying the parts).

If the content type of $msg is C<message/*> then there will only be
one part returned.

If the content type is C<message/http>, then the return value will be
either an C<HTTP::Request> or an C<HTTP::Response> object.

If an @parts argument is given, then the content of the message will be
modified. The array reference form is provided so that an empty list
can be provided.  The @parts array should contain C<HTTP::Message>
objects.  The @parts objects are owned by $mess after this call and
should not be modified or made part of other messages.

When updating the message with this method and the old content type of
$mess is not C<multipart/*> or C<message/*>, then the content type is
set to C<multipart/mixed> and all other content headers are cleared.

This method will croak if the content type is C<message/*> and more
than one part is provided.

=item $mess->add_part( $part )

This will add a part to a message.  The $part argument should be
another C<HTTP::Message> object.  If the previous content type of
$mess is not C<multipart/*> then the old content (together with all
content headers) will be made part #1 and the content type made
C<multipart/mixed> before the new part is added.  The $part object is
owned by $mess after this call and should not be modified or made part
of other messages.

There is no return value.

=item $mess->clear

Will clear the headers and set the content to the empty string.  There
is no return value

=item $mess->protocol

=item $mess->protocol( $proto )

Sets the HTTP protocol used for the message.  The protocol() is a string
like C<HTTP/1.0> or C<HTTP/1.1>.

=item $mess->clone

Returns a copy of the message object.

=item $mess->as_string

=item $mess->as_string( $eol )

Returns the message formatted as a single string.

The optional $eol parameter specifies the line ending sequence to use.
The default is "\n".  If no $eol is given then as_string will ensure
that the returned string is newline terminated (even when the message
content is not).  No extra newline is appended if an explicit $eol is
passed.

=back

All methods unknown to C<HTTP::Message> itself are delegated to the
C<HTTP::Headers> object that is part of every message.  This allows
convenient access to these methods.  Refer to L<HTTP::Headers> for
details of these methods:

    $mess->header( $field => $val )
    $mess->push_header( $field => $val )
    $mess->init_header( $field => $val )
    $mess->remove_header( $field )
    $mess->remove_content_headers
    $mess->header_field_names
    $mess->scan( \&doit )

    $mess->date
    $mess->expires
    $mess->if_modified_since
    $mess->if_unmodified_since
    $mess->last_modified
    $mess->content_type
    $mess->content_encoding
    $mess->content_length
    $mess->content_language
    $mess->title
    $mess->user_agent
    $mess->server
    $mess->from
    $mess->referer
    $mess->www_authenticate
    $mess->authorization
    $mess->proxy_authorization
    $mess->authorization_basic
    $mess->proxy_authorization_basic

=head1 COPYRIGHT

Copyright 1995-2004 Gisle Aas.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

