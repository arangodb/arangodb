package Net::DNS::Resolver::Recurse;
#
# $Id: Recurse.pm 591 2006-05-22 21:32:38Z olaf $
#
use strict;
use Net::DNS::Resolver;

use vars qw($VERSION @ISA);

$VERSION = (qw$LastChangedRevision: 591 $)[1];
@ISA = qw(Net::DNS::Resolver);

sub hints {
  my $self = shift;
  my @hints = @_;
  print ";; hints(@hints)\n" if $self->{'debug'};
  if (!@hints && $self->nameservers) {
    $self->hints($self->nameservers);
  } else {
    $self->nameservers(@hints);
  }

  print ";; verifying (root) zone...\n" if $self->{'debug'};
  # bind always asks one of the hint servers
  # for who it thinks is authoritative for
  # the (root) zone as a sanity check.
  # Nice idea.
  
  $self->recurse(1); 
  my $packet=$self->query(".", "NS", "IN");
  $self->recurse(0); 
  my %hints = ();
  if ($packet) {
    if (my @ans = $packet->answer) {
      foreach my $rr (@ans) {
        if ($rr->name =~ /^\.?$/ and
            $rr->type eq "NS") {
          # Found root authority
          my $server = lc $rr->rdatastr;
          $server =~ s/\.$//;
          print ";; FOUND HINT: $server\n" if $self->{'debug'};
          $hints{$server} = [];
        }
      }
      foreach my $rr ($packet->additional) {
	print ";; ADDITIONAL: ",$rr->string,"\n" if $self->{'debug'};
	if (my $server = lc $rr->name){
	  if ( $rr->type eq "A") {
	    #print ";; ADDITIONAL HELP: $server -> [".$rr->rdatastr."]\n" if $self->{'debug'};
	    if ($hints{$server}) {
	      print ";; STORING IP: $server IN A ",$rr->rdatastr,"\n" if $self->{'debug'};
	      push @{ $hints{$server} }, $rr->rdatastr;
	    }
	  }
	  if ( $rr->type eq "AAAA") {
	    #print ";; ADDITIONAL HELP: $server -> [".$rr->rdatastr."]\n" if $self->{'debug'};
	    if ($hints{$server}) {
	      print ";; STORING IP6: $server IN AAAA ",$rr->rdatastr,"\n" if $self->{'debug'};
	      push @{ $hints{$server} }, $rr->rdatastr;
	    }
	  }
	  
	} 
      }
    }
    foreach my $server (keys %hints) {
      if (!@{ $hints{$server} }) {
	# Wipe the servers without lookups
	delete $hints{$server};
      }
    }
    $self->{'hints'} = \%hints;
  } else {
    $self->{'hints'} = {};
  } 
  if (%{ $self->{'hints'} }) {
    if ($self->{'debug'}) {
      print ";; USING THE FOLLOWING HINT IPS:\n";
      foreach my $ips (values %{ $self->{'hints'} }) {
	foreach my $server (@{ $ips }) {
	  print ";;  $server\n";
	}
      }
    }
  } else {
    warn "Server [".($self->nameservers)[0]."] did not give answers";
  }
  
  # Disable recursion flag.

  
  return $self->nameservers( map { @{ $_ } } values %{ $self->{'hints'} } );
}


sub recursion_callback {
	my ($self, $sub) = @_;
	
	if ($sub && UNIVERSAL::isa($sub, 'CODE')) {
		$self->{'callback'} = $sub;
	}
	
	return $self->{'callback'};
}


# $res->query_dorecursion( args );
# Takes same args as Net::DNS::Resolver->query
# Purpose: Do that "hot pototo dance" on args.
sub query_dorecursion {
  my $self = shift;
  my @query = @_;

  # Make sure the hint servers are initialized.
  $self->hints unless $self->{'hints'};
  $self->recurse(0);
  # Make sure the authority cache is clean.
  # It is only used to store A and AAAA records of
  # the suposedly authoritative name servers.
  $self->{'authority_cache'} = {};

  # Obtain real question Net::DNS::Packet
  my $query_packet = $self->make_query_packet(@query);

  # Seed name servers with hints
  return $self->_dorecursion( $query_packet, ".", $self->{'hints'}, 0);
}

