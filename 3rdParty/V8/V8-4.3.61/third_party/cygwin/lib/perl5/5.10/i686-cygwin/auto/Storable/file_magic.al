# NOTE: Derived from ../../lib/Storable.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Storable;

#line 118 "../../lib/Storable.pm (autosplit into ../../lib/auto/Storable/file_magic.al)"
sub file_magic {
    my $file = shift;
    my $fh = new FileHandle;
    open($fh, "<". $file) || die "Can't open '$file': $!";
    binmode($fh);
    defined(sysread($fh, my $buf, 32)) || die "Can't read from '$file': $!";
    close($fh);

    $file = "./$file" unless $file;  # ensure TRUE value

    return read_magic($buf, $file);
}

# end of Storable::file_magic
1;
