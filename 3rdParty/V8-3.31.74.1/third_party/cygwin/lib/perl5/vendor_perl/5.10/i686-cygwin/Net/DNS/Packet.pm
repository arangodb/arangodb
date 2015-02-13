package Net::DNS::Packet;
#
# $Id: Packet.pm 704 2008-02-06 21:30:59Z olaf $
#
use strict;

BEGIN { 
    eval { require bytes; }
}

use vars qw(@ISA @EXPORT_OK $VERSION $AUTOLOAD);

use Carp;
use Net::DNS ;
use Net::DNS::Question;
use Net::DNS::RR;



require Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(dn_expand);


$VERSION = (qw$LastChangedRevision: 704 $)[1];



=head1 NAME

Net::DNS::Packet - DNS packet object class

=head1 SYNOPSIS

C<use Net::DNS::Packet;>

=head1 DESCRIPTION

A C<Net::DNS::Packet> object represents a DNS packet.

=head1 METHODS

=head2 new

    $packet = Net::DNS::Packet->new("example.com");
    $packet = Net::DNS::Packet->new("example.com", "MX", "IN");

    $packet = Net::DNS::Packet->new(\$data);
    $packet = Net::DNS::Packet->new(\$data, 1);  # set debugging

    ($packet, $err) = Net::DNS::Packet->new(\$data);

    $packet = Net::DNS::Packet->new();

If passed a domain, type, and class, C<new> creates a packet
object appropriate for making a DNS query for the requested
information.  The type and class can be omitted; they default
to A and IN.

If passed a reference to a scalar containing DNS packet data,
C<new> creates a packet object from that data.  A second argument
can be passed to turn on debugging output for packet parsing.

