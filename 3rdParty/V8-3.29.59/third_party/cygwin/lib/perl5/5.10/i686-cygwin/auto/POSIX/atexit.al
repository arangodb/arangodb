# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 419 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/atexit.al)"
sub atexit {
    unimpl "atexit() is C-specific: use END {} instead";
}

# end of POSIX::atexit
1;