sub _dorecursion {
  my $self = shift;
  my $query_packet = shift;
  my $known_zone = shift;
  my $known_authorities = shift;
  my $depth = shift;
  my $cache = $self->{'authority_cache'};

  # die "Recursion too deep, aborting..." if $depth > 255;
  if ( $depth > 255 ) {
      print ";; _dorecursion() Recursion too deep, aborting...\n" if
	  $self->{'debug'};
      $self->errorstring="Recursion to deep, abborted";
      return undef;
  }
  
  $known_zone =~ s/\.*$/./;

  # Get IPs from authorities
  my @ns = ();
  foreach my $ns (keys %{ $known_authorities }) {
    if (scalar @{ $known_authorities->{$ns} }) {
      $cache->{$ns} = $known_authorities->{$ns};
      push (@ns, @{ $cache->{$ns} });
    } elsif ($cache->{$ns}) {
      $known_authorities->{$ns} = $cache->{$ns};
      push (@ns, @{ $cache->{$ns} });
    }
  }

  if (!@ns) {
    my $found_auth = 0;
    if ($self->{'debug'}) {
      require Data::Dumper;
      print ";; _dorecursion() Failed to extract nameserver IPs:\n";
      print Data::Dumper::Dumper([$known_authorities,$cache]);
    }
    foreach my $ns (keys %{ $known_authorities }) {
      if (!@{ $known_authorities->{$ns} }) {
        print ";; _dorecursion() Manual lookup for authority [$ns]\n" if $self->{'debug'};

        my $auth_packet;
	my @ans;

	# Don't query for V6 if its not there.
	if ($Net::DNS::Resolver::Base::has_inet6 && ! $self->{force_v4}){
	    $auth_packet =
		$self->_dorecursion
		($self->make_query_packet($ns,"AAAA"),  # packet
		 ".",               # known_zone
		 $self->{'hints'},  # known_authorities
		 $depth+1);         # depth
	    @ans = $auth_packet->answer if $auth_packet;
	}
	
	$auth_packet =
	    $self->_dorecursion
	    ($self->make_query_packet($ns,"A"),  # packet
	     ".",               # known_zone
	     $self->{'hints'},  # known_authorities
	     $depth+1);         # depth
	
	push (@ans,$auth_packet->answer ) if $auth_packet;

        if ( @ans ) {
          print ";; _dorecursion() Answers found for [$ns]\n" if $self->{'debug'};
          foreach my $rr (@ans) {
	    print ";; RR:".$rr->string."\n" if $self->{'debug'};
            if ($rr->type eq "CNAME") {
              # Follow CNAME
              if (my $server = lc $rr->name) {
                $server =~ s/\.*$/./;
                if ($server eq $ns) {
                  my $cname = lc $rr->rdatastr;
                  $cname =~ s/\.*$/./;
                  print ";; _dorecursion() Following CNAME ns [$ns] -> [$cname]\n" if $self->{'debug'};
                  $known_authorities->{$cname} ||= [];
                  delete $known_authorities->{$ns};
                  next;
                }
              }
            } elsif ($rr->type eq "A" ||$rr->type eq "AAAA" ) {
              if (my $server = lc $rr->name) {
                $server =~ s/\.*$/./;
                if ($known_authorities->{$server}) {
                  my $ip = $rr->rdatastr;
                  print ";; _dorecursion() Found ns: $server IN A $ip\n" if $self->{'debug'};
                  $cache->{$server} = $known_authorities->{$server};
                  push (@{ $cache->{$ns} }, $ip);
                  $found_auth++;
                  next;
                }
              }
            }
            print ";; _dorecursion() Ignoring useless answer: ",$rr->string,"\n" if $self->{'debug'};
          }
        } else {
          print ";; _dorecursion() Could not find A records for [$ns]\n" if $self->{'debug'};
        }
      }
    }
    if ($found_auth) {
      print ";; _dorecursion() Found $found_auth new NS authorities...\n" if $self->{'debug'};
      return $self->_dorecursion( $query_packet, $known_zone, $known_authorities, $depth+1);
    }
    print ";; _dorecursion() No authority information could be obtained.\n" if $self->{'debug'};
    return undef;
  }
  
  # Cut the deck of IPs in a random place.
  print ";; _dorecursion() cutting deck of (".scalar(@ns).") authorities...\n" if $self->{'debug'};
  splice(@ns, 0, 0, splice(@ns, int(rand @ns)));
  
  
 LEVEL:  foreach my $levelns (@ns){
   print ";; _dorecursion() Trying nameserver [$levelns]\n" if $self->{'debug'};
   $self->nameservers($levelns);
   
   if (my $packet = $self->send( $query_packet )) {
     
     if ($self->{'callback'}) {
       $self->{'callback'}->($packet);
     }
     
     my $of = undef;
     print ";; _dorecursion() Response received from [",$self->answerfrom,"]\n" if $self->{'debug'};
     if (my $status = $packet->header->rcode) {
       if ($status eq "NXDOMAIN") {
	 # I guess NXDOMAIN is the best we'll ever get
	 print ";; _dorecursion() returning NXDOMAIN\n" if $self->{'debug'};
	 return $packet;
       } elsif (my @ans = $packet->answer) {
	 print ";; _dorecursion() Answers were found.\n" if $self->{'debug'};
	 return $packet;
       } elsif (my @authority = $packet->authority) {
	 my %auth = ();
	 foreach my $rr (@authority) {
	   if ($rr->type =~ /^(NS|SOA)$/) {
	     my $server = lc ($1 eq "NS" ? $rr->nsdname : $rr->mname);
	     $server =~ s/\.*$/./;
	     $of = lc $rr->name;
	     $of =~ s/\.*$/./;
	     print ";; _dorecursion() Received authority [$of] [",$rr->type(),"] [$server]\n" if $self->{'debug'};
	     if (length $of <= length $known_zone) {
	       print ";; _dorecursion() Deadbeat name server did not provide new information.\n" if $self->{'debug'};
	       next LEVEL;
	     } elsif ($of =~ /$known_zone$/) {
	       print ";; _dorecursion() FOUND closer authority for [$of] at [$server].\n" if $self->{'debug'};
	       $auth{$server} ||= [];
	     } else {
	       print ";; _dorecursion() Confused name server [",$self->answerfrom,"] thinks [$of] is closer than [$known_zone]?\n" if $self->{'debug'};
	       last;
	     }
	   } else {
	     print ";; _dorecursion() Ignoring NON NS entry found in authority section: ",$rr->string,"\n" if $self->{'debug'};
	   }
	 }
	 foreach my $rr ($packet->additional) {
	   if ($rr->type eq "CNAME") {
	     # Store this CNAME into %auth too
	     if (my $server = lc $rr->name) {
	       $server =~ s/\.*$/./;
	       if ($auth{$server}) {
		 my $cname = lc $rr->rdatastr;
		 $cname =~ s/\.*$/./;
		 print ";; _dorecursion() FOUND CNAME authority: ",$rr->string,"\n" if $self->{'debug'};
		 $auth{$cname} ||= [];
		 $auth{$server} = $auth{$cname};
		 next;
	       }
	     }
	   } elsif ($rr->type eq "A" || $rr->type eq "AAAA") {
	     if (my $server = lc $rr->name) {
	       $server =~ s/\.*$/./;
	       if ($auth{$server}) {
		 print ";; _dorecursion() STORING: $server IN A    ",$rr->rdatastr,"\n" if $self->{'debug'} &&  $rr->type eq "A";
		 print ";; _dorecursion() STORING: $server IN AAAA ",$rr->rdatastr,"\n" if $self->{'debug'}&&  $rr->type eq "AAAA";
		 push @{ $auth{$server} }, $rr->rdatastr;
		 next;
	       }
	     }
	   }
	   print ";; _dorecursion() Ignoring useless: ",$rr->string,"\n" if $self->{'debug'};
	 }
	 if ($of =~ /$known_zone$/) {
	   return $self->_dorecursion( $query_packet, $of, \%auth, $depth+1 );
	 } else {
	   return $self->_dorecursion( $query_packet, $known_zone, $known_authorities, $depth+1 );
	 }
      }
     }
   }
 }
  
  return undef;
}

