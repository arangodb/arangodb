# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 696 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/getlogin.al)"
sub getlogin {
    usage "getlogin()" if @_ != 0;
    CORE::getlogin();
}

# end of POSIX::getlogin
1;
