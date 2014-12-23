package Net::DNS::Header;
#
# $Id: Header.pm 704 2008-02-06 21:30:59Z olaf $
#

use strict;

BEGIN { 
    eval { require bytes; }
} 


use vars qw($VERSION $AUTOLOAD);

use Carp;
use Net::DNS;

use constant MAX_ID => 65535;

$VERSION = (qw$LastChangedRevision: 704 $)[1];

=head1 NAME

Net::DNS::Header - DNS packet header class

=head1 SYNOPSIS

C<use Net::DNS::Header;>

=head1 DESCRIPTION

A C<Net::DNS::Header> object represents the header portion of a DNS
packet.

=head1 METHODS

=head2 new

    $header = Net::DNS::Header->new;

C<new> creates a header object appropriate for making a DNS query.

=cut

{
	sub nextid {
		int rand(MAX_ID);
	}
}

sub new {
	my $class = shift;

	my $self = {	id	=> nextid(),
			qr	=> 0,
			opcode	=> $Net::DNS::opcodesbyval{0},
			aa	=> 0,
			tc	=> 0,
			rd	=> 1,
			ra	=> 0,
			ad	=> 0,
			cd	=> 0,
			rcode	=> $Net::DNS::rcodesbyval{0},
			qdcount	=> 0,
			ancount	=> 0,
			nscount	=> 0,
			arcount	=> 0,
			};

	bless $self, $class;
}


=head2 parse

    ($header, $offset) = Net::DNS::Header->parse(\$data);

Parses the header record at the start of a DNS packet.
The argument is a reference to the packet data.

Returns a Net::DNS::Header object and the offset of the next location
in the packet.

Parsing is aborted if the header object cannot be created (e.g.,
corrupt or insufficient data).

=cut

use constant PACKED_LENGTH => length pack 'n C2 n4', (0)x7;

sub parse {
	my ($class, $data) = @_;

	die 'Exception: incomplete data' if length($$data) < PACKED_LENGTH;

	my ($id, $b2, $b3, $qd, $an, $ns, $ar) = unpack('n C2 n4', $$data);

	my $opval  = ($b2 >> 3) & 0xf;
	my $opcode = $Net::DNS::opcodesbyval{$opval} || $opval;
	my $rval  = $b3 & 0xf;
	my $rcode = $Net::DNS::rcodesbyval{$rval} || $rval;

	my $self = {	id	=> $id,
			qr	=> ($b2 >> 7) & 0x1,
			opcode	=> $opcode,
			aa	=> ($b2 >> 2) & 0x1,
			tc	=> ($b2 >> 1) & 0x1,
			rd	=> $b2 & 0x1,
			ra	=> ($b3 >> 7) & 0x1,
			ad	=> ($b3 >> 5) & 0x1,
			cd	=> ($b3 >> 4) & 0x1,
			rcode	=> $rcode,
			qdcount	=> $qd,
			ancount	=> $an,
			nscount	=> $ns,
			arcount	=> $ar
			};

	bless $self, $class;

	return wantarray ? ($self, PACKED_LENGTH) : $self;
}

#
# Some people have reported that Net::DNS dies because AUTOLOAD picks up
# calls to DESTROY.
#
sub DESTROY {}

=head2 print

    $header->print;

Prints the header record on the standard output.

=cut

sub print {	print &string, "\n"; }

=head2 string

    print $header->string;

Returns a string representation of the header object.

=cut

sub string {
	my $self = shift;
	my $retval = "";

	$retval .= ";; id = $self->{id}\n";

	if ($self->{"opcode"} eq "UPDATE") {
		$retval .= ";; qr = $self->{qr}    "      .
		           "opcode = $self->{opcode}    " .
		           "rcode = $self->{rcode}\n";

		$retval .= ";; zocount = $self->{qdcount}  " .
		           "prcount = $self->{ancount}  "    .
		           "upcount = $self->{nscount}  "    .
		           "adcount = $self->{arcount}\n";
	}
	else {
		$retval .= ";; qr = $self->{qr}    "      .
		           "opcode = $self->{opcode}    " .
		           "aa = $self->{aa}    "         .
		           "tc = $self->{tc}    "         .
		           "rd = $self->{rd}\n";

		$retval .= ";; ra = $self->{ra}    " .
		           "ad = $self->{ad}    "         .
		           "cd = $self->{cd}    "         .
		           "rcode  = $self->{rcode}\n";

		$retval .= ";; qdcount = $self->{qdcount}  " .
		           "ancount = $self->{ancount}  "    .
		           "nscount = $self->{nscount}  "    .
		           "arcount = $self->{arcount}\n";
	}

	return $retval;
}

