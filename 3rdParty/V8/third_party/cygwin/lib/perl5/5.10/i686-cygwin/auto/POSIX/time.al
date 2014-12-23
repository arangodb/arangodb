# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 626 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/time.al)"
sub time {
    usage "time()" if @_ != 0;
    CORE::time;
}

# end of POSIX::time
1;
