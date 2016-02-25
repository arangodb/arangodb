# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 744 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/sleep.al)"
sub sleep {
    usage "sleep(seconds)" if @_ != 1;
    $_[0] - CORE::sleep($_[0]);
}

# end of POSIX::sleep
1;
