package DBM_Filter::encode ;

use strict;
use warnings;
use Carp;

our $VERSION = '0.01';

BEGIN
{
    eval { require Encode; };

    croak "Encode module not found.\n"
        if $@;
}


sub Filter
{
    my $encoding_name = shift || "utf8";

    my $encoding = Encode::find_encoding($encoding_name) ;

    croak "Encoding '$encoding_name' is not available"
        unless $encoding;

    return {
        Store   => sub {
			 $_ = $encoding->encode($_)
			     if defined $_ ;
		   },
        Fetch   => sub {
			 $_ = $encoding->decode($_)
			     if defined $_ ;
			}
        } ;
}

1;

__END__

=head1 NAME

DBM_Filter::encode - filter for DBM_Filter

=head1 SYNOPSIS

    use SDBM_File; # or DB_File, or GDBM_File, or NDBM_File, or ODBM_File
    use DBM_Filter ;

    $db = tie %hash, ...
    $db->Filter_Push('encode' => 'iso-8859-16');

=head1 DESCRIPTION

This DBM filter allows you to choose the character encoding will be
store in the DBM file. The usage is

    $db->Filter_Push('encode' => ENCODING);

where "ENCODING" must be a valid encoding name that the Encode module
recognises.

A fatal error will be thrown if:

=over 5

=item 1

The Encode module is not available.

=item 2

The encoding requested is not supported by the Encode module.

=back

=head1 SEE ALSO

L<DBM_Filter>, L<perldbmfilter>, L<Encode>

=head1 AUTHOR

Paul Marquess pmqs@cpan.org