1;

__END__


=head1 NAME

Net::DNS::Resolver::Recurse - Perform recursive dns lookups

=head1 SYNOPSIS

  use Net::DNS::Resolver::Recurse;
  my $res = Net::DNS::Resolver::Recurse->new;

=head1 DESCRIPTION

This module is a sub class of Net::DNS::Resolver. So the methods for
Net::DNS::Resolver still work for this module as well.  There are just a
couple methods added:

=head2 hints

Initialize the hint servers.  Recursive queries need a starting name
server to work off of. This method takes a list of IP addresses to use
as the starting servers.  These name servers should be authoritative for
the root (.) zone.

  $res->hints(@ips);

If no hints are passed, the default nameserver is asked for the hints. 
Normally these IPs can be obtained from the following location:

  ftp://ftp.internic.net/domain/named.root
  
=head2 recursion_callback

This method is takes a code reference, which is then invoked each time a
packet is received during the recursive lookup.  For example to emulate
dig's C<+trace> function:

 $res->recursion_callback(sub {
     my $packet = shift;
		
     $_->print for $packet->additional;
		
     printf(";; Received %d bytes from %s\n\n", 
         $packet->answersize, 
         $packet->answerfrom
     );
 });

=head2 query_dorecursion

This method is much like the normal query() method except it disables
the recurse flag in the packet and explicitly performs the recursion.

  $packet = $res->query_dorecursion( "www.netscape.com.", "A");


=head1 IPv6 transport

If the appropriate IPv6 libraries are installed the recursive resolver
will randomly choose between IPv6 and IPv4 addresses of the
nameservers it encounters during recursion.

If you want to force IPv4 transport use the force_v4() method. Also see
the IPv6 transport notes in the Net::DNS::Resolver documentation.

=head1 AUTHOR

Rob Brown, bbb@cpan.org

=head1 SEE ALSO

L<Net::DNS::Resolver>,

