
# This class is just a hack to act as a "formatter" for
# actually unformatted Pod.
# 
# Note that this isn't the same as just passing thru whatever
# we're given -- we pass thru only the pod source, and suppress
# the Perl code (or whatever non-pod stuff is in the source file).


require 5;
package Pod::Perldoc::ToPod;
use strict;
use warnings;

use base qw(Pod::Perldoc::BaseTo);
sub is_pageable        { 1 }
sub write_with_binmode { 0 }
sub output_extension   { 'pod' }

sub new { return bless {}, ref($_[0]) || $_[0] }

sub parse_from_file {
  my( $self, $in, $outfh ) = @_;

  open(IN, "<", $in) or die "Can't read-open $in: $!\nAborting";

  my $cut_mode = 1;
  
  # A hack for finding things between =foo and =cut, inclusive
  local $_;
  while (<IN>) {
    if(  m/^=(\w+)/s ) {
      if($cut_mode = ($1 eq 'cut')) {
        print $outfh "\n=cut\n\n";
         # Pass thru the =cut line with some harmless
         #  (and occasionally helpful) padding
      }
    }
    next if $cut_mode;
    print $outfh $_ or die "Can't print to $outfh: $!";
  }
  
  close IN or die "Can't close $in: $!";
  return;
}

1;
__END__

=head1 NAME

Pod::Perldoc::ToPod - let Perldoc render Pod as ... Pod!

=head1 SYNOPSIS

  perldoc -opod Some::Modulename

(That's currently the same as the following:)

  perldoc -u Some::Modulename

=head1 DESCRIPTION

This is a "plug-in" class that allows Perldoc to display Pod source as
itself!  Pretty Zen, huh?

Currently this class works by just filtering out the non-Pod stuff from
a given input file.

=head1 SEE ALSO

L<Pod::Perldoc>

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

