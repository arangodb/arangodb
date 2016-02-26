package Tie::Hash::NamedCapture;

our $VERSION = "0.06";

# The real meat implemented in XS in universal.c in the core, but this
# method was left behind because gv.c expects a Purl-Perl method in
# this package when it loads the tie magic for %+ and %-

my ($one, $all) = Tie::Hash::NamedCapture::flags();

sub TIEHASH {
    my ($pkg, %arg) = @_;
    my $flag = $arg{all} ? $all : $one;
    bless \$flag => $pkg;
}

tie %+, __PACKAGE__;
tie %-, __PACKAGE__, all => 1;

1;

__END__

=head1 NAME

Tie::Hash::NamedCapture - Named regexp capture buffers

=head1 SYNOPSIS

    tie my %hash, "Tie::Hash::NamedCapture";
    # %hash now behaves like %+

    tie my %hash, "Tie::Hash::NamedCapture", all => 1;
    # %hash now access buffers from regexp in $qr like %-

=head1 DESCRIPTION

This module is used to implement the special hashes C<%+> and C<%->, but it
can be used to tie other variables as you choose.

When the C<all> parameter is provided, then the tied hash elements will be
array refs listing the contents of each capture buffer whose name is the
same as the associated hash key. If none of these buffers were involved in
the match, the contents of that array ref will be as many C<undef> values
as there are capture buffers with that name. In other words, the tied hash
will behave as C<%->.

When the C<all> parameter is omitted or false, then the tied hash elements
will be the contents of the leftmost defined buffer with the name of the
associated hash key. In other words, the tied hash will behave as
C<%+>.

The keys of C<%->-like hashes correspond to all buffer names found in the
regular expression; the keys of C<%+>-like hashes list only the names of
buffers that have captured (and that are thus associated to defined values).

=head1 SEE ALSO

L<perlreapi>, L<re>, L<perlmodlib/Pragmatic Modules>, L<perlvar/"%+">,
L<perlvar/"%-">.

=cut
