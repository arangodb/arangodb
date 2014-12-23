# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 711 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/getppid.al)"
sub getppid {
    usage "getppid()" if @_ != 0;
    CORE::getppid;
}

# end of POSIX::getppid
1;
