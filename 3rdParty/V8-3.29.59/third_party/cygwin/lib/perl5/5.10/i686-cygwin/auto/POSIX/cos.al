# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 166 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/cos.al)"
sub cos {
    usage "cos(x)" if @_ != 1;
    CORE::cos($_[0]);
}

# end of POSIX::cos
1;
