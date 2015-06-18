# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 385 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/sprintf.al)"
sub sprintf {
    usage "sprintf(pattern,args)" if @_ == 0;
    CORE::sprintf(shift,@_);
}

# end of POSIX::sprintf
1;
