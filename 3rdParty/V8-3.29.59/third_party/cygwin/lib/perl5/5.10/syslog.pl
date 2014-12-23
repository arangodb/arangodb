#
# syslog.pl
#
# $Log:	syslog.pl,v $
# 
# tom christiansen <tchrist@convex.com>
# modified to use sockets by Larry Wall <lwall@jpl-devvax.jpl.nasa.gov>
# NOTE: openlog now takes three arguments, just like openlog(3)
#
# call syslog() with a string priority and a list of printf() args
# like syslog(3)
#
#  usage: require 'syslog.pl';
#
#  then (put these all in a script to test function)
#		
#
#	do openlog($program,'cons,pid','user');
#	do syslog('info','this is another test');
#	do syslog('mail|warning','this is a better test: %d', time);
#	do closelog();
#	
#	do syslog('debug','this is the last test');
#	do openlog("$program $$",'ndelay','user');
#	do syslog('notice','fooprogram: this is really done');
#
#	$! = 55;
#	do syslog('info','problem was %m'); # %m == $! in syslog(3)

package syslog;

use warnings::register;

$host = 'localhost' unless $host;	# set $syslog'host to change

if ($] >= 5 && warnings::enabled()) {
    warnings::warn("You should 'use Sys::Syslog' instead; continuing");
} 

require 'syslog.ph';

 eval 'use Socket; 1' 			||
     eval { require "socket.ph" } 	||
     require "sys/socket.ph";

$maskpri = &LOG_UPTO(&LOG_DEBUG);

sub main'openlog {
    ($ident, $logopt, $facility) = @_;  # package vars
    $lo_pid = $logopt =~ /\bpid\b/;
    $lo_ndelay = $logopt =~ /\bndelay\b/;
    $lo_cons = $logopt =~ /\bcons\b/;
    $lo_nowait = $logopt =~ /\bnowait\b/;
    &connect if $lo_ndelay;
} 

sub main'closelog {
    $facility = $ident = '';
    &disconnect;
} 

sub main'setlogmask {
    local($oldmask) = $maskpri;
    $maskpri = shift;
    $oldmask;
}
 
sub main'syslog {
    local($priority) = shift;
    local($mask) = shift;
    local($message, $whoami);
    local(@words, $num, $numpri, $numfac, $sum);
    local($facility) = $facility;	# may need to change temporarily.

    die "syslog: expected both priority and mask" unless $mask && $priority;

    @words = split(/\W+/, $priority, 2);# Allow "level" or "level|facility".
    undef $numpri;
    undef $numfac;
    foreach (@words) {
	$num = &xlate($_);		# Translate word to number.
	if (/^kern$/ || $num < 0) {
	    die "syslog: invalid level/facility: $_\n";
	}
	elsif ($num <= &LOG_PRIMASK) {
	    die "syslog: too many levels given: $_\n" if defined($numpri);
	    $numpri = $num;
	    return 0 unless &LOG_MASK($numpri) & $maskpri;
	}
	else {
	    die "syslog: too many facilities given: $_\n" if defined($numfac);
	    $facility = $_;
	    $numfac = $num;
	}
    }

    die "syslog: level must be given\n" unless defined($numpri);

    if (!defined($numfac)) {	# Facility not specified in this call.
	$facility = 'user' unless $facility;
	$numfac = &xlate($facility);
    }

    &connect unless $connected;

    $whoami = $ident;

    if (!$ident && $mask =~ /^(\S.*):\s?(.*)/) {
	$whoami = $1;
	$mask = $2;
    } 

    unless ($whoami) {
	($whoami = getlogin) ||
	    ($whoami = getpwuid($<)) ||
		($whoami = 'syslog');
    }

    $whoami .= "[$$]" if $lo_pid;

    $mask =~ s/%m/$!/g;
    $mask .= "\n" unless $mask =~ /\n$/;
    $message = sprintf ($mask, @_);

    $sum = $numpri + $numfac;
    unless (send(SYSLOG,"<$sum>$whoami: $message",0)) {
	if ($lo_cons) {
	    if ($pid = fork) {
		unless ($lo_nowait) {
		    do {$died = wait;} until $died == $pid || $died < 0;
		}
	    }
	    else {
		open(CONS,">/dev/console");
		print CONS "<$facility.$priority>$whoami: $message\r";
		exit if defined $pid;		# if fork failed, we're parent
		close CONS;
	    }
	}
    }
}

sub xlate {
    local($name) = @_;
    $name = uc $name;
    $name = "LOG_$name" unless $name =~ /^LOG_/;
    $name = "syslog'$name";
    defined &$name ? &$name : -1;
}

sub connect {
    $pat = 'S n C4 x8';

    $af_unix = &AF_UNIX;
    $af_inet = &AF_INET;

    $stream = &SOCK_STREAM;
    $datagram = &SOCK_DGRAM;

    ($name,$aliases,$proto) = getprotobyname('udp');
    $udp = $proto;

    ($name,$aliases,$port,$proto) = getservbyname('syslog','udp');
    $syslog = $port;

    if (chop($myname = `hostname`)) {
	($name,$aliases,$addrtype,$length,@addrs) = gethostbyname($myname);
	die "Can't lookup $myname\n" unless $name;
	@bytes = unpack("C4",$addrs[0]);
    }
    else {
	@bytes = (0,0,0,0);
    }
    $this = pack($pat, $af_inet, 0, @bytes);

    if ($host =~ /^\d+\./) {
	@bytes = split(/\./,$host);
    }
    else {
	($name,$aliases,$addrtype,$length,@addrs) = gethostbyname($host);
	die "Can't lookup $host\n" unless $name;
	@bytes = unpack("C4",$addrs[0]);
    }
    $that = pack($pat,$af_inet,$syslog,@bytes);

    socket(SYSLOG,$af_inet,$datagram,$udp) || die "socket: $!\n";
    bind(SYSLOG,$this) || die "bind: $!\n";
    connect(SYSLOG,$that) || die "connect: $!\n";

    local($old) = select(SYSLOG); $| = 1; select($old);
    $connected = 1;
}

sub disconnect {
    close SYSLOG;
    $connected = 0;
}

1;
