# NOTE: Derived from ../../lib/Storable.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Storable;

#line 330 "../../lib/Storable.pm (autosplit into ../../lib/auto/Storable/_freeze.al)"
# Internal freeze routine
sub _freeze {
	my $xsptr = shift;
	my $self = shift;
	logcroak "not a reference" unless ref($self);
	logcroak "too many arguments" unless @_ == 0;	# No @foo in arglist
	my $da = $@;				# Don't mess if called from exception handler
	my $ret;
	# Call C routine mstore or net_mstore, depending on network order
	eval { $ret = &$xsptr($self) };
	logcroak $@ if $@ =~ s/\.?\n$/,/;
	$@ = $da;
	return $ret ? $ret : undef;
}

# end of Storable::_freeze
1;
