# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 675 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/getegid.al)"
sub getegid {
    usage "getegid()" if @_ != 0;
    $) + 0;
}

# end of POSIX::getegid
1;
