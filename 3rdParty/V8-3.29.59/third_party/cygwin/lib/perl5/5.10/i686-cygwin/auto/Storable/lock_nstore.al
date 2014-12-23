# NOTE: Derived from ../../lib/Storable.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Storable;

#line 230 "../../lib/Storable.pm (autosplit into ../../lib/auto/Storable/lock_nstore.al)"
#
# lock_nstore
#
# Same as nstore, but flock the file first (advisory locking).
#
sub lock_nstore {
	return _store(\&net_pstore, @_, 1);
}

# end of Storable::lock_nstore
1;
