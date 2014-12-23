package Socket;

our($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
$VERSION = "1.81";

=head1 NAME

Socket, sockaddr_in, sockaddr_un, inet_aton, inet_ntoa - load the C socket.h defines and structure manipulators 

=head1 SYNOPSIS

    use Socket;

    $proto = getprotobyname('udp');
    socket(Socket_Handle, PF_INET, SOCK_DGRAM, $proto);
    $iaddr = gethostbyname('hishost.com');
    $port = getservbyname('time', 'udp');
    $sin = sockaddr_in($port, $iaddr);
    send(Socket_Handle, 0, 0, $sin);

    $proto = getprotobyname('tcp');
    socket(Socket_Handle, PF_INET, SOCK_STREAM, $proto);
    $port = getservbyname('smtp', 'tcp');
    $sin = sockaddr_in($port,inet_aton("127.1"));
    $sin = sockaddr_in(7,inet_aton("localhost"));
    $sin = sockaddr_in(7,INADDR_LOOPBACK);
    connect(Socket_Handle,$sin);

    ($port, $iaddr) = sockaddr_in(getpeername(Socket_Handle));
    $peer_host = gethostbyaddr($iaddr, AF_INET);
    $peer_addr = inet_ntoa($iaddr);

    $proto = getprotobyname('tcp');
    socket(Socket_Handle, PF_UNIX, SOCK_STREAM, $proto);
    unlink('/var/run/usock');
    $sun = sockaddr_un('/var/run/usock');
    connect(Socket_Handle,$sun);

=head1 DESCRIPTION

This module is just a translation of the C F<socket.h> file.
Unlike the old mechanism of requiring a translated F<socket.ph>
file, this uses the B<h2xs> program (see the Perl source distribution)
and your native C compiler.  This means that it has a 
far more likely chance of getting the numbers right.  This includes
all of the commonly used pound-defines like AF_INET, SOCK_STREAM, etc.

Also, some common socket "newline" constants are provided: the
constants C<CR>, C<LF>, and C<CRLF>, as well as C<$CR>, C<$LF>, and
C<$CRLF>, which map to C<\015>, C<\012>, and C<\015\012>.  If you do
not want to use the literal characters in your programs, then use
the constants provided here.  They are not exported by default, but can
be imported individually, and with the C<:crlf> export tag:

    use Socket qw(:DEFAULT :crlf);

In addition, some structure manipulation functions are available:

=over 4

=item inet_aton HOSTNAME

Takes a string giving the name of a host, and translates that to an
opaque string (if programming in C, struct in_addr). Takes arguments
of both the 'rtfm.mit.edu' type and '18.181.0.24'. If the host name
cannot be resolved, returns undef.  For multi-homed hosts (hosts with
more than one address), the first address found is returned.

For portability do not assume that the result of inet_aton() is 32
bits wide, in other words, that it would contain only the IPv4 address
in network order.

=item inet_ntoa IP_ADDRESS

Takes a string (an opaque string as returned by inet_aton(),
or a v-string representing the four octets of the IPv4 address in
network order) and translates it into a string of the form 'd.d.d.d'
where the 'd's are numbers less than 256 (the normal human-readable
four dotted number notation for Internet addresses).

=item INADDR_ANY

Note: does not return a number, but a packed string.

Returns the 4-byte wildcard ip address which specifies any
of the hosts ip addresses.  (A particular machine can have
more than one ip address, each address corresponding to
a particular network interface. This wildcard address
allows you to bind to all of them simultaneously.)
Normally equivalent to inet_aton('0.0.0.0').

=item INADDR_BROADCAST

Note: does not return a number, but a packed string.

Returns the 4-byte 'this-lan' ip broadcast address.
This can be useful for some protocols to solicit information
from all servers on the same LAN cable.
Normally equivalent to inet_aton('255.255.255.255').

=item INADDR_LOOPBACK

Note - does not return a number.

Returns the 4-byte loopback address.  Normally equivalent
to inet_aton('localhost').

=item INADDR_NONE

Note - does not return a number.

Returns the 4-byte 'invalid' ip address.  Normally equivalent
to inet_aton('255.255.255.255').

=item sockaddr_family SOCKADDR

Takes a sockaddr structure (as returned by pack_sockaddr_in(),
pack_sockaddr_un() or the perl builtin functions getsockname() and
getpeername()) and returns the address family tag.  It will match the
constant AF_INET for a sockaddr_in and AF_UNIX for a sockaddr_un.  It
can be used to figure out what unpacker to use for a sockaddr of
unknown type.

=item sockaddr_in PORT, ADDRESS

=item sockaddr_in SOCKADDR_IN

In a list context, unpacks its SOCKADDR_IN argument and returns an array
consisting of (PORT, ADDRESS).  In a scalar context, packs its (PORT,
ADDRESS) arguments as a SOCKADDR_IN and returns it.  If this is confusing,
use pack_sockaddr_in() and unpack_sockaddr_in() explicitly.

=item pack_sockaddr_in PORT, IP_ADDRESS

Takes two arguments, a port number and an opaque string, IP_ADDRESS
(as returned by inet_aton(), or a v-string).  Returns the sockaddr_in
structure with those arguments packed in with AF_INET filled in.  For
Internet domain sockets, this structure is normally what you need for
the arguments in bind(), connect(), and send(), and is also returned
by getpeername(), getsockname() and recv().

=item unpack_sockaddr_in SOCKADDR_IN

Takes a sockaddr_in structure (as returned by pack_sockaddr_in()) and
returns an array of two elements: the port and an opaque string
representing the IP address (you can use inet_ntoa() to convert the
address to the four-dotted numeric format).  Will croak if the
structure does not have AF_INET in the right place.

=item sockaddr_un PATHNAME

=item sockaddr_un SOCKADDR_UN

In a list context, unpacks its SOCKADDR_UN argument and returns an array
consisting of (PATHNAME).  In a scalar context, packs its PATHNAME
arguments as a SOCKADDR_UN and returns it.  If this is confusing, use
pack_sockaddr_un() and unpack_sockaddr_un() explicitly.
These are only supported if your system has E<lt>F<sys/un.h>E<gt>.

=item pack_sockaddr_un PATH

Takes one argument, a pathname. Returns the sockaddr_un structure with
that path packed in with AF_UNIX filled in. For unix domain sockets, this
structure is normally what you need for the arguments in bind(),
connect(), and send(), and is also returned by getpeername(),
getsockname() and recv().

=item unpack_sockaddr_un SOCKADDR_UN

Takes a sockaddr_un structure (as returned by pack_sockaddr_un())
and returns the pathname.  Will croak if the structure does not
have AF_UNIX in the right place.

=back

=cut

use Carp;
use warnings::register;

require Exporter;
use XSLoader ();
@ISA = qw(Exporter);
@EXPORT = qw(
	inet_aton inet_ntoa
	sockaddr_family
	pack_sockaddr_in unpack_sockaddr_in
	pack_sockaddr_un unpack_sockaddr_un
	sockaddr_in sockaddr_un
	INADDR_ANY INADDR_BROADCAST INADDR_LOOPBACK INADDR_NONE
	AF_802
	AF_AAL
	AF_APPLETALK
	AF_CCITT
	AF_CHAOS
	AF_CTF
	AF_DATAKIT
	AF_DECnet
	AF_DLI
	AF_ECMA
	AF_GOSIP
	AF_HYLINK
	AF_IMPLINK
	AF_INET
	AF_INET6
	AF_ISO
	AF_KEY
	AF_LAST
	AF_LAT
	AF_LINK
	AF_MAX
	AF_NBS
	AF_NIT
	AF_NS
	AF_OSI
	AF_OSINET
	AF_PUP
	AF_ROUTE
	AF_SNA
	AF_UNIX
	AF_UNSPEC
	AF_USER
	AF_WAN
	AF_X25
	IOV_MAX
	IP_OPTIONS
	IP_HDRINCL
	IP_TOS
	IP_TTL
	IP_RECVOPTS
	IP_RECVRETOPTS
	IP_RETOPTS
	MSG_BCAST
	MSG_BTAG
	MSG_CTLFLAGS
	MSG_CTLIGNORE
	MSG_CTRUNC
	MSG_DONTROUTE
	MSG_DONTWAIT
	MSG_EOF
	MSG_EOR
	MSG_ERRQUEUE
	MSG_ETAG
	MSG_FIN
	MSG_MAXIOVLEN
	MSG_MCAST
	MSG_NOSIGNAL
	MSG_OOB
	MSG_PEEK
	MSG_PROXY
	MSG_RST
	MSG_SYN
	MSG_TRUNC
	MSG_URG
	MSG_WAITALL
	MSG_WIRE
	PF_802
	PF_AAL
	PF_APPLETALK
	PF_CCITT
	PF_CHAOS
	PF_CTF
	PF_DATAKIT
	PF_DECnet
	PF_DLI
	PF_ECMA
	PF_GOSIP
	PF_HYLINK
	PF_IMPLINK
	PF_INET
	PF_INET6
	PF_ISO
	PF_KEY
	PF_LAST
	PF_LAT
	PF_LINK
	PF_MAX
	PF_NBS
	PF_NIT
	PF_NS
	PF_OSI
	PF_OSINET
	PF_PUP
	PF_ROUTE
	PF_SNA
	PF_UNIX
	PF_UNSPEC
	PF_USER
	PF_WAN
	PF_X25
	SCM_CONNECT
	SCM_CREDENTIALS
	SCM_CREDS
	SCM_RIGHTS
	SCM_TIMESTAMP
	SHUT_RD
	SHUT_RDWR
	SHUT_WR
	SOCK_DGRAM
	SOCK_RAW
	SOCK_RDM
	SOCK_SEQPACKET
	SOCK_STREAM
	SOL_SOCKET
	SOMAXCONN
	SO_ACCEPTCONN
	SO_ATTACH_FILTER
	SO_BACKLOG
	SO_BROADCAST
	SO_CHAMELEON
	SO_DEBUG
	SO_DETACH_FILTER
	SO_DGRAM_ERRIND
	SO_DONTLINGER
	SO_DONTROUTE
	SO_ERROR
	SO_FAMILY
	SO_KEEPALIVE
	SO_LINGER
	SO_OOBINLINE
	SO_PASSCRED
	SO_PASSIFNAME
	SO_PEERCRED
	SO_PROTOCOL
	SO_PROTOTYPE
	SO_RCVBUF
	SO_RCVLOWAT
	SO_RCVTIMEO
	SO_REUSEADDR
	SO_REUSEPORT
	SO_SECURITY_AUTHENTICATION
	SO_SECURITY_ENCRYPTION_NETWORK
	SO_SECURITY_ENCRYPTION_TRANSPORT
	SO_SNDBUF
	SO_SNDLOWAT
	SO_SNDTIMEO
	SO_STATE
	SO_TYPE
	SO_USELOOPBACK
	SO_XOPEN
	SO_XSE
	UIO_MAXIOV
);

@EXPORT_OK = qw(CR LF CRLF $CR $LF $CRLF

	       IPPROTO_IP
	       IPPROTO_IPV6
	       IPPROTO_RAW
	       IPPROTO_ICMP
	       IPPROTO_TCP
	       IPPROTO_UDP

	       TCP_KEEPALIVE
	       TCP_MAXRT
	       TCP_MAXSEG
	       TCP_NODELAY
	       TCP_STDURG);

%EXPORT_TAGS = (
    crlf    => [qw(CR LF CRLF $CR $LF $CRLF)],
    all     => [@EXPORT, @EXPORT_OK],
);

BEGIN {
    sub CR   () {"\015"}
    sub LF   () {"\012"}
    sub CRLF () {"\015\012"}
}

*CR   = \CR();
*LF   = \LF();
*CRLF = \CRLF();

sub sockaddr_in {
    if (@_ == 6 && !wantarray) { # perl5.001m compat; use this && die
	my($af, $port, @quad) = @_;
	warnings::warn "6-ARG sockaddr_in call is deprecated" 
	    if warnings::enabled();
	pack_sockaddr_in($port, inet_aton(join('.', @quad)));
    } elsif (wantarray) {
	croak "usage:   (port,iaddr) = sockaddr_in(sin_sv)" unless @_ == 1;
        unpack_sockaddr_in(@_);
    } else {
	croak "usage:   sin_sv = sockaddr_in(port,iaddr))" unless @_ == 2;
        pack_sockaddr_in(@_);
    }
}

sub sockaddr_un {
    if (wantarray) {
	croak "usage:   (filename) = sockaddr_un(sun_sv)" unless @_ == 1;
        unpack_sockaddr_un(@_);
    } else {
	croak "usage:   sun_sv = sockaddr_un(filename)" unless @_ == 1;
        pack_sockaddr_un(@_);
    }
}

sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "&Socket::constant not defined" if $constname eq 'constant';
    my ($error, $val) = constant($constname);
    if ($error) {
	croak $error;
    }
    *$AUTOLOAD = sub { $val };
    goto &$AUTOLOAD;
}

XSLoader::load 'Socket', $VERSION;

1;
