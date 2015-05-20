package Sys::Hostname;

use strict;

use Carp;

require Exporter;
require AutoLoader;

our @ISA     = qw/ Exporter AutoLoader /;
our @EXPORT  = qw/ hostname /;

our $VERSION;

our $host;

BEGIN {
    $VERSION = '1.11';
    {
	local $SIG{__DIE__};
	eval {
	    require XSLoader;
	    XSLoader::load('Sys::Hostname', $VERSION);
	};
	warn $@ if $@;
    }
}


sub hostname {

  # method 1 - we already know it
  return $host if defined $host;

  # method 1' - try to ask the system
  $host = ghname() if defined &ghname;
  return $host if defined $host;

  if ($^O eq 'VMS') {

    # method 2 - no sockets ==> return DECnet node name
    eval { local $SIG{__DIE__}; $host = (gethostbyname('me'))[0] };
    if ($@) { return $host = $ENV{'SYS$NODE'}; }

    # method 3 - has someone else done the job already?  It's common for the
    #    TCP/IP stack to advertise the hostname via a logical name.  (Are
    #    there any other logicals which TCP/IP stacks use for the host name?)
    $host = $ENV{'ARPANET_HOST_NAME'}  || $ENV{'INTERNET_HOST_NAME'} ||
            $ENV{'MULTINET_HOST_NAME'} || $ENV{'UCX$INET_HOST'}      ||
            $ENV{'TCPWARE_DOMAINNAME'} || $ENV{'NEWS_ADDRESS'};
    return $host if $host;

    # method 4 - does hostname happen to work?
    my($rslt) = `hostname`;
    if ($rslt !~ /IVVERB/) { ($host) = $rslt =~ /^(\S+)/; }
    return $host if $host;

    # rats!
    $host = '';
    croak "Cannot get host name of local machine";  

  }
  elsif ($^O eq 'MSWin32') {
    ($host) = gethostbyname('localhost');
    chomp($host = `hostname 2> NUL`) unless defined $host;
    return $host;
  }
  elsif ($^O eq 'epoc') {
    $host = 'localhost';
    return $host;
  }
  else {  # Unix
    # is anyone going to make it here?

    local $ENV{PATH} = '/usr/bin:/bin:/usr/sbin:/sbin'; # Paranoia.

    # method 2 - syscall is preferred since it avoids tainting problems
    # XXX: is it such a good idea to return hostname untainted?
    eval {
	local $SIG{__DIE__};
	require "syscall.ph";
	$host = "\0" x 65; ## preload scalar
	syscall(&SYS_gethostname, $host, 65) == 0;
    }

    # method 2a - syscall using systeminfo instead of gethostname
    #           -- needed on systems like Solaris
    || eval {
	local $SIG{__DIE__};
	require "sys/syscall.ph";
	require "sys/systeminfo.ph";
	$host = "\0" x 65; ## preload scalar
	syscall(&SYS_systeminfo, &SI_HOSTNAME, $host, 65) != -1;
    }

    # method 3 - trusty old hostname command
    || eval {
	local $SIG{__DIE__};
	local $SIG{CHLD};
	$host = `(hostname) 2>/dev/null`; # bsdish
    }

    # method 4 - use POSIX::uname(), which strictly can't be expected to be
    # correct
    || eval {
	local $SIG{__DIE__};
	require POSIX;
	$host = (POSIX::uname())[1];
    }

    # method 5 - sysV uname command (may truncate)
    || eval {
	local $SIG{__DIE__};
	$host = `uname -n 2>/dev/null`; ## sysVish
    }

    # method 6 - Apollo pre-SR10
    || eval {
	local $SIG{__DIE__};
        my($a,$b,$c,$d);
	($host,$a,$b,$c,$d)=split(/[:\. ]/,`/com/host`,6);
    }

    # bummer
    || croak "Cannot get host name of local machine";  

    # remove garbage 
    $host =~ tr/\0\r\n//d;
    $host;
  }
}

1;

__END__

=head1 NAME

Sys::Hostname - Try every conceivable way to get hostname

=head1 SYNOPSIS

    use Sys::Hostname;
    $host = hostname;

=head1 DESCRIPTION

Attempts several methods of getting the system hostname and
then caches the result.  It tries the first available of the C
library's gethostname(), C<`$Config{aphostname}`>, uname(2),
C<syscall(SYS_gethostname)>, C<`hostname`>, C<`uname -n`>,
and the file F</com/host>.  If all that fails it C<croak>s.

All NULs, returns, and newlines are removed from the result.

=head1 AUTHOR

David Sundstrom E<lt>F<sunds@asictest.sc.ti.com>E<gt>

Texas Instruments

XS code added by Greg Bacon E<lt>F<gbacon@cs.uah.edu>E<gt>

=cut

