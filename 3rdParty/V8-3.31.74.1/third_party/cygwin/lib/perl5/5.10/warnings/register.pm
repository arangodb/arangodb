package warnings::register;

our $VERSION = '1.01';

=pod

=head1 NAME

warnings::register - warnings import function

=head1 SYNOPSIS

    use warnings::register;

=head1 DESCRIPTION

Creates a warnings category with the same name as the current package.

See L<warnings> and L<perllexwarn> for more information on this module's
usage.

=cut

require warnings;

sub mkMask
{
    my ($bit) = @_;
    my $mask = "";

    vec($mask, $bit, 1) = 1;
    return $mask;
}

sub import
{
    shift;
    my $package = (caller(0))[0];
    if (! defined $warnings::Bits{$package}) {
        $warnings::Bits{$package}     = mkMask($warnings::LAST_BIT);
        vec($warnings::Bits{'all'}, $warnings::LAST_BIT, 1) = 1;
        $warnings::Offsets{$package}  = $warnings::LAST_BIT ++;
	foreach my $k (keys %warnings::Bits) {
	    vec($warnings::Bits{$k}, $warnings::LAST_BIT, 1) = 0;
	}
        $warnings::DeadBits{$package} = mkMask($warnings::LAST_BIT);
        vec($warnings::DeadBits{'all'}, $warnings::LAST_BIT++, 1) = 1;
    }
}

1;
