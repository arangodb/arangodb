
require 5;
package Pod::Perldoc::ToChecker;
use strict;
use warnings;
use vars qw(@ISA);

# Pick our superclass...
#
eval 'require Pod::Simple::Checker';
if($@) {
  require Pod::Checker;
  @ISA = ('Pod::Checker');
} else {
  @ISA = ('Pod::Simple::Checker');
}

sub is_pageable        { 1 }
sub write_with_binmode { 0 }
sub output_extension   { 'txt' }

sub if_zero_length {
  my( $self, $file, $tmp, $tmpfd ) = @_;
  print "No Pod errors in $file\n";
}


1;

__END__

=head1 NAME

Pod::Perldoc::ToChecker - let Perldoc check Pod for errors

=head1 SYNOPSIS

  % perldoc -o checker SomeFile.pod
  No Pod errors in SomeFile.pod
  (or an error report)

=head1 DESCRIPTION

This is a "plug-in" class that allows Perldoc to use
Pod::Simple::Checker as a "formatter" class (or if that is
not available, then Pod::Checker), to check for errors in a given
Pod file.

This is actually a Pod::Simple::Checker (or Pod::Checker) subclass, and
inherits all its options.

=head1 SEE ALSO

L<Pod::Simple::Checker>, L<Pod::Simple>, L<Pod::Checker>, L<Pod::Perldoc>

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

