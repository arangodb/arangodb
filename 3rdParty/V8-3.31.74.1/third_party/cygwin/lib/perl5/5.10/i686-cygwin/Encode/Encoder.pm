#
# $Id: Encoder.pm,v 2.1 2006/05/03 18:24:10 dankogai Exp $
#
package Encode::Encoder;
use strict;
use warnings;
our $VERSION = do { my @r = ( q$Revision: 2.1 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };

require Exporter;
our @ISA       = qw(Exporter);
our @EXPORT_OK = qw ( encoder );

our $AUTOLOAD;
sub DEBUG () { 0 }
use Encode qw(encode decode find_encoding from_to);
use Carp;

sub new {
    my ( $class, $data, $encname ) = @_;
    unless ($encname) {
        $encname = Encode::is_utf8($data) ? 'utf8' : '';
    }
    else {
        my $obj = find_encoding($encname)
          or croak __PACKAGE__, ": unknown encoding: $encname";
        $encname = $obj->name;
    }
    my $self = {
        data     => $data,
        encoding => $encname,
    };
    bless $self => $class;
}

sub encoder { __PACKAGE__->new(@_) }

sub data {
    my ( $self, $data ) = @_;
    if ( defined $data ) {
        $self->{data} = $data;
        return $data;
    }
    else {
        return $self->{data};
    }
}

sub encoding {
    my ( $self, $encname ) = @_;
    if ($encname) {
        my $obj = find_encoding($encname)
          or confess __PACKAGE__, ": unknown encoding: $encname";
        $self->{encoding} = $obj->name;
        return $self;
    }
    else {
        return $self->{encoding};
    }
}

sub bytes {
    my ( $self, $encname ) = @_;
    $encname ||= $self->{encoding};
    my $obj = find_encoding($encname)
      or confess __PACKAGE__, ": unknown encoding: $encname";
    $self->{data} = $obj->decode( $self->{data}, 1 );
    $self->{encoding} = '';
    return $self;
}

sub DESTROY {    # defined so it won't autoload.
    DEBUG and warn shift;
}

sub AUTOLOAD {
    my $self = shift;
    my $type = ref($self)
      or confess "$self is not an object";
    my $myname = $AUTOLOAD;
    $myname =~ s/.*://;    # strip fully-qualified portion
    my $obj = find_encoding($myname)
      or confess __PACKAGE__, ": unknown encoding: $myname";
    DEBUG and warn $self->{encoding}, " => ", $obj->name;
    if ( $self->{encoding} ) {
        from_to( $self->{data}, $self->{encoding}, $obj->name, 1 );
    }
    else {
        $self->{data} = $obj->encode( $self->{data}, 1 );
    }
    $self->{encoding} = $obj->name;
    return $self;
}

use overload
  q("") => sub { $_[0]->{data} },
  q(0+) => sub { use bytes(); bytes::length( $_[0]->{data} ) },
  fallback => 1,
  ;

1;
__END__

=head1 NAME

Encode::Encoder -- Object Oriented Encoder

=head1 SYNOPSIS

  use Encode::Encoder;
  # Encode::encode("ISO-8859-1", $data); 
  Encode::Encoder->new($data)->iso_8859_1; # OOP way
  # shortcut
  use Encode::Encoder qw(encoder);
  encoder($data)->iso_8859_1;
  # you can stack them!
  encoder($data)->iso_8859_1->base64;  # provided base64() is defined
  # you can use it as a decoder as well
  encoder($base64)->bytes('base64')->latin1;
  # stringified
  print encoder($data)->utf8->latin1;  # prints the string in latin1
  # numified
  encoder("\x{abcd}\x{ef}g")->utf8 == 6; # true. bytes::length($data)

=head1 ABSTRACT

B<Encode::Encoder> allows you to use Encode in an object-oriented
style.  This is not only more intuitive than a functional approach,
but also handier when you want to stack encodings.  Suppose you want
your UTF-8 string converted to Latin1 then Base64: you can simply say

  my $base64 = encoder($utf8)->latin1->base64;

instead of

  my $latin1 = encode("latin1", $utf8);
  my $base64 = encode_base64($utf8);

or the lazier and more convoluted

  my $base64 = encode_base64(encode("latin1", $utf8));

=head1 Description

Here is how to use this module.

=over 4

=item *

There are at least two instance variables stored in a hash reference,
{data} and {encoding}.

=item *

When there is no method, it takes the method name as the name of the
encoding and encodes the instance I<data> with I<encoding>.  If successful,
the instance I<encoding> is set accordingly.

=item *

You can retrieve the result via -E<gt>data but usually you don't have to 
because the stringify operator ("") is overridden to do exactly that.

=back

=head2 Predefined Methods

This module predefines the methods below:

=over 4

=item $e = Encode::Encoder-E<gt>new([$data, $encoding]);

returns an encoder object.  Its data is initialized with $data if
present, and its encoding is set to $encoding if present.

When $encoding is omitted, it defaults to utf8 if $data is already in
utf8 or "" (empty string) otherwise.

=item encoder()

is an alias of Encode::Encoder-E<gt>new().  This one is exported on demand.

=item $e-E<gt>data([$data])

When $data is present, sets the instance data to $data and returns the
object itself.  Otherwise, the current instance data is returned.

=item $e-E<gt>encoding([$encoding])

When $encoding is present, sets the instance encoding to $encoding and
returns the object itself.  Otherwise, the current instance encoding is
returned.

=item $e-E<gt>bytes([$encoding])

decodes instance data from $encoding, or the instance encoding if
omitted.  If the conversion is successful, the instance encoding
will be set to "".

The name I<bytes> was deliberately picked to avoid namespace tainting
-- this module may be used as a base class so method names that appear
in Encode::Encoding are avoided.

=back

=head2 Example: base64 transcoder

This module is designed to work with L<Encode::Encoding>.
To make the Base64 transcoder example above really work, you could
write a module like this:

  package Encode::Base64;
  use base 'Encode::Encoding';
  __PACKAGE__->Define('base64');
  use MIME::Base64;
  sub encode{ 
      my ($obj, $data) = @_; 
      return encode_base64($data);
  }
  sub decode{
      my ($obj, $data) = @_; 
      return decode_base64($data);
  }
  1;
  __END__

And your caller module would be something like this:

  use Encode::Encoder;
  use Encode::Base64;

  # now you can really do the following

  encoder($data)->iso_8859_1->base64;
  encoder($base64)->bytes('base64')->latin1;

=head2 Operator Overloading

This module overloads two operators, stringify ("") and numify (0+).

Stringify dumps the data inside the object.

Numify returns the number of bytes in the instance data.

They come in handy when you want to print or find the size of data.

=head1 SEE ALSO

L<Encode>,
L<Encode::Encoding>

=cut