=head2 id

    print "query id = ", $header->id, "\n";
    $header->id(1234);

Gets or sets the query identification number.

=head2 qr

    print "query response flag = ", $header->qr, "\n";
    $header->qr(0);

Gets or sets the query response flag.

=head2 opcode

    print "query opcode = ", $header->opcode, "\n";
    $header->opcode("UPDATE");

Gets or sets the query opcode (the purpose of the query).

=head2 aa

    print "answer is ", $header->aa ? "" : "non-", "authoritative\n";
    $header->aa(0);

Gets or sets the authoritative answer flag.

=head2 tc

    print "packet is ", $header->tc ? "" : "not ", "truncated\n";
    $header->tc(0);

Gets or sets the truncated packet flag.

=head2 rd

    print "recursion was ", $header->rd ? "" : "not ", "desired\n";
    $header->rd(0);

Gets or sets the recursion desired flag.


=head2 cd

    print "checking was ", $header->cd ? "not" : "", "desired\n";
    $header->cd(0);

Gets or sets the checking disabled flag.



=head2 ra

    print "recursion is ", $header->ra ? "" : "not ", "available\n";
    $header->ra(0);

Gets or sets the recursion available flag.


=head2 ad

    print "The result has ", $header->ad ? "" : "not", "been verified\n"


Relevant in DNSSEC context.

(The AD bit is only set on answers where signatures have been
cryptographically verified or the server is authoritative for the data
and is allowed to set the bit by policy.)


=head2 rcode

    print "query response code = ", $header->rcode, "\n";
    $header->rcode("SERVFAIL");

Gets or sets the query response code (the status of the query).

=head2 qdcount, zocount

    print "# of question records: ", $header->qdcount, "\n";
    $header->qdcount(2);

Gets or sets the number of records in the question section of the packet.
In dynamic update packets, this field is known as C<zocount> and refers
to the number of RRs in the zone section.

=head2 ancount, prcount

    print "# of answer records: ", $header->ancount, "\n";
    $header->ancount(5);

Gets or sets the number of records in the answer section of the packet.
In dynamic update packets, this field is known as C<prcount> and refers
to the number of RRs in the prerequisite section.

=head2 nscount, upcount

    print "# of authority records: ", $header->nscount, "\n";
    $header->nscount(2);

Gets or sets the number of records in the authority section of the packet.
In dynamic update packets, this field is known as C<upcount> and refers
to the number of RRs in the update section.

=head2 arcount, adcount

    print "# of additional records: ", $header->arcount, "\n";
    $header->arcount(3);

Gets or sets the number of records in the additional section of the packet.
In dynamic update packets, this field is known as C<adcount>.

=cut

sub zocount { &qdcount; }
sub prcount { &ancount; }
sub upcount { &nscount; }
sub adcount { &arcount; }


sub AUTOLOAD {
	my $self = shift;

	my $name = $AUTOLOAD;
	$name =~ s/.*://o;

	croak "$AUTOLOAD: no such method" unless exists $self->{$name};

	return $self->{$name} unless @_;

	my $value = shift;
	$self->{$name} = $value;
}


=head2 data

    $hdata = $header->data;

Returns the header data in binary format, appropriate for use in a
DNS query packet.

=cut

sub data {
	my $self = shift;

	my $opcode = $Net::DNS::opcodesbyname{ $self->{opcode} };
	my $rcode  = $Net::DNS::rcodesbyname{ $self->{rcode} };

	my $byte2 =	($self->{qr} ? 0x80 : 0)
			| ($opcode << 3)
			| ($self->{aa} ? 0x04 : 0)
			| ($self->{tc} ? 0x02 : 0)
			| ($self->{rd} ? 0x01 : 0);

	my $byte3 =	($self->{ra} ? 0x80 : 0)
			| ($self->{ad} ? 0x20 : 0)
			| ($self->{cd} ? 0x10 : 0)
			| ($rcode || 0);

	pack('n C2 n4', $self->{id}, $byte2, $byte3,
			map{$self->{$_} || 0} qw(qdcount ancount nscount arcount) );
}

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

Portions Copyright (c) 2007 Dick Franks.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Update>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1035 Section 4.1.1

=cut

1;
