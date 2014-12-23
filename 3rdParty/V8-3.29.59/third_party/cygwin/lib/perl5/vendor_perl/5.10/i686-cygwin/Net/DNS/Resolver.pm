package Net::DNS::Resolver;
#
# $Id: Resolver.pm 614 2006-09-25 08:12:29Z olaf $
#

use strict;
use vars qw($VERSION @ISA);

$VERSION = (qw$LastChangedRevision: 614 $)[1];

BEGIN {
	if ($^O eq 'MSWin32') {
		require Net::DNS::Resolver::Win32;
		@ISA = qw(Net::DNS::Resolver::Win32);
	} elsif ($^O eq 'cygwin') {
		require Net::DNS::Resolver::Cygwin;
		@ISA = qw(Net::DNS::Resolver::Cygwin);
	} else {   
		require Net::DNS::Resolver::UNIX;
		@ISA = qw(Net::DNS::Resolver::UNIX);
	}
}

__PACKAGE__->init();

1;

__END__

=head1 NAME

Net::DNS::Resolver - DNS resolver class

=head1 SYNOPSIS

  use Net::DNS;
  
  my $res = Net::DNS::Resolver->new;
  
  # Perform a lookup, using the searchlist if appropriate.
  my $answer = $res->search('example.com');
  
  # Perform a lookup, without the searchlist
  my $answer = $res->query('example.com', 'MX');
  
  # Perform a lookup, without pre or post-processing
  my $answer = $res->send('example.com', 'MX', 'CH');
  
  # Send a prebuilt packet
  my $packet = Net::DNS::Packet->new(...);
  my $answer = $res->send($packet);
  
=head1 DESCRIPTION

Instances of the C<Net::DNS::Resolver> class represent resolver objects.
A program can have multiple resolver objects, each maintaining its
own state information such as the nameservers to be queried, whether
recursion is desired, etc.

=head1 METHODS

=head2 new

  # Use the system defaults
  my $res = Net::DNS::Resolver->new;
  
  # Use my own configuration file
  my $res = Net::DNS::Resolver->new(config_file => '/my/dns.conf');
  
  # Set options in the constructor
  my $res = Net::DNS::Resolver->new(
  	nameservers => [qw(10.1.1.128 10.1.2.128)],
  	recurse     => 0,
  	debug       => 1,
  );

Returns a resolver object.  If given no arguments, C<new()> returns an
object configured to your system's defaults.  On UNIX systems the 
defaults are read from the following files, in the order indicated:

    /etc/resolv.conf
    $HOME/.resolv.conf
    ./.resolv.conf

The following keywords are recognized in resolver configuration files:

=over 4

=item domain

The default domain.

=item search

A space-separated list of domains to put in the search list.

=item nameserver

A space-separated list of nameservers to query.

=back

Files except for F</etc/resolv.conf> must be owned by the effective
userid running the program or they won't be read.  In addition, several
environment variables can also contain configuration information; see
L</ENVIRONMENT>.

On Windows systems, an attempt is made to determine the system defaults
using the registry.  This is still a work in progress; systems with many
dynamically configured network interfaces may confuse Net::DNS.

You can include a configuration file of your own when creating a
resolver object:

 # Use my own configuration file 
 my $res = Net::DNS::Resolver->new(config_file => '/my/dns.conf');

This is supported on both UNIX and Windows.  Values pulled from a custom
configuration file override the the system's defaults, but can still be
overridden by the other arguments to new().

Explicit arguments to new override both the system's defaults and the
values of the custom configuration file, if any.  The following
arguments to new() are supported:

=over 4

=item nameservers

An array reference of nameservers to query.  

=item searchlist

An array reference of domains.

=item recurse

=item debug

=item domain

=item port

=item srcaddr

=item srcport

=item tcp_timeout

=item udp_timeout

=item retrans

=item retry

=item usevc

=item stayopen

=item igntc

=item defnames

=item dnsrch

=item persistent_tcp

=item persistent_udp

=item dnssec

=back

For more information on any of these options, please consult the method
of the same name.

