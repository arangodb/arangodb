package CPANPLUS::Module::Checksums;

use strict;
use vars qw[@ISA];


use CPANPLUS::Error;
use CPANPLUS::Internals::Constants;

use FileHandle;

use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';
use Params::Check               qw[check];
use Module::Load::Conditional   qw[can_load];

$Params::Check::VERBOSE = 1;

@ISA = qw[ CPANPLUS::Module::Signature ];

=head1 NAME

CPANPLUS::Module::Checksums

=head1 SYNOPSIS

    $file   = $modobj->checksums;
    $bool   = $mobobj->_validate_checksum;

=head1 DESCRIPTION

This is a class that provides functions for checking the checksum 
of a distribution. Should not be loaded directly, but used via the
interface provided via C<CPANPLUS::Module>.

=head1 METHODS

=head2 $mod->checksums

Fetches the checksums file for this module object.
For the options it can take, see C<CPANPLUS::Module::fetch()>.

Returns the location of the checksums file on success and false
on error.

The location of the checksums file is also stored as

    $mod->status->checksums

=cut

sub checksums {
    my $mod = shift or return;

    my $file = $mod->_get_checksums_file( @_ );

    return $mod->status->checksums( $file ) if $file;

    return;
}

### checks if the package checksum matches the one
### from the checksums file
sub _validate_checksum {
    my $self = shift; #must be isa CPANPLUS::Module
    my $conf = $self->parent->configure_object;
    my %hash = @_;

    my $verbose;
    my $tmpl = {
        verbose => {    default => $conf->get_conf('verbose'),
                        store   => \$verbose },
    };

    check( $tmpl, \%hash ) or return;

    ### if we can't check it, we must assume it's ok ###
    return $self->status->checksum_ok(1)
            unless can_load( modules => { 'Digest::MD5' => '0.0' } );
    #class CPANPLUS::Module::Status is runtime-generated

    my $file = $self->_get_checksums_file( verbose => $verbose ) or (
        error(loc(q[Could not fetch '%1' file], CHECKSUMS)), return );

    $self->_check_signature_for_checksum_file( file => $file ) or (
        error(loc(q[Could not verify '%1' file], CHECKSUMS)), return );
    #for whole CHECKSUMS file

    my $href = $self->_parse_checksums_file( file => $file ) or (
        error(loc(q[Could not parse '%1' file], CHECKSUMS)), return );

    my $size = $href->{ $self->package }->{'size'};

    ### the checksums file tells us the size of the archive
    ### but the downloaded file is of different size
    if( defined $size ) {
        if( not (-s $self->status->fetch == $size) ) {
            error(loc(  "Archive size does not match for '%1': " .
                        "size is '%2' but should be '%3'",
                        $self->package, -s $self->status->fetch, $size));
            return $self->status->checksum_ok(0);
        }
    } else {
        msg(loc("Archive size is not known for '%1'",$self->package),$verbose);
    }
    
    my $md5 = $href->{ $self->package }->{'md5'};

    unless( defined $md5 ) {
        msg(loc("No 'md5' checksum known for '%1'",$self->package),$verbose);

        return $self->status->checksum_ok(1);
    }

    $self->status->checksum_value($md5);


    my $fh = FileHandle->new( $self->status->fetch ) or return;
    binmode $fh;

    my $ctx = Digest::MD5->new;
    $ctx->addfile( $fh );

    my $flag = $ctx->hexdigest eq $md5;
    $flag
        ? msg(loc("Checksum matches for '%1'", $self->package),$verbose)
        : error(loc("Checksum does not match for '%1': " .
                    "MD5 is '%2' but should be '%3'",
                    $self->package, $ctx->hexdigest, $md5),$verbose);


    return $self->status->checksum_ok(1) if $flag;
    return $self->status->checksum_ok(0);
}


### fetches the module objects checksum file ###
sub _get_checksums_file {
    my $self = shift;
    my %hash = @_;

    my $clone = $self->clone;
    $clone->package( CHECKSUMS );

    my $file = $clone->fetch( %hash, force => 1 ) or return;

    return $file;
}

sub _parse_checksums_file {
    my $self = shift;
    my %hash = @_;

    my $file;
    my $tmpl = {
        file    => { required => 1, allow => FILE_READABLE, store => \$file },
    };
    my $args = check( $tmpl, \%hash );

    my $fh = OPEN_FILE->( $file ) or return;

    ### loop over the header, there might be a pgp signature ###
    my $signed;
    while (<$fh>) {
        last if /^\$cksum = \{\s*$/;    # skip till this line
        my $header = PGP_HEADER;        # but be tolerant of whitespace
        $signed = 1 if /^${header}\s*$/;# due to crossplatform linebreaks
   }

    ### read the filehandle, parse it rather than eval it, even though it
    ### *should* be valid perl code
    my $dist;
    my $cksum = {};
    while (<$fh>) {

        if (/^\s*'([^']+)' => \{\s*$/) {
            $dist = $1;

        } elsif (/^\s*'([^']+)' => '?([^'\n]+)'?,?\s*$/ and defined $dist) {
            $cksum->{$dist}{$1} = $2;

        } elsif (/^\s*}[,;]?\s*$/) {
            undef $dist;

        } elsif (/^__END__\s*$/) {
            last;

        } else {
            error( loc("Malformed %1 line: %2", CHECKSUMS, $_) );
        }
    }

    return $cksum;
}

sub _check_signature_for_checksum_file {
    my $self = shift;

    my $conf = $self->parent->configure_object;
    my %hash = @_;

    ### you don't want to check signatures,
    ### so let's just return true;
    return 1 unless $conf->get_conf('signature');

    my($force,$file,$verbose);
    my $tmpl = {
        file    => { required => 1, allow => FILE_READABLE, store => \$file },
        force   => { default => $conf->get_conf('force'), store => \$force },
        verbose => { default => $conf->get_conf('verbose'), store => \$verbose },
    };

    my $args = check( $tmpl, \%hash ) or return;

    my $fh = OPEN_FILE->($file) or return;

    my $signed;
    while (<$fh>) {
        my $header = PGP_HEADER;
        $signed = 1 if /^$header$/;
    }

    if ( !$signed ) {
        msg(loc("No signature found in %1 file '%2'",
                CHECKSUMS, $file), $verbose);

        return 1 unless $force;

        error( loc( "%1 file '%2' is not signed -- aborting",
                    CHECKSUMS, $file ) );
        return;

    }

    if( can_load( modules => { 'Module::Signature' => '0.06' } ) ) {
        # local $Module::Signature::SIGNATURE = $file;
        # ... check signatures ...
    }

    return 1;
}



# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

1;
