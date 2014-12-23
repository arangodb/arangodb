# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 151 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/getgrgid.al)"
sub getgrgid {
    usage "getgrgid(gid)" if @_ != 1;
    CORE::getgrgid($_[0]);
}

# end of POSIX::getgrgid
1;
