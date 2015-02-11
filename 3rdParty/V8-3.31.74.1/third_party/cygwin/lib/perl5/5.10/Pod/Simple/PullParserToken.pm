
require 5;
package Pod::Simple::PullParserToken;
 # Base class for tokens gotten from Pod::Simple::PullParser's $parser->get_token
@ISA = ();
$VERSION = '2.02';
use strict;

sub new {  # Class->new('type', stuff...);  ## Overridden in derived classes anyway
  my $class = shift;
  return bless [@_], ref($class) || $class;
}

sub type { $_[0][0] }  # Can't change the type of an object
sub dump { Pod::Simple::pretty( [ @{ $_[0] } ] ) }

sub is_start { $_[0][0] eq 'start' }
sub is_end   { $_[0][0] eq 'end'   }
sub is_text  { $_[0][0] eq 'text'  }

1;
__END__

sub dump { '[' . _esc( @{ $_[0] } ) . ']' }

# JUNK:

sub _esc {
  return '' unless @_;
  my @out;
  foreach my $in (@_) {
    push @out, '"' . $in . '"';
    $out[-1] =~ s/([^- \:\:\.\,\'\>\<\"\/\=\?\+\|\[\]\{\}\_a-zA-Z0-9_\`\~\!\#\%\^\&\*\(\)])/
      sprintf( (ord($1) < 256) ? "\\x%02X" : "\\x{%X}", ord($1))
    /eg;
  }
  return join ', ', @out;
}


__END__

=head1 NAME

Pod::Simple::PullParserToken -- tokens from Pod::Simple::PullParser

=head1 SYNOPSIS

Given a $parser that's an object of class Pod::Simple::PullParser
(or a subclass)...

  while(my $token = $parser->get_token) {
    $DEBUG and print "Token: ", $token->dump, "\n";
    if($token->is_start) {
      ...access $token->tagname, $token->attr, etc...

    } elsif($token->is_text) {
      ...access $token->text, $token->text_r, etc...
    
    } elsif($token->is_end) {
      ...access $token->tagname...
    
    }
  }

(Also see L<Pod::Simple::PullParser>)

=head1 DESCRIPTION

When you do $parser->get_token on a L<Pod::Simple::PullParser>, you should
get an object of a subclass of Pod::Simple::PullParserToken.

Subclasses will add methods, and will also inherit these methods:

=over

=item $token->type

This returns the type of the token.  This will be either the string
"start", the string "text", or the string "end".

Once you know what the type of an object is, you then know what
subclass it belongs to, and therefore what methods it supports.

Yes, you could probably do the same thing with code like
$token->isa('Pod::Simple::PullParserEndToken'), but that's not so
pretty as using just $token->type, or even the following shortcuts:

=item $token->is_start

This is a shortcut for C<< $token->type() eq "start" >>

=item $token->is_text

This is a shortcut for C<< $token->type() eq "text" >>

=item $token->is_end

This is a shortcut for C<< $token->type() eq "end" >>

=item $token->dump

This returns a handy stringified value of this object.  This
is useful for debugging, as in:

  while(my $token = $parser->get_token) {
    $DEBUG and print "Token: ", $token->dump, "\n";
    ...
  }

=back

=head1 SEE ALSO

My subclasses:
L<Pod::Simple::PullParserStartToken>,
L<Pod::Simple::PullParserTextToken>, and
L<Pod::Simple::PullParserEndToken>.

L<Pod::Simple::PullParser> and L<Pod::Simple>

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

