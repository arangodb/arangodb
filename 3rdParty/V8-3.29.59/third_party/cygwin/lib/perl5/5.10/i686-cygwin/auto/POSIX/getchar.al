# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 334 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/getchar.al)"
sub getchar {
    usage "getchar()" if @_ != 0;
    CORE::getc(STDIN);
}

# end of POSIX::getchar
1;
