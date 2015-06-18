# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 329 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/getc.al)"
sub getc {
    usage "getc(handle)" if @_ != 1;
    CORE::getc($_[0]);
}

# end of POSIX::getc
1;
