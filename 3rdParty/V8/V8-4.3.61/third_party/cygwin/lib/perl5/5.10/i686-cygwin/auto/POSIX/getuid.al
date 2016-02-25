# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 716 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/getuid.al)"
sub getuid {
    usage "getuid()" if @_ != 0;
    $<;
}

# end of POSIX::getuid
1;
