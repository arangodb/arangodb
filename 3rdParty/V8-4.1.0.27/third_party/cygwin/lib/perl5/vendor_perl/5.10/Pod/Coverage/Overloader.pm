package Pod::Coverage::Overloader;
use strict;
use Pod::Coverage ();
use base qw(Pod::Coverage);

sub new {
    my $class = shift;

    warn "Pod::Coverage::Overloader is deprecated.  Please use Pod::Coverage instead";
    $class->SUPER::new( @_ );
}

1;
__END__

=head1 NAME

Pod::Coverage::Overloader - deprecated subclass of Pod::Coverage

=head1 SYNOPSIS

  # Please do not use this module, it was an experiment that went
  # awry.  Use Pod::Coverage instead

=head1 DESCRIPTION

=head1 SEE ALSO

L<Pod::Coverage>, L<overload>

=head1 AUTHORS

Copyright (c) 2001 Richard Clamp, Micheal Stevens. All rights
reserved.  This program is free software; you can redistribute it
and/or modify it under the same terms as Perl itself.

=cut
