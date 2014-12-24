
require 5;
package Pod::Simple::PullParserEndToken;
use Pod::Simple::PullParserToken ();
@ISA = ('Pod::Simple::PullParserToken');
use strict;

sub new {  # Class->new(tagname);
  my $class = shift;
  return bless ['end', @_], ref($class) || $class;
}

# Purely accessors:

sub tagname { (@_ == 2) ? ($_[0][1] = $_[1]) : $_[0][1] }
sub tag { shift->tagname(@_) }

# shortcut:
sub is_tagname { $_[0][1] eq $_[1] }
sub is_tag { shift->is_tagname(@_) }

1;


__END__

=head1 NAME

Pod::Simple::PullParserEndToken -- end-tokens from Pod::Simple::PullParser

=head1 SYNOPSIS

(See L<Pod::Simple::PullParser>)

=head1 DESCRIPTION

When you do $parser->get_token on a L<Pod::Simple::PullParser>, you might
get an object of this class.

This is a subclass of L<Pod::Simple::PullParserToken> and inherits all its methods,
and adds these methods:

=over

=item $token->tagname

This returns the tagname for this end-token object.
For example, parsing a "=head1 ..." line will give you
a start-token with the tagname of "head1", token(s) for its
content, and then an end-token with the tagname of "head1".

=item $token->tagname(I<somestring>)

This changes the tagname for this end-token object.
You probably won't need to do this.

=item $token->tag(...)

A shortcut for $token->tagname(...)

=item $token->is_tag(I<somestring>) or $token->is_tagname(I<somestring>)

These are shortcuts for C<< $token->tag() eq I<somestring> >>

=back

You're unlikely to ever need to construct an object of this class for
yourself, but if you want to, call
C<<
Pod::Simple::PullParserEndToken->new( I<tagname> )
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

