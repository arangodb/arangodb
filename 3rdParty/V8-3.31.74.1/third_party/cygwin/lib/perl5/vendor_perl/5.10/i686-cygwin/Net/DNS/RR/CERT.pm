package Net::DNS::RR::CERT;
#
# $Id: CERT.pm 388 2005-06-22 10:06:05Z olaf $
#
# Written by Mike Schiraldi <raldi@research.netsol.com> for VeriSign

use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

use MIME::Base64;

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 388 $)[1];

my %formats = (
	PKIX => 1,
	SPKI => 2,
	PGP  => 3,
	URI  => 253,
	OID  => 254,
);

my %r_formats = reverse %formats;

my %algorithms = (
	RSAMD5     => 1,
	DH         => 2,
	DSA        => 3,
	ECC        => 4,
	INDIRECT   => 252,
	PRIVATEDNS => 253,
	PRIVATEOID => 254,
);

my %r_algorithms = reverse %algorithms;

sub new {
	my ($class, $self, $data, $offset) = @_;
	
	if ($self->{"rdlength"} > 0) {
		my ($format, $tag, $algorithm) = unpack("\@$offset n2C", $$data);
		
		$offset        += 2 * Net::DNS::INT16SZ() + 1;
		
		my $length      = $self->{"rdlength"} - (2 * Net::DNS::INT16SZ() + 1);
		my $certificate = substr($$data, $offset, $length);
		
		$self->{"format"}      = $format;
		$self->{"tag"}         = $tag;
		$self->{"algorithm"}   = $algorithm;
		$self->{"certificate"} = $certificate;
	}
        
	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;
	
	$string or return bless $self, $class;
        
	my ($format, $tag, $algorithm, @rest) = split " ", $string;        
	@rest or return bless $self, $class;
	
	# look up mnemonics
	# the "die"s may be rash, but proceeding would be dangerous
	if ($algorithm =~ /\D/) {
		$algorithm = $algorithms{$algorithm} || die	"Unknown algorithm mnemonic: '$algorithm'";
	}
	
	if ($format =~ /\D/) {
		$format = $formats{$format} || die "Unknown format mnemonic: '$format'";
	}
	
	$self->{"format"}      = $format;
	$self->{"tag"}         = $tag;
	$self->{"algorithm"}   = $algorithm;
	$self->{"certificate"} = MIME::Base64::decode(join('', @rest));


	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;
	my $rdatastr;
        
	if (exists $self->{"format"}) {
		my $cert = MIME::Base64::encode $self->{certificate};
		$cert =~ s/\n//g;
		
		my $format = defined $r_formats{$self->{"format"}} 
		? $r_formats{$self->{"format"}} : $self->{"format"};
		
		my $algorithm = defined $r_algorithms{$self->{algorithm}} 
		? $r_algorithms{$self->{algorithm}} : $self->{algorithm};
		
		$rdatastr = "$format $self->{tag} $algorithm $cert";
	} else {
		$rdatastr = '';
	}
        
	return $rdatastr;
}

sub rr_rdata {
	my ($self, $packet, $offset) = @_;
	
	my $rdata = "";
	
	if (exists $self->{"format"}) {
		$rdata .= pack("n2", $self->{"format"}, $self->{tag});
		$rdata .= pack("C",  $self->{algorithm});
		$rdata .= $self->{certificate};
	}
	
	return $rdata;
}

1;
__END__

=head1 NAME

Net::DNS::RR::CERT - DNS CERT resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Certificate (CERT) resource records. (see RFC 2538)

=head1 METHODS

=head2 format

    print "format = ", $rr->format, "\n";

Returns the format code for the certificate (in numeric form)

=head2 tag

    print "tag = ", $rr->tag, "\n";

Returns the key tag for the public key in the certificate

=head2 algorithm

    print "algorithm = ", $rr->algorithm, "\n";

Returns the algorithm used by the certificate (in numeric form)

=head2 certificate

    print "certificate = ", $rr->certificate, "\n";

Returns the data comprising the certificate itself (in raw binary form)

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 2782