=head2 search

    $packet = $res->search('mailhost');
    $packet = $res->search('mailhost.example.com');
    $packet = $res->search('192.168.1.1');
    $packet = $res->search('example.com', 'MX');
    $packet = $res->search('user.passwd.example.com', 'TXT', 'HS');

Performs a DNS query for the given name, applying the searchlist
if appropriate.  The search algorithm is as follows:

=over 4

=item 1.

If the name contains at least one dot, try it as is.

=item 2.

If the name doesn't end in a dot then append each item in
the search list to the name.  This is only done if B<dnsrch>
is true.

=item 3.

If the name doesn't contain any dots, try it as is.

=back

The record type and class can be omitted; they default to A and
IN.  If the name looks like an IP address (4 dot-separated numbers),
then an appropriate PTR query will be performed.

Returns a "Net::DNS::Packet" object, or "undef" if no answers were
found.  If you need to examine the response packet whether it contains
any answers or not, use the send() method instead.

=head2 query

    $packet = $res->query('mailhost');
    $packet = $res->query('mailhost.example.com');
    $packet = $res->query('192.168.1.1');
    $packet = $res->query('example.com', 'MX');
    $packet = $res->query('user.passwd.example.com', 'TXT', 'HS');

Performs a DNS query for the given name; the search list is not
applied.  If the name doesn't contain any dots and B<defnames>
is true then the default domain will be appended.

The record type and class can be omitted; they default to A and
IN.  If the name looks like an IP address (IPv4 or IPv6),
then an appropriate PTR query will be performed.

Returns a "Net::DNS::Packet" object, or "undef" if no answers were
found.  If you need to examine the response packet whether it contains
any answers or not, use the send() method instead.

=head2 send

    $packet = $res->send($packet_object);
    $packet = $res->send('mailhost.example.com');
    $packet = $res->send('example.com', 'MX');
    $packet = $res->send('user.passwd.example.com', 'TXT', 'HS');

Performs a DNS query for the given name.  Neither the searchlist
nor the default domain will be appended.  

The argument list can be either a C<Net::DNS::Packet> object or a list
of strings.  The record type and class can be omitted; they default to
A and IN.  If the name looks like an IP address (Ipv4 or IPv6),
then an appropriate PTR query will be performed.

Returns a C<Net::DNS::Packet> object whether there were any answers or not.
Use C<< $packet->header->ancount >> or C<< $packet->answer >> to find out
if there were any records in the answer section.  Returns C<undef> if there
was an error.

=head2 axfr

    @zone = $res->axfr;
    @zone = $res->axfr('example.com');
    @zone = $res->axfr('passwd.example.com', 'HS');

Performs a zone transfer from the first nameserver listed in C<nameservers>.
If the zone is omitted, it defaults to the first zone listed in the resolver's
search list.  If the class is omitted, it defaults to IN.

Returns a list of C<Net::DNS::RR> objects, or C<undef> if the zone
transfer failed.

The redundant SOA record that terminates the zone transfer is not
returned to the caller.

See also L</axfr_start> and L</axfr_next>.

Here's an example that uses a timeout:

    $res->tcp_timeout(10);
    my @zone = $res->axfr('example.com');

    if (@zone) {
        foreach my $rr (@zone) {
            $rr->print;
        }
    } else {
        print 'Zone transfer failed: ', $res->errorstring, "\n";
    }

=head2 axfr_start

    $res->axfr_start;
    $res->axfr_start('example.com');
    $res->axfr_start('example.com', 'HS');

Starts a zone transfer from the first nameserver listed in C<nameservers>.
If the zone is omitted, it defaults to the first zone listed in the resolver's
search list.  If the class is omitted, it defaults to IN.

B<IMPORTANT>:

This method currently returns the C<IO::Socket::INET> object that will
be used for reading, or C<undef> on error.  DO NOT DEPEND ON C<axfr_start()>
returning a socket object.  THIS MIGHT CHANGE in future releases.

Use C<axfr_next> to read the zone records one at a time.

=head2 axfr_next

    $res->axfr_start('example.com');

    while (my $rr = $res->axfr_next) {
	    $rr->print;
    }

Reads records from a zone transfer one at a time.

Returns C<undef> at the end of the zone transfer.  The redundant
SOA record that terminates the zone transfer is not returned.

See also L</axfr>.

=head2 nameservers

    @nameservers = $res->nameservers;
    $res->nameservers('192.168.1.1', '192.168.2.2', '192.168.3.3');

Gets or sets the nameservers to be queried.

Also see the IPv6 transport notes below    

=head2 print

    $res->print;

Prints the resolver state on the standard output.

=head2 string

    print $res->string;

Returns a string representation of the resolver state.

=head2 searchlist

    @searchlist = $res->searchlist;
    $res->searchlist('example.com', 'a.example.com', 'b.example.com');

Gets or sets the resolver search list.

=head2 port

    print 'sending queries to port ', $res->port, "\n";
    $res->port(9732);

Gets or sets the port to which we send queries.  This can be useful
for testing a nameserver running on a non-standard port.  The
default is port 53.

=head2 srcport

    print 'sending queries from port ', $res->srcport, "\n";
    $res->srcport(5353);

Gets or sets the port from which we send queries.  The default is 0,
meaning any port.

=head2 srcaddr

    print 'sending queries from address ', $res->srcaddr, "\n";
    $res->srcaddr('192.168.1.1');

Gets or sets the source address from which we send queries.  Convenient
for forcing queries out a specific interfaces on a multi-homed host.
The default is 0.0.0.0, meaning any local address.

=head2 bgsend

    $socket = $res->bgsend($packet_object) || die " $res->errorstring";

    $socket = $res->bgsend('mailhost.example.com');
    $socket = $res->bgsend('example.com', 'MX');
    $socket = $res->bgsend('user.passwd.example.com', 'TXT', 'HS');



Performs a background DNS query for the given name, i.e., sends a
query packet to the first nameserver listed in C<< $res->nameservers >>
and returns immediately without waiting for a response.  The program
can then perform other tasks while waiting for a response from the 
nameserver.

The argument list can be either a C<Net::DNS::Packet> object or a list
of strings.  The record type and class can be omitted; they default to
A and IN.  If the name looks like an IP address (4 dot-separated numbers),
then an appropriate PTR query will be performed.

Returns an C<IO::Socket::INET> object or C<undef> on error in which
case the reason for failure can be found through a call to the
errorstring method.

The program must determine when the socket is ready for reading and
call C<< $res->bgread >> to get the response packet.  You can use C<<
$res->bgisready >> or C<IO::Select> to find out if the socket is ready
before reading it.

=head2 bgread

    $packet = $res->bgread($socket);
    undef $socket;

Reads the answer from a background query (see L</bgsend>).  The argument
is an C<IO::Socket> object returned by C<bgsend>.

Returns a C<Net::DNS::Packet> object or C<undef> on error.

The programmer should close or destroy the socket object after reading it.

=head2 bgisready

    $socket = $res->bgsend('foo.example.com');
    until ($res->bgisready($socket)) {
        # do some other processing
    }
    $packet = $res->bgread($socket);
    $socket = undef;

Determines whether a socket is ready for reading.  The argument is
an C<IO::Socket> object returned by C<< $res->bgsend >>.

Returns true if the socket is ready, false if not.

=head2 tsig

    my $tsig = $res->tsig;

    $res->tsig(Net::DNS::RR->new("$key_name TSIG $key"));

    $tsig = Net::DNS::RR->new("$key_name TSIG $key");
    $tsig->fudge(60);
    $res->tsig($tsig);

    $res->tsig($key_name, $key);

    $res->tsig(0);

Get or set the TSIG record used to automatically sign outgoing
queries and updates.  Call with an argument of 0 or '' to turn off
automatic signing.

The default resolver behavior is not to sign any packets.  You must
call this method to set the key if you'd like the resolver to sign
packets automatically.

You can also sign packets manually -- see the C<Net::DNS::Packet>
and C<Net::DNS::Update> manual pages for examples.  TSIG records
in manually-signed packets take precedence over those that the
resolver would add automatically.

=head2 retrans

    print 'retrans interval: ', $res->retrans, "\n";
    $res->retrans(3);

Get or set the retransmission interval.  The default is 5.

=head2 retry

    print 'number of tries: ', $res->retry, "\n";
    $res->retry(2);

Get or set the number of times to try the query.  The default is 4.

=head2 recurse

    print 'recursion flag: ', $res->recurse, "\n";
    $res->recurse(0);

Get or set the recursion flag.  If this is true, nameservers will
be requested to perform a recursive query.  The default is true.

=head2 defnames

    print 'defnames flag: ', $res->defnames, "\n";
    $res->defnames(0);

Get or set the defnames flag.  If this is true, calls to B<query> will
append the default domain to names that contain no dots.  The default
is true.

=head2 dnsrch

    print 'dnsrch flag: ', $res->dnsrch, "\n";
    $res->dnsrch(0);

Get or set the dnsrch flag.  If this is true, calls to B<search> will
apply the search list.  The default is true.

=head2 debug

    print 'debug flag: ', $res->debug, "\n";
    $res->debug(1);

Get or set the debug flag.  If set, calls to B<search>, B<query>,
and B<send> will print debugging information on the standard output.
The default is false.

=head2 usevc

    print 'usevc flag: ', $res->usevc, "\n";
    $res->usevc(1);

Get or set the usevc flag.  If true, then queries will be performed
using virtual circuits (TCP) instead of datagrams (UDP).  The default
is false.

=head2 tcp_timeout

    print 'TCP timeout: ', $res->tcp_timeout, "\n";
    $res->tcp_timeout(10);

Get or set the TCP timeout in seconds.  A timeout of C<undef> means
indefinite.  The default is 120 seconds (2 minutes).

=head2 udp_timeout

    print 'UDP timeout: ', $res->udp_timeout, "\n";
    $res->udp_timeout(10);

Get or set the UDP timeout in seconds.  A timeout of C<undef> means
the retry and retrans settings will be just utilized to perform the
retries until they are exhausted.  The default is C<undef>.

=head2 persistent_tcp

    print 'Persistent TCP flag: ', $res->persistent_tcp, "\n";
    $res->persistent_tcp(1);

Get or set the persistent TCP setting.  If set to true, Net::DNS
will keep a TCP socket open for each host:port to which it connects.
This is useful if you're using TCP and need to make a lot of queries
or updates to the same nameserver.

This option defaults to false unless you're running under a
SOCKSified Perl, in which case it defaults to true.

=head2 persistent_udp

    print 'Persistent UDP flag: ', $res->persistent_udp, "\n";
    $res->persistent_udp(1);

Get or set the persistent UDP setting.  If set to true, Net::DNS
will keep a single UDP socket open for all queries.
This is useful if you're using UDP and need to make a lot of queries
or updates.

=head2 igntc

    print 'igntc flag: ', $res->igntc, "\n";
    $res->igntc(1);

Get or set the igntc flag.  If true, truncated packets will be
ignored.  If false, truncated packets will cause the query to
be retried using TCP.  The default is false.

=head2 errorstring

    print 'query status: ', $res->errorstring, "\n";

Returns a string containing the status of the most recent query.

=head2 answerfrom

    print 'last answer was from: ', $res->answerfrom, "\n";

Returns the IP address from which we received the last answer in
response to a query.

=head2 answersize

    print 'size of last answer: ', $res->answersize, "\n";

Returns the size in bytes of the last answer we received in
response to a query.


=head2 dnssec

    print "dnssec flag: ", $res->dnssec, "\n";
    $res->dnssec(0);

Enabled DNSSEC this will set the checking disabled flag in the query header
and add EDNS0 data as in RFC2671 and RFC3225

When set to true the answer and additional section of queries from
secured zones will contain DNSKEY, NSEC and RRSIG records.

Setting calling the dnssec method with a non-zero value will set the
UDP packet size to the default value of 2048. If that is to small or
to big for your environement you should call the udppacketsize()
method immeditatly after.

   $res->dnssec(1);    # turns on DNSSEC and sets udp packetsize to 2048
   $res->udppacketsize(1028);   # lowers the UDP pakcet size

The method will Croak::croak with the message "You called the
Net::DNS::Resolver::dnssec() method but do not have Net::DNS::SEC
installed at ..." if you call it without Net::DNS::SEC being in your
@INC path.



=head2 cdflag

    print "checking disabled flag: ", $res->dnssec, "\n";
    $res->dnssec(1);
    $res->cdflag(1);

Sets or gets the CD bit for a dnssec query.  This bit is always zero
for non dnssec queries. When the dnssec is enabled the flag can be set
to 1.

=head2 udppacketsize

    print "udppacketsize: ", $res->udppacketsize, "\n";
    $res->udppacketsize(2048);

udppacketsize will set or get the packet size. If set to a value greater than 
Net::DNS::PACKETSZ() an EDNS extension will be added indicating suppport for MTU path 
recovery.

Default udppacketsize is Net::DNS::PACKETSZ() (512)

=head1 CUSTOMIZING

Net::DNS::Resolver is actually an empty subclass.  At compile time a
super class is chosen based on the current platform.  A side benefit of
this allows for easy modification of the methods in Net::DNS::Resolver. 
You simply add a method to the namespace!  

For example, if we wanted to cache lookups:

 package Net::DNS::Resolver;
 
 my %cache;
 
 sub search {
	my ($self, @args) = @_;
	
	return $cache{@args} ||= $self->SUPER::search(@args);
 } 


=head1 IPv6 transport

The Net::DNS::Resolver library will use IPv6 transport if the
appropriate libraries (Socket6 and IO::Socket::INET6) are available
and the address the server tries to connect to is an IPv6 address.

The print() will method will report if IPv6 transport is available.

You can use the force_v4() method with a non-zero argument
to force IPv4 transport.

The nameserver() method has IPv6 dependend behavior. If IPv6 is not
available or IPv4 transport has been forced the nameserver() method
will only return IPv4 addresses.

For example

    $res->nameservers('192.168.1.1', '192.168.2.2', '2001:610:240:0:53:0:0:3');
    $res->force_v4(1);
    print join (" ",$res->nameserver());	    

Will print: 192.168.1.1 192.168.2.2




=head1 ENVIRONMENT

The following environment variables can also be used to configure
the resolver:

=head2 RES_NAMESERVERS

    # Bourne Shell
    RES_NAMESERVERS="192.168.1.1 192.168.2.2 192.168.3.3"
    export RES_NAMESERVERS

    # C Shell
    setenv RES_NAMESERVERS "192.168.1.1 192.168.2.2 192.168.3.3"

A space-separated list of nameservers to query.

=head2 RES_SEARCHLIST

    # Bourne Shell
    RES_SEARCHLIST="example.com sub1.example.com sub2.example.com"
    export RES_SEARCHLIST

    # C Shell
    setenv RES_SEARCHLIST "example.com sub1.example.com sub2.example.com"

A space-separated list of domains to put in the search list.

=head2 LOCALDOMAIN

    # Bourne Shell
    LOCALDOMAIN=example.com
    export LOCALDOMAIN

    # C Shell
    setenv LOCALDOMAIN example.com

The default domain.

=head2 RES_OPTIONS

    # Bourne Shell
    RES_OPTIONS="retrans:3 retry:2 debug"
    export RES_OPTIONS

    # C Shell
    setenv RES_OPTIONS "retrans:3 retry:2 debug"

A space-separated list of resolver options to set.  Options that
take values are specified as I<option>:I<value>.

=head1 BUGS

Error reporting and handling needs to be improved.

The current implementation supports TSIG only on outgoing packets.
No validation of server replies is performed.

bgsend does not honor the usevc flag and only uses UDP for transport.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.
Portions Copyright (c) 2005 Olaf M. Kolkman, NLnet Labs.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Packet>, L<Net::DNS::Update>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
L<resolver(5)>, RFC 1035, RFC 1034 Section 4.3.5

=cut
