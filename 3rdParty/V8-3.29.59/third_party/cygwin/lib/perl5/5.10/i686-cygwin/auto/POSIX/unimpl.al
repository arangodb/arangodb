# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 90 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/unimpl.al)"
sub unimpl {
    my ($mess) = @_;
    $mess =~ s/xxx//;
    croak "Unimplemented: POSIX::$mess";
}

# end of POSIX::unimpl
1;
