# NOTE: Derived from ../../lib/Storable.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Storable;

#line 221 "../../lib/Storable.pm (autosplit into ../../lib/auto/Storable/lock_store.al)"
#
# lock_store
#
# Same as store, but flock the file first (advisory locking).
#
sub lock_store {
	return _store(\&pstore, @_, 1);
}

# end of Storable::lock_store
1;