If called in array context, returns a packet object and an
error string.  The error string will only be defined if the
packet object is undefined (i.e., couldn't be created).

Returns B<undef> if unable to create a packet object (e.g., if
the packet data is truncated).

If called with an empty argument list, C<new> creates an empty packet.

=cut

sub new {
	my $class = shift;
	my ($data) = @_;
	return $class->parse(@_) if ref $data;

	my %self = (	header		=> Net::DNS::Header->new,
			question	=> [],
			answer		=> [],
			authority	=> [],
			additional	=> []	);

	push @{$self{question}}, Net::DNS::Question->new(@_) if @_;

	bless \%self, $class;
}



sub parse {
	my $class = shift;
	my $data  = shift;
	my $debug = shift || 0;

	my %self = (	question	=> [],
			answer		=> [],
			authority	=> [],
			additional	=> [],
			answersize	=> length $$data,
			buffer		=> $data	);

	my $self = eval {
		# Parse header section
		my ($header, $offset) = Net::DNS::Header->parse($data);
		$self{header} = $header;

		# Parse question/zone section
		for ( 1 .. $header->qdcount ) {
			my $qd;
			($qd, $offset) = Net::DNS::Question->parse($data, $offset);
			push(@{$self{question}}, $qd);
		}
			
		# Retain offset for on-demand parse of remaining data
		$self{offset} = $offset;

		bless \%self, $class;
	};

	($self || die $@)->print if $debug;

	return wantarray ? ($self, $@) : $self;
}



=head2 data

    $data = $packet->data;

Returns the packet data in binary format, suitable for sending to
a nameserver.

=cut

sub data {
	my $self = shift;
	my $data = '';
	my $header = $self->{header};

	# Default question for empty packet
	$self->push('question', Net::DNS::Question->new('','ANY','ANY'))
						unless @{$self->{question}};

	#----------------------------------------------------------------------
	# Set record counts in packet header
	#----------------------------------------------------------------------
	$header->qdcount( scalar @{$self->{question}} );
	$header->ancount( scalar @{$self->{answer}} );
	$header->nscount( scalar @{$self->{authority}} );
	$header->arcount( scalar @{$self->{additional}} );

	#----------------------------------------------------------------------
	# Get the data for each section in the packet
	#----------------------------------------------------------------------
	$self->{compnames} = {};
	foreach my $component ( $header,
				@{$self->{question}},
				@{$self->{answer}},
				@{$self->{authority}},
				@{$self->{additional}}	) {
		$data .= $component->data($self, length $data);
	}

	return $data;
}


=head2 header

    $header = $packet->header;

Returns a C<Net::DNS::Header> object representing the header section
of the packet.

=cut

sub header {
	return shift->{header};
}

=head2 question, zone

    @question = $packet->question;

Returns a list of C<Net::DNS::Question> objects representing the
question section of the packet.

In dynamic update packets, this section is known as C<zone> and
specifies the zone to be updated.

=cut

sub question {
	return @{shift->{question}};
}

sub zone { &question }

=head2 answer, pre, prerequisite

    @answer = $packet->answer;

Returns a list of C<Net::DNS::RR> objects representing the answer
section of the packet.

In dynamic update packets, this section is known as C<pre> or
C<prerequisite> and specifies the RRs or RRsets which must or
must not preexist.

=cut

sub answer {
	my @rr = eval { &_answer };
	carp "$@ caught" if $@;
	return @rr;
}

sub _answer {
	my ($self) = @_;

	my @rr = @{$self->{answer}};
	return @rr if @rr;				# return if already parsed

	my $data = $self->{buffer};			# parse answer data
	my $offset = $self->{offset} || return;
	undef $self->{offset};
	my $ancount = $self->{header}->ancount;
	my $rr;
	while ( $ancount-- ) {
		($rr, $offset) = Net::DNS::RR->parse($data, $offset);
		push(@rr, $rr);
	}
	$self->{offset} = $offset;			# index next section
	@{$self->{answer}} = @rr;
}

sub pre		{ &answer }
sub prerequisite { &answer }

=head2 authority, update

    @authority = $packet->authority;

Returns a list of C<Net::DNS::RR> objects representing the authority
section of the packet.

In dynamic update packets, this section is known as C<update> and
specifies the RRs or RRsets to be added or deleted.

=cut

sub authority {
	my @rr = eval { &_authority };
	carp "$@ caught" if $@;
	return @rr;
}

sub _authority {
	my ($self) = @_;

	my @rr = @{$self->{authority}};
	return @rr if @rr;				# return if already parsed

	&_answer unless @{$self->{answer}};		# parse answer data

	my $data = $self->{buffer};			# parse authority data
	my $offset = $self->{offset} || return;
	undef $self->{offset};
	my $nscount = $self->{header}->nscount;
	my $rr;
	while ( $nscount-- ) {
		($rr, $offset) = Net::DNS::RR->parse($data, $offset);
		push(@rr, $rr);
	}
	$self->{offset} = $offset;			# index next section
	@{$self->{authority}} = @rr;
}

sub update { &authority }

=head2 additional

    @additional = $packet->additional;

Returns a list of C<Net::DNS::RR> objects representing the additional
section of the packet.

=cut

sub additional {
	my @rr = eval { &_additional };
	carp "$@ caught" if $@;
	return @rr;
}

sub _additional {
	my ($self) = @_;

	my @rr = @{$self->{additional}};
	return @rr if @rr;				# return if already parsed

	&_authority unless @{$self->{authority}};	# parse authority data

	my $data = $self->{buffer};			# parse additional data
	undef $self->{buffer};				# discard raw data after use
	my $offset = $self->{offset} || return;
	undef $self->{offset};
	my $arcount = $self->{header}->arcount;
	my $rr;
	while ( $arcount-- ) {
		($rr, $offset) = Net::DNS::RR->parse($data, $offset);
		push(@rr, $rr);
	}
	@{$self->{additional}} = @rr;
}


=head2 print

    $packet->print;

Prints the packet data on the standard output in an ASCII format
similar to that used in DNS zone files.

=cut

sub print {	print &string; }

=head2 string

    print $packet->string;

Returns a string representation of the packet.

=cut

sub string {
	my $self = shift;

	my $header = $self->{header};
	my $update = $header->opcode eq 'UPDATE';

	my $server = $self->{answerfrom};
	my $string = $server ? ";; Answer received from $server ($self->{answersize} bytes)\n" : "";

	$string .= ";; HEADER SECTION\n".$header->string;

	my $question = $update ? 'ZONE' : 'QUESTION';
	my @question = map{$_->string} $self->question;
	my $qdcount = @question;
	my $qds = $qdcount != 1 ? 's' : '';
	$string .= join "\n;; ", "\n;; $question SECTION ($qdcount record$qds)", @question;

	my $answer = $update ? 'PREREQUISITE' : 'ANSWER';
	my @answer = map{$_->string} $self->answer;
	my $ancount = @answer;
	my $ans = $ancount != 1 ? 's' : '';
	$string .= join "\n", "\n\n;; $answer SECTION ($ancount record$ans)", @answer;

	my $authority = $update ? 'UPDATE' : 'AUTHORITY';
	my @authority = map{$_->string} $self->authority;
	my $nscount = @authority;
	my $nss = $nscount != 1 ? 's' : '';
	$string .= join "\n", "\n\n;; $authority SECTION ($nscount record$nss)", @authority;

	my @additional = map{$_->string} $self->additional;
	my $arcount = @additional;
	my $ars = $arcount != 1 ? 's' : '';
	$string .= join "\n", "\n\n;; ADDITIONAL SECTION ($arcount record$ars)", @additional;

	return $string."\n\n";
}

=head2 answerfrom

    print "packet received from ", $packet->answerfrom, "\n";

Returns the IP address from which we received this packet.  User-created
packets will return undef for this method.

=cut

sub answerfrom {
	my $self = shift;

	return $self->{answerfrom} = shift if @_;

	return $self->{answerfrom};
}

=head2 answersize

    print "packet size: ", $packet->answersize, " bytes\n";

Returns the size of the packet in bytes as it was received from a
nameserver.  User-created packets will return undef for this method
(use C<< length $packet->data >> instead).

=cut

sub answersize {
	return shift->{answersize};
}

=head2 push

    $ancount = $packet->push(pre        => $rr);
    $nscount = $packet->push(update     => $rr);
    $arcount = $packet->push(additional => $rr);

    $nscount = $packet->push(update => $rr1, $rr2, $rr3);
    $nscount = $packet->push(update => @rr);

Adds RRs to the specified section of the packet.

Returns the number of resource records in the specified section.


=cut

sub push {
	my $self = shift;
	my $section = lc shift || '';
	my @rr = map{ref $_ ? $_ : ()} @_;

	my $hdr = $self->{header};
	for ( $section ) {
		return $hdr->qdcount(push(@{$self->{question}}, @rr)) if /^question/;

		if ( $hdr->opcode eq 'UPDATE' ) {
			my ($zone) = $self->zone;
			my $zclass = $zone->zclass;
			foreach ( @rr ) {
				$_->class($zclass) unless $_->class =~ /ANY|NONE/;
			}
		}

		return $hdr->ancount(push(@{$self->{answer}}, @rr)) if /^ans|^pre/;
		return $hdr->nscount(push(@{$self->{authority}}, @rr)) if /^auth|^upd/;
		return $hdr->adcount(push(@{$self->{additional}}, @rr)) if /^add/;
	}

	carp qq(invalid section "$section");
	return undef;	# undefined record count
}


=head2 unique_push

    $ancount = $packet->unique_push(pre        => $rr);
    $nscount = $packet->unique_push(update     => $rr);
    $arcount = $packet->unique_push(additional => $rr);

    $nscount = $packet->unique_push(update => $rr1, $rr2, $rr3);
    $nscount = $packet->unique_push(update => @rr);

Adds RRs to the specified section of the packet provided that 
the RRs do not already exist in the packet.

Returns the number of resource records in the specified section.

=cut

sub unique_push {
	my $self = shift;
	my $section = shift;
	my @rr = map{ref $_ ? $_ : ()} @_;

	my @unique = map{$self->{seen}->{ (lc $_->name) . $_->class . $_->type  . $_->rdatastr }++ ? () : $_} @rr;

	return $self->push($section, @unique);
}

=head2 safe_push

A deprecated name for C<unique_push()>.

=cut

sub safe_push {
	carp('safe_push() is deprecated, use unique_push() instead,');
	&unique_push;
}
	

=head2 pop

    my $rr = $packet->pop("pre");
    my $rr = $packet->pop("update");
    my $rr = $packet->pop("additional");
    my $rr = $packet->pop("question");

Removes RRs from the specified section of the packet.

=cut

sub pop {
	my $self = shift;
	my $section = lc shift || '';

	for ( $section ) {
		return pop(@{$self->{answer}}) if /^ans|^pre/;
		return pop(@{$self->{question}}) if /^question/;

		$self->additional if $self->{buffer};	# parse remaining data

		return pop(@{$self->{authority}}) if /^auth|^upd/;
		return pop(@{$self->{additional}}) if /^add/;
	}

	carp qq(invalid section "$section");
	return undef;
}


=head2 dn_comp

    $compname = $packet->dn_comp("foo.example.com", $offset);

Returns a domain name compressed for a particular packet object, to
be stored beginning at the given offset within the packet data.  The
name will be added to a running list of compressed domain names for
future use.

=cut

sub dn_comp {
	my ($self, $name, $offset) = @_;
	# The Exporter module does not seem to catch this baby...
	my @names=Net::DNS::name2labels($name);
	my $namehash = $self->{compnames};
	my $compname='';

	while (@names) {
		my $dname = join('.', @names);

		if ( my $pointer = $namehash->{$dname} ) {
			$compname .= pack('n', 0xc000 | $pointer);
			last;
		}
		$namehash->{$dname} = $offset;

		my $label  = shift @names;
		my $length = length $label || next;	# skip if null
		if ( $length > 63 ) {
			$length = 63;
			$label = substr($label, 0, $length);
			carp "\n$label...\ntruncated to $length octets (RFC1035 2.3.1)";
		}
		$compname .= pack('C a*', $length, $label);
		$offset   += $length + 1;
	}

	$compname .= pack('C', 0) unless @names;

	return $compname;
}

=head2 dn_expand

    use Net::DNS::Packet qw(dn_expand);
    ($name, $nextoffset) = dn_expand(\$data, $offset);

    ($name, $nextoffset) = Net::DNS::Packet::dn_expand(\$data, $offset);

Expands the domain name stored at a particular location in a DNS
packet.  The first argument is a reference to a scalar containing
the packet data.  The second argument is the offset within the
packet where the (possibly compressed) domain name is stored.

Returns the domain name and the offset of the next location in the
packet.

Returns B<(undef)> if the domain name couldn't be expanded.

=cut
# '

# This is very hot code, so we try to keep things fast.  This makes for
# odd style sometimes.

sub dn_expand {
#FYI	my ($packet, $offset) = @_;
	return dn_expand_XS(@_) if $Net::DNS::HAVE_XS;
#	warn "USING PURE PERL dn_expand()\n";
	return dn_expand_PP(@_, {} );	# $packet, $offset, anonymous hash
}

sub dn_expand_PP {
	my ($packet, $offset, $visited) = @_;
	my $packetlen = length $$packet;
	my $name = '';

	while ( $offset < $packetlen ) {
		unless ( my $length = unpack("\@$offset C", $$packet) ) {
			$name =~ s/\.$//o;
			return ($name, ++$offset);

		} elsif ( ($length & 0xc0) == 0xc0 ) {		# pointer
			my $point = 0x3fff & unpack("\@$offset n", $$packet);
			die 'Exception: unbounded name expansion' if $visited->{$point}++;

			my ($suffix) = dn_expand_PP($packet, $point, $visited);

			return ($name.$suffix, $offset+2) if defined $suffix;

		} else {
			my $element = substr($$packet, ++$offset, $length);
			$name .= Net::DNS::wire2presentation($element).'.';
			$offset += $length;
		}
	}
	return undef;
}

=head2 sign_tsig

    $key_name = "tsig-key";
    $key      = "awwLOtRfpGE+rRKF2+DEiw==";

    $update = Net::DNS::Update->new("example.com");
    $update->push("update", rr_add("foo.example.com A 10.1.2.3"));

    $update->sign_tsig($key_name, $key);

    $response = $res->send($update);

Signs a packet with a TSIG resource record (see RFC 2845).  Uses the
following defaults:

    algorithm   = HMAC-MD5.SIG-ALG.REG.INT
    time_signed = current time
    fudge       = 300 seconds

If you wish to customize the TSIG record, you'll have to create it
yourself and call the appropriate Net::DNS::RR::TSIG methods.  The
following example creates a TSIG record and sets the fudge to 60
seconds:

    $key_name = "tsig-key";
    $key      = "awwLOtRfpGE+rRKF2+DEiw==";

    $tsig = Net::DNS::RR->new("$key_name TSIG $key");
    $tsig->fudge(60);

    $query = Net::DNS::Packet->new("www.example.com");
    $query->sign_tsig($tsig);

    $response = $res->send($query);

You shouldn't modify a packet after signing it; otherwise authentication
will probably fail.

=cut

sub sign_tsig {
	my $self = shift;
	my $tsig = shift || return undef;

	unless ( ref $tsig && ($tsig->type eq "TSIG") ) {
		my $key = shift || return undef;
		$tsig = Net::DNS::RR->new("$tsig TSIG $key");
	}

	$self->push('additional', $tsig) if $tsig;
	return $tsig;
}



=head2 sign_sig0

SIG0 support is provided through the Net::DNS::RR::SIG class. This class is not part
of the default Net::DNS distribution but resides in the Net::DNS::SEC distribution.

    $update = Net::DNS::Update->new("example.com");
    $update->push("update", rr_add("foo.example.com A 10.1.2.3"));
    $update->sign_sig0("Kexample.com+003+25317.private");


SIG0 support is experimental see Net::DNS::RR::SIG for details.

The method will call C<Carp::croak()> if Net::DNS::RR::SIG cannot be found.


=cut

sub sign_sig0 {
	my $self = shift;
	my $arg = shift || return undef;
	my $sig0;
	
	croak('sign_sig0() is only available when Net::DNS::SEC is installed') 
		unless $Net::DNS::DNSSEC;
	
	if ( ref $arg ) {
		if ( UNIVERSAL::isa($arg,'Net::DNS::RR::SIG') ) {
			$sig0 = $arg;
		
		} elsif ( UNIVERSAL::isa($arg,'Net::DNS::SEC::Private') ) {
			$sig0 = Net::DNS::RR::SIG->create('', $arg);
		
		} elsif ( UNIVERSAL::isa($arg,'Net::DNS::RR::SIG::Private') ) {
			carp ref($arg).' is deprecated - use Net::DNS::SEC::Private instead';
			$sig0 = Net::DNS::RR::SIG->create('', $arg);

		} else {
			croak 'Incompatible class as argument to sign_sig0: '.ref($arg);

		}

	} else {
		$sig0 = Net::DNS::RR::SIG->create('', $arg);
	}
	
	$self->push('additional', $sig0) if $sig0;
	return $sig0;
}



=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

Portions Copyright (c) 2002-2005 Olaf Kolkman

Portions Copyright (c) 2007-2008 Dick Franks

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.



=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Update>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1035 Section 4.1, RFC 2136 Section 2, RFC 2845

=cut

1;
