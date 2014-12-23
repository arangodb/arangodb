# NOTE: Derived from ../../lib/Storable.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Storable;

#line 131 "../../lib/Storable.pm (autosplit into ../../lib/auto/Storable/read_magic.al)"
sub read_magic {
    my($buf, $file) = @_;
    my %info;

    my $buflen = length($buf);
    my $magic;
    if ($buf =~ s/^(pst0|perl-store)//) {
	$magic = $1;
	$info{file} = $file || 1;
    }
    else {
	return undef if $file;
	$magic = "";
    }

    return undef unless length($buf);

    my $net_order;
    if ($magic eq "perl-store" && ord(substr($buf, 0, 1)) > 1) {
	$info{version} = -1;
	$net_order = 0;
    }
    else {
	$net_order = ord(substr($buf, 0, 1, ""));
	my $major = $net_order >> 1;
	return undef if $major > 4; # sanity (assuming we never go that high)
	$info{major} = $major;
	$net_order &= 0x01;
	if ($major > 1) {
	    return undef unless length($buf);
	    my $minor = ord(substr($buf, 0, 1, ""));
	    $info{minor} = $minor;
	    $info{version} = "$major.$minor";
	    $info{version_nv} = sprintf "%d.%03d", $major, $minor;
	}
	else {
	    $info{version} = $major;
	}
    }
    $info{version_nv} ||= $info{version};
    $info{netorder} = $net_order;

    unless ($net_order) {
	return undef unless length($buf);
	my $len = ord(substr($buf, 0, 1, ""));
	return undef unless length($buf) >= $len;
	return undef unless $len == 4 || $len == 8;  # sanity
	$info{byteorder} = substr($buf, 0, $len, "");
	$info{intsize} = ord(substr($buf, 0, 1, ""));
	$info{longsize} = ord(substr($buf, 0, 1, ""));
	$info{ptrsize} = ord(substr($buf, 0, 1, ""));
	if ($info{version_nv} >= 2.002) {
	    return undef unless length($buf);
	    $info{nvsize} = ord(substr($buf, 0, 1, ""));
	}
    }
    $info{hdrsize} = $buflen - length($buf);

    return \%info;
}

# end of Storable::read_magic
1;
