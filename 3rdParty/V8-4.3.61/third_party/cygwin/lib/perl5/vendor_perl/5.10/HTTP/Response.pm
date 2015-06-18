package HTTP::Response;

require HTTP::Message;
@ISA = qw(HTTP::Message);
$VERSION = "5.813";

use strict;
use HTTP::Status ();



sub new
{
    my($class, $rc, $msg, $header, $content) = @_;
    my $self = $class->SUPER::new($header, $content);
    $self->code($rc);
    $self->message($msg);
    $self;
}


sub parse
{
    my($class, $str) = @_;
    my $status_line;
    if ($str =~ s/^(.*)\n//) {
	$status_line = $1;
    }
    else {
	$status_line = $str;
	$str = "";
    }

    my $self = $class->SUPER::parse($str);
    my($protocol, $code, $message);
    if ($status_line =~ /^\d{3} /) {
       # Looks like a response created by HTTP::Response->new
       ($code, $message) = split(' ', $status_line, 2);
    } else {
       ($protocol, $code, $message) = split(' ', $status_line, 3);
    }
    $self->protocol($protocol) if $protocol;
    $self->code($code) if defined($code);
    $self->message($message) if defined($message);
    $self;
}


sub clone
{
    my $self = shift;
    my $clone = bless $self->SUPER::clone, ref($self);
    $clone->code($self->code);
    $clone->message($self->message);
    $clone->request($self->request->clone) if $self->request;
    # we don't clone previous
    $clone;
}


sub code      { shift->_elem('_rc',      @_); }
sub message   { shift->_elem('_msg',     @_); }
sub previous  { shift->_elem('_previous',@_); }
sub request   { shift->_elem('_request', @_); }


sub status_line
{
    my $self = shift;
    my $code = $self->{'_rc'}  || "000";
    my $mess = $self->{'_msg'} || HTTP::Status::status_message($code) || "Unknown code";
    return "$code $mess";
}


sub base
{
    my $self = shift;
    my $base = $self->header('Content-Base')     ||  # used to be HTTP/1.1
               $self->header('Content-Location') ||  # HTTP/1.1
               $self->header('Base');                # HTTP/1.0
    if ($base && $base =~ /^$URI::scheme_re:/o) {
	# already absolute
	return $HTTP::URI_CLASS->new($base);
    }

    my $req = $self->request;
    if ($req) {
        # if $base is undef here, the return value is effectively
        # just a copy of $self->request->uri.
        return $HTTP::URI_CLASS->new_abs($base, $req->uri);
    }

    # can't find an absolute base
    return undef;
}


sub filename
{
    my $self = shift;
    my $file;

    my $cd = $self->header('Content-Disposition');
    if ($cd) {
	require HTTP::Headers::Util;
	if (my @cd = HTTP::Headers::Util::split_header_words($cd)) {
	    my ($disposition, undef, %cd_param) = @{$cd[-1]};
	    $file = $cd_param{filename};

	    # RFC 2047 encoded?
	    if ($file && $file =~ /^=\?(.+?)\?(.+?)\?(.+)\?=$/) {
		my $charset = $1;
		my $encoding = uc($2);
		my $encfile = $3;

		if ($encoding eq 'Q' || $encoding eq 'B') {
		    local($SIG{__DIE__});
		    eval {
			if ($encoding eq 'Q') {
			    $encfile =~ s/_/ /g;
			    require MIME::QuotedPrint;
			    $encfile = MIME::QuotedPrint::decode($encfile);
			}
			else { # $encoding eq 'B'
			    require MIME::Base64;
			    $encfile = MIME::Base64::decode($encfile);
			}

			require Encode;
			require encoding;
			# This is ugly use of non-public API, but is there
			# a better way to accomplish what we want (locally
			# as-is usable filename string)?
			my $locale_charset = encoding::_get_locale_encoding();
			Encode::from_to($encfile, $charset, $locale_charset);
		    };

		    $file = $encfile unless $@;
		}
	    }
	}
    }

    my $uri;
    unless (defined($file) && length($file)) {
	if (my $cl = $self->header('Content-Location')) {
	    $uri = URI->new($cl);
	}
	elsif (my $request = $self->request) {
	    $uri = $request->uri;
	}

	if ($uri) {
	    $file = ($uri->path_segments)[-1];
	}
    }

    if ($file) {
	$file =~ s,.*[\\/],,;  # basename
    }

    if ($file && !length($file)) {
	$file = undef;
    }

    $file;
}


sub as_string
{
    require HTTP::Status;
    my $self = shift;
    my($eol) = @_;
    $eol = "\n" unless defined $eol;

    my $status_line = $self->status_line;
    my $proto = $self->protocol;
    $status_line = "$proto $status_line" if $proto;

    return join($eol, $status_line, $self->SUPER::as_string(@_));
}


