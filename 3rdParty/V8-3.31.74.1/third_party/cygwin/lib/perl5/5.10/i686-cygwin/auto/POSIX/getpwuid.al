# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 206 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/getpwuid.al)"
sub getpwuid {
    usage "getpwuid(uid)" if @_ != 1;
    CORE::getpwuid($_[0]);
}

# end of POSIX::getpwuid
1;
