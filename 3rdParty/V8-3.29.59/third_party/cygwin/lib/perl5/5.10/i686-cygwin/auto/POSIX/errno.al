# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 136 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/errno.al)"
sub errno {
    usage "errno()" if @_ != 0;
    $! + 0;
}

# end of POSIX::errno
1;
