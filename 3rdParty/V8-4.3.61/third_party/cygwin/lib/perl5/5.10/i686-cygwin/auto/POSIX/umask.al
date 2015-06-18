# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 601 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/umask.al)"
sub umask {
    usage "umask(mask)" if @_ != 1;
    CORE::umask($_[0]);
}

# end of POSIX::umask
1;
