# NOTE: Derived from ../../lib/Storable.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Storable;

#line 272 "../../lib/Storable.pm (autosplit into ../../lib/auto/Storable/store_fd.al)"
#
# store_fd
#
# Same as store, but perform on an already opened file descriptor instead.
# Returns undef if an I/O error occurred.
#
sub store_fd {
	return _store_fd(\&pstore, @_);
}

# end of Storable::store_fd
1;
