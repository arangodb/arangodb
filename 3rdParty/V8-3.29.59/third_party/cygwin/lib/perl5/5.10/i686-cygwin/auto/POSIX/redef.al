# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 85 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/redef.al)"
sub redef {
    my ($mess) = @_;
    croak "Use method $mess instead";
}

# end of POSIX::redef
1;
