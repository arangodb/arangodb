package Config::Extensions;
use strict;
use vars qw(%Extensions $VERSION @ISA @EXPORT_OK);
use Config;
require Exporter;

$VERSION = '0.01';
@ISA = 'Exporter';
@EXPORT_OK = '%Extensions';

foreach my $type (qw(static dynamic nonxs)) {
    foreach (split /\s+/, $Config{$type . '_ext'}) {
	s!/!::!g;
	$Extensions{$_} = $type;
    }
}

1;
__END__
=head1 NAME

Config::Extensions - hash lookup of which core extensions were built.

=head1 SYNOPSIS

    use Config::Extensions '%Extensions';
    if ($Extensions{PerlIO::via}) {
        # This perl has PerlIO::via built
    }

=head1 DESCRIPTION

The Config::Extensions module provides a hash C<%Extensions> containing all
the core extensions that were enabled for this perl. The hash is keyed by
extension name, with each entry having one of 3 possible values:

=over 4

=item dynamic

The extension is dynamically linked

=item nonxs

The extension is pure perl, so doesn't need linking to the perl executable

=item static

The extension is statically linked to the perl binary

=back

As all values evaluate to true, a simple C<if> test is good enough to determine
whether an extension is present.

All the data uses to generate the C<%Extensions> hash is already present in
the C<Config> module, but not in such a convenient format to quickly reference.

=head1 AUTHOR

Nicholas Clark <nick@ccl4.org>

=cut
