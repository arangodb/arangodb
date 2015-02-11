package subs;

our $VERSION = '1.00';

=head1 NAME

subs - Perl pragma to predeclare sub names

=head1 SYNOPSIS

    use subs qw(frob);
    frob 3..10;

=head1 DESCRIPTION

This will predeclare all the subroutine whose names are 
in the list, allowing you to use them without parentheses
even before they're declared.

Unlike pragmas that affect the C<$^H> hints variable, the C<use vars> and
C<use subs> declarations are not BLOCK-scoped.  They are thus effective
for the entire file in which they appear.  You may not rescind such
declarations with C<no vars> or C<no subs>.

See L<perlmodlib/Pragmatic Modules> and L<strict/strict subs>.

=cut

require 5.000;

sub import {
    my $callpack = caller;
    my $pack = shift;
    my @imports = @_;
    foreach $sym (@imports) {
	*{"${callpack}::$sym"} = \&{"${callpack}::$sym"};
    }
};

1;
