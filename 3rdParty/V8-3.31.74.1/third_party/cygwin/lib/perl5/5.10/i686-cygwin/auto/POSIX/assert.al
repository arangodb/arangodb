# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 96 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/assert.al)"
sub assert {
    usage "assert(expr)" if @_ != 1;
    if (!$_[0]) {
	croak "Assertion failed";
    }
}

# end of POSIX::assert
1;
