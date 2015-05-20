package Pod::Coverage::ExportOnly;
use strict;
use Pod::Coverage ();
use base qw(Pod::Coverage);

sub _get_syms {
    my $self = shift;
    my $package = shift;

    # lifted from UNIVERSAL::exports
    no strict 'refs';
    my %exports = map { $_ => 1 } @{$package.'::EXPORT'}, 
                                  @{$package.'::EXPORT_OK'};

    return keys %exports;
}

1;
__END__

=head1 NAME

Pod::Coverage::ExportOnly - subclass of Pod::Coverage than only examines exported functions

=head1 SYNOPSIS

  # all in one invocation
  use Pod::Coverage::ExportOnly package => 'Fishy';

  # straight OO
  use Pod::Coverage::ExportOnly;
  my $pc = new Pod::Coverage::ExportOnly package => 'Pod::Coverage';
  print "We rock!" if $pc->coverage == 1;

=head1 DESCRIPTION

This module extends Pod::Coverage to only check things explicitly set
up for export by the Exporter or UNIVERSAL::exports modules.  If you
want full documentation we suggest you check the L<Pod::Coverage>
documentation

=head1 SEE ALSO

L<Pod::Coverage>, L<Exporter>, L<UNIVERSAL::exports>

=head1 AUTHORS

Copyright (c) 2001 Richard Clamp, Micheal Stevens. All rights
reserved.  This program is free software; you can redistribute it
and/or modify it under the same terms as Perl itself.

=cut