sub is_info     { HTTP::Status::is_info     (shift->{'_rc'}); }
sub is_success  { HTTP::Status::is_success  (shift->{'_rc'}); }
sub is_redirect { HTTP::Status::is_redirect (shift->{'_rc'}); }
sub is_error    { HTTP::Status::is_error    (shift->{'_rc'}); }


sub error_as_HTML
{
    require HTML::Entities;
    my $self = shift;
    my $title = 'An Error Occurred';
    my $body  = HTML::Entities::encode($self->status_line);
    return <<EOM;
<html>
<head><title>$title</title></head>
<body>
<h1>$title</h1>
<p>$body</p>
</body>
</html>
EOM
}


sub current_age
{
    my $self = shift;
    # Implementation of RFC 2616 section 13.2.3
    # (age calculations)
    my $response_time = $self->client_date;
    my $date = $self->date;

    my $age = 0;
    if ($response_time && $date) {
	$age = $response_time - $date;  # apparent_age
	$age = 0 if $age < 0;
    }

    my $age_v = $self->header('Age');
    if ($age_v && $age_v > $age) {
	$age = $age_v;   # corrected_received_age
    }

    my $request = $self->request;
    if ($request) {
	my $request_time = $request->date;
	if ($request_time) {
	    # Add response_delay to age to get 'corrected_initial_age'
	    $age += $response_time - $request_time;
	}
    }
    if ($response_time) {
	$age += time - $response_time;
    }
    return $age;
}


sub freshness_lifetime
{
    my $self = shift;

    # First look for the Cache-Control: max-age=n header
    my @cc = $self->header('Cache-Control');
    if (@cc) {
	my $cc;
	for $cc (@cc) {
	    my $cc_dir;
	    for $cc_dir (split(/\s*,\s*/, $cc)) {
		if ($cc_dir =~ /max-age\s*=\s*(\d+)/i) {
		    return $1;
		}
	    }
	}
    }

    # Next possibility is to look at the "Expires" header
    my $date = $self->date || $self->client_date || time;      
    my $expires = $self->expires;
    unless ($expires) {
	# Must apply heuristic expiration
	my $last_modified = $self->last_modified;
	if ($last_modified) {
	    my $h_exp = ($date - $last_modified) * 0.10;  # 10% since last-mod
	    if ($h_exp < 60) {
		return 60;  # minimum
	    }
	    elsif ($h_exp > 24 * 3600) {
		# Should give a warning if more than 24 hours according to
		# RFC 2616 section 13.2.4, but I don't know how to do it
		# from this function interface, so I just make this the
		# maximum value.
		return 24 * 3600;
	    }
	    return $h_exp;
	}
	else {
	    return 3600;  # 1 hour is fallback when all else fails
	}
    }
    return $expires - $date;
}


sub is_fresh
{
    my $self = shift;
    $self->freshness_lifetime > $self->current_age;
}


sub fresh_until
{
    my $self = shift;
    return $self->freshness_lifetime - $self->current_age + time;
}

1;


__END__

=head1 NAME

HTTP::Response - HTTP style response message

=head1 SYNOPSIS

Response objects are returned by the request() method of the C<LWP::UserAgent>:

    # ...
    $response = $ua->request($request)
    if ($response->is_success) {
        print $response->content;
    }
    else {
        print STDERR $response->status_line, "\n";
    }

=head1 DESCRIPTION

The C<HTTP::Response> class encapsulates HTTP style responses.  A
response consists of a response line, some headers, and a content
body. Note that the LWP library uses HTTP style responses even for
non-HTTP protocol schemes.  Instances of this class are usually
created and returned by the request() method of an C<LWP::UserAgent>
object.

C<HTTP::Response> is a subclass of C<HTTP::Message> and therefore
inherits its methods.  The following additional methods are available:

=over 4

=item $r = HTTP::Response->new( $code )

=item $r = HTTP::Response->new( $code, $msg )

=item $r = HTTP::Response->new( $code, $msg, $header )

=item $r = HTTP::Response->new( $code, $msg, $header, $content )

Constructs a new C<HTTP::Response> object describing a response with
response code $code and optional message $msg.  The optional $header
argument should be a reference to an C<HTTP::Headers> object or a
plain array reference of key/value pairs.  The optional $content
argument should be a string of bytes.  The meaning these arguments are
described below.

=item $r = HTTP::Response->parse( $str )

This constructs a new response object by parsing the given string.

=item $r->code

=item $r->code( $code )

This is used to get/set the code attribute.  The code is a 3 digit
number that encode the overall outcome of a HTTP response.  The
C<HTTP::Status> module provide constants that provide mnemonic names
for the code attribute.

