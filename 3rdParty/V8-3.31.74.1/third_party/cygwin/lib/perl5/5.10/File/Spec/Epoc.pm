package File::Spec::Epoc;

use strict;
use vars qw($VERSION @ISA);

$VERSION = '3.2701';

require File::Spec::Unix;
@ISA = qw(File::Spec::Unix);

=head1 NAME

File::Spec::Epoc - methods for Epoc file specs

=head1 SYNOPSIS

 require File::Spec::Epoc; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See File::Spec::Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

This package is still work in progress ;-)

=cut

sub case_tolerant {
    return 1;
}

=pod

=over 4

=item canonpath()

No physical check on the filesystem, but a logical cleanup of a
path. On UNIX eliminated successive slashes and successive "/.".

=back

=cut

sub canonpath {
    my ($self,$path) = @_;
    return unless defined $path;

    $path =~ s|/+|/|g;                             # xx////xx  -> xx/xx
    $path =~ s|(/\.)+/|/|g;                        # xx/././xx -> xx/xx
    $path =~ s|^(\./)+||s unless $path eq "./";    # ./xx      -> xx
    $path =~ s|^/(\.\./)+|/|s;                     # /../../xx -> xx
    $path =~  s|/\Z(?!\n)|| unless $path eq "/";          # xx/       -> xx
    return $path;
}

=pod

=head1 AUTHOR

o.flebbe@gmx.de

=head1 COPYRIGHT

Copyright (c) 2004 by the Perl 5 Porters.  All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 SEE ALSO

See L<File::Spec> and L<File::Spec::Unix>.  This package overrides the
implementation of these methods, not the semantics.

=cut

1;
