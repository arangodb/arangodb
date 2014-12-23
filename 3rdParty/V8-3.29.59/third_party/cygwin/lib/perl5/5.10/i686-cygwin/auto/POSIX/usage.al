# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 80 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/usage.al)"
sub usage {
    my ($mess) = @_;
    croak "Usage: POSIX::$mess";
}

# end of POSIX::usage
1;