=item $r->message

=item $r->message( $message )

This is used to get/set the message attribute.  The message is a short
human readable single line string that explains the response code.

=item $r->header( $field )

=item $r->header( $field => $value )

This is used to get/set header values and it is inherited from
C<HTTP::Headers> via C<HTTP::Message>.  See L<HTTP::Headers> for
details and other similar methods that can be used to access the
headers.

=item $r->content

=item $r->content( $bytes )

This is used to get/set the raw content and it is inherited from the
C<HTTP::Message> base class.  See L<HTTP::Message> for details and
other methods that can be used to access the content.

=item $r->decoded_content( %options )

This will return the content after any C<Content-Encoding> and
charsets have been decoded.  See L<HTTP::Message> for details.

=item $r->request

=item $r->request( $request )

This is used to get/set the request attribute.  The request attribute
is a reference to the the request that caused this response.  It does
not have to be the same request passed to the $ua->request() method,
because there might have been redirects and authorization retries in
between.

=item $r->previous

=item $r->previous( $response )

This is used to get/set the previous attribute.  The previous
attribute is used to link together chains of responses.  You get
chains of responses if the first response is redirect or unauthorized.
The value is C<undef> if this is the first response in a chain.

=item $r->status_line

Returns the string "E<lt>code> E<lt>message>".  If the message attribute
is not set then the official name of E<lt>code> (see L<HTTP::Status>)
is substituted.

=item $r->base

Returns the base URI for this response.  The return value will be a
reference to a URI object.

The base URI is obtained from one the following sources (in priority
order):

=over 4

=item 1.

Embedded in the document content, for instance <BASE HREF="...">
in HTML documents.

=item 2.

A "Content-Base:" or a "Content-Location:" header in the response.

For backwards compatibility with older HTTP implementations we will
also look for the "Base:" header.

=item 3.

The URI used to request this response. This might not be the original
URI that was passed to $ua->request() method, because we might have
received some redirect responses first.

=back

If none of these sources provide an absolute URI, undef is returned.

When the LWP protocol modules produce the HTTP::Response object, then
any base URI embedded in the document (step 1) will already have
initialized the "Content-Base:" header. This means that this method
only performs the last 2 steps (the content is not always available
either).

=item $r->filename

Returns a filename for this response.  Note that doing sanity checks
on the returned filename (eg. removing characters that cannot be used
on the target filesystem where the filename would be used, and
laundering it for security purposes) are the caller's responsibility;
the only related thing done by this method is that it makes a simple
attempt to return a plain filename with no preceding path segments.

The filename is obtained from one the following sources (in priority
order):

=over 4

=item 1.

A "Content-Disposition:" header in the response.  Proper decoding of
RFC 2047 encoded filenames requires the C<MIME::QuotedPrint> (for "Q"
encoding), C<MIME::Base64> (for "B" encoding), and C<Encode> modules.

=item 2.

A "Content-Location:" header in the response.

=item 3.

The URI used to request this response. This might not be the original
URI that was passed to $ua->request() method, because we might have
received some redirect responses first.

=back

If a filename cannot be derived from any of these sources, undef is
returned.

=item $r->as_string

=item $r->as_string( $eol )

Returns a textual representation of the response.

=item $r->is_info

=item $r->is_success

=item $r->is_redirect

=item $r->is_error

These methods indicate if the response was informational, successful, a
redirection, or an error.  See L<HTTP::Status> for the meaning of these.

=item $r->error_as_HTML

Returns a string containing a complete HTML document indicating what
error occurred.  This method should only be called when $r->is_error
is TRUE.

=item $r->current_age

Calculates the "current age" of the response as specified by RFC 2616
section 13.2.3.  The age of a response is the time since it was sent
by the origin server.  The returned value is a number representing the
age in seconds.

=item $r->freshness_lifetime

Calculates the "freshness lifetime" of the response as specified by
RFC 2616 section 13.2.4.  The "freshness lifetime" is the length of
time between the generation of a response and its expiration time.
The returned value is a number representing the freshness lifetime in
seconds.

If the response does not contain an "Expires" or a "Cache-Control"
header, then this function will apply some simple heuristic based on
'Last-Modified' to determine a suitable lifetime.

=item $r->is_fresh

Returns TRUE if the response is fresh, based on the values of
freshness_lifetime() and current_age().  If the response is no longer
fresh, then it has to be refetched or revalidated by the origin
server.

=item $r->fresh_until

Returns the time when this entity is no longer fresh.

=back

=head1 SEE ALSO

L<HTTP::Headers>, L<HTTP::Message>, L<HTTP::Status>, L<HTTP::Request>

=head1 COPYRIGHT

Copyright 1995-2004 Gisle Aas.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

