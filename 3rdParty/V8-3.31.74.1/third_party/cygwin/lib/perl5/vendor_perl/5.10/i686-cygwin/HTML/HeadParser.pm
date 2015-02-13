package HTML::HeadParser;

=head1 NAME

HTML::HeadParser - Parse <HEAD> section of a HTML document

=head1 SYNOPSIS

 require HTML::HeadParser;
 $p = HTML::HeadParser->new;
 $p->parse($text) and  print "not finished";

 $p->header('Title')          # to access <title>....</title>
 $p->header('Content-Base')   # to access <base href="http://...">
 $p->header('Foo')            # to access <meta http-equiv="Foo" content="...">

=head1 DESCRIPTION

The C<HTML::HeadParser> is a specialized (and lightweight)
C<HTML::Parser> that will only parse the E<lt>HEAD>...E<lt>/HEAD>
section of an HTML document.  The parse() method
will return a FALSE value as soon as some E<lt>BODY> element or body
text are found, and should not be called again after this.

Note that the C<HTML::HeadParser> might get confused if raw undecoded
UTF-8 is passed to the parse() method.  Make sure the strings are
properly decoded before passing them on.

The C<HTML::HeadParser> keeps a reference to a header object, and the
parser will update this header object as the various elements of the
E<lt>HEAD> section of the HTML document are recognized.  The following
header fields are affected:

=over 4

=item Content-Base:

The I<Content-Base> header is initialized from the E<lt>base
href="..."> element.

=item Title:

The I<Title> header is initialized from the E<lt>title>...E<lt>/title>
element.

=item Isindex:

The I<Isindex> header will be added if there is a E<lt>isindex>
element in the E<lt>head>.  The header value is initialized from the
I<prompt> attribute if it is present.  If no I<prompt> attribute is
given it will have '?' as the value.

=item X-Meta-Foo:

All E<lt>meta> elements will initialize headers with the prefix
"C<X-Meta->" on the name.  If the E<lt>meta> element contains a
C<http-equiv> attribute, then it will be honored as the header name.

=back

=head1 METHODS

The following methods (in addition to those provided by the
superclass) are available:

=over 4

=cut


require HTML::Parser;
@ISA = qw(HTML::Parser);

use HTML::Entities ();

use strict;
use vars qw($VERSION $DEBUG);
#$DEBUG = 1;
$VERSION = sprintf("%d.%02d", q$Revision: 2.22 $ =~ /(\d+)\.(\d+)/);

=item $hp = HTML::HeadParser->new

=item $hp = HTML::HeadParser->new( $header )

The object constructor.  The optional $header argument should be a
reference to an object that implement the header() and push_header()
methods as defined by the C<HTTP::Headers> class.  Normally it will be
of some class that isa or delegates to the C<HTTP::Headers> class.

If no $header is given C<HTML::HeadParser> will create an
C<HTTP::Header> object by itself (initially empty).

=cut

sub new
{
    my($class, $header) = @_;
    unless ($header) {
	require HTTP::Headers;
	$header = HTTP::Headers->new;
    }

    my $self = $class->SUPER::new(api_version => 2,
				  ignore_elements => [qw(script style)],
				 );
    $self->{'header'} = $header;
    $self->{'tag'} = '';   # name of active element that takes textual content
    $self->{'text'} = '';  # the accumulated text associated with the element
    $self;
}

=item $hp->header;

Returns a reference to the header object.

=item $hp->header( $key )

Returns a header value.  It is just a shorter way to write
C<$hp-E<gt>header-E<gt>header($key)>.

=cut

sub header
{
    my $self = shift;
    return $self->{'header'} unless @_;
    $self->{'header'}->header(@_);
}

sub as_string    # legacy
{
    my $self = shift;
    $self->{'header'}->as_string;
}

sub flush_text   # internal
{
    my $self = shift;
    my $tag  = $self->{'tag'};
    my $text = $self->{'text'};
    $text =~ s/^\s+//;
    $text =~ s/\s+$//;
    $text =~ s/\s+/ /g;
    print "FLUSH $tag => '$text'\n"  if $DEBUG;
    if ($tag eq 'title') {
	HTML::Entities::decode($text);
	$self->{'header'}->push_header(Title => $text);
    }
    $self->{'tag'} = $self->{'text'} = '';
}

# This is an quote from the HTML3.2 DTD which shows which elements
# that might be present in a <HEAD>...</HEAD>.  Also note that the
# <HEAD> tags themselves might be missing:
#
# <!ENTITY % head.content "TITLE & ISINDEX? & BASE? & STYLE? &
#                            SCRIPT* & META* & LINK*">
#
# <!ELEMENT HEAD O O  (%head.content)>


sub start
{
    my($self, $tag, $attr) = @_;  # $attr is reference to a HASH
    print "START[$tag]\n" if $DEBUG;
    $self->flush_text if $self->{'tag'};
    if ($tag eq 'meta') {
	my $key = $attr->{'http-equiv'};
	if (!defined($key) || !length($key)) {
	    return unless $attr->{'name'};
	    $key = "X-Meta-\u$attr->{'name'}";
	}
	$self->{'header'}->push_header($key => $attr->{content});
    } elsif ($tag eq 'base') {
	return unless exists $attr->{href};
	$self->{'header'}->push_header('Content-Base' => $attr->{href});
    } elsif ($tag eq 'isindex') {
	# This is a non-standard header.  Perhaps we should just ignore
	# this element
	$self->{'header'}->push_header(Isindex => $attr->{prompt} || '?');
    } elsif ($tag =~ /^(?:title|script|style)$/) {
	# Just remember tag.  Initialize header when we see the end tag.
	$self->{'tag'} = $tag;
    } elsif ($tag eq 'link') {
	return unless exists $attr->{href};
	# <link href="http:..." rel="xxx" rev="xxx" title="xxx">
	my $h_val = "<" . delete($attr->{href}) . ">";
	for (sort keys %{$attr}) {
	    $h_val .= qq(; $_="$attr->{$_}");
	}
	$self->{'header'}->push_header(Link => $h_val);
    } elsif ($tag eq 'head' || $tag eq 'html') {
	# ignore
    } else {
	 # stop parsing
	$self->eof;
    }
}

sub end
{
    my($self, $tag) = @_;
    print "END[$tag]\n" if $DEBUG;
    $self->flush_text if $self->{'tag'};
    $self->eof if $tag eq 'head';
}

sub text
{
    my($self, $text) = @_;
    $text =~ s/\x{FEFF}//;  # drop Unicode BOM if found
    print "TEXT[$text]\n" if $DEBUG;
    my $tag = $self->{tag};
    if (!$tag && $text =~ /\S/) {
	# Normal text means start of body
        $self->eof;
	return;
    }
    return if $tag ne 'title';
    $self->{'text'} .= $text;
}

1;

__END__

=back

=head1 EXAMPLE

 $h = HTTP::Headers->new;
 $p = HTML::HeadParser->new($h);
 $p->parse(<<EOT);
 <title>Stupid example</title>
 <base href="http://www.linpro.no/lwp/">
 Normal text starts here.
 EOT
 undef $p;
 print $h->title;   # should print "Stupid example"

=head1 SEE ALSO

L<HTML::Parser>, L<HTTP::Headers>

The C<HTTP::Headers> class is distributed as part of the
I<libwww-perl> package.  If you don't have that distribution installed
you need to provide the $header argument to the C<HTML::HeadParser>
constructor with your own object that implements the documented
protocol.

=head1 COPYRIGHT

Copyright 1996-2001 Gisle Aas. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut

