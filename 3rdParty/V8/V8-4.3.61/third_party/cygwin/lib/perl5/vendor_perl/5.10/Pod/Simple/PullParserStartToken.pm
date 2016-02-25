
require 5;
package Pod::Simple::PullParserStartToken;
use Pod::Simple::PullParserToken ();
@ISA = ('Pod::Simple::PullParserToken');
use strict;

sub new {  # Class->new(tagname, optional_attrhash);
  my $class = shift;
  return bless ['start', @_], ref($class) || $class;
}

# Purely accessors:

sub tagname   { (@_ == 2) ? ($_[0][1] = $_[1]) : $_[0][1] }
sub tag { shift->tagname(@_) }

sub is_tagname { $_[0][1] eq $_[1] }
sub is_tag { shift->is_tagname(@_) }


sub attr_hash { $_[0][2] ||= {} }

sub attr      {
  if(@_ == 2) {      # Reading: $token->attr('attrname')
    ${$_[0][2] || return undef}{ $_[1] };
  } elsif(@_ > 2) {  # Writing: $token->attr('attrname', 'newval')
    ${$_[0][2] ||= {}}{ $_[1] } = $_[2];
  } else {
    require Carp;
    Carp::croak(
      'usage: $object->attr("val") or $object->attr("key", "newval")');
    return undef;
  }
}

1;


__END__

=head1 NAME

Pod::Simple::PullParserStartToken -- start-tokens from Pod::Simple::PullParser

=head1 SYNOPSIS

(See L<Pod::Simple::PullParser>)

=head1 DESCRIPTION

When you do $parser->get_token on a L<Pod::Simple::PullParser> object, you might
get an object of this class.

This is a subclass of L<Pod::Simple::PullParserToken> and inherits all its methods,
and adds these methods:

=over

=item $token->tagname

This returns the tagname for this start-token object.
For example, parsing a "=head1 ..." line will give you
a start-token with the tagname of "head1", token(s) for its
content, and then an end-token with the tagname of "head1".

=item $token->tagname(I<somestring>)

This changes the tagname for this start-token object.
You probably won't need
to do this.

=item $token->tag(...)

A shortcut for $token->tagname(...)

=item $token->is_tag(I<somestring>) or $token->is_tagname(I<somestring>)

These are shortcuts for C<< $token->tag() eq I<somestring> >>

=item $token->attr(I<attrname>)

This returns the value of the I<attrname> attribute for this start-token
object, or undef.

For example, parsing a LZ<><Foo/"Bar"> link will produce a start-token
with a "to" attribute with the value "Foo", a "type" attribute with the
value "pod", and a "section" attribute with the value "Bar".

=item $token->attr(I<attrname>, I<newvalue>)

This sets the I<attrname> attribute for this start-token object to
I<newvalue>.  You probably won't need to do this.

=item $token->attr_hash

This returns the hashref that is the attribute set for this start-token.
This is useful if (for example) you want to ask what all the attributes
are -- you can just do C<< keys %{$token->attr_hash} >>

=back


You're unlikely to ever need to construct an object of this class for
yourself, but if you want to, call
C<<
Pod::Simple::PullParserStartToken->new( I<tagname>, I<attrhash> )
>>

=head1 SEE ALSO

L<Pod::Simple::PullParserToken>, L<Pod::Simple>, L<Pod::Simple::Subclassing>

=head1 COPYRIGHT AND DISCLAIMERS

Copyright (c) 2002 Sean M. Burke.  All rights reserved.

This library is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

This program is distributed in the hope that it will be useful, but
without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.

=head1 AUTHOR

Sean M. Burke C<sburke@cpan.org>

=cut

