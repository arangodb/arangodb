package Net::DNS::Resolver::Base;
#
# $Id: Base.pm 704 2008-02-06 21:30:59Z olaf $
#

use strict;

BEGIN { 
    eval { require bytes; }
}

use vars qw(
	    $VERSION
	    $has_inet6
	    $AUTOLOAD
);

use Carp;
use Config ();
use Socket;
use IO::Socket;
use IO::Select;

use Net::DNS;
use Net::DNS::Packet;

$VERSION = (qw$LastChangedRevision: 704 $)[1];


#
#  A few implementation notes wrt IPv6 support.
#
#  In general we try to be gracious to those stacks that do not have ipv6 support.
#  We test that by means of the availability of Socket6 and IO::Socket::INET6
#


#  We have chosen to not use mapped IPv4 addresses, there seem to be
#  issues with this; as a result we have to use sockets for both
#  family types.  To be able to deal with persistent sockets and
#  sockets of both family types we use an array that is indexed by the
#  socketfamily type to store the socket handlers. I think this could
#  be done more efficiently.

 
#  inet_pton is not available on WIN32, so we only use the getaddrinfo
#  call to translate IP addresses to socketaddress


 
#  Set the $force_inet4_only variable inside the BEGIN block to force
#  not to use the IPv6 stuff. You can use this for compatibility
#  test. We do not see a need to do this from the calling code.

 
# Olaf Kolkman, RIPE NCC, December 2003.
 
 
BEGIN {
    if ( 
	 eval {require Socket6;} &&
	 # INET6 prior to 2.01 will not work; sorry.
	 eval {require IO::Socket::INET6; IO::Socket::INET6->VERSION("2.00");}
	 ) {
 	import Socket6;
 	$has_inet6=1;
    }else{
 	$has_inet6=0;
    }
 }
	    
 
 
 
 
  
#
# Set up a closure to be our class data.
#
{
	my %defaults = (
		nameservers	   => ['127.0.0.1'],
		port		   => 53,
		srcaddr        => '0.0.0.0',
		srcport        => 0,
		domain	       => '',
		searchlist	   => [],
		retrans	       => 5,
		retry		   => 4,
		usevc		   => 0,
		stayopen       => 0,
		igntc          => 0,
		recurse        => 1,
		defnames       => 1,
		dnsrch         => 1,
		debug          => 0,
		errorstring	   => 'unknown error or no error',
		tsig_rr        => undef,
		answerfrom     => '',
		querytime      => undef,
		tcp_timeout    => 120,
		udp_timeout    => undef,
		axfr_sel       => undef,
		axfr_rr        => [],
		axfr_soa_count => 0,
		persistent_tcp => 0,
		persistent_udp => 0,
		dnssec         => 0,
		udppacketsize  => 0,  # The actual default is lower bound by Net::DNS::PACKETSZ
		cdflag         => 1,  # this is only used when {dnssec} == 1
		force_v4       => 0,  # force_v4 is only relevant when we have
                                      # v6 support available
		ignqrid        => 0,  # normally packets with non-matching ID 
                                      # or with the qr bit of are thrown away
			              # in 'ignqrid' these packets are 
			              # are accepted.
			              # USE WITH CARE, YOU ARE VULNARABLE TO
			              # SPOOFING IF SET.
			              # This is may be a temporary feature
	);
	
	# If we're running under a SOCKSified Perl, use TCP instead of UDP
	# and keep the sockets open.
	if ($Config::Config{'usesocks'}) {
		$defaults{'usevc'} = 1;
		$defaults{'persistent_tcp'} = 1;
	}
	
	sub defaults { \%defaults }
}

# These are the attributes that we let the user specify in the new().
# We also deprecate access to these with AUTOLOAD (some may be useful).
my %public_attr = map { $_ => 1 } qw(
	nameservers
	port
	srcaddr
	srcport
	domain
	searchlist
	retrans
	retry
	usevc
	stayopen
	igntc
	recurse
	defnames
	dnsrch
	debug
	tcp_timeout
	udp_timeout
	persistent_tcp
	persistent_udp
	dnssec
	ignqrid
);


sub new {
	my $class = shift;
	my $self = bless({ %{$class->defaults} }, $class);

	$self->_process_args(@_) if @_ and @_ % 2 == 0;
	return $self;
}



sub _process_args {
	my ($self, %args) = @_;
	
	if ($args{'config_file'}) {
		$self->read_config_file($args{'config_file'});
	}
	
	foreach my $attr (keys %args) {
		next unless $public_attr{$attr};

		if ($attr eq 'nameservers' || $attr eq 'searchlist') {

			die "Net::DNS::Resolver->new(): $attr must be an arrayref\n" unless
			  defined($args{$attr}) &&  UNIVERSAL::isa($args{$attr}, 'ARRAY');

		}
		
		if ($attr eq 'nameservers') {
			$self->nameservers(@{$args{$attr}});
		} else {
			$self->{$attr} = $args{$attr};
		}
	}


}
			
			
			


#
# Some people have reported that Net::DNS dies because AUTOLOAD picks up
# calls to DESTROY.
#
sub DESTROY {}


sub read_env {
	my ($invocant) = @_;
	my $config     = ref $invocant ? $invocant : $invocant->defaults;
		
	$config->{'nameservers'} = [ $ENV{'RES_NAMESERVERS'} =~ m/(\S+)/g ]
		if exists $ENV{'RES_NAMESERVERS'};

	$config->{'searchlist'}  = [ split(' ', $ENV{'RES_SEARCHLIST'})  ]
		if exists $ENV{'RES_SEARCHLIST'};
	
	$config->{'domain'} = $ENV{'LOCALDOMAIN'}
		if exists $ENV{'LOCALDOMAIN'};

	if (exists $ENV{'RES_OPTIONS'}) {
		foreach ($ENV{'RES_OPTIONS'} =~ m/(\S+)/g) {
			my ($name, $val) = split(m/:/);
			$val = 1 unless defined $val;
			$config->{$name} = $val if exists $config->{$name};
		}
	}
}

#
# $class->read_config_file($filename) or $self->read_config_file($file)
#
sub read_config_file {
	my ($invocant, $file) = @_;
	my $config            = ref $invocant ? $invocant : $invocant->defaults;

	
	my @ns;
	my @searchlist;
	
	local *FILE;

	open(FILE, "< $file") or croak "Could not open $file: $!";
	local $/ = "\n";
	local $_;
	
	while (<FILE>) {
 		s/\s*[;#].*//;
		
		# Skip ahead unless there's non-whitespace characters 
		next unless m/\S/;

		SWITCH: {
			/^\s*domain\s+(\S+)/ && do {
				$config->{'domain'} = $1;
				last SWITCH;
			};

			/^\s*search\s+(.*)/ && do {
				push(@searchlist, split(' ', $1));
				last SWITCH;
			};

			/^\s*nameserver\s+(.*)/ && do {
				foreach my $ns (split(' ', $1)) {
					$ns = '0.0.0.0' if $ns eq '0';
#					next if $ns =~ m/:/;  # skip IPv6 nameservers
					push @ns, $ns;
				}
				last SWITCH;
			};
		    }
		  }
		close FILE || croak "Could not close $file: $!";
		
		$config->{'nameservers'} = [ @ns ]         if @ns;
		$config->{'searchlist'}  = [ @searchlist ] if @searchlist;
	    }
 



sub print { print $_[0]->string }

sub string {
	my $self = shift;

	my $timeout = defined $self->{'tcp_timeout'} ? $self->{'tcp_timeout'} : 'indefinite';
	my $hasINET6line= $has_inet6 ?" (IPv6 Transport is available)":" (IPv6 Transport is not available)";
	my $ignqrid=$self->{'ignqrid'} ? "\n;; ACCEPTING ALL PACKETS (IGNQRID)":"";
	return <<END;
;; RESOLVER state:
;;  domain       = $self->{domain}
;;  searchlist   = @{$self->{searchlist}}
;;  nameservers  = @{$self->{nameservers}}
;;  port         = $self->{port}
;;  srcport      = $self->{srcport}
;;  srcaddr      = $self->{srcaddr}
;;  tcp_timeout  = $timeout
;;  retrans  = $self->{retrans}  retry    = $self->{retry}
;;  usevc    = $self->{usevc}  stayopen = $self->{stayopen}    igntc = $self->{igntc}
;;  defnames = $self->{defnames}  dnsrch   = $self->{dnsrch}
;;  recurse  = $self->{recurse}  debug    = $self->{debug}
;;  force_v4 = $self->{force_v4} $hasINET6line $ignqrid
END

}


sub searchlist {
	my $self = shift;
	$self->{'searchlist'} = [ @_ ] if @_;
	return @{$self->{'searchlist'}};
}

sub nameservers {
    my $self   = shift;

    if (@_) {
	my @a;
	foreach my $ns (@_) {
	    next unless defined($ns);
	    if ( _ip_is_ipv4($ns) ) {
		push @a, ($ns eq '0') ? '0.0.0.0' : $ns;

	    } elsif ( _ip_is_ipv6($ns) ) {
		push @a, ($ns eq '0') ? '::0' : $ns;

	    } else  {
		my $defres = Net::DNS::Resolver->new;
		my @names;
		
		if ($ns !~ /\./) {
		    if (defined $defres->searchlist) {
			@names = map { $ns . '.' . $_ }
			$defres->searchlist;
		    } elsif (defined $defres->domain) {
			@names = ($ns . '.' . $defres->domain);
		    }
		}
		else {
		    @names = ($ns);
		}
		
		my $packet = $defres->search($ns);
		$self->errorstring($defres->errorstring);
		if (defined($packet)) {
		    push @a, cname_addr([@names], $packet);
		}
	    }
	}
	

	$self->{'nameservers'} = [ @a ];
    }
    my @returnval;
    foreach my $ns (@{$self->{'nameservers'}}){
	next if _ip_is_ipv6($ns) && (! $has_inet6 || $self->force_v4() );
	push @returnval, $ns;
    }
    
    return @returnval;
}

sub nameserver { &nameservers }

sub cname_addr {
	my $names  = shift;
	my $packet = shift;
	my @addr;
	my @names = @{$names};

	my $oct2 = '(?:2[0-4]\d|25[0-5]|[0-1]?\d\d|\d)';

	RR: foreach my $rr ($packet->answer) {
		next RR unless grep {$rr->name} @names;
				
		if ($rr->type eq 'CNAME') {
			push(@names, $rr->cname);
		} elsif ($rr->type eq 'A') {
			# Run a basic taint check.
			next RR unless $rr->address =~ m/^($oct2\.$oct2\.$oct2\.$oct2)$/o;
			
			push(@addr, $1)
		}
	}
	
	
	return @addr;
}


# if ($self->{"udppacketsize"}  > Net::DNS::PACKETSZ() 
# then we use EDNS and $self->{"udppacketsize"} 
# should be taken as the maximum packet_data length
sub _packetsz {
	my ($self) = @_;

	return $self->{"udppacketsize"} > Net::DNS::PACKETSZ() ? 
		   $self->{"udppacketsize"} : Net::DNS::PACKETSZ(); 
}

sub _reset_errorstring {
	my ($self) = @_;
	
	$self->errorstring($self->defaults->{'errorstring'});
}


sub search {
	my $self = shift;
	my $name = shift || '.';

	my $defdomain = $self->{domain} if $self->{defnames};
	my @searchlist = @{$self->{searchlist}} if $self->{dnsrch};

	# resolve name by trying as absolute name, then applying searchlist
	my @list = (undef, @searchlist);
	for ($name) {
		# resolve name with no dots or colons by applying searchlist (or domain)
		@list = @searchlist ? @searchlist : ($defdomain) unless  m/[:.]/;
		# resolve name with trailing dot as absolute name
		@list = (undef) if m/\.$/;
	}

	foreach my $suffix ( @list ) {
	        my $fqname = join '.', $name, ($suffix || ());

		print ';; search(', join(', ', $fqname, @_), ")\n" if $self->{debug};

		my $packet = $self->send($fqname, @_) || return undef;

		next unless ($packet->header->rcode eq "NOERROR"); # something 
								 #useful happened
		return $packet if $packet->header->ancount;	# answer found
		next unless $packet->header->qdcount;           # question empty?

		last if ($packet->question)[0]->qtype eq 'PTR';	# abort search if IP
	}
	return undef;
}


sub query {
	my $self = shift;
	my $name = shift || '.';

	# resolve name containing no dots or colons by appending domain
	my @suffix = ($self->{domain} || ()) if $name !~ m/[:.]/ and $self->{defnames};

	my $fqname = join '.', $name, @suffix;

	print ';; query(', join(', ', $fqname, @_), ")\n" if $self->{debug};

	my $packet = $self->send($fqname, @_) || return undef;

	return $packet if $packet->header->ancount;	# answer found
	return undef;
}


sub send {
	my $self = shift;
	my $packet = $self->make_query_packet(@_);
	my $packet_data = $packet->data;


	my $ans;

	if ($self->{'usevc'} || length $packet_data > $self->_packetsz) {
	  
	    $ans = $self->send_tcp($packet, $packet_data);
	    
	} else {
	    $ans = $self->send_udp($packet, $packet_data);

	    if ($ans && $ans->header->tc && !$self->{'igntc'}) {
			print ";;\n;; packet truncated: retrying using TCP\n" if $self->{'debug'};
			$ans = $self->send_tcp($packet, $packet_data);
	    }
	}
	
	return $ans;
}



sub send_tcp {
	my ($self, $packet, $packet_data) = @_;
	my $lastanswer;
	
	my $srcport = $self->{'srcport'};
	my $srcaddr = $self->{'srcaddr'};
	my $dstport = $self->{'port'};

	unless ( $self->nameservers()) {
		$self->errorstring('no nameservers');
		print ";; ERROR: send_tcp: no nameservers\n" if $self->{'debug'};
		return;
	}
	
	$self->_reset_errorstring;

	
      NAMESERVER: foreach my $ns ($self->nameservers()) {
	      
	      print ";; attempt to send_tcp($ns:$dstport) (src port = $srcport)\n"
		  if $self->{'debug'};
	      
	      
	      
	      my $sock;
	      my $sock_key = "$ns:$dstport";
	      my ($host,$port);
	      if ($self->persistent_tcp && $self->{'sockets'}[AF_UNSPEC]{$sock_key}) {
		      $sock = $self->{'sockets'}[AF_UNSPEC]{$sock_key};
		      print ";; using persistent socket\n"
			if $self->{'debug'};
		      unless ($sock->connected){
			print ";; persistent socket disconnected (trying to reconnect)" 
			  if $self->{'debug'};
			undef($sock);
			$sock= $self->_create_tcp_socket($ns);
			next NAMESERVER unless $sock;
			$self->{'sockets'}[AF_UNSPEC]{$sock_key} = $sock;
		      }
		      
	      } else {
		      $sock= $self->_create_tcp_socket($ns);
		      next NAMESERVER unless $sock;
		      
		      $self->{'sockets'}[AF_UNSPEC]{$sock_key} = $sock if 
			  $self->persistent_tcp;
	      }
	      

	      my $lenmsg = pack('n', length($packet_data));
	      print ';; sending ', length($packet_data), " bytes\n"
		  if $self->{'debug'};
	      
	      # note that we send the length and packet data in a single call
	      # as this produces a single TCP packet rather than two. This
	      # is more efficient and also makes things much nicer for sniffers.
	      # (ethereal doesn't seem to reassemble DNS over TCP correctly)
	      
	      
	      unless ($sock->send( $lenmsg . $packet_data)) {
		      $self->errorstring($!);
		      print ";; ERROR: send_tcp: data send failed: $!\n"
			  if $self->{'debug'};
		      next NAMESERVER;
	      }
	      
	      my $sel = IO::Select->new($sock);
	      my $timeout=$self->{'tcp_timeout'};
	      if ($sel->can_read($timeout)) {
		      my $buf = read_tcp($sock, Net::DNS::INT16SZ(), $self->{'debug'});
		      next NAMESERVER unless length($buf); # Failure to get anything
		      my ($len) = unpack('n', $buf);
		      next NAMESERVER unless $len;         # Cannot determine size
		      
		      unless ($sel->can_read($timeout)) {
			      $self->errorstring('timeout');
			      print ";; TIMEOUT\n" if $self->{'debug'};
			      next;
		      }
		      
		      $buf = read_tcp($sock, $len, $self->{'debug'});
		      
		      $self->answerfrom($sock->peerhost);
		      
		      print ';; received ', length($buf), " bytes\n"
			  if $self->{'debug'};

		      unless (length($buf) == $len) {
				$self->errorstring("expected $len bytes, " .
						   'received ' . length($buf));
				next;
			}

			my ($ans, $err) = Net::DNS::Packet->new(\$buf, $self->{'debug'});
			if (defined $ans) {
				$self->errorstring($ans->header->rcode);
				$ans->answerfrom($self->answerfrom);

				if ($ans->header->rcode ne "NOERROR" &&
				    $ans->header->rcode ne "NXDOMAIN"){
					# Remove this one from the stack
					print "RCODE: ".$ans->header->rcode ."; trying next nameserver\n" if $self->{'debug'};
					$lastanswer=$ans;
					next NAMESERVER ;
					
				}

			}
			elsif (defined $err) {
				$self->errorstring($err);
			}

			return $ans;
		}
		else {
			$self->errorstring('timeout');
			next;
		}
	}

	if ($lastanswer){
		$self->errorstring($lastanswer->header->rcode );
		return $lastanswer;

	}

	return;
}



sub send_udp {
	my ($self, $packet, $packet_data) = @_;
	my $retrans = $self->{'retrans'};
	my $timeout = $retrans;
	
	my $lastanswer;

	my $stop_time = time + $self->{'udp_timeout'} if $self->{'udp_timeout'};
	
	$self->_reset_errorstring;
	
 	my @ns;
  	my $dstport = $self->{'port'};
  	my $srcport = $self->{'srcport'};
  	my $srcaddr = $self->{'srcaddr'};
	
 	my @sock;
	
	
 	if ($self->persistent_udp){
 	    if ($has_inet6){
 		if ( defined ($self->{'sockets'}[AF_INET6()]{'UDP'})) {
 		    $sock[AF_INET6()] = $self->{'sockets'}[AF_INET6()]{'UDP'};
 		    print ";; using persistent AF_INET6() family type socket\n"
			if $self->{'debug'};
 		}
 	    }
 	    if ( defined ($self->{'sockets'}[AF_INET]{'UDP'})) {
 		$sock[AF_INET] = $self->{'sockets'}[AF_INET]{'UDP'};
 		print ";; using persistent AF_INET() family type socket\n"
 		    if $self->{'debug'};
 	    }
	}
	
	if ($has_inet6  && ! $self->force_v4() && !defined( $sock[AF_INET6()] )){


	    # '::' Otherwise the INET6 socket will fail.
	    
            my $srcaddr6 = $srcaddr eq '0.0.0.0' ? '::' : $srcaddr;
	    
	    print ";; Trying to set up a AF_INET6() family type UDP socket with srcaddr: $srcaddr ... "
		if $self->{'debug'};

	    
	    # IO::Socket carps on errors if Perl's -w flag is turned on.
	    # Uncomment the next two lines and the line following the "new"
	    # call to turn off these messages.
	    
	    #my $old_wflag = $^W;
	    #$^W = 0;
	    
	    $sock[AF_INET6()] = IO::Socket::INET6->new(
						       LocalAddr => $srcaddr6,
						       LocalPort => ($srcport || undef),
						       Proto     => 'udp',
						       );
	    



	    print (defined($sock[AF_INET6()])?"done\n":"failed\n") if $has_inet6 && $self->debug();

	}
	
	# Always set up an AF_INET socket. 
	# It will be used if the address familly of for the endpoint is V4.

	if (!defined( $sock[AF_INET]))

	{
	    print ";; setting up an AF_INET() family type UDP socket\n"
		if $self->{'debug'};
	    
	    #my $old_wflag = $^W;
	    #$^W = 0;
	    
 	    $sock[AF_INET] = IO::Socket::INET->new(
 						   LocalAddr => $srcaddr,
 						   LocalPort => ($srcport || undef),
 						   Proto     => 'udp',
 						   ) ;
 	    
 	    #$^W = $old_wflag;
	}
	


	unless (defined $sock[AF_INET] || ($has_inet6 && defined $sock[AF_INET6()])) {

	    $self->errorstring("could not get socket");   #'
	    return;
	}
	
	$self->{'sockets'}[AF_INET]{'UDP'} = $sock[AF_INET] if ($self->persistent_udp) && defined( $sock[AF_INET] );
	$self->{'sockets'}[AF_INET6()]{'UDP'} = $sock[AF_INET6()] if $has_inet6 && ($self->persistent_udp) && defined( $sock[AF_INET6()]) && ! $self->force_v4();

 	# Constructing an array of arrays that contain 3 elements: The
 	# nameserver IP address, its sockaddr and the sockfamily for
 	# which the sockaddr structure is constructed.
	
	my $nmbrnsfailed=0;
      NSADDRESS: foreach my $ns_address ($self->nameservers()){
	  # The logic below determines the $dst_sockaddr.
	  # If getaddrinfo is available that is used for both INET4 and INET6
	  # If getaddrinfo is not avialable (Socket6 failed to load) we revert
	  # to the 'classic mechanism
	  if ($has_inet6  && ! $self->force_v4() ){ 
	      # we can use getaddrinfo
	      no strict 'subs';   # Because of the eval statement in the BEGIN
	      # AI_NUMERICHOST is not available at compile time.
	      # The AI_NUMERICHOST surpresses lookups.
	      
	      my $old_wflag = $^W; 		#circumvent perl -w warnings about 'udp'
	      $^W = 0;
	      


	      my @res = getaddrinfo($ns_address, $dstport, AF_UNSPEC, SOCK_DGRAM, 
				    0, AI_NUMERICHOST);
	      
	      $^W=$old_wflag ;
	      
	      
	      use strict 'subs';
	      
	      my ($sockfamily, $socktype_tmp, 
		  $proto_tmp, $dst_sockaddr, $canonname_tmp) = @res;
	      
	      if (scalar(@res) < 5) {
		  die ("can't resolve \"$ns_address\" to address");
	      }
	      
	      push @ns,[$ns_address,$dst_sockaddr,$sockfamily];
	      
	  }else{
	      next NSADDRESS unless( _ip_is_ipv4($ns_address));
	      my $dst_sockaddr = sockaddr_in($dstport, inet_aton($ns_address));
	      push @ns, [$ns_address,$dst_sockaddr,AF_INET];
	  }
	  
      }

      	unless (@ns) {
	    print "No nameservers" if $self->debug();
	    $self->errorstring('no nameservers');
	    return;
	}

 	my $sel = IO::Select->new() ;
	# We allready tested that one of the two socket exists
	
 	$sel->add($sock[AF_INET]) if defined ($sock[AF_INET]);
 	$sel->add($sock[AF_INET6()]) if $has_inet6 &&  defined ($sock[AF_INET6()]) && ! $self->force_v4();
	

	# Perform each round of retries.
	for (my $i = 0;
	     $i < $self->{'retry'};
	     ++$i, $retrans *= 2, $timeout = int($retrans / (@ns || 1))) {

		$timeout = 1 if ($timeout < 1);
		
		# Try each nameserver.
	      NAMESERVER: foreach my $ns (@ns) {
		  next if defined $ns->[3];
			if ($stop_time) {
				my $now = time;
				if ($stop_time < $now) {
					$self->errorstring('query timed out');
					return;
				}
				if ($timeout > 1 && $timeout > ($stop_time-$now)) {
					$timeout = $stop_time-$now;
				}
			}
			my $nsname = $ns->[0];
			my $nsaddr = $ns->[1];
   	                my $nssockfamily = $ns->[2];

			# If we do not have a socket for the transport
			# we are supposed to reach the namserver on we
			# should skip it.
			unless (defined ($sock[ $nssockfamily ])){
			    print "Send error: cannot reach $nsname (".

				( ($has_inet6 && $nssockfamily == AF_INET6()) ? "IPv6" : "" ).
				( ($nssockfamily == AF_INET) ? "IPv4" : "" ).
				") not available"
				if $self->debug();


			    $self->errorstring("Send error: cannot reach $nsname (" .
					       ( ($has_inet6 && $nssockfamily == AF_INET6()) ? "IPv6" : "" ).
					       ( ($nssockfamily == AF_INET) ? "IPv4" : "" ).
					       ") not available"

);
			    next NAMESERVER ;
			    }

			print ";; send_udp($nsname:$dstport)\n"
				if $self->{'debug'};

			unless ($sock[$nssockfamily]->send($packet_data, 0, $nsaddr)) {
				print ";; send error: $!\n" if $self->{'debug'};
				$self->errorstring("Send error: $!");
				$nmbrnsfailed++;
				$ns->[3]="Send error".$self->errorstring();
				next;
			}

			# See ticket 11931 but this works not quite yet
			my $oldpacket_timeout=time+$timeout;
			until ( $oldpacket_timeout && ($oldpacket_timeout < time())) {
			    my @ready = $sel->can_read($timeout);
			  SELECTOR: foreach my $ready (@ready) {
			      my $buf = '';
			      
			      if ($ready->recv($buf, $self->_packetsz)) {
				  
				  $self->answerfrom($ready->peerhost);
				  
				  print ';; answer from ',
				  $ready->peerhost, ':',
				  $ready->peerport, ' : ',
				  length($buf), " bytes\n"
				      if $self->{'debug'};
				  
				  my ($ans, $err) = Net::DNS::Packet->new(\$buf, $self->{'debug'});
				  
				  if (defined $ans) {
				      next SELECTOR unless ( $ans->header->qr || $self->{'ignqrid'});
				      next SELECTOR unless  ( ($ans->header->id == $packet->header->id) || $self->{'ignqrid'} );
				      $self->errorstring($ans->header->rcode);
				      $ans->answerfrom($self->answerfrom);
				      if ($ans->header->rcode ne "NOERROR" &&
					  $ans->header->rcode ne "NXDOMAIN"){
					  # Remove this one from the stack

					  print "RCODE: ".$ans->header->rcode ."; trying next nameserver\n" if $self->{'debug'};
					  $nmbrnsfailed++;
					  $ns->[3]="RCODE: ".$ans->header->rcode();
					  $lastanswer=$ans;
					  next NAMESERVER ;
					  
				      }
				  } elsif (defined $err) {
				      $self->errorstring($err);
				  }
				  return $ans;
			      } else {
				  $self->errorstring($!);
      				  print ';; recv ERROR(',
				  $ready->peerhost, ':',
				  $ready->peerport, '): ',
				  $self->errorstring, "\n"
				      if $self->{'debug'};
				  $ns->[3]="Recv error ".$self->errorstring();
				  $nmbrnsfailed++;
				  # We want to remain in the SELECTOR LOOP...
				  # unless there are no more nameservers
				  return unless ($nmbrnsfailed < @ns);
				  print ';; Number of failed nameservers: $nmbrnsfailed out of '.scalar @ns."\n" if $self->{'debug'};

			      }
			  } #SELECTOR LOOP
			} # until stop_time loop
		    } #NAMESERVER LOOP
		
	}
	
	if ($lastanswer){
		$self->errorstring($lastanswer->header->rcode );
		return $lastanswer;

	}
	if ($sel->handles) {
	    # If there are valid hanndles than we have either a timeout or 
	    # a send error.
	    $self->errorstring('query timed out') unless ($self->errorstring =~ /Send error:/);
	}
	else {
	    if ($nmbrnsfailed < @ns){
		$self->errorstring('Unexpected Error') ;
	    }else{
		$self->errorstring('all nameservers failed');
	    }
	}
	return;
}


sub bgsend {
	my $self = shift;

	unless ($self->nameservers()) {
		$self->errorstring('no nameservers');
		return;
	}

		$self->_reset_errorstring;

	my $packet = $self->make_query_packet(@_);
	my $packet_data = $packet->data;

	my $srcaddr = $self->{'srcaddr'};
	my $srcport = $self->{'srcport'};


	my (@res, $sockfamily, $dst_sockaddr);
	my $ns_address = ($self->nameservers())[0];
	my $dstport = $self->{'port'};


	# The logic below determines ther $dst_sockaddr.
	# If getaddrinfo is available that is used for both INET4 and INET6
	# If getaddrinfo is not avialable (Socket6 failed to load) we revert
	# to the 'classic mechanism
	if ($has_inet6  && ! $self->force_v4()){ 

	    my ( $socktype_tmp, $proto_tmp, $canonname_tmp);

	    no strict 'subs';   # Because of the eval statement in the BEGIN
	                      # AI_NUMERICHOST is not available at compile time.

	    # The AI_NUMERICHOST surpresses lookups.
	    my @res = getaddrinfo($ns_address, $dstport, AF_UNSPEC, SOCK_DGRAM,
				  0 , AI_NUMERICHOST);

	    use strict 'subs';

	    ($sockfamily, $socktype_tmp, 
	     $proto_tmp, $dst_sockaddr, $canonname_tmp) = @res;

	    if (scalar(@res) < 5) {
		die ("can't resolve \"$ns_address\" to address (it could have been an IP address)");
	    }

	}else{
	    $sockfamily=AF_INET;
	    
	    if (! _ip_is_ipv4($ns_address)){
		$self->errorstring("bgsend(ipv4 only):$ns_address does not seem to be a valid IPv4 address");
		return;
	    }

	    $dst_sockaddr = sockaddr_in($dstport, inet_aton($ns_address));
	}
	my @socket;  

	if ($sockfamily == AF_INET) {
	    $socket[$sockfamily] = IO::Socket::INET->new(
							 Proto => 'udp',
							 Type => SOCK_DGRAM,
							 LocalAddr => $srcaddr,
							 LocalPort => ($srcport || undef),
					    );
	} elsif ($has_inet6 && $sockfamily == AF_INET6() ) {
	    # Otherwise the INET6 socket will just fail
	    my $srcaddr6 = $srcaddr eq "0.0.0.0" ? '::' : $srcaddr;
	    $socket[$sockfamily] = IO::Socket::INET6->new(
							  Proto => 'udp',
							  Type => SOCK_DGRAM,
							  LocalAddr => $srcaddr6,
							  LocalPort => ($srcport || undef),
					     );
	} else {
	    die ref($self)." bgsend:Unsoported Socket Family: $sockfamily";
	}
	
	unless (scalar(@socket)) {
		$self->errorstring("could not get socket");   #'
		return;
	}

	print ";; bgsend($ns_address : $dstport)\n" if $self->{'debug'}	;

	foreach my $socket (@socket){
	    next if !defined $socket;
	    
	    unless ($socket->send($packet_data,0,$dst_sockaddr)){
		my $err = $!;
		print ";; send ERROR($ns_address): $err\n" if $self->{'debug'};
		
		$self->errorstring("Send: ".$err);
		return;
	    }
	    return $socket;
	}
	$self->errorstring("Could not find a socket to send on");
	return;
	    
}

sub bgread {
	my ($self, $sock) = @_;

	my $buf = '';

	my $peeraddr = $sock->recv($buf, $self->_packetsz);
	
	if ($peeraddr) {
		print ';; answer from ', $sock->peerhost, ':',
		      $sock->peerport, ' : ', length($buf), " bytes\n"
			if $self->{'debug'};

		my ($ans, $err) = Net::DNS::Packet->new(\$buf, $self->{'debug'});
		
		if (defined $ans) {
			$self->errorstring($ans->header->rcode);
			$ans->answerfrom($sock->peerhost);
		} elsif (defined $err) {
			$self->errorstring($err);
		}
		
		return $ans;
	} else {
		$self->errorstring($!);
		return;
	}
}

sub bgisready {
	my $self = shift;
	my $sel = IO::Select->new(@_);
	my @ready = $sel->can_read(0.0);
	return @ready > 0;
}

sub make_query_packet {
	my $self = shift;
	my $packet;

	if (ref($_[0]) and $_[0]->isa('Net::DNS::Packet')) {
		$packet = shift;
	} else {
		$packet = Net::DNS::Packet->new(@_);
	}

	if ($packet->header->opcode eq 'QUERY') {
		$packet->header->rd($self->{'recurse'});
	}

    if ($self->{'dnssec'}) {
	    # RFC 3225
    	print ";; Adding EDNS extention with UDP packetsize $self->{'udppacketsize'} and DNS OK bit set\n" 
    		if $self->{'debug'};
    	
    	my $optrr = Net::DNS::RR->new(
						Type         => 'OPT',
						Name         => '',
						Class        => $self->{'udppacketsize'},  # Decimal UDPpayload
						ednsflags    => 0x8000, # first bit set see RFC 3225 
				   );

	
	    $packet->push('additional', $optrr) unless defined  $packet->{'optadded'} ;
	    $packet->{'optadded'}=1;
	} elsif ($self->{'udppacketsize'} > Net::DNS::PACKETSZ()) {
	    print ";; Adding EDNS extention with UDP packetsize  $self->{'udppacketsize'}.\n" if $self->{'debug'};
	    # RFC 3225
	    my $optrr = Net::DNS::RR->new( 
						Type         => 'OPT',
						Name         => '',
						Class        => $self->{'udppacketsize'},  # Decimal UDPpayload
						TTL          => 0x0000 # RCODE 32bit Hex
				    );
				    
	    $packet->push('additional', $optrr) unless defined  $packet->{'optadded'} ;
	    $packet->{'optadded'}=1;
	}
	

	if ($self->{'tsig_rr'}) {
		if (!grep { $_->type eq 'TSIG' } $packet->additional) {
			$packet->push('additional', $self->{'tsig_rr'});
		}
	}

	return $packet;
}

sub axfr {
	my $self = shift;
	my @zone;

	if ($self->axfr_start(@_)) {
		my ($rr, $err);
		while (($rr, $err) = $self->axfr_next, $rr && !$err) {
			push @zone, $rr;
		}
		@zone = () if $err;
	}

	return @zone;
}

sub axfr_old {
	croak "Use of Net::DNS::Resolver::axfr_old() is deprecated, use axfr() or axfr_start().";
}


sub axfr_start {
	my $self = shift;
	my ($dname, $class) = @_;
	$dname ||= $self->{'searchlist'}->[0];
	$class ||= 'IN';
	my $timeout = $self->{'tcp_timeout'};

	unless ($dname) {
		print ";; ERROR: axfr: no zone specified\n" if $self->{'debug'};
		$self->errorstring('no zone');
		return;
	}


	print ";; axfr_start($dname, $class)\n" if $self->{'debug'};

	unless ($self->nameservers()) {
		$self->errorstring('no nameservers');
		print ";; ERROR: no nameservers\n" if $self->{'debug'};
		return;
	}

	my $packet = $self->make_query_packet($dname, 'AXFR', $class);
	my $packet_data = $packet->data;

	my $ns = ($self->nameservers())[0];


	my $srcport = $self->{'srcport'};
	my $srcaddr = $self->{'srcaddr'};
	my $dstport = $self->{'port'};

	print ";; axfr_start nameserver = $ns\n" if $self->{'debug'};
	print ";; axfr_start srcport: $srcport, srcaddr: $srcaddr, dstport: $dstport\n" if $self->{'debug'};


	my $sock;
	my $sock_key = "$ns:$self->{'port'}";

	
	if ($self->persistent_tcp && $self->{'axfr_sockets'}[AF_UNSPEC]{$sock_key}) {
		$sock = $self->{'axfr_sockets'}[AF_UNSPEC]{$sock_key};
		print ";; using persistent socket\n"
		    if $self->{'debug'};
	} else {
		$sock=$self->_create_tcp_socket($ns);

		return unless ($sock);  # all error messages 
		                        # are set by _create_tcp_socket


		$self->{'axfr_sockets'}[AF_UNSPEC]{$sock_key} = $sock if 
		    $self->persistent_tcp;
	}
	
	my $lenmsg = pack('n', length($packet_data));

	unless ($sock->send($lenmsg)) {
		$self->errorstring($!);
		return;
	}

	unless ($sock->send($packet_data)) {
		$self->errorstring($!);
		return;
	}

	my $sel = IO::Select->new($sock);

	$self->{'axfr_sel'}       = $sel;
	$self->{'axfr_rr'}        = [];
	$self->{'axfr_soa_count'} = 0;

	return $sock;
}


sub axfr_next {
	my $self = shift;
	my $err  = '';
	
	unless (@{$self->{'axfr_rr'}}) {
		unless ($self->{'axfr_sel'}) {
			my $err = 'no zone transfer in progress';
			
			print ";; $err\n" if $self->{'debug'};
			$self->errorstring($err);
					
			return wantarray ? (undef, $err) : undef;
		}

		my $sel = $self->{'axfr_sel'};
		my $timeout = $self->{'tcp_timeout'};

		#--------------------------------------------------------------
		# Read the length of the response packet.
		#--------------------------------------------------------------

		my @ready = $sel->can_read($timeout);
		unless (@ready) {
			$err = 'timeout';
			$self->errorstring($err);
			return wantarray ? (undef, $err) : undef;
		}

		my $buf = read_tcp($ready[0], Net::DNS::INT16SZ(), $self->{'debug'});
		unless (length $buf) {
			$err = 'truncated zone transfer';
			$self->errorstring($err);
			return wantarray ? (undef, $err) : undef;
		}

		my ($len) = unpack('n', $buf);
		unless ($len) {
			$err = 'truncated zone transfer';
			$self->errorstring($err);
			return wantarray ? (undef, $err) : undef;
		}

		#--------------------------------------------------------------
		# Read the response packet.
		#--------------------------------------------------------------

		@ready = $sel->can_read($timeout);
		unless (@ready) {
			$err = 'timeout';
			$self->errorstring($err);
			return wantarray ? (undef, $err) : undef;
		}

		$buf = read_tcp($ready[0], $len, $self->{'debug'});

		print ';; received ', length($buf), " bytes\n"
			if $self->{'debug'};

		unless (length($buf) == $len) {
			$err = "expected $len bytes, received " . length($buf);
			$self->errorstring($err);
			print ";; $err\n" if $self->{'debug'};
			return wantarray ? (undef, $err) : undef;
		}

		my $ans;
		($ans, $err) = Net::DNS::Packet->new(\$buf, $self->{'debug'});

		if ($ans) {
			if ($ans->header->rcode ne 'NOERROR') {	
				$self->errorstring('Response code from server: ' . $ans->header->rcode);
				print ';; Response code from server: ' . $ans->header->rcode . "\n" if $self->{'debug'};
				return wantarray ? (undef, $err) : undef;
			}
			if ($ans->header->ancount < 1) {
				$err = 'truncated zone transfer';
				$self->errorstring($err);
				print ";; $err\n" if $self->{'debug'};
				return wantarray ? (undef, $err) : undef;
			}
		}
		else {
			$err ||= 'unknown error during packet parsing';
			$self->errorstring($err);
			print ";; $err\n" if $self->{'debug'};
			return wantarray ? (undef, $err) : undef;
		}

		foreach my $rr ($ans->answer) {
			if ($rr->type eq 'SOA') {
				if (++$self->{'axfr_soa_count'} < 2) {
					push @{$self->{'axfr_rr'}}, $rr;
				}
			}
			else {
				push @{$self->{'axfr_rr'}}, $rr;
			}
		}

		if ($self->{'axfr_soa_count'} >= 2) {
			$self->{'axfr_sel'} = undef;
			# we need to mark the transfer as over if the responce was in 
			# many answers.  Otherwise, the user will call axfr_next again
			# and that will cause a 'no transfer in progress' error.
			push(@{$self->{'axfr_rr'}}, undef);
		}
	}

	my $rr = shift @{$self->{'axfr_rr'}};

	return wantarray ? ($rr, undef) : $rr;
}




sub dnssec {
    my ($self, $new_val) = @_;
    if (defined $new_val) {
	$self->{"dnssec"} = $new_val;
	# Setting the udppacket size to some higher default
	$self->udppacketsize(2048) if $new_val;
    }
    
    Carp::carp ("You called the Net::DNS::Resolver::dnssec() method but do not have Net::DNS::SEC installed") if $self->{"dnssec"} && ! $Net::DNS::DNSSEC;
    return $self->{"dnssec"};
};



sub tsig {
	my $self = shift;

	if (@_ == 1) {
		if ($_[0] && ref($_[0])) {
			$self->{'tsig_rr'} = $_[0];
		}
		else {
			$self->{'tsig_rr'} = undef;
		}
	}
	elsif (@_ == 2) {
		my ($key_name, $key) = @_;
		$self->{'tsig_rr'} = Net::DNS::RR->new("$key_name TSIG $key");
	}

	return $self->{'tsig_rr'};
}

#
# Usage:  $data = read_tcp($socket, $nbytes, $debug);
#
sub read_tcp {
	my ($sock, $nbytes, $debug) = @_;
	my $buf = '';

	while (length($buf) < $nbytes) {
		my $nread = $nbytes - length($buf);
		my $read_buf = '';

		print ";; read_tcp: expecting $nread bytes\n" if $debug;

		# During some of my tests recv() returned undef even
		# though there wasn't an error.  Checking for the amount
		# of data read appears to work around that problem.

		unless ($sock->recv($read_buf, $nread)) {
			if (length($read_buf) < 1) {
				my $errstr = $!;

				print ";; ERROR: read_tcp: recv failed: $!\n"
					if $debug;

				if ($errstr eq 'Resource temporarily unavailable') {
					warn "ERROR: read_tcp: recv failed: $errstr\n";
					warn "ERROR: try setting \$res->timeout(undef)\n";
				}

				last;
			}
		}

		print ';; read_tcp: received ', length($read_buf), " bytes\n"
			if $debug;

		last unless length($read_buf);
		$buf .= $read_buf;
	}

	return $buf;
}



sub _create_tcp_socket {
	my $self=shift;
	my $ns=shift;
	my $sock;

	my $srcport = $self->{'srcport'};
	my $srcaddr = $self->{'srcaddr'};
	my $dstport = $self->{'port'};

	my $timeout = $self->{'tcp_timeout'};
	# IO::Socket carps on errors if Perl's -w flag is
	# turned on.  Uncomment the next two lines and the
	# line following the "new" call to turn off these
	# messages.
	
	#my $old_wflag = $^W;
	#$^W = 0;
	
	if ($has_inet6 && ! $self->force_v4() && _ip_is_ipv6($ns) ){
		# XXX IO::Socket::INET6 fails in a cryptic way upon send()
		# on AIX5L if "0" is passed in as LocalAddr
		# $srcaddr="0" if $srcaddr eq "0.0.0.0";  # Otherwise the INET6 socket will just fail
		
		my $srcaddr6 = $srcaddr eq '0.0.0.0' ? '::' : $srcaddr;
		
		$sock = 
		    IO::Socket::INET6->new(
					   PeerPort =>    $dstport,
					   PeerAddr =>    $ns,
					   LocalAddr => $srcaddr6,
					   LocalPort => ($srcport || undef),
					   Proto     => 'tcp',
					   Timeout   => $timeout,
					   );
		
		unless($sock){
			$self->errorstring('connection failed(IPv6 socket failure)');
			print ";; ERROR: send_tcp: IPv6 connection to $ns".
			    "failed: $!\n" if $self->{'debug'};
			return();
		}
	}
	
	# At this point we have sucessfully obtained an
	# INET6 socket to an IPv6 nameserver, or we are
	# running forced v4, or we do not have v6 at all.
	# Try v4.
	
	unless($sock){
		if (_ip_is_ipv6($ns)){
			$self->errorstring(
					   'connection failed (trying IPv6 nameserver without having IPv6)');
			print 
			    ';; ERROR: send_tcp: You are trying to connect to '.
			    $ns . " but you do not have IPv6 available\n"
			    if $self->{'debug'};
			return();
		}		    
		
		
		$sock = IO::Socket::INET->new(
					      PeerAddr  => $ns,
					      PeerPort  => $dstport,
					      LocalAddr => $srcaddr,
					      LocalPort => ($srcport || undef),
					      Proto     => 'tcp',
					      Timeout   => $timeout
					      )
	    }
	
	#$^W = $old_wflag;
	
	unless ($sock) {
		$self->errorstring('connection failed');
		print ';; ERROR: send_tcp: connection ',
		"failed: $!\n" if $self->{'debug'};
		return();
	}

	return $sock;
}


# Lightweight versions of subroutines from Net::IP module, recoded to fix rt#28198

sub _ip_is_ipv4 {
	my @field = split /\./, shift;

	return 0 if @field > 4;				# too many fields
	return 0 if @field == 0;			# no fields at all

	foreach ( @field ) {
		return 0 unless /./;			# reject if empty
		return 0 if /[^0-9]/;			# reject non-digit
		return 0 if $_ > 255;			# reject bad value
	}


	return 1;
}


sub _ip_is_ipv6 {

	for ( shift ) {
		my @field = split /:/;			# split into fields
		return 0 if (@field < 3) or (@field > 8);

		return 0 if /::.*::/;			# reject multiple ::

		if ( /\./ ) {				# IPv6:IPv4
			return 0 unless _ip_is_ipv4(pop @field);
		}

		foreach ( @field ) {
			next unless /./;		# skip ::
			return 0 if /[^0-9a-f]/i;	# reject non-hexdigit
			return 0 if length $_ > 4;	# reject bad value
		}
	}
	return 1;
}



sub AUTOLOAD {
	my ($self) = @_;

	my $name = $AUTOLOAD;
	$name =~ s/.*://;

	Carp::croak "$name: no such method" unless exists $self->{$name};
	
	no strict q/refs/;
	
	
	*{$AUTOLOAD} = sub {
		my ($self, $new_val) = @_;
		
		if (defined $new_val) {
			$self->{"$name"} = $new_val;
		}
		
		return $self->{"$name"};
	};

	
	goto &{$AUTOLOAD};	
}

1;

__END__

=head1 NAME

Net::DNS::Resolver::Base - Common Resolver Class

=head1 SYNOPSIS

 use base qw/Net::DNS::Resolver::Base/;

=head1 DESCRIPTION

This class is the common base class for the different platform
sub-classes of L<Net::DNS::Resolver|Net::DNS::Resolver>.  

No user serviceable parts inside, see L<Net::DNS::Resolver|Net::DNS::Resolver>
for all your resolving needs.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.
Portions Copyright (c) 2005 Olaf Kolkman  <olaf@net-dns.org>
Portions Copyright (c) 2006 Dick Franks.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>

=cut


