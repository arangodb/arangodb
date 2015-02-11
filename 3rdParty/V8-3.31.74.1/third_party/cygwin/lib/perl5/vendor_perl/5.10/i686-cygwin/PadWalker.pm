package PadWalker;

use strict;
use vars qw($VERSION @ISA @EXPORT_OK %EXPORT_TAGS);

require Exporter;
require DynaLoader;

require 5.008;

@ISA = qw(Exporter DynaLoader);
@EXPORT_OK = qw(peek_my peek_our closed_over peek_sub var_name);
%EXPORT_TAGS = (all => \@EXPORT_OK);

$VERSION = '1.7';

bootstrap PadWalker $VERSION;

sub peek_my;
sub peek_our;
sub closed_over;
sub peek_sub;
sub var_name;

1;
__END__

=head1 NAME

PadWalker - play with other peoples' lexical variables

=head1 SYNOPSIS

  use PadWalker qw(peek_my peek_our peek_sub closed_over);
  ...

=head1 DESCRIPTION

PadWalker is a module which allows you to inspect (and even change!)
lexical variables in any subroutine which called you. It will only
show those variables which are in scope at the point of the call.

PadWalker is particularly useful for debugging. It's even
used by Perl's built-in debugger. (It can also be used
for evil, of course.)

I wouldn't recommend using PadWalker directly in production
code, but it's your call. Some of the modules that use
PadWalker internally are certainly safe for and useful
in production.

=over 4

=item peek_my LEVEL

=item peek_our LEVEL

The LEVEL argument is interpreted just like the argument to C<caller>.
So C<peek_my(0)> returns a reference to a hash of all the C<my>
variables that are currently in scope;
C<peek_my(1)> returns a reference to a hash of all the C<my>
variables that are in scope at the point where the current
sub was called, and so on.

C<peek_our> works in the same way, except that it lists
the C<our> variables rather than the C<my> variables.

The hash associates each variable name with a reference
to its value. The variable names include the sigil, so
the variable $x is represented by the string '$x'.

For example:

  my $x = 12;
  my $h = peek_my (0);
  ${$h->{'$x'}}++;

  print $x;  # prints 13

Or a more complex example:

  sub increment_my_x {
    my $h = peek_my (1);
    ${$h->{'$x'}}++;
  }

  my $x=5;
  increment_my_x;
  print $x;  # prints 6

=item peek_sub SUB

The C<peek_sub> routine takes a coderef as its argument, and returns a hash
of the C<my> variables used in that sub. The values will usually be undefined
unless the sub is in use (i.e. in the call-chain) at the time. On the other
hand:

  my $x = "Hello!";
  my $r = peek_sub(sub {$x})->{'$x'};
  print "$$r\n";	# prints 'Hello!'

If the sub defines several C<my> variables with the same name, you'll get the
last one. I don't know of any use for C<peek_sub> that isn't broken as a result
of this, and it will probably be deprecated in a future version in favour of
some alternative interface.

=item closed_over SUB

C<closed_over> is similar to C<peek_sub>, except that it only lists
the C<my> variables which are used in the subroutine but defined outside:
in other words, the variables which it closes over. This I<does> have
reasonable uses: see L<Data::Dump::Streamer>, for example (a future version
of which may in fact use C<closed_over>).

=item var_name LEVEL, VAR_REF

=item var_name SUB,   VAR_REF

C<var_name(sub, var_ref)> returns the name of the variable referred to
by C<var_ref>, provided it is a C<my> variable used in the sub. The C<sub>
parameter can be either a CODE reference or a number. If it's a number,
it's treated the same way as the argument to C<peek_my>.

For example,

  my $foo;
  print var_name(0, \$foo);    # prints '$foo'
  
  sub my_name {
    return var_name(1, shift);
  }
  print my_name(\$foo);        # ditto

=back

=head1 AUTHOR

Robin Houston <robin@cpan.org>

With contributions from Richard Soberberg, bug-spotting
from Peter Scott and Dave Mitchell, and suggestions from
demerphq.

=head1 SEE ALSO

Devel::LexAlias, Devel::Caller, Sub::Parameters

=head1 COPYRIGHT

Copyright (c) 2000-2007, Robin Houston. All Rights Reserved.
This module is free software. It may be used, redistributed
and/or modified under the same terms as Perl itself.

=cut
