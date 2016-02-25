package Pod::Coverage::CountParents;
use strict;
use Pod::Coverage ();
use base 'Pod::Coverage';

# this code considered lightly fugly :)

sub _get_pods {
    my $self = shift;
    my $package = $self->{package};

    eval qq{ require $package };
    if ($@) {
        $self->{why_unrated} = "Couldn't compile '$package' to inspect: $@";
        return;
    }

    my %pods;
    $pods{$package} = $self->SUPER::_get_pods;

    __walk_up($package, \%pods);
    my %flat = map { $_ => 1 }  map { @{ $_ || [] } } values %pods;
    return [ keys %flat ];
}

sub __walk_up {
    my $package = shift;
    my $pods    = shift;

    $pods->{$package} = Pod::Coverage->new(package => $package)->_get_pods();

    my @parents;
    {
        no strict 'refs';
        @parents = @{"$package\::ISA"};
    }

    do { $pods->{$_} || __walk_up($_, $pods) } for @parents;
}

1;
__END__


=head1 NAME

Pod::Coverage::CountParents - subclass of Pod::Coverage that examines the inheritance tree

=head1 SYNOPSIS

  # all in one invocation
  use Pod::Coverage::CountParents package => 'Fishy';

  # straight OO
  use Pod::Coverage::CountParents;
  my $pc = new Pod::Coverage::CountParents package => 'Pod::Coverage';
  print "We rock!" if $pc->coverage == 1;

=head1 DESCRIPTION

This module extends Pod::Coverage to include the documentation from
parent classes when identifying the coverage of the code.

If you want full documentation we suggest you check the
L<Pod::Coverage> documentation.

=head1 SEE ALSO

L<Pod::Coverage>, L<base>

=head1 AUTHOR

Copyright (c) 2002 Richard Clamp. All rights reserved.  This program
is free software; you can redistribute it and/or modify it under the
same terms as Perl itself.

=cut