=head1 COPYRIGHT

Copyright (c) 2002, Rob Brown.  All rights reserved.
Portions Copyright (c) 2005, Olaf M Kolkman.

This module is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

$Id: Recurse.pm 591 2006-05-22 21:32:38Z olaf $

=cut

Example lookup process:

[root@box root]# dig +trace www.rob.com.au.

; <<>> DiG 9.2.0 <<>> +trace www.rob.com.au.
;; global options:  printcmd
.                       507343  IN      NS      C.ROOT-SERVERS.NET.
.                       507343  IN      NS      D.ROOT-SERVERS.NET.
.                       507343  IN      NS      E.ROOT-SERVERS.NET.
.                       507343  IN      NS      F.ROOT-SERVERS.NET.
.                       507343  IN      NS      G.ROOT-SERVERS.NET.
.                       507343  IN      NS      H.ROOT-SERVERS.NET.
.                       507343  IN      NS      I.ROOT-SERVERS.NET.
.                       507343  IN      NS      J.ROOT-SERVERS.NET.
.                       507343  IN      NS      K.ROOT-SERVERS.NET.
.                       507343  IN      NS      L.ROOT-SERVERS.NET.
.                       507343  IN      NS      M.ROOT-SERVERS.NET.
.                       507343  IN      NS      A.ROOT-SERVERS.NET.
.                       507343  IN      NS      B.ROOT-SERVERS.NET.
;; Received 436 bytes from 127.0.0.1#53(127.0.0.1) in 9 ms
  ;;; But these should be hard coded as the hints

  ;;; Ask H.ROOT-SERVERS.NET gave:
au.                     172800  IN      NS      NS2.BERKELEY.EDU.
au.                     172800  IN      NS      NS1.BERKELEY.EDU.
au.                     172800  IN      NS      NS.UU.NET.
au.                     172800  IN      NS      BOX2.AUNIC.NET.
au.                     172800  IN      NS      SEC1.APNIC.NET.
au.                     172800  IN      NS      SEC3.APNIC.NET.
;; Received 300 bytes from 128.63.2.53#53(H.ROOT-SERVERS.NET) in 322 ms
  ;;; A little closer than before

  ;;; Ask NS2.BERKELEY.EDU gave:
com.au.                 259200  IN      NS      ns4.ausregistry.net.
com.au.                 259200  IN      NS      dns1.telstra.net.
com.au.                 259200  IN      NS      au2ld.CSIRO.au.
com.au.                 259200  IN      NS      audns01.syd.optus.net.
com.au.                 259200  IN      NS      ns.ripe.net.
com.au.                 259200  IN      NS      ns1.ausregistry.net.
com.au.                 259200  IN      NS      ns2.ausregistry.net.
com.au.                 259200  IN      NS      ns3.ausregistry.net.
com.au.                 259200  IN      NS      ns3.melbourneit.com.
;; Received 387 bytes from 128.32.206.12#53(NS2.BERKELEY.EDU) in 10312 ms
  ;;; A little closer than before

  ;;; Ask ns4.ausregistry.net gave:
com.au.                 259200  IN      NS      ns1.ausregistry.net.
com.au.                 259200  IN      NS      ns2.ausregistry.net.
com.au.                 259200  IN      NS      ns3.ausregistry.net.
com.au.                 259200  IN      NS      ns4.ausregistry.net.
com.au.                 259200  IN      NS      ns3.melbourneit.com.
com.au.                 259200  IN      NS      dns1.telstra.net.
com.au.                 259200  IN      NS      au2ld.CSIRO.au.
com.au.                 259200  IN      NS      ns.ripe.net.
com.au.                 259200  IN      NS      audns01.syd.optus.net.
;; Received 259 bytes from 137.39.1.3#53(ns4.ausregistry.net) in 606 ms
  ;;; Uh... yeah... I already knew this
  ;;; from what NS2.BERKELEY.EDU told me.
  ;;; ns4.ausregistry.net must have brain damage

  ;;; Ask ns1.ausregistry.net gave:
rob.com.au.             86400   IN      NS      sy-dns02.tmns.net.au.
rob.com.au.             86400   IN      NS      sy-dns01.tmns.net.au.
;; Received 87 bytes from 203.18.56.41#53(ns1.ausregistry.net) in 372 ms
  ;;; Ah, much better.  Something more useful.

  ;;; Ask sy-dns02.tmns.net.au gave:
www.rob.com.au.         7200    IN      A       139.134.5.123
rob.com.au.             7200    IN      NS      sy-dns01.tmns.net.au.
rob.com.au.             7200    IN      NS      sy-dns02.tmns.net.au.
;; Received 135 bytes from 139.134.2.18#53(sy-dns02.tmns.net.au) in 525 ms
  ;;; FINALLY, THE ANSWER!
