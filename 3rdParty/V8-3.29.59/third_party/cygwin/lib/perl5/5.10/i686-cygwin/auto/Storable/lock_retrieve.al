# NOTE: Derived from ../../lib/Storable.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Storable;

#line 355 "../../lib/Storable.pm (autosplit into ../../lib/auto/Storable/lock_retrieve.al)"
#
# lock_retrieve
#
# Same as retrieve, but with advisory locking.
#
sub lock_retrieve {
	_retrieve($_[0], 1);
}

# end of Storable::lock_retrieve
1;
