# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 176 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/fabs.al)"
sub fabs {
    usage "fabs(x)" if @_ != 1;
    CORE::abs($_[0]);
}

# end of POSIX::fabs
1;
