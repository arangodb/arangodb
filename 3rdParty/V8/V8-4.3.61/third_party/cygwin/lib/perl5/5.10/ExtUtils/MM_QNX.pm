package ExtUtils::MM_QNX;

use strict;
our $VERSION = '6.44';

require ExtUtils::MM_Unix;
our @ISA = qw(ExtUtils::MM_Unix);


=head1 NAME

ExtUtils::MM_QNX - QNX specific subclass of ExtUtils::MM_Unix

=head1 SYNOPSIS

  Don't use this module directly.
  Use ExtUtils::MM and let it choose.

=head1 DESCRIPTION

This is a subclass of ExtUtils::MM_Unix which contains functionality for
QNX.

Unless otherwise stated it works just like ExtUtils::MM_Unix

=head2 Overridden methods

=head3 extra_clean_files

Add .err files corresponding to each .c file.

=cut

sub extra_clean_files {
    my $self = shift;

    my @errfiles = @{$self->{C}};
    for ( @errfiles ) {
	s/.c$/.err/;
    }

    return( @errfiles, 'perlmain.err' );
}


=head1 AUTHOR

Michael G Schwern <schwern@pobox.com> with code from ExtUtils::MM_Unix

=head1 SEE ALSO

L<ExtUtils::MakeMaker>

=cut


1;
