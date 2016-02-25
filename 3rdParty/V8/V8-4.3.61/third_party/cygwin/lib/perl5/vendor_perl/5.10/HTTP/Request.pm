package HTTP::Request;

require HTTP::Message;
@ISA = qw(HTTP::Message);
$VERSION = "5.811";

use strict;



sub new
{
    my($class, $method, $uri, $header, $content) = @_;
    my $self = $class->SUPER::new($header, $content);
    $self->method($method);
    $self->uri($uri);
    $self;
}


sub parse
{
    my($class, $str) = @_;
    my $request_line;
    if ($str =~ s/^(.*)\n//) {
	$request_line = $1;
    }
    else {
	$request_line = $str;
	$str = "";
    }

    my $self = $class->SUPER::parse($str);
    my($method, $uri, $protocol) = split(' ', $request_line);
    $self->method($method) if defined($method);
    $self->uri($uri) if defined($uri);
    $self->protocol($protocol) if $protocol;
    $self;
}


sub clone
{
    my $self = shift;
    my $clone = bless $self->SUPER::clone, ref($self);
    $clone->method($self->method);
    $clone->uri($self->uri);
    $clone;
}


sub method
{
    shift->_elem('_method', @_);
}


sub uri
{
    my $self = shift;
    my $old = $self->{'_uri'};
    if (@_) {
	my $uri = shift;
	if (!defined $uri) {
	    # that's ok
	}
	elsif (ref $uri) {
	    Carp::croak("A URI can't be a " . ref($uri) . " reference")
		if ref($uri) eq 'HASH' or ref($uri) eq 'ARRAY';
	    Carp::croak("Can't use a " . ref($uri) . " object as a URI")
		unless $uri->can('scheme');
	    $uri = $uri->clone;
	    unless ($HTTP::URI_CLASS eq "URI") {
		# Argh!! Hate this... old LWP legacy!
		eval { local $SIG{__DIE__}; $uri = $uri->abs; };
		die $@ if $@ && $@ !~ /Missing base argument/;
	    }
	}
	else {
	    $uri = $HTTP::URI_CLASS->new($uri);
	}
	$self->{'_uri'} = $uri;
    }
    $old;
}

*url = \&uri;  # legacy


sub as_string
{
    my $self = shift;
    my($eol) = @_;
    $eol = "\n" unless defined $eol;

    my $req_line = $self->method || "-";
    my $uri = $self->uri;
    $uri = (defined $uri) ? $uri->as_string : "-";
    $req_line .= " $uri";
    my $proto = $self->protocol;
    $req_line .= " $proto" if $proto;

    return join($eol, $req_line, $self->SUPER::as_string(@_));
}


1;

__END__

=head1 NAME

HTTP::Request - HTTP style request message

=head1 SYNOPSIS

 require HTTP::Request;
 $request = HTTP::Request->new(GET => 'http://www.example.com/');

and usually used like this:

 $ua = LWP::UserAgent->new;
 $response = $ua->request($request);

=head1 DESCRIPTION

C<HTTP::Request> is a class encapsulating HTTP style requests,
consisting of a request line, some headers, and a content body. Note
that the LWP library uses HTTP style requests even for non-HTTP
protocols.  Instances of this class are usually passed to the
request() method of an C<LWP::UserAgent> object.

C<HTTP::Request> is a subclass of C<HTTP::Message> and therefore
inherits its methods.  The following additional methods are available:

=over 4

=item $r = HTTP::Request->new( $method, $uri )

=item $r = HTTP::Request->new( $method, $uri, $header )

=item $r = HTTP::Request->new( $method, $uri, $header, $content )

Constructs a new C<HTTP::Request> object describing a request on the
object $uri using method $method.  The $method argument must be a
string.  The $uri argument can be either a string, or a reference to a
C<URI> object.  The optional $header argument should be a reference to
an C<HTTP::Headers> object or a plain array reference of key/value
pairs.  The optional $content argument should be a string of bytes.

=item $r = HTTP::Request->parse( $str )

This constructs a new request object by parsing the given string.

=item $r->method

=item $r->method( $val )

This is used to get/set the method attribute.  The method should be a
short string like "GET", "HEAD", "PUT" or "POST".

=item $r->uri

=item $r->uri( $val )

This is used to get/set the uri attribute.  The $val can be a
reference to a URI object or a plain string.  If a string is given,
then it should be parseable as an absolute URI.

=item $r->header( $field )

=item $r->header( $field => $value )

This is used to get/set header values and it is inherited from
C<HTTP::Headers> via C<HTTP::Message>.  See L<HTTP::Headers> for
details and other similar methods that can be used to access the
headers.

=item $r->content

=item $r->content( $bytes )

This is used to get/set the content and it is inherited from the
C<HTTP::Message> base class.  See L<HTTP::Message> for details and
other methods that can be used to access the content.

Note that the content should be a string of bytes.  Strings in perl
can contain characters outside the range of a byte.  The C<Encode>
module can be used to turn such strings into a string of bytes.

=item $r->as_string

=item $r->as_string( $eol )

Method returning a textual representation of the request.

=back

=head1 SEE ALSO

L<HTTP::Headers>, L<HTTP::Message>, L<HTTP::Request::Common>,
L<HTTP::Response>

=head1 COPYRIGHT

Copyright 1995-2004 Gisle Aas.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

