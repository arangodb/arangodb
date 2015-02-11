package ExtUtils::MM_VOS;

use strict;
our $VERSION = '6.44';

require ExtUtils::MM_Unix;
our @ISA = qw(ExtUtils::MM_Unix);


=head1 NAME

ExtUtils::MM_VOS - VOS specific subclass of ExtUtils::MM_Unix

=head1 SYNOPSIS

  Don't use this module directly.
  Use ExtUtils::MM and let it choose.

=head1 DESCRIPTION

This is a subclass of ExtUtils::MM_Unix which contains functionality for
VOS.

Unless otherwise stated it works just like ExtUtils::MM_Unix

=head2 Overridden methods

=head3 extra_clean_files

Cleanup VOS core files

=cut

sub extra_clean_files {
    return qw(*.kp);
}


=head1 AUTHOR

Michael G Schwern <schwern@pobox.com> with code from ExtUtils::MM_Unix

=head1 SEE ALSO

L<ExtUtils::MakeMaker>

=cut


1;
