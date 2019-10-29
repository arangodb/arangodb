#!/usr/bin/env perl
#***************************************************************************
#                                  _   _ ____  _
#  Project                     ___| | | |  _ \| |
#                             / __| | | | |_) | |
#                            | (__| |_| |  _ <| |___
#                             \___|\___/|_| \_\_____|
#
# Copyright (C) 1998 - 2019, Daniel Stenberg, <daniel@haxx.se>, et al.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution. The terms
# are also available at https://curl.haxx.se/docs/copyright.html.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the COPYING file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
###########################################################################

# Experimental hooks are available to run tests remotely on machines that
# are able to run curl but are unable to run the test harness.
# The following sections need to be modified:
#
#  $HOSTIP, $HOST6IP - Set to the address of the host running the test suite
#  $CLIENTIP, $CLIENT6IP - Set to the address of the host running curl
#  runclient, runclientoutput - Modify to copy all the files in the log/
#    directory to the system running curl, run the given command remotely
#    and save the return code or returned stdout (respectively), then
#    copy all the files from the remote system's log/ directory back to
#    the host running the test suite.  This can be done a few ways, such
#    as using scp & ssh, rsync & telnet, or using a NFS shared directory
#    and ssh.
#
# 'make && make test' needs to be done on both machines before making the
# above changes and running runtests.pl manually.  In the shared NFS case,
# the contents of the tests/server/ directory must be from the host
# running the test suite, while the rest must be from the host running curl.
#
# Note that even with these changes a number of tests will still fail (mainly
# to do with cookies, those that set environment variables, or those that
# do more than touch the file system in a <precheck> or <postcheck>
# section). These can be added to the $TESTCASES line below,
# e.g. $TESTCASES="!8 !31 !63 !cookies..."
#
# Finally, to properly support -g and -n, checktestcmd needs to change
# to check the remote system's PATH, and the places in the code where
# the curl binary is read directly to determine its type also need to be
# fixed. As long as the -g option is never given, and the -n is always
# given, this won't be a problem.


# These should be the only variables that might be needed to get edited:

BEGIN {
    push(@INC, $ENV{'srcdir'}) if(defined $ENV{'srcdir'});
    push(@INC, ".");
    # run time statistics needs Time::HiRes
    eval {
        no warnings "all";
        require Time::HiRes;
        import  Time::HiRes qw( time );
    }
}

use strict;
use warnings;
use Cwd;

# Subs imported from serverhelp module
use serverhelp qw(
    serverfactors
    servername_id
    servername_str
    servername_canon
    server_pidfilename
    server_logfilename
    );

# Variables and subs imported from sshhelp module
use sshhelp qw(
    $sshdexe
    $sshexe
    $sftpexe
    $sshconfig
    $sftpconfig
    $sshdlog
    $sshlog
    $sftplog
    $sftpcmds
    display_sshdconfig
    display_sshconfig
    display_sftpconfig
    display_sshdlog
    display_sshlog
    display_sftplog
    exe_ext
    find_sshd
    find_ssh
    find_sftp
    find_httptlssrv
    sshversioninfo
    );

use pathhelp;

require "getpart.pm"; # array functions
require "valgrind.pm"; # valgrind report parser
require "ftp.pm";

my $HOSTIP="127.0.0.1";   # address on which the test server listens
my $HOST6IP="[::1]";      # address on which the test server listens
my $CLIENTIP="127.0.0.1"; # address which curl uses for incoming connections
my $CLIENT6IP="[::1]";    # address which curl uses for incoming connections

my $base = 8990; # base port number

my $HTTPPORT;            # HTTP server port
my $HTTP6PORT;           # HTTP IPv6 server port
my $HTTPSPORT;           # HTTPS (stunnel) server port
my $FTPPORT;             # FTP server port
my $FTP2PORT;            # FTP server 2 port
my $FTPSPORT;            # FTPS (stunnel) server port
my $FTP6PORT;            # FTP IPv6 server port
my $TFTPPORT;            # TFTP
my $TFTP6PORT;           # TFTP
my $SSHPORT;             # SCP/SFTP
my $SOCKSPORT;           # SOCKS4/5 port
my $POP3PORT;            # POP3
my $POP36PORT;           # POP3 IPv6 server port
my $IMAPPORT;            # IMAP
my $IMAP6PORT;           # IMAP IPv6 server port
my $SMTPPORT;            # SMTP
my $SMTP6PORT;           # SMTP IPv6 server port
my $RTSPPORT;            # RTSP
my $RTSP6PORT;           # RTSP IPv6 server port
my $GOPHERPORT;          # Gopher
my $GOPHER6PORT;         # Gopher IPv6 server port
my $HTTPTLSPORT;         # HTTP TLS (non-stunnel) server port
my $HTTPTLS6PORT;        # HTTP TLS (non-stunnel) IPv6 server port
my $HTTPPROXYPORT;       # HTTP proxy port, when using CONNECT
my $HTTPUNIXPATH;        # HTTP server Unix domain socket path
my $HTTP2PORT;           # HTTP/2 server port
my $DICTPORT;            # DICT server port
my $SMBPORT;             # SMB server port
my $SMBSPORT;            # SMBS server port
my $NEGTELNETPORT;       # TELNET server port with negotiation

my $srcdir = $ENV{'srcdir'} || '.';
my $CURL="../src/curl".exe_ext(); # what curl executable to run on the tests
my $VCURL=$CURL;   # what curl binary to use to verify the servers with
                   # VCURL is handy to set to the system one when the one you
                   # just built hangs or crashes and thus prevent verification
my $DBGCURL=$CURL; #"../src/.libs/curl";  # alternative for debugging
my $LOGDIR="log";
my $TESTDIR="$srcdir/data";
my $LIBDIR="./libtest";
my $UNITDIR="./unit";
# TODO: change this to use server_inputfilename()
my $SERVERIN="$LOGDIR/server.input"; # what curl sent the server
my $SERVER2IN="$LOGDIR/server2.input"; # what curl sent the second server
my $PROXYIN="$LOGDIR/proxy.input"; # what curl sent the proxy
my $CURLLOG="$LOGDIR/curl.log"; # all command lines run
my $FTPDCMD="$LOGDIR/ftpserver.cmd"; # copy ftp server instructions here
my $SERVERLOGS_LOCK="$LOGDIR/serverlogs.lock"; # server logs advisor read lock
my $CURLCONFIG="../curl-config"; # curl-config from current build

# Normally, all test cases should be run, but at times it is handy to
# simply run a particular one:
my $TESTCASES="all";

# To run specific test cases, set them like:
# $TESTCASES="1 2 3 7 8";

#######################################################################
# No variables below this point should need to be modified
#

# invoke perl like this:
my $perl="perl -I$srcdir";
my $server_response_maxtime=13;

my $debug_build=0;          # built debug enabled (--enable-debug)
my $has_memory_tracking=0;  # built with memory tracking (--enable-curldebug)
my $libtool;

# name of the file that the memory debugging creates:
my $memdump="$LOGDIR/memdump";

# the path to the script that analyzes the memory debug output file:
my $memanalyze="$perl $srcdir/memanalyze.pl";

my $pwd = getcwd();          # current working directory
my $posix_pwd = $pwd;

my $start;
my $ftpchecktime=1; # time it took to verify our test FTP server
my $scrambleorder;
my $stunnel = checkcmd("stunnel4") || checkcmd("tstunnel") || checkcmd("stunnel");
my $valgrind = checktestcmd("valgrind");
my $valgrind_logfile="--logfile";
my $valgrind_tool;
my $gdb = checktestcmd("gdb");
my $httptlssrv = find_httptlssrv();

my $has_ssl;        # set if libcurl is built with SSL support
my $has_largefile;  # set if libcurl is built with large file support
my $has_idn;        # set if libcurl is built with IDN support
my $http_ipv6;      # set if HTTP server has IPv6 support
my $http_unix;      # set if HTTP server has Unix sockets support
my $ftp_ipv6;       # set if FTP server has IPv6 support
my $tftp_ipv6;      # set if TFTP server has IPv6 support
my $gopher_ipv6;    # set if Gopher server has IPv6 support
my $has_ipv6;       # set if libcurl is built with IPv6 support
my $has_unix;       # set if libcurl is built with Unix sockets support
my $has_libz;       # set if libcurl is built with libz support
my $has_brotli;     # set if libcurl is built with brotli support
my $has_getrlimit;  # set if system has getrlimit()
my $has_ntlm;       # set if libcurl is built with NTLM support
my $has_ntlm_wb;    # set if libcurl is built with NTLM delegation to winbind
my $has_sspi;       # set if libcurl is built with Windows SSPI
my $has_gssapi;     # set if libcurl is built with a GSS-API library
my $has_kerberos;   # set if libcurl is built with Kerberos support
my $has_spnego;     # set if libcurl is built with SPNEGO support
my $has_charconv;   # set if libcurl is built with CharConv support
my $has_tls_srp;    # set if libcurl is built with TLS-SRP support
my $has_metalink;   # set if curl is built with Metalink support
my $has_http2;      # set if libcurl is built with HTTP2 support
my $has_crypto;     # set if libcurl is built with cryptographic support
my $has_cares;      # set if built with c-ares
my $has_threadedres;# set if built with threaded resolver
my $has_psl;        # set if libcurl is built with PSL support
my $has_altsvc;     # set if libcurl is built with alt-svc support
my $has_ldpreload;  # set if curl is built for systems supporting LD_PRELOAD
my $has_multissl;   # set if curl is build with MultiSSL support
my $has_manual;     # set if curl is built with built-in manual

# this version is decided by the particular nghttp2 library that is being used
my $h2cver = "h2c";

my $has_openssl;    # built with a lib using an OpenSSL-like API
my $has_gnutls;     # built with GnuTLS
my $has_nss;        # built with NSS
my $has_wolfssl;    # built with wolfSSL
my $has_polarssl;   # built with polarssl
my $has_winssl;     # built with WinSSL    (Secure Channel aka Schannel)
my $has_darwinssl;  # built with DarwinSSL (Secure Transport)
my $has_boringssl;  # built with BoringSSL
my $has_libressl;   # built with libressl
my $has_mbedtls;    # built with mbedTLS
my $has_mesalink;   # built with MesaLink

my $has_sslpinning; # built with a TLS backend that supports pinning

my $has_shared = "unknown";  # built shared

my $resolver;       # name of the resolver backend (for human presentation)

my $has_textaware;  # set if running on a system that has a text mode concept
                    # on files. Windows for example

my @protocols;   # array of lowercase supported protocol servers

my $skipped=0;  # number of tests skipped; reported in main loop
my %skipped;    # skipped{reason}=counter, reasons for skip
my @teststat;   # teststat[testnum]=reason, reasons for skip
my %disabled_keywords;  # key words of tests to skip
my %enabled_keywords;   # key words of tests to run
my %disabled;           # disabled test cases

my $sshdid;      # for socks server, ssh daemon version id
my $sshdvernum;  # for socks server, ssh daemon version number
my $sshdverstr;  # for socks server, ssh daemon version string
my $sshderror;   # for socks server, ssh daemon version error

my $defserverlogslocktimeout = 2; # timeout to await server logs lock removal
my $defpostcommanddelay = 0; # delay between command and postcheck sections

my $timestats;   # time stamping and stats generation
my $fullstats;   # show time stats for every single test
my %timeprepini; # timestamp for each test preparation start
my %timesrvrini; # timestamp for each test required servers verification start
my %timesrvrend; # timestamp for each test required servers verification end
my %timetoolini; # timestamp for each test command run starting
my %timetoolend; # timestamp for each test command run stopping
my %timesrvrlog; # timestamp for each test server logs lock removal
my %timevrfyend; # timestamp for each test result verification end

my $testnumcheck; # test number, set in singletest sub.
my %oldenv;
my %feature; # array of enabled features

#######################################################################
# variables that command line options may set
#

my $short;
my $automakestyle;
my $verbose;
my $debugprotocol;
my $anyway;
my $gdbthis;      # run test case with gdb debugger
my $gdbxwin;      # use windowed gdb when using gdb
my $keepoutfiles; # keep stdout and stderr files after tests
my $listonly;     # only list the tests
my $postmortem;   # display detailed info about failed tests
my $run_event_based; # run curl with --test-event to test the event API

my %run;          # running server
my %doesntrun;    # servers that don't work, identified by pidfile
my %serverpidfile;# all server pid file names, identified by server id
my %runcert;      # cert file currently in use by an ssl running server

# torture test variables
my $torture;
my $tortnum;
my $tortalloc;

#######################################################################
# logmsg is our general message logging subroutine.
#
sub logmsg {
    for(@_) {
        print "$_";
    }
}

# get the name of the current user
my $USER = $ENV{USER};          # Linux
if (!$USER) {
    $USER = $ENV{USERNAME};     # Windows
    if (!$USER) {
        $USER = $ENV{LOGNAME};  # Some Unix (I think)
    }
}

# enable memory debugging if curl is compiled with it
$ENV{'CURL_MEMDEBUG'} = $memdump;
$ENV{'CURL_ENTROPY'}="12345678";
$ENV{'CURL_FORCETIME'}=1; # for debug NTLM magic
$ENV{'HOME'}=$pwd;
$ENV{'COLUMNS'}=79; # screen width!

sub catch_zap {
    my $signame = shift;
    logmsg "runtests.pl received SIG$signame, exiting\n";
    stopservers($verbose);
    die "Somebody sent me a SIG$signame";
}
$SIG{INT} = \&catch_zap;
$SIG{TERM} = \&catch_zap;

##########################################################################
# Clear all possible '*_proxy' environment variables for various protocols
# to prevent them to interfere with our testing!

my $protocol;
foreach $protocol (('ftp', 'http', 'ftps', 'https', 'no', 'all')) {
    my $proxy = "${protocol}_proxy";
    # clear lowercase version
    delete $ENV{$proxy} if($ENV{$proxy});
    # clear uppercase version
    delete $ENV{uc($proxy)} if($ENV{uc($proxy)});
}

# make sure we don't get affected by other variables that control our
# behaviour

delete $ENV{'SSL_CERT_DIR'} if($ENV{'SSL_CERT_DIR'});
delete $ENV{'SSL_CERT_PATH'} if($ENV{'SSL_CERT_PATH'});
delete $ENV{'CURL_CA_BUNDLE'} if($ENV{'CURL_CA_BUNDLE'});

#######################################################################
# Load serverpidfile hash with pidfile names for all possible servers.
#
sub init_serverpidfile_hash {
  for my $proto (('ftp', 'http', 'imap', 'pop3', 'smtp', 'http/2')) {
    for my $ssl (('', 's')) {
      for my $ipvnum ((4, 6)) {
        for my $idnum ((1, 2, 3)) {
          my $serv = servername_id("$proto$ssl", $ipvnum, $idnum);
          my $pidf = server_pidfilename("$proto$ssl", $ipvnum, $idnum);
          $serverpidfile{$serv} = $pidf;
        }
      }
    }
  }
  for my $proto (('tftp', 'sftp', 'socks', 'ssh', 'rtsp', 'gopher', 'httptls',
                  'dict', 'smb', 'smbs', 'telnet')) {
    for my $ipvnum ((4, 6)) {
      for my $idnum ((1, 2)) {
        my $serv = servername_id($proto, $ipvnum, $idnum);
        my $pidf = server_pidfilename($proto, $ipvnum, $idnum);
        $serverpidfile{$serv} = $pidf;
      }
    }
  }
  for my $proto (('http', 'imap', 'pop3', 'smtp', 'http/2')) {
    for my $ssl (('', 's')) {
      my $serv = servername_id("$proto$ssl", "unix", 1);
      my $pidf = server_pidfilename("$proto$ssl", "unix", 1);
      $serverpidfile{$serv} = $pidf;
    }
  }
}

#######################################################################
# Check if a given child process has just died. Reaps it if so.
#
sub checkdied {
    use POSIX ":sys_wait_h";
    my $pid = $_[0];
    if((not defined $pid) || $pid <= 0) {
        return 0;
    }
    my $rc = waitpid($pid, &WNOHANG);
    return ($rc == $pid)?1:0;
}

#######################################################################
# Start a new thread/process and run the given command line in there.
# Return the pids (yes plural) of the new child process to the parent.
#
sub startnew {
    my ($cmd, $pidfile, $timeout, $fake)=@_;

    logmsg "startnew: $cmd\n" if ($verbose);

    my $child = fork();
    my $pid2 = 0;

    if(not defined $child) {
        logmsg "startnew: fork() failure detected\n";
        return (-1,-1);
    }

    if(0 == $child) {
        # Here we are the child. Run the given command.

        # Put an "exec" in front of the command so that the child process
        # keeps this child's process ID.
        exec("exec $cmd") || die "Can't exec() $cmd: $!";

        # exec() should never return back here to this process. We protect
        # ourselves by calling die() just in case something goes really bad.
        die "error: exec() has returned";
    }

    # Ugly hack but ssh client and gnutls-serv don't support pid files
    if ($fake) {
        if(open(OUT, ">$pidfile")) {
            print OUT $child . "\n";
            close(OUT);
            logmsg "startnew: $pidfile faked with pid=$child\n" if($verbose);
        }
        else {
            logmsg "startnew: failed to write fake $pidfile with pid=$child\n";
        }
        # could/should do a while connect fails sleep a bit and loop
        sleep $timeout;
        if (checkdied($child)) {
            logmsg "startnew: child process has failed to start\n" if($verbose);
            return (-1,-1);
        }
    }

    my $count = $timeout;
    while($count--) {
        if(-f $pidfile && -s $pidfile && open(PID, "<$pidfile")) {
            $pid2 = 0 + <PID>;
            close(PID);
            if(($pid2 > 0) && pidexists($pid2)) {
                # if $pid2 is valid, then make sure this pid is alive, as
                # otherwise it is just likely to be the _previous_ pidfile or
                # similar!
                last;
            }
            # invalidate $pid2 if not actually alive
            $pid2 = 0;
        }
        if (checkdied($child)) {
            logmsg "startnew: child process has died, server might start up\n"
                if($verbose);
            # We can't just abort waiting for the server with a
            # return (-1,-1);
            # because the server might have forked and could still start
            # up normally. Instead, just reduce the amount of time we remain
            # waiting.
            $count >>= 2;
        }
        sleep(1);
    }

    # Return two PIDs, the one for the child process we spawned and the one
    # reported by the server itself (in case it forked again on its own).
    # Both (potentially) need to be killed at the end of the test.
    return ($child, $pid2);
}


#######################################################################
# Check for a command in the PATH of the test server.
#
sub checkcmd {
    my ($cmd)=@_;
    my @paths=(split(":", $ENV{'PATH'}), "/usr/sbin", "/usr/local/sbin",
               "/sbin", "/usr/bin", "/usr/local/bin",
               "./libtest/.libs", "./libtest");
    for(@paths) {
        if( -x "$_/$cmd" && ! -d "$_/$cmd") {
            # executable bit but not a directory!
            return "$_/$cmd";
        }
    }
}

#######################################################################
# Get the list of tests that the tests/data/Makefile.am knows about!
#
my $disttests;
sub get_disttests {
    my @dist = `cd data && make show`;
    $disttests = join("", @dist);
}

#######################################################################
# Check for a command in the PATH of the machine running curl.
#
sub checktestcmd {
    my ($cmd)=@_;
    return checkcmd($cmd);
}

#######################################################################
# Run the application under test and return its return code
#
sub runclient {
    my ($cmd)=@_;
    my $ret = system($cmd);
    print "CMD ($ret): $cmd\n" if($verbose && !$torture);
    return $ret;

# This is one way to test curl on a remote machine
#    my $out = system("ssh $CLIENTIP cd \'$pwd\' \\; \'$cmd\'");
#    sleep 2;    # time to allow the NFS server to be updated
#    return $out;
}

#######################################################################
# Run the application under test and return its stdout
#
sub runclientoutput {
    my ($cmd)=@_;
    return `$cmd`;

# This is one way to test curl on a remote machine
#    my @out = `ssh $CLIENTIP cd \'$pwd\' \\; \'$cmd\'`;
#    sleep 2;    # time to allow the NFS server to be updated
#    return @out;
 }

#######################################################################
# Memory allocation test and failure torture testing.
#
sub torture {
    my ($testcmd, $testnum, $gdbline) = @_;

    # remove memdump first to be sure we get a new nice and clean one
    unlink($memdump);

    # First get URL from test server, ignore the output/result
    runclient($testcmd);

    logmsg " CMD: $testcmd\n" if($verbose);

    # memanalyze -v is our friend, get the number of allocations made
    my $count=0;
    my @out = `$memanalyze -v $memdump`;
    for(@out) {
        if(/^Operations: (\d+)/) {
            $count = $1;
            last;
        }
    }
    if(!$count) {
        logmsg " found no functions to make fail\n";
        return 0;
    }

    logmsg " $count functions to make fail\n";

    for ( 1 .. $count ) {
        my $limit = $_;
        my $fail;
        my $dumped_core;

        if($tortalloc && ($tortalloc != $limit)) {
            next;
        }

        if($verbose) {
            my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) =
                localtime(time());
            my $now = sprintf("%02d:%02d:%02d ", $hour, $min, $sec);
            logmsg "Fail function no: $limit at $now\r";
        }

        # make the memory allocation function number $limit return failure
        $ENV{'CURL_MEMLIMIT'} = $limit;

        # remove memdump first to be sure we get a new nice and clean one
        unlink($memdump);

        my $cmd = $testcmd;
        if($valgrind && !$gdbthis) {
            my @valgrindoption = getpart("verify", "valgrind");
            if((!@valgrindoption) || ($valgrindoption[0] !~ /disable/)) {
                my $valgrindcmd = "$valgrind ";
                $valgrindcmd .= "$valgrind_tool " if($valgrind_tool);
                $valgrindcmd .= "--quiet --leak-check=yes ";
                $valgrindcmd .= "--suppressions=$srcdir/valgrind.supp ";
                # $valgrindcmd .= "--gen-suppressions=all ";
                $valgrindcmd .= "--num-callers=16 ";
                $valgrindcmd .= "${valgrind_logfile}=$LOGDIR/valgrind$testnum";
                $cmd = "$valgrindcmd $testcmd";
            }
        }
        logmsg "*** Function number $limit is now set to fail ***\n" if($gdbthis);

        my $ret = 0;
        if($gdbthis) {
            runclient($gdbline);
        }
        else {
            $ret = runclient($cmd);
        }
        #logmsg "$_ Returned " . ($ret >> 8) . "\n";

        # Now clear the variable again
        delete $ENV{'CURL_MEMLIMIT'} if($ENV{'CURL_MEMLIMIT'});

        if(-r "core") {
            # there's core file present now!
            logmsg " core dumped\n";
            $dumped_core = 1;
            $fail = 2;
        }

        if($valgrind) {
            my @e = valgrindparse("$LOGDIR/valgrind$testnum");
            if(@e && $e[0]) {
                if($automakestyle) {
                    logmsg "FAIL: torture $testnum - valgrind\n";
                }
                else {
                    logmsg " valgrind ERROR ";
                    logmsg @e;
                }
                $fail = 1;
            }
        }

        # verify that it returns a proper error code, doesn't leak memory
        # and doesn't core dump
        if(($ret & 255) || ($ret >> 8) >= 128) {
            logmsg " system() returned $ret\n";
            $fail=1;
        }
        else {
            my @memdata=`$memanalyze $memdump`;
            my $leak=0;
            for(@memdata) {
                if($_ ne "") {
                    # well it could be other memory problems as well, but
                    # we call it leak for short here
                    $leak=1;
                }
            }
            if($leak) {
                logmsg "** MEMORY FAILURE\n";
                logmsg @memdata;
                logmsg `$memanalyze -l $memdump`;
                $fail = 1;
            }
        }
        if($fail) {
            logmsg " Failed on function number $limit in test.\n",
            " invoke with \"-t$limit\" to repeat this single case.\n";
            stopservers($verbose);
            return 1;
        }
    }

    logmsg "torture OK\n";
    return 0;
}

#######################################################################
# Stop a test server along with pids which aren't in the %run hash yet.
# This also stops all servers which are relative to the given one.
#
sub stopserver {
    my ($server, $pidlist) = @_;
    #
    # kill sockfilter processes for pingpong relative server
    #
    if($server =~ /^(ftp|imap|pop3|smtp)s?(\d*)(-ipv6|)$/) {
        my $proto  = $1;
        my $idnum  = ($2 && ($2 > 1)) ? $2 : 1;
        my $ipvnum = ($3 && ($3 =~ /6$/)) ? 6 : 4;
        killsockfilters($proto, $ipvnum, $idnum, $verbose);
    }
    #
    # All servers relative to the given one must be stopped also
    #
    my @killservers;
    if($server =~ /^(ftp|http|imap|pop3|smtp)s((\d*)(-ipv6|-unix|))$/) {
        # given a stunnel based ssl server, also kill non-ssl underlying one
        push @killservers, "${1}${2}";
    }
    elsif($server =~ /^(ftp|http|imap|pop3|smtp)((\d*)(-ipv6|-unix|))$/) {
        # given a non-ssl server, also kill stunnel based ssl piggybacking one
        push @killservers, "${1}s${2}";
    }
    elsif($server =~ /^(socks)((\d*)(-ipv6|))$/) {
        # given a socks server, also kill ssh underlying one
        push @killservers, "ssh${2}";
    }
    elsif($server =~ /^(ssh)((\d*)(-ipv6|))$/) {
        # given a ssh server, also kill socks piggybacking one
        push @killservers, "socks${2}";
    }
    push @killservers, $server;
    #
    # kill given pids and server relative ones clearing them in %run hash
    #
    foreach my $server (@killservers) {
        if($run{$server}) {
            # we must prepend a space since $pidlist may already contain a pid
            $pidlist .= " $run{$server}";
            $run{$server} = 0;
        }
        $runcert{$server} = 0 if($runcert{$server});
    }
    killpid($verbose, $pidlist);
    #
    # cleanup server pid files
    #
    foreach my $server (@killservers) {
        my $pidfile = $serverpidfile{$server};
        my $pid = processexists($pidfile);
        if($pid > 0) {
            logmsg "Warning: $server server unexpectedly alive\n";
            killpid($verbose, $pid);
        }
        unlink($pidfile) if(-f $pidfile);
    }
}

#######################################################################
# Verify that the server that runs on $ip, $port is our server.  This also
# implies that we can speak with it, as there might be occasions when the
# server runs fine but we cannot talk to it ("Failed to connect to ::1: Can't
# assign requested address")
#
sub verifyhttp {
    my ($proto, $ipvnum, $idnum, $ip, $port_or_path) = @_;
    my $server = servername_id($proto, $ipvnum, $idnum);
    my $pid = 0;
    my $bonus="";
    # $port_or_path contains a path for Unix sockets, sws ignores the port
    my $port = ($ipvnum eq "unix") ? 80 : $port_or_path;

    my $verifyout = "$LOGDIR/".
        servername_canon($proto, $ipvnum, $idnum) .'_verify.out';
    unlink($verifyout) if(-f $verifyout);

    my $verifylog = "$LOGDIR/".
        servername_canon($proto, $ipvnum, $idnum) .'_verify.log';
    unlink($verifylog) if(-f $verifylog);

    if($proto eq "gopher") {
        # gopher is funny
        $bonus="1/";
    }

    my $flags = "--max-time $server_response_maxtime ";
    $flags .= "--output $verifyout ";
    $flags .= "--silent ";
    $flags .= "--verbose ";
    $flags .= "--globoff ";
    $flags .= "--unix-socket '$port_or_path' " if $ipvnum eq "unix";
    $flags .= "--insecure " if($proto eq 'https');
    $flags .= "\"$proto://$ip:$port/${bonus}verifiedserver\"";

    my $cmd = "$VCURL $flags 2>$verifylog";

    # verify if our/any server is running on this port
    logmsg "RUN: $cmd\n" if($verbose);
    my $res = runclient($cmd);

    $res >>= 8; # rotate the result
    if($res & 128) {
        logmsg "RUN: curl command died with a coredump\n";
        return -1;
    }

    if($res && $verbose) {
        logmsg "RUN: curl command returned $res\n";
        if(open(FILE, "<$verifylog")) {
            while(my $string = <FILE>) {
                logmsg "RUN: $string" if($string !~ /^([ \t]*)$/);
            }
            close(FILE);
        }
    }

    my $data;
    if(open(FILE, "<$verifyout")) {
        while(my $string = <FILE>) {
            $data = $string;
            last; # only want first line
        }
        close(FILE);
    }

    if($data && ($data =~ /WE ROOLZ: (\d+)/)) {
        $pid = 0+$1;
    }
    elsif($res == 6) {
        # curl: (6) Couldn't resolve host '::1'
        logmsg "RUN: failed to resolve host ($proto://$ip:$port/verifiedserver)\n";
        return -1;
    }
    elsif($data || ($res && ($res != 7))) {
        logmsg "RUN: Unknown server on our $server port: $port ($res)\n";
        return -1;
    }
    return $pid;
}

#######################################################################
# Verify that the server that runs on $ip, $port is our server.  This also
# implies that we can speak with it, as there might be occasions when the
# server runs fine but we cannot talk to it ("Failed to connect to ::1: Can't
# assign requested address")
#
sub verifyftp {
    my ($proto, $ipvnum, $idnum, $ip, $port) = @_;
    my $server = servername_id($proto, $ipvnum, $idnum);
    my $pid = 0;
    my $time=time();
    my $extra="";

    my $verifylog = "$LOGDIR/".
        servername_canon($proto, $ipvnum, $idnum) .'_verify.log';
    unlink($verifylog) if(-f $verifylog);

    if($proto eq "ftps") {
        $extra .= "--insecure --ftp-ssl-control ";
    }

    my $flags = "--max-time $server_response_maxtime ";
    $flags .= "--silent ";
    $flags .= "--verbose ";
    $flags .= "--globoff ";
    $flags .= $extra;
    $flags .= "\"$proto://$ip:$port/verifiedserver\"";

    my $cmd = "$VCURL $flags 2>$verifylog";

    # check if this is our server running on this port:
    logmsg "RUN: $cmd\n" if($verbose);
    my @data = runclientoutput($cmd);

    my $res = $? >> 8; # rotate the result
    if($res & 128) {
        logmsg "RUN: curl command died with a coredump\n";
        return -1;
    }

    foreach my $line (@data) {
        if($line =~ /WE ROOLZ: (\d+)/) {
            # this is our test server with a known pid!
            $pid = 0+$1;
            last;
        }
    }
    if($pid <= 0 && @data && $data[0]) {
        # this is not a known server
        logmsg "RUN: Unknown server on our $server port: $port\n";
        return 0;
    }
    # we can/should use the time it took to verify the FTP server as a measure
    # on how fast/slow this host/FTP is.
    my $took = int(0.5+time()-$time);

    if($verbose) {
        logmsg "RUN: Verifying our test $server server took $took seconds\n";
    }
    $ftpchecktime = $took>=1?$took:1; # make sure it never is below 1

    return $pid;
}

#######################################################################
# Verify that the server that runs on $ip, $port is our server.  This also
# implies that we can speak with it, as there might be occasions when the
# server runs fine but we cannot talk to it ("Failed to connect to ::1: Can't
# assign requested address")
#
sub verifyrtsp {
    my ($proto, $ipvnum, $idnum, $ip, $port) = @_;
    my $server = servername_id($proto, $ipvnum, $idnum);
    my $pid = 0;

    my $verifyout = "$LOGDIR/".
        servername_canon($proto, $ipvnum, $idnum) .'_verify.out';
    unlink($verifyout) if(-f $verifyout);

    my $verifylog = "$LOGDIR/".
        servername_canon($proto, $ipvnum, $idnum) .'_verify.log';
    unlink($verifylog) if(-f $verifylog);

    my $flags = "--max-time $server_response_maxtime ";
    $flags .= "--output $verifyout ";
    $flags .= "--silent ";
    $flags .= "--verbose ";
    $flags .= "--globoff ";
    # currently verification is done using http
    $flags .= "\"http://$ip:$port/verifiedserver\"";

    my $cmd = "$VCURL $flags 2>$verifylog";

    # verify if our/any server is running on this port
    logmsg "RUN: $cmd\n" if($verbose);
    my $res = runclient($cmd);

    $res >>= 8; # rotate the result
    if($res & 128) {
        logmsg "RUN: curl command died with a coredump\n";
        return -1;
    }

    if($res && $verbose) {
        logmsg "RUN: curl command returned $res\n";
        if(open(FILE, "<$verifylog")) {
            while(my $string = <FILE>) {
                logmsg "RUN: $string" if($string !~ /^([ \t]*)$/);
            }
            close(FILE);
        }
    }

    my $data;
    if(open(FILE, "<$verifyout")) {
        while(my $string = <FILE>) {
            $data = $string;
            last; # only want first line
        }
        close(FILE);
    }

    if($data && ($data =~ /RTSP_SERVER WE ROOLZ: (\d+)/)) {
        $pid = 0+$1;
    }
    elsif($res == 6) {
        # curl: (6) Couldn't resolve host '::1'
        logmsg "RUN: failed to resolve host ($proto://$ip:$port/verifiedserver)\n";
        return -1;
    }
    elsif($data || ($res != 7)) {
        logmsg "RUN: Unknown server on our $server port: $port\n";
        return -1;
    }
    return $pid;
}

#######################################################################
# Verify that the ssh server has written out its pidfile, recovering
# the pid from the file and returning it if a process with that pid is
# actually alive.
#
sub verifyssh {
    my ($proto, $ipvnum, $idnum, $ip, $port) = @_;
    my $server = servername_id($proto, $ipvnum, $idnum);
    my $pidfile = server_pidfilename($proto, $ipvnum, $idnum);
    my $pid = 0;
    if(open(FILE, "<$pidfile")) {
        $pid=0+<FILE>;
        close(FILE);
    }
    if($pid > 0) {
        # if we have a pid it is actually our ssh server,
        # since runsshserver() unlinks previous pidfile
        if(!pidexists($pid)) {
            logmsg "RUN: SSH server has died after starting up\n";
            checkdied($pid);
            unlink($pidfile);
            $pid = -1;
        }
    }
    return $pid;
}

#######################################################################
# Verify that we can connect to the sftp server, properly authenticate
# with generated config and key files and run a simple remote pwd.
#
sub verifysftp {
    my ($proto, $ipvnum, $idnum, $ip, $port) = @_;
    my $server = servername_id($proto, $ipvnum, $idnum);
    my $verified = 0;
    # Find out sftp client canonical file name
    my $sftp = find_sftp();
    if(!$sftp) {
        logmsg "RUN: SFTP server cannot find $sftpexe\n";
        return -1;
    }
    # Find out ssh client canonical file name
    my $ssh = find_ssh();
    if(!$ssh) {
        logmsg "RUN: SFTP server cannot find $sshexe\n";
        return -1;
    }
    # Connect to sftp server, authenticate and run a remote pwd
    # command using our generated configuration and key files
    my $cmd = "\"$sftp\" -b $sftpcmds -F $sftpconfig -S \"$ssh\" $ip > $sftplog 2>&1";
    my $res = runclient($cmd);
    # Search for pwd command response in log file
    if(open(SFTPLOGFILE, "<$sftplog")) {
        while(<SFTPLOGFILE>) {
            if(/^Remote working directory: /) {
                $verified = 1;
                last;
            }
        }
        close(SFTPLOGFILE);
    }
    return $verified;
}

#######################################################################
# Verify that the non-stunnel HTTP TLS extensions capable server that runs
# on $ip, $port is our server.  This also implies that we can speak with it,
# as there might be occasions when the server runs fine but we cannot talk
# to it ("Failed to connect to ::1: Can't assign requested address")
#
sub verifyhttptls {
    my ($proto, $ipvnum, $idnum, $ip, $port) = @_;
    my $server = servername_id($proto, $ipvnum, $idnum);
    my $pidfile = server_pidfilename($proto, $ipvnum, $idnum);
    my $pid = 0;

    my $verifyout = "$LOGDIR/".
        servername_canon($proto, $ipvnum, $idnum) .'_verify.out';
    unlink($verifyout) if(-f $verifyout);

    my $verifylog = "$LOGDIR/".
        servername_canon($proto, $ipvnum, $idnum) .'_verify.log';
    unlink($verifylog) if(-f $verifylog);

    my $flags = "--max-time $server_response_maxtime ";
    $flags .= "--output $verifyout ";
    $flags .= "--verbose ";
    $flags .= "--globoff ";
    $flags .= "--insecure ";
    $flags .= "--tlsauthtype SRP ";
    $flags .= "--tlsuser jsmith ";
    $flags .= "--tlspassword abc ";
    $flags .= "\"https://$ip:$port/verifiedserver\"";

    my $cmd = "$VCURL $flags 2>$verifylog";

    # verify if our/any server is running on this port
    logmsg "RUN: $cmd\n" if($verbose);
    my $res = runclient($cmd);

    $res >>= 8; # rotate the result
    if($res & 128) {
        logmsg "RUN: curl command died with a coredump\n";
        return -1;
    }

    if($res && $verbose) {
        logmsg "RUN: curl command returned $res\n";
        if(open(FILE, "<$verifylog")) {
            while(my $string = <FILE>) {
                logmsg "RUN: $string" if($string !~ /^([ \t]*)$/);
            }
            close(FILE);
        }
    }

    my $data;
    if(open(FILE, "<$verifyout")) {
        while(my $string = <FILE>) {
            $data .= $string;
        }
        close(FILE);
    }

    if($data && ($data =~ /(GNUTLS|GnuTLS)/) && open(FILE, "<$pidfile")) {
        $pid=0+<FILE>;
        close(FILE);
        if($pid > 0) {
            # if we have a pid it is actually our httptls server,
            # since runhttptlsserver() unlinks previous pidfile
            if(!pidexists($pid)) {
                logmsg "RUN: $server server has died after starting up\n";
                checkdied($pid);
                unlink($pidfile);
                $pid = -1;
            }
        }
        return $pid;
    }
    elsif($res == 6) {
        # curl: (6) Couldn't resolve host '::1'
        logmsg "RUN: failed to resolve host (https://$ip:$port/verifiedserver)\n";
        return -1;
    }
    elsif($data || ($res && ($res != 7))) {
        logmsg "RUN: Unknown server on our $server port: $port ($res)\n";
        return -1;
    }
    return $pid;
}

#######################################################################
# STUB for verifying socks
#
sub verifysocks {
    my ($proto, $ipvnum, $idnum, $ip, $port) = @_;
    my $server = servername_id($proto, $ipvnum, $idnum);
    my $pidfile = server_pidfilename($proto, $ipvnum, $idnum);
    my $pid = 0;
    if(open(FILE, "<$pidfile")) {
        $pid=0+<FILE>;
        close(FILE);
    }
    if($pid > 0) {
        # if we have a pid it is actually our socks server,
        # since runsocksserver() unlinks previous pidfile
        if(!pidexists($pid)) {
            logmsg "RUN: SOCKS server has died after starting up\n";
            checkdied($pid);
            unlink($pidfile);
            $pid = -1;
        }
    }
    return $pid;
}

#######################################################################
# Verify that the server that runs on $ip, $port is our server.  This also
# implies that we can speak with it, as there might be occasions when the
# server runs fine but we cannot talk to it ("Failed to connect to ::1: Can't
# assign requested address")
#
sub verifysmb {
    my ($proto, $ipvnum, $idnum, $ip, $port) = @_;
    my $server = servername_id($proto, $ipvnum, $idnum);
    my $pid = 0;
    my $time=time();
    my $extra="";

    my $verifylog = "$LOGDIR/".
        servername_canon($proto, $ipvnum, $idnum) .'_verify.log';
    unlink($verifylog) if(-f $verifylog);

    my $flags = "--max-time $server_response_maxtime ";
    $flags .= "--silent ";
    $flags .= "--verbose ";
    $flags .= "--globoff ";
    $flags .= "-u 'curltest:curltest' ";
    $flags .= $extra;
    $flags .= "\"$proto://$ip:$port/SERVER/verifiedserver\"";

    my $cmd = "$VCURL $flags 2>$verifylog";

    # check if this is our server running on this port:
    logmsg "RUN: $cmd\n" if($verbose);
    my @data = runclientoutput($cmd);

    my $res = $? >> 8; # rotate the result
    if($res & 128) {
        logmsg "RUN: curl command died with a coredump\n";
        return -1;
    }

    foreach my $line (@data) {
        if($line =~ /WE ROOLZ: (\d+)/) {
            # this is our test server with a known pid!
            $pid = 0+$1;
            last;
        }
    }
    if($pid <= 0 && @data && $data[0]) {
        # this is not a known server
        logmsg "RUN: Unknown server on our $server port: $port\n";
        return 0;
    }
    # we can/should use the time it took to verify the server as a measure
    # on how fast/slow this host is.
    my $took = int(0.5+time()-$time);

    if($verbose) {
        logmsg "RUN: Verifying our test $server server took $took seconds\n";
    }
    $ftpchecktime = $took>=1?$took:1; # make sure it never is below 1

    return $pid;
}

#######################################################################
# Verify that the server that runs on $ip, $port is our server.  This also
# implies that we can speak with it, as there might be occasions when the
# server runs fine but we cannot talk to it ("Failed to connect to ::1: Can't
# assign requested address")
#
sub verifytelnet {
    my ($proto, $ipvnum, $idnum, $ip, $port) = @_;
    my $server = servername_id($proto, $ipvnum, $idnum);
    my $pid = 0;
    my $time=time();
    my $extra="";

    my $verifylog = "$LOGDIR/".
        servername_canon($proto, $ipvnum, $idnum) .'_verify.log';
    unlink($verifylog) if(-f $verifylog);

    my $flags = "--max-time $server_response_maxtime ";
    $flags .= "--silent ";
    $flags .= "--verbose ";
    $flags .= "--globoff ";
    $flags .= "--upload-file - ";
    $flags .= $extra;
    $flags .= "\"$proto://$ip:$port\"";

    my $cmd = "echo 'verifiedserver' | $VCURL $flags 2>$verifylog";

    # check if this is our server running on this port:
    logmsg "RUN: $cmd\n" if($verbose);
    my @data = runclientoutput($cmd);

    my $res = $? >> 8; # rotate the result
    if($res & 128) {
        logmsg "RUN: curl command died with a coredump\n";
        return -1;
    }

    foreach my $line (@data) {
        if($line =~ /WE ROOLZ: (\d+)/) {
            # this is our test server with a known pid!
            $pid = 0+$1;
            last;
        }
    }
    if($pid <= 0 && @data && $data[0]) {
        # this is not a known server
        logmsg "RUN: Unknown server on our $server port: $port\n";
        return 0;
    }
    # we can/should use the time it took to verify the server as a measure
    # on how fast/slow this host is.
    my $took = int(0.5+time()-$time);

    if($verbose) {
        logmsg "RUN: Verifying our test $server server took $took seconds\n";
    }

    return $pid;
}


#######################################################################
# Verify that the server that runs on $ip, $port is our server.
# Retry over several seconds before giving up.  The ssh server in
# particular can take a long time to start if it needs to generate
# keys on a slow or loaded host.
#
# Just for convenience, test harness uses 'https' and 'httptls' literals
# as values for 'proto' variable in order to differentiate different
# servers. 'https' literal is used for stunnel based https test servers,
# and 'httptls' is used for non-stunnel https test servers.
#

my %protofunc = ('http' => \&verifyhttp,
                 'https' => \&verifyhttp,
                 'rtsp' => \&verifyrtsp,
                 'ftp' => \&verifyftp,
                 'pop3' => \&verifyftp,
                 'imap' => \&verifyftp,
                 'smtp' => \&verifyftp,
                 'ftps' => \&verifyftp,
                 'tftp' => \&verifyftp,
                 'ssh' => \&verifyssh,
                 'socks' => \&verifysocks,
                 'gopher' => \&verifyhttp,
                 'httptls' => \&verifyhttptls,
                 'dict' => \&verifyftp,
                 'smb' => \&verifysmb,
                 'telnet' => \&verifytelnet);

sub verifyserver {
    my ($proto, $ipvnum, $idnum, $ip, $port) = @_;

    my $count = 30; # try for this many seconds
    my $pid;

    while($count--) {
        my $fun = $protofunc{$proto};

        $pid = &$fun($proto, $ipvnum, $idnum, $ip, $port);

        if($pid > 0) {
            last;
        }
        elsif($pid < 0) {
            # a real failure, stop trying and bail out
            return 0;
        }
        sleep(1);
    }
    return $pid;
}

#######################################################################
# Single shot server responsiveness test. This should only be used
# to verify that a server present in %run hash is still functional
#
sub responsiveserver {
    my ($proto, $ipvnum, $idnum, $ip, $port) = @_;
    my $prev_verbose = $verbose;

    $verbose = 0;
    my $fun = $protofunc{$proto};
    my $pid = &$fun($proto, $ipvnum, $idnum, $ip, $port);
    $verbose = $prev_verbose;

    if($pid > 0) {
        return 1; # responsive
    }

    my $srvrname = servername_str($proto, $ipvnum, $idnum);
    logmsg " server precheck FAILED (unresponsive $srvrname server)\n";
    return 0;
}

#######################################################################
# start the http2 server
#
sub runhttp2server {
    my ($verbose, $port) = @_;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";
    my $proto="http/2";
    my $ipvnum = 4;
    my $idnum = 0;
    my $exe = "$perl $srcdir/http2-server.pl";
    my $verbose_flag = "--verbose ";

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--pidfile \"$pidfile\" --logfile \"$logfile\" ";
    $flags .= "--port $HTTP2PORT ";
    $flags .= "--connect $HOSTIP:$HTTPPORT ";
    $flags .= $verbose_flag if($debugprotocol);

    my $cmd = "$exe $flags";
    my ($http2pid, $pid2) = startnew($cmd, $pidfile, 15, 0);

    if($http2pid <= 0 || !pidexists($http2pid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $http2pid\n";
    }

    return ($http2pid, $pid2);
}

#######################################################################
# start the http server
#
sub runhttpserver {
    my ($proto, $verbose, $alt, $port_or_path) = @_;
    my $ip = $HOSTIP;
    my $ipvnum = 4;
    my $idnum = 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";
    my $exe = "$perl $srcdir/httpserver.pl";
    my $verbose_flag = "--verbose ";

    if($alt eq "ipv6") {
        # if IPv6, use a different setup
        $ipvnum = 6;
        $ip = $HOST6IP;
    }
    elsif($alt eq "proxy") {
        # basically the same, but another ID
        $idnum = 2;
    }
    elsif($alt eq "unix") {
        # IP (protocol) is mutually exclusive with Unix sockets
        $ipvnum = "unix";
    }

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--gopher " if($proto eq "gopher");
    $flags .= "--connect $HOSTIP " if($alt eq "proxy");
    $flags .= $verbose_flag if($debugprotocol);
    $flags .= "--pidfile \"$pidfile\" --logfile \"$logfile\" ";
    $flags .= "--id $idnum " if($idnum > 1);
    if($ipvnum eq "unix") {
        $flags .= "--unix-socket '$port_or_path' ";
    } else {
        $flags .= "--ipv$ipvnum --port $port_or_path ";
    }
    $flags .= "--srcdir \"$srcdir\"";

    my $cmd = "$exe $flags";
    my ($httppid, $pid2) = startnew($cmd, $pidfile, 15, 0);

    if($httppid <= 0 || !pidexists($httppid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    # Server is up. Verify that we can speak to it.
    my $pid3 = verifyserver($proto, $ipvnum, $idnum, $ip, $port_or_path);
    if(!$pid3) {
        logmsg "RUN: $srvrname server failed verification\n";
        # failed to talk to it properly. Kill the server and return failure
        stopserver($server, "$httppid $pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }
    $pid2 = $pid3;

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $httppid\n";
    }

    sleep(1);

    return ($httppid, $pid2);
}

#######################################################################
# start the https stunnel based server
#
sub runhttpsserver {
    my ($verbose, $ipv6, $certfile) = @_;
    my $proto = 'https';
    my $ip = ($ipv6 && ($ipv6 =~ /6$/)) ? "$HOST6IP" : "$HOSTIP";
    my $ipvnum = ($ipv6 && ($ipv6 =~ /6$/)) ? 6 : 4;
    my $idnum = 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";

    if(!$stunnel) {
        return (0,0);
    }

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $certfile = 'stunnel.pem' unless($certfile);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--verbose " if($debugprotocol);
    $flags .= "--pidfile \"$pidfile\" --logfile \"$logfile\" ";
    $flags .= "--id $idnum " if($idnum > 1);
    $flags .= "--ipv$ipvnum --proto $proto ";
    $flags .= "--certfile \"$certfile\" " if($certfile ne 'stunnel.pem');
    $flags .= "--stunnel \"$stunnel\" --srcdir \"$srcdir\" ";
    $flags .= "--connect $HTTPPORT --accept $HTTPSPORT";

    my $cmd = "$perl $srcdir/secureserver.pl $flags";
    my ($httpspid, $pid2) = startnew($cmd, $pidfile, 15, 0);

    if($httpspid <= 0 || !pidexists($httpspid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return(0,0);
    }

    # Server is up. Verify that we can speak to it.
    my $pid3 = verifyserver($proto, $ipvnum, $idnum, $ip, $HTTPSPORT);
    if(!$pid3) {
        logmsg "RUN: $srvrname server failed verification\n";
        # failed to talk to it properly. Kill the server and return failure
        stopserver($server, "$httpspid $pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }
    # Here pid3 is actually the pid returned by the unsecure-http server.

    $runcert{$server} = $certfile;

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $httpspid\n";
    }

    sleep(1);

    return ($httpspid, $pid2);
}

#######################################################################
# start the non-stunnel HTTP TLS extensions capable server
#
sub runhttptlsserver {
    my ($verbose, $ipv6) = @_;
    my $proto = "httptls";
    my $port = ($ipv6 && ($ipv6 =~ /6$/)) ? $HTTPTLS6PORT : $HTTPTLSPORT;
    my $ip = ($ipv6 && ($ipv6 =~ /6$/)) ? "$HOST6IP" : "$HOSTIP";
    my $ipvnum = ($ipv6 && ($ipv6 =~ /6$/)) ? 6 : 4;
    my $idnum = 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";

    if(!$httptlssrv) {
        return (0,0);
    }

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--http ";
    $flags .= "--debug 1 " if($debugprotocol);
    $flags .= "--port $port ";
    $flags .= "--priority NORMAL:+SRP ";
    $flags .= "--srppasswd $srcdir/certs/srp-verifier-db ";
    $flags .= "--srppasswdconf $srcdir/certs/srp-verifier-conf";

    my $cmd = "$httptlssrv $flags > $logfile 2>&1";
    my ($httptlspid, $pid2) = startnew($cmd, $pidfile, 10, 1); # fake pidfile

    if($httptlspid <= 0 || !pidexists($httptlspid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    # Server is up. Verify that we can speak to it. PID is from fake pidfile
    my $pid3 = verifyserver($proto, $ipvnum, $idnum, $ip, $port);
    if(!$pid3) {
        logmsg "RUN: $srvrname server failed verification\n";
        # failed to talk to it properly. Kill the server and return failure
        stopserver($server, "$httptlspid $pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }
    $pid2 = $pid3;

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $httptlspid\n";
    }

    sleep(1);

    return ($httptlspid, $pid2);
}

#######################################################################
# start the pingpong server (FTP, POP3, IMAP, SMTP)
#
sub runpingpongserver {
    my ($proto, $id, $verbose, $ipv6) = @_;
    my $port;
    my $ip = ($ipv6 && ($ipv6 =~ /6$/)) ? "$HOST6IP" : "$HOSTIP";
    my $ipvnum = ($ipv6 && ($ipv6 =~ /6$/)) ? 6 : 4;
    my $idnum = ($id && ($id =~ /^(\d+)$/) && ($id > 1)) ? $id : 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";

    if($proto eq "ftp") {
        $port = ($idnum>1)?$FTP2PORT:$FTPPORT;

        if($ipvnum==6) {
            # if IPv6, use a different setup
            $port = $FTP6PORT;
        }
    }
    elsif($proto eq "pop3") {
        $port = ($ipvnum==6) ? $POP36PORT : $POP3PORT;
    }
    elsif($proto eq "imap") {
        $port = ($ipvnum==6) ? $IMAP6PORT : $IMAPPORT;
    }
    elsif($proto eq "smtp") {
        $port = ($ipvnum==6) ? $SMTP6PORT : $SMTPPORT;
    }
    else {
        print STDERR "Unsupported protocol $proto!!\n";
        return 0;
    }

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--verbose " if($debugprotocol);
    $flags .= "--pidfile \"$pidfile\" --logfile \"$logfile\" ";
    $flags .= "--srcdir \"$srcdir\" --proto $proto ";
    $flags .= "--id $idnum " if($idnum > 1);
    $flags .= "--ipv$ipvnum --port $port --addr \"$ip\"";

    my $cmd = "$perl $srcdir/ftpserver.pl $flags";
    my ($ftppid, $pid2) = startnew($cmd, $pidfile, 15, 0);

    if($ftppid <= 0 || !pidexists($ftppid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    # Server is up. Verify that we can speak to it.
    my $pid3 = verifyserver($proto, $ipvnum, $idnum, $ip, $port);
    if(!$pid3) {
        logmsg "RUN: $srvrname server failed verification\n";
        # failed to talk to it properly. Kill the server and return failure
        stopserver($server, "$ftppid $pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    $pid2 = $pid3;

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $ftppid\n";
    }

    sleep(1);

    return ($pid2, $ftppid);
}

#######################################################################
# start the ftps server (or rather, tunnel)
#
sub runftpsserver {
    my ($verbose, $ipv6, $certfile) = @_;
    my $proto = 'ftps';
    my $ip = ($ipv6 && ($ipv6 =~ /6$/)) ? "$HOST6IP" : "$HOSTIP";
    my $ipvnum = ($ipv6 && ($ipv6 =~ /6$/)) ? 6 : 4;
    my $idnum = 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";

    if(!$stunnel) {
        return (0,0);
    }

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $certfile = 'stunnel.pem' unless($certfile);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--verbose " if($debugprotocol);
    $flags .= "--pidfile \"$pidfile\" --logfile \"$logfile\" ";
    $flags .= "--id $idnum " if($idnum > 1);
    $flags .= "--ipv$ipvnum --proto $proto ";
    $flags .= "--certfile \"$certfile\" " if($certfile ne 'stunnel.pem');
    $flags .= "--stunnel \"$stunnel\" --srcdir \"$srcdir\" ";
    $flags .= "--connect $FTPPORT --accept $FTPSPORT";

    my $cmd = "$perl $srcdir/secureserver.pl $flags";
    my ($ftpspid, $pid2) = startnew($cmd, $pidfile, 15, 0);

    if($ftpspid <= 0 || !pidexists($ftpspid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return(0,0);
    }

    # Server is up. Verify that we can speak to it.
    my $pid3 = verifyserver($proto, $ipvnum, $idnum, $ip, $FTPSPORT);
    if(!$pid3) {
        logmsg "RUN: $srvrname server failed verification\n";
        # failed to talk to it properly. Kill the server and return failure
        stopserver($server, "$ftpspid $pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }
    # Here pid3 is actually the pid returned by the unsecure-ftp server.

    $runcert{$server} = $certfile;

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $ftpspid\n";
    }

    sleep(1);

    return ($ftpspid, $pid2);
}

#######################################################################
# start the tftp server
#
sub runtftpserver {
    my ($id, $verbose, $ipv6) = @_;
    my $port = $TFTPPORT;
    my $ip = $HOSTIP;
    my $proto = 'tftp';
    my $ipvnum = 4;
    my $idnum = ($id && ($id =~ /^(\d+)$/) && ($id > 1)) ? $id : 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";

    if($ipv6) {
        # if IPv6, use a different setup
        $ipvnum = 6;
        $port = $TFTP6PORT;
        $ip = $HOST6IP;
    }

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--verbose " if($debugprotocol);
    $flags .= "--pidfile \"$pidfile\" --logfile \"$logfile\" ";
    $flags .= "--id $idnum " if($idnum > 1);
    $flags .= "--ipv$ipvnum --port $port --srcdir \"$srcdir\"";

    my $cmd = "$perl $srcdir/tftpserver.pl $flags";
    my ($tftppid, $pid2) = startnew($cmd, $pidfile, 15, 0);

    if($tftppid <= 0 || !pidexists($tftppid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    # Server is up. Verify that we can speak to it.
    my $pid3 = verifyserver($proto, $ipvnum, $idnum, $ip, $port);
    if(!$pid3) {
        logmsg "RUN: $srvrname server failed verification\n";
        # failed to talk to it properly. Kill the server and return failure
        stopserver($server, "$tftppid $pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }
    $pid2 = $pid3;

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $tftppid\n";
    }

    sleep(1);

    return ($pid2, $tftppid);
}


#######################################################################
# start the rtsp server
#
sub runrtspserver {
    my ($verbose, $ipv6) = @_;
    my $port = $RTSPPORT;
    my $ip = $HOSTIP;
    my $proto = 'rtsp';
    my $ipvnum = 4;
    my $idnum = 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";

    if($ipv6) {
        # if IPv6, use a different setup
        $ipvnum = 6;
        $port = $RTSP6PORT;
        $ip = $HOST6IP;
    }

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--verbose " if($debugprotocol);
    $flags .= "--pidfile \"$pidfile\" --logfile \"$logfile\" ";
    $flags .= "--id $idnum " if($idnum > 1);
    $flags .= "--ipv$ipvnum --port $port --srcdir \"$srcdir\"";

    my $cmd = "$perl $srcdir/rtspserver.pl $flags";
    my ($rtsppid, $pid2) = startnew($cmd, $pidfile, 15, 0);

    if($rtsppid <= 0 || !pidexists($rtsppid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    # Server is up. Verify that we can speak to it.
    my $pid3 = verifyserver($proto, $ipvnum, $idnum, $ip, $port);
    if(!$pid3) {
        logmsg "RUN: $srvrname server failed verification\n";
        # failed to talk to it properly. Kill the server and return failure
        stopserver($server, "$rtsppid $pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }
    $pid2 = $pid3;

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $rtsppid\n";
    }

    sleep(1);

    return ($rtsppid, $pid2);
}


#######################################################################
# Start the ssh (scp/sftp) server
#
sub runsshserver {
    my ($id, $verbose, $ipv6) = @_;
    my $ip=$HOSTIP;
    my $port = $SSHPORT;
    my $proto = 'ssh';
    my $ipvnum = 4;
    my $idnum = ($id && ($id =~ /^(\d+)$/) && ($id > 1)) ? $id : 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--verbose " if($verbose);
    $flags .= "--debugprotocol " if($debugprotocol);
    $flags .= "--pidfile \"$pidfile\" ";
    $flags .= "--id $idnum " if($idnum > 1);
    $flags .= "--ipv$ipvnum --addr \"$ip\" ";
    $flags .= "--sshport $port ";
    $flags .= "--user \"$USER\"";

    my $cmd = "$perl $srcdir/sshserver.pl $flags";
    my ($sshpid, $pid2) = startnew($cmd, $pidfile, 60, 0);

    # on loaded systems sshserver start up can take longer than the timeout
    # passed to startnew, when this happens startnew completes without being
    # able to read the pidfile and consequently returns a zero pid2 above.

    if($sshpid <= 0 || !pidexists($sshpid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    # ssh server verification allows some extra time for the server to start up
    # and gives us the opportunity of recovering the pid from the pidfile, when
    # this verification succeeds the recovered pid is assigned to pid2.

    my $pid3 = verifyserver($proto, $ipvnum, $idnum, $ip, $port);
    if(!$pid3) {
        logmsg "RUN: $srvrname server failed verification\n";
        # failed to fetch server pid. Kill the server and return failure
        stopserver($server, "$sshpid $pid2");
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }
    $pid2 = $pid3;

    # once it is known that the ssh server is alive, sftp server verification
    # is performed actually connecting to it, authenticating and performing a
    # very simple remote command.  This verification is tried only one time.

    $sshdlog = server_logfilename($LOGDIR, 'ssh', $ipvnum, $idnum);
    $sftplog = server_logfilename($LOGDIR, 'sftp', $ipvnum, $idnum);

    if(verifysftp('sftp', $ipvnum, $idnum, $ip, $port) < 1) {
        logmsg "RUN: SFTP server failed verification\n";
        # failed to talk to it properly. Kill the server and return failure
        display_sftplog();
        display_sftpconfig();
        display_sshdlog();
        display_sshdconfig();
        stopserver($server, "$sshpid $pid2");
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $pid2\n";
    }

    return ($pid2, $sshpid);
}

#######################################################################
# Start the socks server
#
sub runsocksserver {
    my ($id, $verbose, $ipv6) = @_;
    my $ip=$HOSTIP;
    my $port = $SOCKSPORT;
    my $proto = 'socks';
    my $ipvnum = 4;
    my $idnum = ($id && ($id =~ /^(\d+)$/) && ($id > 1)) ? $id : 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    # start our socks server, get commands from the FTP cmd file
    my $cmd="server/socksd".
        " --port $port ".
        " --pidfile $pidfile".
        " --backend $HOSTIP".
        " --config $FTPDCMD";
    my ($sockspid, $pid2) = startnew($cmd, $pidfile, 30, 0);

    if($sockspid <= 0 || !pidexists($sockspid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $pid2\n";
    }

    return ($pid2, $sockspid);
}

#######################################################################
# start the dict server
#
sub rundictserver {
    my ($verbose, $alt, $port) = @_;
    my $proto = "dict";
    my $ip = $HOSTIP;
    my $ipvnum = 4;
    my $idnum = 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";

    if($alt eq "ipv6") {
        # No IPv6
    }

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--verbose 1 " if($debugprotocol);
    $flags .= "--pidfile \"$pidfile\" --logfile \"$logfile\" ";
    $flags .= "--id $idnum " if($idnum > 1);
    $flags .= "--port $port --srcdir \"$srcdir\" ";
    $flags .= "--host $HOSTIP";

    my $cmd = "$srcdir/dictserver.py $flags";
    my ($dictpid, $pid2) = startnew($cmd, $pidfile, 15, 0);

    if($dictpid <= 0 || !pidexists($dictpid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    # Server is up. Verify that we can speak to it.
    my $pid3 = verifyserver($proto, $ipvnum, $idnum, $ip, $port);
    if(!$pid3) {
        logmsg "RUN: $srvrname server failed verification\n";
        # failed to talk to it properly. Kill the server and return failure
        stopserver($server, "$dictpid $pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }
    $pid2 = $pid3;

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $dictpid\n";
    }

    sleep(1);

    return ($dictpid, $pid2);
}

#######################################################################
# start the SMB server
#
sub runsmbserver {
    my ($verbose, $alt, $port) = @_;
    my $proto = "smb";
    my $ip = $HOSTIP;
    my $ipvnum = 4;
    my $idnum = 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";

    if($alt eq "ipv6") {
        # No IPv6
    }

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--verbose 1 " if($debugprotocol);
    $flags .= "--pidfile \"$pidfile\" --logfile \"$logfile\" ";
    $flags .= "--id $idnum " if($idnum > 1);
    $flags .= "--port $port --srcdir \"$srcdir\" ";
    $flags .= "--host $HOSTIP";

    my $cmd = "$srcdir/smbserver.py $flags";
    my ($smbpid, $pid2) = startnew($cmd, $pidfile, 15, 0);

    if($smbpid <= 0 || !pidexists($smbpid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    # Server is up. Verify that we can speak to it.
    my $pid3 = verifyserver($proto, $ipvnum, $idnum, $ip, $port);
    if(!$pid3) {
        logmsg "RUN: $srvrname server failed verification\n";
        # failed to talk to it properly. Kill the server and return failure
        stopserver($server, "$smbpid $pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }
    $pid2 = $pid3;

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $smbpid\n";
    }

    sleep(1);

    return ($smbpid, $pid2);
}

#######################################################################
# start the telnet server
#
sub runnegtelnetserver {
    my ($verbose, $alt, $port) = @_;
    my $proto = "telnet";
    my $ip = $HOSTIP;
    my $ipvnum = 4;
    my $idnum = 1;
    my $server;
    my $srvrname;
    my $pidfile;
    my $logfile;
    my $flags = "";

    if($alt eq "ipv6") {
        # No IPv6
    }

    $server = servername_id($proto, $ipvnum, $idnum);

    $pidfile = $serverpidfile{$server};

    # don't retry if the server doesn't work
    if ($doesntrun{$pidfile}) {
        return (0,0);
    }

    my $pid = processexists($pidfile);
    if($pid > 0) {
        stopserver($server, "$pid");
    }
    unlink($pidfile) if(-f $pidfile);

    $srvrname = servername_str($proto, $ipvnum, $idnum);

    $logfile = server_logfilename($LOGDIR, $proto, $ipvnum, $idnum);

    $flags .= "--verbose 1 " if($debugprotocol);
    $flags .= "--pidfile \"$pidfile\" --logfile \"$logfile\" ";
    $flags .= "--id $idnum " if($idnum > 1);
    $flags .= "--port $port --srcdir \"$srcdir\"";

    my $cmd = "$srcdir/negtelnetserver.py $flags";
    my ($ntelpid, $pid2) = startnew($cmd, $pidfile, 15, 0);

    if($ntelpid <= 0 || !pidexists($ntelpid)) {
        # it is NOT alive
        logmsg "RUN: failed to start the $srvrname server\n";
        stopserver($server, "$pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }

    # Server is up. Verify that we can speak to it.
    my $pid3 = verifyserver($proto, $ipvnum, $idnum, $ip, $port);
    if(!$pid3) {
        logmsg "RUN: $srvrname server failed verification\n";
        # failed to talk to it properly. Kill the server and return failure
        stopserver($server, "$ntelpid $pid2");
        displaylogs($testnumcheck);
        $doesntrun{$pidfile} = 1;
        return (0,0);
    }
    $pid2 = $pid3;

    if($verbose) {
        logmsg "RUN: $srvrname server is now running PID $ntelpid\n";
    }

    sleep(1);

    return ($ntelpid, $pid2);
}


#######################################################################
# Single shot http and gopher server responsiveness test. This should only
# be used to verify that a server present in %run hash is still functional
#
sub responsive_http_server {
    my ($proto, $verbose, $alt, $port_or_path) = @_;
    my $ip = $HOSTIP;
    my $ipvnum = 4;
    my $idnum = 1;

    if($alt eq "ipv6") {
        # if IPv6, use a different setup
        $ipvnum = 6;
        $ip = $HOST6IP;
    }
    elsif($alt eq "proxy") {
        $idnum = 2;
    }
    elsif($alt eq "unix") {
        # IP (protocol) is mutually exclusive with Unix sockets
        $ipvnum = "unix";
    }

    return &responsiveserver($proto, $ipvnum, $idnum, $ip, $port_or_path);
}

#######################################################################
# Single shot pingpong server responsiveness test. This should only be
# used to verify that a server present in %run hash is still functional
#
sub responsive_pingpong_server {
    my ($proto, $id, $verbose, $ipv6) = @_;
    my $port;
    my $ip = ($ipv6 && ($ipv6 =~ /6$/)) ? "$HOST6IP" : "$HOSTIP";
    my $ipvnum = ($ipv6 && ($ipv6 =~ /6$/)) ? 6 : 4;
    my $idnum = ($id && ($id =~ /^(\d+)$/) && ($id > 1)) ? $id : 1;

    if($proto eq "ftp") {
        $port = ($idnum>1)?$FTP2PORT:$FTPPORT;

        if($ipvnum==6) {
            # if IPv6, use a different setup
            $port = $FTP6PORT;
        }
    }
    elsif($proto eq "pop3") {
        $port = ($ipvnum==6) ? $POP36PORT : $POP3PORT;
    }
    elsif($proto eq "imap") {
        $port = ($ipvnum==6) ? $IMAP6PORT : $IMAPPORT;
    }
    elsif($proto eq "smtp") {
        $port = ($ipvnum==6) ? $SMTP6PORT : $SMTPPORT;
    }
    else {
        print STDERR "Unsupported protocol $proto!!\n";
        return 0;
    }

    return &responsiveserver($proto, $ipvnum, $idnum, $ip, $port);
}

#######################################################################
# Single shot rtsp server responsiveness test. This should only be
# used to verify that a server present in %run hash is still functional
#
sub responsive_rtsp_server {
    my ($verbose, $ipv6) = @_;
    my $port = $RTSPPORT;
    my $ip = $HOSTIP;
    my $proto = 'rtsp';
    my $ipvnum = 4;
    my $idnum = 1;

    if($ipv6) {
        # if IPv6, use a different setup
        $ipvnum = 6;
        $port = $RTSP6PORT;
        $ip = $HOST6IP;
    }

    return &responsiveserver($proto, $ipvnum, $idnum, $ip, $port);
}

#######################################################################
# Single shot tftp server responsiveness test. This should only be
# used to verify that a server present in %run hash is still functional
#
sub responsive_tftp_server {
    my ($id, $verbose, $ipv6) = @_;
    my $port = $TFTPPORT;
    my $ip = $HOSTIP;
    my $proto = 'tftp';
    my $ipvnum = 4;
    my $idnum = ($id && ($id =~ /^(\d+)$/) && ($id > 1)) ? $id : 1;

    if($ipv6) {
        # if IPv6, use a different setup
        $ipvnum = 6;
        $port = $TFTP6PORT;
        $ip = $HOST6IP;
    }

    return &responsiveserver($proto, $ipvnum, $idnum, $ip, $port);
}

#######################################################################
# Single shot non-stunnel HTTP TLS extensions capable server
# responsiveness test. This should only be used to verify that a
# server present in %run hash is still functional
#
sub responsive_httptls_server {
    my ($verbose, $ipv6) = @_;
    my $proto = "httptls";
    my $port = ($ipv6 && ($ipv6 =~ /6$/)) ? $HTTPTLS6PORT : $HTTPTLSPORT;
    my $ip = ($ipv6 && ($ipv6 =~ /6$/)) ? "$HOST6IP" : "$HOSTIP";
    my $ipvnum = ($ipv6 && ($ipv6 =~ /6$/)) ? 6 : 4;
    my $idnum = 1;

    return &responsiveserver($proto, $ipvnum, $idnum, $ip, $port);
}

#######################################################################
# Remove all files in the specified directory
#
sub cleardir {
    my $dir = $_[0];
    my $count;
    my $file;

    # Get all files
    opendir(DIR, $dir) ||
        return 0; # can't open dir
    while($file = readdir(DIR)) {
        if($file !~ /^\./) {
            unlink("$dir/$file");
            $count++;
        }
    }
    closedir DIR;
    return $count;
}

#######################################################################
# compare test results with the expected output, we might filter off
# some pattern that is allowed to differ, output test results
#
sub compare {
    my ($testnum, $testname, $subject, $firstref, $secondref)=@_;

    my $result = compareparts($firstref, $secondref);

    if($result) {
        # timestamp test result verification end
        $timevrfyend{$testnum} = Time::HiRes::time();

        if(!$short) {
            logmsg "\n $testnum: $subject FAILED:\n";
            logmsg showdiff($LOGDIR, $firstref, $secondref);
        }
        elsif(!$automakestyle) {
            logmsg "FAILED\n";
        }
        else {
            # automakestyle
            logmsg "FAIL: $testnum - $testname - $subject\n";
        }
    }
    return $result;
}

sub setupfeatures {
    $feature{"SSL"} = $has_ssl;
    $feature{"MultiSSL"} = $has_multissl;
    $feature{"SSLpinning"} = $has_sslpinning;
    $feature{"OpenSSL"} = $has_openssl;
    $feature{"GnuTLS"} = $has_gnutls;
    $feature{"NSS"} = $has_nss;
    $feature{"WinSSL"} = $has_winssl;
    $feature{"Schannel"} = $has_winssl; # alias
    $feature{"sectransp"} = $has_darwinssl;
    $feature{"DarwinSSL"} = $has_darwinssl; # alias
    $feature{"ld_preload"} = ($has_ldpreload && !$debug_build);
    $feature{"unittest"} = $debug_build;
    $feature{"debug"} = $debug_build;
    $feature{"TrackMemory"} = $has_memory_tracking;
    $feature{"large_file"} = $has_largefile;
    $feature{"idn"} = $has_idn;
    $feature{"ipv6"} = $has_ipv6;
    $feature{"libz"} = $has_libz;
    $feature{"brotli"} = $has_brotli;
    $feature{"NTLM"} = $has_ntlm;
    $feature{"NTLM_WB"} = $has_ntlm_wb;
    $feature{"SSPI"} = $has_sspi;
    $feature{"GSS-API"} = $has_gssapi;
    $feature{"Kerberos"} = $has_kerberos;
    $feature{"SPNEGO"} = $has_spnego;
    $feature{"getrlimit"} = $has_getrlimit;
    $feature{"crypto"} = $has_crypto;
    $feature{"TLS-SRP"} = $has_tls_srp;
    $feature{"Metalink"} = $has_metalink;
    $feature{"http/2"} = $has_http2;
    $feature{"threaded-resolver"} = $has_threadedres;
    $feature{"PSL"} = $has_psl;
    $feature{"alt-svc"} = $has_altsvc;
    $feature{"manual"} = $has_manual;
    $feature{"unix-sockets"} = $has_unix;

    # make each protocol an enabled "feature"
    for my $p (@protocols) {
        $feature{$p} = 1;
    }
    # 'socks' was once here but is now removed

    #
    # strings that must match the names used in server/disabled.c
    #
    $feature{"cookies"} = 1;
    $feature{"DoH"} = 1;
    $feature{"HTTP-auth"} = 1;
    $feature{"Mime"} = 1;
    $feature{"netrc"} = 1;
    $feature{"parsedate"} = 1;
    $feature{"proxy"} = 1;
    $feature{"shuffle-dns"} = 1;
    $feature{"typecheck"} = 1;
    $feature{"verbose-strings"} = 1;

}

#######################################################################
# display information about curl and the host the test suite runs on
#
sub checksystem {

    unlink($memdump); # remove this if there was one left

    my $feat;
    my $curl;
    my $libcurl;
    my $versretval;
    my $versnoexec;
    my @version=();
    my @disabled;
    my $dis = "";

    my $curlverout="$LOGDIR/curlverout.log";
    my $curlvererr="$LOGDIR/curlvererr.log";
    my $versioncmd="$CURL --version 1>$curlverout 2>$curlvererr";

    unlink($curlverout);
    unlink($curlvererr);

    $versretval = runclient($versioncmd);
    $versnoexec = $!;

    open(VERSOUT, "<$curlverout");
    @version = <VERSOUT>;
    close(VERSOUT);

    open(DISABLED, "server/disabled|");
    @disabled = <DISABLED>;
    close(DISABLED);

    if($disabled[0]) {
        map s/[\r\n]//g, @disabled;
        $dis = join(", ", @disabled);
    }

    $resolver="stock";
    for(@version) {
        chomp;

        if($_ =~ /^curl/) {
            $curl = $_;
            $curl =~ s/^(.*)(libcurl.*)/$1/g;

            $libcurl = $2;
            if($curl =~ /linux|bsd|solaris/) {
                $has_ldpreload = 1;
            }
            if($curl =~ /win32|Windows|mingw(32|64)/) {
                # This is a Windows MinGW build or native build, we need to use
                # Win32-style path.
                $pwd = pathhelp::sys_native_current_path();
            }
           if ($libcurl =~ /(winssl|schannel)/i) {
               $has_winssl=1;
               $has_sslpinning=1;
           }
           elsif ($libcurl =~ /openssl/i) {
               $has_openssl=1;
               $has_sslpinning=1;
           }
           elsif ($libcurl =~ /gnutls/i) {
               $has_gnutls=1;
               $has_sslpinning=1;
           }
           elsif ($libcurl =~ /nss/i) {
               $has_nss=1;
               $has_sslpinning=1;
           }
           elsif ($libcurl =~ /wolfssl/i) {
               $has_wolfssl=1;
               $has_sslpinning=1;
           }
           elsif ($libcurl =~ /polarssl/i) {
               $has_polarssl=1;
               $has_sslpinning=1;
           }
           elsif ($libcurl =~ /securetransport/i) {
               $has_darwinssl=1;
               $has_sslpinning=1;
           }
           elsif ($libcurl =~ /BoringSSL/i) {
               $has_boringssl=1;
               $has_sslpinning=1;
           }
           elsif ($libcurl =~ /libressl/i) {
               $has_libressl=1;
               $has_sslpinning=1;
           }
           elsif ($libcurl =~ /mbedTLS/i) {
               $has_mbedtls=1;
               $has_sslpinning=1;
           }
           if ($libcurl =~ /ares/i) {
               $has_cares=1;
               $resolver="c-ares";
           }
           if ($libcurl =~ /mesalink/i) {
               $has_mesalink=1;
           }
        }
        elsif($_ =~ /^Protocols: (.*)/i) {
            # these are the protocols compiled in to this libcurl
            @protocols = split(' ', lc($1));

            # Generate a "proto-ipv6" version of each protocol to match the
            # IPv6 <server> name and a "proto-unix" to match the variant which
            # uses Unix domain sockets. This works even if support isn't
            # compiled in because the <features> test will fail.
            push @protocols, map(("$_-ipv6", "$_-unix"), @protocols);

            # 'http-proxy' is used in test cases to do CONNECT through
            push @protocols, 'http-proxy';

            # 'none' is used in test cases to mean no server
            push @protocols, 'none';
        }
        elsif($_ =~ /^Features: (.*)/i) {
            $feat = $1;
            if($feat =~ /TrackMemory/i) {
                # built with memory tracking support (--enable-curldebug)
                $has_memory_tracking = 1;
            }
            if($feat =~ /debug/i) {
                # curl was built with --enable-debug
                $debug_build = 1;
            }
            if($feat =~ /SSL/i) {
                # ssl enabled
                $has_ssl=1;
            }
            if($feat =~ /MultiSSL/i) {
                # multiple ssl backends available.
                $has_multissl=1;
            }
            if($feat =~ /Largefile/i) {
                # large file support
                $has_largefile=1;
            }
            if($feat =~ /IDN/i) {
                # IDN support
                $has_idn=1;
            }
            if($feat =~ /IPv6/i) {
                $has_ipv6 = 1;
            }
            if($feat =~ /UnixSockets/i) {
                $has_unix = 1;
            }
            if($feat =~ /libz/i) {
                $has_libz = 1;
            }
            if($feat =~ /brotli/i) {
                $has_brotli = 1;
            }
            if($feat =~ /NTLM/i) {
                # NTLM enabled
                $has_ntlm=1;

                # Use this as a proxy for any cryptographic authentication
                $has_crypto=1;
            }
            if($feat =~ /NTLM_WB/i) {
                # NTLM delegation to winbind daemon ntlm_auth helper enabled
                $has_ntlm_wb=1;
            }
            if($feat =~ /SSPI/i) {
                # SSPI enabled
                $has_sspi=1;
            }
            if($feat =~ /GSS-API/i) {
                # GSS-API enabled
                $has_gssapi=1;
            }
            if($feat =~ /Kerberos/i) {
                # Kerberos enabled
                $has_kerberos=1;

                # Use this as a proxy for any cryptographic authentication
                $has_crypto=1;
            }
            if($feat =~ /SPNEGO/i) {
                # SPNEGO enabled
                $has_spnego=1;

                # Use this as a proxy for any cryptographic authentication
                $has_crypto=1;
            }
            if($feat =~ /CharConv/i) {
                # CharConv enabled
                $has_charconv=1;
            }
            if($feat =~ /TLS-SRP/i) {
                # TLS-SRP enabled
                $has_tls_srp=1;
            }
            if($feat =~ /Metalink/i) {
                # Metalink enabled
                $has_metalink=1;
            }
            if($feat =~ /PSL/i) {
                # PSL enabled
                $has_psl=1;
            }
            if($feat =~ /alt-svc/i) {
                # alt-svc enabled
                $has_altsvc=1;
            }
            if($feat =~ /AsynchDNS/i) {
                if(!$has_cares) {
                    # this means threaded resolver
                    $has_threadedres=1;
                    $resolver="threaded";
                }
            }
            if($feat =~ /HTTP2/) {
                # http2 enabled
                $has_http2=1;

                push @protocols, 'http/2';
            }
        }
        #
        # Test harness currently uses a non-stunnel server in order to
        # run HTTP TLS-SRP tests required when curl is built with https
        # protocol support and TLS-SRP feature enabled. For convenience
        # 'httptls' may be included in the test harness protocols array
        # to differentiate this from classic stunnel based 'https' test
        # harness server.
        #
        if($has_tls_srp) {
            my $add_httptls;
            for(@protocols) {
                if($_ =~ /^https(-ipv6|)$/) {
                    $add_httptls=1;
                    last;
                }
            }
            if($add_httptls && (! grep /^httptls$/, @protocols)) {
                push @protocols, 'httptls';
                push @protocols, 'httptls-ipv6';
            }
        }
    }
    if(!$curl) {
        logmsg "unable to get curl's version, further details are:\n";
        logmsg "issued command: \n";
        logmsg "$versioncmd \n";
        if ($versretval == -1) {
            logmsg "command failed with: \n";
            logmsg "$versnoexec \n";
        }
        elsif ($versretval & 127) {
            logmsg sprintf("command died with signal %d, and %s coredump.\n",
                           ($versretval & 127), ($versretval & 128)?"a":"no");
        }
        else {
            logmsg sprintf("command exited with value %d \n", $versretval >> 8);
        }
        logmsg "contents of $curlverout: \n";
        displaylogcontent("$curlverout");
        logmsg "contents of $curlvererr: \n";
        displaylogcontent("$curlvererr");
        die "couldn't get curl's version";
    }

    if(-r "../lib/curl_config.h") {
        open(CONF, "<../lib/curl_config.h");
        while(<CONF>) {
            if($_ =~ /^\#define HAVE_GETRLIMIT/) {
                $has_getrlimit = 1;
            }
        }
        close(CONF);
    }

    if($has_ipv6) {
        # client has IPv6 support

        # check if the HTTP server has it!
        my @sws = `server/sws --version`;
        if($sws[0] =~ /IPv6/) {
            # HTTP server has IPv6 support!
            $http_ipv6 = 1;
            $gopher_ipv6 = 1;
        }

        # check if the FTP server has it!
        @sws = `server/sockfilt --version`;
        if($sws[0] =~ /IPv6/) {
            # FTP server has IPv6 support!
            $ftp_ipv6 = 1;
        }
    }

    if($has_unix) {
        # client has Unix sockets support, check whether the HTTP server has it
        my @sws = `server/sws --version`;
        $http_unix = 1 if($sws[0] =~ /unix/);
    }

    if(!$has_memory_tracking && $torture) {
        die "can't run torture tests since curl was built without ".
            "TrackMemory feature (--enable-curldebug)";
    }

    open(M, "$CURL -M 2>&1|");
    while(my $s = <M>) {
        if($s =~ /built-in manual was disabled at build-time/) {
            $has_manual = 0;
            last;
        }
        $has_manual = 1;
        last;
    }
    close(M);

    $has_shared = `sh $CURLCONFIG --built-shared`;
    chomp $has_shared;

    my $hostname=join(' ', runclientoutput("hostname"));
    my $hosttype=join(' ', runclientoutput("uname -a"));

    logmsg ("********* System characteristics ******** \n",
            "* $curl\n",
            "* $libcurl\n",
            "* Features: $feat\n",
            "* Disabled: $dis\n",
            "* Host: $hostname",
            "* System: $hosttype");

    if($has_memory_tracking && $has_threadedres) {
        $has_memory_tracking = 0;
        logmsg("*\n",
               "*** DISABLES memory tracking when using threaded resolver\n",
               "*\n");
    }

    logmsg sprintf("* Servers: %s", $stunnel?"SSL ":"");
    logmsg sprintf("%s", $http_ipv6?"HTTP-IPv6 ":"");
    logmsg sprintf("%s", $http_unix?"HTTP-unix ":"");
    logmsg sprintf("%s\n", $ftp_ipv6?"FTP-IPv6 ":"");

    logmsg sprintf("* Env: %s%s", $valgrind?"Valgrind ":"",
                   $run_event_based?"event-based ":"");
    logmsg sprintf("%s\n", $libtool?"Libtool ":"");

    if($verbose) {
        logmsg "* Ports:\n";

        logmsg sprintf("*   HTTP/%d ", $HTTPPORT);
        logmsg sprintf("FTP/%d ", $FTPPORT);
        logmsg sprintf("FTP2/%d ", $FTP2PORT);
        logmsg sprintf("RTSP/%d ", $RTSPPORT);
        if($stunnel) {
            logmsg sprintf("FTPS/%d ", $FTPSPORT);
            logmsg sprintf("HTTPS/%d ", $HTTPSPORT);
        }
        logmsg sprintf("\n*   TFTP/%d ", $TFTPPORT);
        if($http_ipv6) {
            logmsg sprintf("HTTP-IPv6/%d ", $HTTP6PORT);
            logmsg sprintf("RTSP-IPv6/%d ", $RTSP6PORT);
        }
        if($ftp_ipv6) {
            logmsg sprintf("FTP-IPv6/%d ", $FTP6PORT);
        }
        if($tftp_ipv6) {
            logmsg sprintf("TFTP-IPv6/%d ", $TFTP6PORT);
        }
        logmsg sprintf("\n*   GOPHER/%d ", $GOPHERPORT);
        if($gopher_ipv6) {
            logmsg sprintf("GOPHER-IPv6/%d", $GOPHER6PORT);
        }
        logmsg sprintf("\n*   SSH/%d ", $SSHPORT);
        logmsg sprintf("SOCKS/%d ", $SOCKSPORT);
        logmsg sprintf("POP3/%d ", $POP3PORT);
        logmsg sprintf("IMAP/%d ", $IMAPPORT);
        logmsg sprintf("SMTP/%d\n", $SMTPPORT);
        if($ftp_ipv6) {
            logmsg sprintf("*   POP3-IPv6/%d ", $POP36PORT);
            logmsg sprintf("IMAP-IPv6/%d ", $IMAP6PORT);
            logmsg sprintf("SMTP-IPv6/%d\n", $SMTP6PORT);
        }
        if($httptlssrv) {
            logmsg sprintf("*   HTTPTLS/%d ", $HTTPTLSPORT);
            if($has_ipv6) {
                logmsg sprintf("HTTPTLS-IPv6/%d ", $HTTPTLS6PORT);
            }
            logmsg "\n";
        }

        if($has_unix) {
            logmsg "* Unix socket paths:\n";
            if($http_unix) {
                logmsg sprintf("*   HTTP-Unix:%s\n", $HTTPUNIXPATH);
            }
        }
    }
    $has_textaware = ($^O eq 'MSWin32') || ($^O eq 'msys');

    logmsg "***************************************** \n";

    setupfeatures();
    # toggle off the features that were disabled in the build
    for my $d(@disabled) {
        $feature{$d} = 0;
    }
}

#######################################################################
# substitute the variable stuff into either a joined up file or
# a command, in either case passed by reference
#
sub subVariables {
  my ($thing) = @_;

  # ports

  $$thing =~ s/%FTP6PORT/$FTP6PORT/g;
  $$thing =~ s/%FTP2PORT/$FTP2PORT/g;
  $$thing =~ s/%FTPSPORT/$FTPSPORT/g;
  $$thing =~ s/%FTPPORT/$FTPPORT/g;

  $$thing =~ s/%GOPHER6PORT/$GOPHER6PORT/g;
  $$thing =~ s/%GOPHERPORT/$GOPHERPORT/g;

  $$thing =~ s/%HTTPTLS6PORT/$HTTPTLS6PORT/g;
  $$thing =~ s/%HTTPTLSPORT/$HTTPTLSPORT/g;
  $$thing =~ s/%HTTP6PORT/$HTTP6PORT/g;
  $$thing =~ s/%HTTPSPORT/$HTTPSPORT/g;
  $$thing =~ s/%HTTP2PORT/$HTTP2PORT/g;
  $$thing =~ s/%HTTPPORT/$HTTPPORT/g;
  $$thing =~ s/%PROXYPORT/$HTTPPROXYPORT/g;

  $$thing =~ s/%IMAP6PORT/$IMAP6PORT/g;
  $$thing =~ s/%IMAPPORT/$IMAPPORT/g;

  $$thing =~ s/%POP36PORT/$POP36PORT/g;
  $$thing =~ s/%POP3PORT/$POP3PORT/g;

  $$thing =~ s/%RTSP6PORT/$RTSP6PORT/g;
  $$thing =~ s/%RTSPPORT/$RTSPPORT/g;

  $$thing =~ s/%SMTP6PORT/$SMTP6PORT/g;
  $$thing =~ s/%SMTPPORT/$SMTPPORT/g;

  $$thing =~ s/%SOCKSPORT/$SOCKSPORT/g;
  $$thing =~ s/%SSHPORT/$SSHPORT/g;

  $$thing =~ s/%TFTP6PORT/$TFTP6PORT/g;
  $$thing =~ s/%TFTPPORT/$TFTPPORT/g;

  $$thing =~ s/%DICTPORT/$DICTPORT/g;

  $$thing =~ s/%SMBPORT/$SMBPORT/g;
  $$thing =~ s/%SMBSPORT/$SMBSPORT/g;

  $$thing =~ s/%NEGTELNETPORT/$NEGTELNETPORT/g;

  # server Unix domain socket paths

  $$thing =~ s/%HTTPUNIXPATH/$HTTPUNIXPATH/g;

  # client IP addresses

  $$thing =~ s/%CLIENT6IP/$CLIENT6IP/g;
  $$thing =~ s/%CLIENTIP/$CLIENTIP/g;

  # server IP addresses

  $$thing =~ s/%HOST6IP/$HOST6IP/g;
  $$thing =~ s/%HOSTIP/$HOSTIP/g;

  # misc

  $$thing =~ s/%CURL/$CURL/g;
  $$thing =~ s/%PWD/$pwd/g;
  $$thing =~ s/%POSIX_PWD/$posix_pwd/g;

  my $file_pwd = $pwd;
  if($file_pwd !~ /^\//) {
      $file_pwd = "/$file_pwd";
  }

  $$thing =~ s/%FILE_PWD/$file_pwd/g;
  $$thing =~ s/%SRCDIR/$srcdir/g;
  $$thing =~ s/%USER/$USER/g;

  # The purpose of FTPTIME2 and FTPTIME3 is to provide times that can be
  # used for time-out tests and that whould work on most hosts as these
  # adjust for the startup/check time for this particular host. We needed
  # to do this to make the test suite run better on very slow hosts.

  my $ftp2 = $ftpchecktime * 2;
  my $ftp3 = $ftpchecktime * 3;

  $$thing =~ s/%FTPTIME2/$ftp2/g;
  $$thing =~ s/%FTPTIME3/$ftp3/g;

  # HTTP2

  $$thing =~ s/%H2CVER/$h2cver/g;
}

sub fixarray {
    my @in = @_;

    for(@in) {
        subVariables(\$_);
    }
    return @in;
}

#######################################################################
# Provide time stamps for single test skipped events
#
sub timestampskippedevents {
    my $testnum = $_[0];

    return if((not defined($testnum)) || ($testnum < 1));

    if($timestats) {

        if($timevrfyend{$testnum}) {
            return;
        }
        elsif($timesrvrlog{$testnum}) {
            $timevrfyend{$testnum} = $timesrvrlog{$testnum};
            return;
        }
        elsif($timetoolend{$testnum}) {
            $timevrfyend{$testnum} = $timetoolend{$testnum};
            $timesrvrlog{$testnum} = $timetoolend{$testnum};
        }
        elsif($timetoolini{$testnum}) {
            $timevrfyend{$testnum} = $timetoolini{$testnum};
            $timesrvrlog{$testnum} = $timetoolini{$testnum};
            $timetoolend{$testnum} = $timetoolini{$testnum};
        }
        elsif($timesrvrend{$testnum}) {
            $timevrfyend{$testnum} = $timesrvrend{$testnum};
            $timesrvrlog{$testnum} = $timesrvrend{$testnum};
            $timetoolend{$testnum} = $timesrvrend{$testnum};
            $timetoolini{$testnum} = $timesrvrend{$testnum};
        }
        elsif($timesrvrini{$testnum}) {
            $timevrfyend{$testnum} = $timesrvrini{$testnum};
            $timesrvrlog{$testnum} = $timesrvrini{$testnum};
            $timetoolend{$testnum} = $timesrvrini{$testnum};
            $timetoolini{$testnum} = $timesrvrini{$testnum};
            $timesrvrend{$testnum} = $timesrvrini{$testnum};
        }
        elsif($timeprepini{$testnum}) {
            $timevrfyend{$testnum} = $timeprepini{$testnum};
            $timesrvrlog{$testnum} = $timeprepini{$testnum};
            $timetoolend{$testnum} = $timeprepini{$testnum};
            $timetoolini{$testnum} = $timeprepini{$testnum};
            $timesrvrend{$testnum} = $timeprepini{$testnum};
            $timesrvrini{$testnum} = $timeprepini{$testnum};
        }
    }
}

#######################################################################
# Run a single specified test case
#
sub singletest {
    my ($evbased, # 1 means switch on if possible (and "curl" is tested)
                  # returns "not a test" if it can't be used for this test
        $testnum,
        $count,
        $total)=@_;

    my @what;
    my $why;
    my $cmd;
    my $disablevalgrind;

    # fist, remove all lingering log files
    cleardir($LOGDIR);

    # copy test number to a global scope var, this allows
    # testnum checking when starting test harness servers.
    $testnumcheck = $testnum;

    # timestamp test preparation start
    $timeprepini{$testnum} = Time::HiRes::time();

    if($disttests !~ /test$testnum\W/ ) {
        logmsg "Warning: test$testnum not present in tests/data/Makefile.inc\n";
    }
    if($disabled{$testnum}) {
        logmsg "Warning: test$testnum is explicitly disabled\n";
    }

    # load the test case file definition
    if(loadtest("${TESTDIR}/test${testnum}")) {
        if($verbose) {
            # this is not a test
            logmsg "RUN: $testnum doesn't look like a test case\n";
        }
        $why = "no test";
    }
    else {
        @what = getpart("client", "features");
    }

    # We require a feature to be present
    for(@what) {
        my $f = $_;
        $f =~ s/\s//g;

        if($f =~ /^([^!].*)$/) {
            if($feature{$1}) {
                next;
            }

            $why = "curl lacks $1 support";
            last;
        }
    }

    # We require a feature to not be present
    if(!$why) {
        for(@what) {
            my $f = $_;
            $f =~ s/\s//g;

            if($f =~ /^!(.*)$/) {
                if(!$feature{$1}) {
                    next;
                }
            }
            else {
                next;
            }

            $why = "curl has $1 support";
            last;
        }
    }

    if(!$why) {
        my @keywords = getpart("info", "keywords");
        my $match;
        my $k;

        if(!$keywords[0]) {
            $why = "missing the <keywords> section!";
        }

        for $k (@keywords) {
            chomp $k;
            if ($disabled_keywords{lc($k)}) {
                $why = "disabled by keyword";
            } elsif ($enabled_keywords{lc($k)}) {
                $match = 1;
            }
        }

        if(!$why && !$match && %enabled_keywords) {
            $why = "disabled by missing keyword";
        }
    }

    # test definition may instruct to (un)set environment vars
    # this is done this early, so that the precheck can use environment
    # variables and still bail out fine on errors

    # restore environment variables that were modified in a previous run
    foreach my $var (keys %oldenv) {
        if($oldenv{$var} eq 'notset') {
            delete $ENV{$var} if($ENV{$var});
        }
        else {
            $ENV{$var} = $oldenv{$var};
        }
        delete $oldenv{$var};
    }

    # remove test server commands file before servers are started/verified
    unlink($FTPDCMD) if(-f $FTPDCMD);

    # timestamp required servers verification start
    $timesrvrini{$testnum} = Time::HiRes::time();

    if(!$why) {
        $why = serverfortest($testnum);
    }

    # timestamp required servers verification end
    $timesrvrend{$testnum} = Time::HiRes::time();

    my @setenv = getpart("client", "setenv");
    if(@setenv) {
        foreach my $s (@setenv) {
            chomp $s;
            subVariables(\$s);
            if($s =~ /([^=]*)=(.*)/) {
                my ($var, $content) = ($1, $2);
                # remember current setting, to restore it once test runs
                $oldenv{$var} = ($ENV{$var})?"$ENV{$var}":'notset';
                # set new value
                if(!$content) {
                    delete $ENV{$var} if($ENV{$var});
                }
                else {
                    if($var =~ /^LD_PRELOAD/) {
                        if(exe_ext() && (exe_ext() eq '.exe')) {
                            # print "Skipping LD_PRELOAD due to lack of OS support\n";
                            next;
                        }
                        if($debug_build || ($has_shared ne "yes")) {
                            # print "Skipping LD_PRELOAD due to no release shared build\n";
                            next;
                        }
                    }
                    $ENV{$var} = "$content";
                    print "setenv $var = $content\n" if($verbose);
                }
            }
        }
    }

    if(!$why) {
        my @precheck = getpart("client", "precheck");
        if(@precheck) {
            $cmd = $precheck[0];
            chomp $cmd;
            subVariables \$cmd;
            if($cmd) {
                my @p = split(/ /, $cmd);
                if($p[0] !~ /\//) {
                    # the first word, the command, does not contain a slash so
                    # we will scan the "improved" PATH to find the command to
                    # be able to run it
                    my $fullp = checktestcmd($p[0]);

                    if($fullp) {
                        $p[0] = $fullp;
                    }
                    $cmd = join(" ", @p);
                }

                my @o = `$cmd 2>/dev/null`;
                if($o[0]) {
                    $why = $o[0];
                    chomp $why;
                } elsif($?) {
                    $why = "precheck command error";
                }
                logmsg "prechecked $cmd\n" if($verbose);
            }
        }
    }

    if($why && !$listonly) {
        # there's a problem, count it as "skipped"
        $skipped++;
        $skipped{$why}++;
        $teststat[$testnum]=$why; # store reason for this test case

        if(!$short) {
            if($skipped{$why} <= 3) {
                # show only the first three skips for each reason
                logmsg sprintf("test %04d SKIPPED: $why\n", $testnum);
            }
        }

        timestampskippedevents($testnum);
        return -1;
    }
    logmsg sprintf("test %04d...", $testnum) if(!$automakestyle);

    my %replyattr = getpartattr("reply", "data");
    my @reply;
    if (partexists("reply", "datacheck")) {
        for my $partsuffix (('', '1', '2', '3', '4')) {
            my @replycheckpart = getpart("reply", "datacheck".$partsuffix);
            if(@replycheckpart) {
                my %replycheckpartattr = getpartattr("reply", "datacheck".$partsuffix);
                # get the mode attribute
                my $filemode=$replycheckpartattr{'mode'};
                if($filemode && ($filemode eq "text") && $has_textaware) {
                    # text mode when running on windows: fix line endings
                    map s/\r\n/\n/g, @replycheckpart;
                    map s/\n/\r\n/g, @replycheckpart;
                }
                if($replycheckpartattr{'nonewline'}) {
                    # Yes, we must cut off the final newline from the final line
                    # of the datacheck
                    chomp($replycheckpart[$#replycheckpart]);
                }
                push(@reply, @replycheckpart);
            }
        }
    }
    else {
        # check against the data section
        @reply = getpart("reply", "data");
        # get the mode attribute
        my $filemode=$replyattr{'mode'};
        if($filemode && ($filemode eq "text") && $has_textaware) {
            # text mode when running on windows: fix line endings
            map s/\r\n/\n/g, @reply;
            map s/\n/\r\n/g, @reply;
        }
    }

    # this is the valid protocol blurb curl should generate
    my @protocol= fixarray ( getpart("verify", "protocol") );

    # this is the valid protocol blurb curl should generate to a proxy
    my @proxyprot = fixarray ( getpart("verify", "proxy") );

    # redirected stdout/stderr to these files
    $STDOUT="$LOGDIR/stdout$testnum";
    $STDERR="$LOGDIR/stderr$testnum";

    # if this section exists, we verify that the stdout contained this:
    my @validstdout = fixarray ( getpart("verify", "stdout") );
    my @validstderr = fixarray ( getpart("verify", "stderr") );

    # if this section exists, we verify upload
    my @upload = getpart("verify", "upload");
    if(@upload) {
      my %hash = getpartattr("verify", "upload");
      if($hash{'nonewline'}) {
          # cut off the final newline from the final line of the upload data
          chomp($upload[$#upload]);
      }
    }

    # if this section exists, it might be FTP server instructions:
    my @ftpservercmd = fixarray ( getpart("reply", "servercmd") );

    my $CURLOUT="$LOGDIR/curl$testnum.out"; # curl output if not stdout

    # name of the test
    my @testname= getpart("client", "name");
    my $testname = $testname[0];
    $testname =~ s/\n//g;
    logmsg "[$testname]\n" if(!$short);

    if($listonly) {
        timestampskippedevents($testnum);
        return 0; # look successful
    }

    my @codepieces = getpart("client", "tool");

    my $tool="";
    if(@codepieces) {
        $tool = $codepieces[0];
        chomp $tool;
    }

    # remove server output logfile
    unlink($SERVERIN);
    unlink($SERVER2IN);
    unlink($PROXYIN);

    if(@ftpservercmd) {
        # write the instructions to file
        writearray($FTPDCMD, \@ftpservercmd);
    }

    # get the command line options to use
    my @blaha;
    ($cmd, @blaha)= getpart("client", "command");

    if($cmd) {
        # make some nice replace operations
        $cmd =~ s/\n//g; # no newlines please
        # substitute variables in the command line
        subVariables \$cmd;
    }
    else {
        # there was no command given, use something silly
        $cmd="-";
    }
    if($has_memory_tracking) {
        unlink($memdump);
    }

    # create (possibly-empty) files before starting the test
    for my $partsuffix (('', '1', '2', '3', '4')) {
        my @inputfile=getpart("client", "file".$partsuffix);
        my %fileattr = getpartattr("client", "file".$partsuffix);
        my $filename=$fileattr{'name'};
        if(@inputfile || $filename) {
            if(!$filename) {
                logmsg "ERROR: section client=>file has no name attribute\n";
                timestampskippedevents($testnum);
                return -1;
            }
            my $fileContent = join('', @inputfile);
            subVariables \$fileContent;
#            logmsg "DEBUG: writing file " . $filename . "\n";
            open(OUTFILE, ">$filename");
            binmode OUTFILE; # for crapage systems, use binary
            print OUTFILE $fileContent;
            close(OUTFILE);
        }
    }

    my %cmdhash = getpartattr("client", "command");

    my $out="";

    if((!$cmdhash{'option'}) || ($cmdhash{'option'} !~ /no-output/)) {
        #We may slap on --output!
        if (!@validstdout ||
                ($cmdhash{'option'} && $cmdhash{'option'} =~ /force-output/)) {
            $out=" --output $CURLOUT ";
        }
    }

    my $serverlogslocktimeout = $defserverlogslocktimeout;
    if($cmdhash{'timeout'}) {
        # test is allowed to override default server logs lock timeout
        if($cmdhash{'timeout'} =~ /(\d+)/) {
            $serverlogslocktimeout = $1 if($1 >= 0);
        }
    }

    my $postcommanddelay = $defpostcommanddelay;
    if($cmdhash{'delay'}) {
        # test is allowed to specify a delay after command is executed
        if($cmdhash{'delay'} =~ /(\d+)/) {
            $postcommanddelay = $1 if($1 > 0);
        }
    }

    my $CMDLINE;
    my $cmdargs;
    my $cmdtype = $cmdhash{'type'} || "default";
    my $fail_due_event_based = $evbased;
    if($cmdtype eq "perl") {
        # run the command line prepended with "perl"
        $cmdargs ="$cmd";
        $CMDLINE = "$perl ";
        $tool=$CMDLINE;
        $disablevalgrind=1;
    }
    elsif($cmdtype eq "shell") {
        # run the command line prepended with "/bin/sh"
        $cmdargs ="$cmd";
        $CMDLINE = "/bin/sh ";
        $tool=$CMDLINE;
        $disablevalgrind=1;
    }
    elsif(!$tool) {
        # run curl, add suitable command line options
        my $inc="";
        if((!$cmdhash{'option'}) || ($cmdhash{'option'} !~ /no-include/)) {
            $inc = " --include";
        }

        $cmdargs = "$out$inc ";
        $cmdargs .= "--trace-ascii log/trace$testnum ";
        $cmdargs .= "--trace-time ";
        if($evbased) {
            $cmdargs .= "--test-event ";
            $fail_due_event_based--;
        }
        $cmdargs .= $cmd;
    }
    else {
        $cmdargs = " $cmd"; # $cmd is the command line for the test file
        $CURLOUT = $STDOUT; # sends received data to stdout

        if($tool =~ /^lib/) {
            $CMDLINE="$LIBDIR/$tool";
        }
        elsif($tool =~ /^unit/) {
            $CMDLINE="$UNITDIR/$tool";
        }

        if(! -f $CMDLINE) {
            logmsg "The tool set in the test case for this: '$tool' does not exist\n";
            timestampskippedevents($testnum);
            return -1;
        }
        $DBGCURL=$CMDLINE;
    }

    if($gdbthis) {
        # gdb is incompatible with valgrind, so disable it when debugging
        # Perhaps a better approach would be to run it under valgrind anyway
        # with --db-attach=yes or --vgdb=yes.
        $disablevalgrind=1;
    }

    if($fail_due_event_based) {
        logmsg "This test cannot run event based\n";
        return -1;
    }

    my @stdintest = getpart("client", "stdin");

    if(@stdintest) {
        my $stdinfile="$LOGDIR/stdin-for-$testnum";

        my %hash = getpartattr("client", "stdin");
        if($hash{'nonewline'}) {
            # cut off the final newline from the final line of the stdin data
            chomp($stdintest[$#stdintest]);
        }

        writearray($stdinfile, \@stdintest);

        $cmdargs .= " <$stdinfile";
    }

    if(!$tool) {
        $CMDLINE="$CURL";
    }

    my $usevalgrind;
    if($valgrind && !$disablevalgrind) {
        my @valgrindoption = getpart("verify", "valgrind");
        if((!@valgrindoption) || ($valgrindoption[0] !~ /disable/)) {
            $usevalgrind = 1;
            my $valgrindcmd = "$valgrind ";
            $valgrindcmd .= "$valgrind_tool " if($valgrind_tool);
            $valgrindcmd .= "--quiet --leak-check=yes ";
            $valgrindcmd .= "--suppressions=$srcdir/valgrind.supp ";
           # $valgrindcmd .= "--gen-suppressions=all ";
            $valgrindcmd .= "--num-callers=16 ";
            $valgrindcmd .= "${valgrind_logfile}=$LOGDIR/valgrind$testnum";
            $CMDLINE = "$valgrindcmd $CMDLINE";
        }
    }

    $CMDLINE .= "$cmdargs >$STDOUT 2>$STDERR";

    if($verbose) {
        logmsg "$CMDLINE\n";
    }

    print CMDLOG "$CMDLINE\n";

    unlink("core");

    my $dumped_core;
    my $cmdres;

    if($gdbthis) {
        my $gdbinit = "$TESTDIR/gdbinit$testnum";
        open(GDBCMD, ">$LOGDIR/gdbcmd");
        print GDBCMD "set args $cmdargs\n";
        print GDBCMD "show args\n";
        print GDBCMD "source $gdbinit\n" if -e $gdbinit;
        close(GDBCMD);
    }

    # timestamp starting of test command
    $timetoolini{$testnum} = Time::HiRes::time();

    # run the command line we built
    if ($torture) {
        $cmdres = torture($CMDLINE,
                          $testnum,
                          "$gdb --directory libtest $DBGCURL -x $LOGDIR/gdbcmd");
    }
    elsif($gdbthis) {
        my $GDBW = ($gdbxwin) ? "-w" : "";
        runclient("$gdb --directory libtest $DBGCURL $GDBW -x $LOGDIR/gdbcmd");
        $cmdres=0; # makes it always continue after a debugged run
    }
    else {
        $cmdres = runclient("$CMDLINE");
        my $signal_num  = $cmdres & 127;
        $dumped_core = $cmdres & 128;

        if(!$anyway && ($signal_num || $dumped_core)) {
            $cmdres = 1000;
        }
        else {
            $cmdres >>= 8;
            $cmdres = (2000 + $signal_num) if($signal_num && !$cmdres);
        }
    }

    # timestamp finishing of test command
    $timetoolend{$testnum} = Time::HiRes::time();

    if(!$dumped_core) {
        if(-r "core") {
            # there's core file present now!
            $dumped_core = 1;
        }
    }

    if($dumped_core) {
        logmsg "core dumped\n";
        if(0 && $gdb) {
            logmsg "running gdb for post-mortem analysis:\n";
            open(GDBCMD, ">$LOGDIR/gdbcmd2");
            print GDBCMD "bt\n";
            close(GDBCMD);
            runclient("$gdb --directory libtest -x $LOGDIR/gdbcmd2 -batch $DBGCURL core ");
     #       unlink("$LOGDIR/gdbcmd2");
        }
    }

    # If a server logs advisor read lock file exists, it is an indication
    # that the server has not yet finished writing out all its log files,
    # including server request log files used for protocol verification.
    # So, if the lock file exists the script waits here a certain amount
    # of time until the server removes it, or the given time expires.

    if($serverlogslocktimeout) {
        my $lockretry = $serverlogslocktimeout * 20;
        while((-f $SERVERLOGS_LOCK) && $lockretry--) {
            select(undef, undef, undef, 0.05);
        }
        if(($lockretry < 0) &&
           ($serverlogslocktimeout >= $defserverlogslocktimeout)) {
            logmsg "Warning: server logs lock timeout ",
                   "($serverlogslocktimeout seconds) expired\n";
        }
    }

    # Test harness ssh server does not have this synchronization mechanism,
    # this implies that some ssh server based tests might need a small delay
    # once that the client command has run to avoid false test failures.
    #
    # gnutls-serv also lacks this synchronization mechanism, so gnutls-serv
    # based tests might need a small delay once that the client command has
    # run to avoid false test failures.

    sleep($postcommanddelay) if($postcommanddelay);

    # timestamp removal of server logs advisor read lock
    $timesrvrlog{$testnum} = Time::HiRes::time();

    # test definition might instruct to stop some servers
    # stop also all servers relative to the given one

    my @killtestservers = getpart("client", "killserver");
    if(@killtestservers) {
        #
        # All servers relative to the given one must be stopped also
        #
        my @killservers;
        foreach my $server (@killtestservers) {
            chomp $server;
            if($server =~ /^(ftp|http|imap|pop3|smtp)s((\d*)(-ipv6|-unix|))$/) {
                # given a stunnel ssl server, also kill non-ssl underlying one
                push @killservers, "${1}${2}";
            }
            elsif($server =~ /^(ftp|http|imap|pop3|smtp)((\d*)(-ipv6|-unix|))$/) {
                # given a non-ssl server, also kill stunnel piggybacking one
                push @killservers, "${1}s${2}";
            }
            elsif($server =~ /^(socks)((\d*)(-ipv6|))$/) {
                # given a socks server, also kill ssh underlying one
                push @killservers, "ssh${2}";
            }
            elsif($server =~ /^(ssh)((\d*)(-ipv6|))$/) {
                # given a ssh server, also kill socks piggybacking one
                push @killservers, "socks${2}";
            }
            push @killservers, $server;
        }
        #
        # kill sockfilter processes for pingpong relative servers
        #
        foreach my $server (@killservers) {
            if($server =~ /^(ftp|imap|pop3|smtp)s?(\d*)(-ipv6|)$/) {
                my $proto  = $1;
                my $idnum  = ($2 && ($2 > 1)) ? $2 : 1;
                my $ipvnum = ($3 && ($3 =~ /6$/)) ? 6 : 4;
                killsockfilters($proto, $ipvnum, $idnum, $verbose);
            }
        }
        #
        # kill server relative pids clearing them in %run hash
        #
        my $pidlist;
        foreach my $server (@killservers) {
            if($run{$server}) {
                $pidlist .= "$run{$server} ";
                $run{$server} = 0;
            }
            $runcert{$server} = 0 if($runcert{$server});
        }
        killpid($verbose, $pidlist);
        #
        # cleanup server pid files
        #
        foreach my $server (@killservers) {
            my $pidfile = $serverpidfile{$server};
            my $pid = processexists($pidfile);
            if($pid > 0) {
                logmsg "Warning: $server server unexpectedly alive\n";
                killpid($verbose, $pid);
            }
            unlink($pidfile) if(-f $pidfile);
        }
    }

    # remove the test server commands file after each test
    unlink($FTPDCMD) if(-f $FTPDCMD);

    # run the postcheck command
    my @postcheck= getpart("client", "postcheck");
    if(@postcheck) {
        $cmd = join("", @postcheck);
        chomp $cmd;
        subVariables \$cmd;
        if($cmd) {
            logmsg "postcheck $cmd\n" if($verbose);
            my $rc = runclient("$cmd");
            # Must run the postcheck command in torture mode in order
            # to clean up, but the result can't be relied upon.
            if($rc != 0 && !$torture) {
                logmsg " postcheck FAILED\n";
                # timestamp test result verification end
                $timevrfyend{$testnum} = Time::HiRes::time();
                return 1;
            }
        }
    }

    # restore environment variables that were modified
    if(%oldenv) {
        foreach my $var (keys %oldenv) {
            if($oldenv{$var} eq 'notset') {
                delete $ENV{$var} if($ENV{$var});
            }
            else {
                $ENV{$var} = "$oldenv{$var}";
            }
        }
    }

    # Skip all the verification on torture tests
    if ($torture) {
        # timestamp test result verification end
        $timevrfyend{$testnum} = Time::HiRes::time();
        return $cmdres;
    }

    my @err = getpart("verify", "errorcode");
    my $errorcode = $err[0] || "0";
    my $ok="";
    my $res;
    chomp $errorcode;
    if (@validstdout) {
        # verify redirected stdout
        my @actual = loadarray($STDOUT);

        # what parts to cut off from stdout
        my @stripfile = getpart("verify", "stripfile");

        foreach my $strip (@stripfile) {
            chomp $strip;
            my @newgen;
            for(@actual) {
                eval $strip;
                if($_) {
                    push @newgen, $_;
                }
            }
            # this is to get rid of array entries that vanished (zero
            # length) because of replacements
            @actual = @newgen;
        }

        # variable-replace in the stdout we have from the test case file
        @validstdout = fixarray(@validstdout);

        # get all attributes
        my %hash = getpartattr("verify", "stdout");

        # get the mode attribute
        my $filemode=$hash{'mode'};
        if($filemode && ($filemode eq "text") && $has_textaware) {
            # text mode when running on windows: fix line endings
            map s/\r\n/\n/g, @validstdout;
            map s/\n/\r\n/g, @validstdout;
        }

        if($hash{'nonewline'}) {
            # Yes, we must cut off the final newline from the final line
            # of the protocol data
            chomp($validstdout[$#validstdout]);
        }

        $res = compare($testnum, $testname, "stdout", \@actual, \@validstdout);
        if($res) {
            return 1;
        }
        $ok .= "s";
    }
    else {
        $ok .= "-"; # stdout not checked
    }

    if (@validstderr) {
        # verify redirected stderr
        my @actual = loadarray($STDERR);

        # what parts to cut off from stderr
        my @stripfile = getpart("verify", "stripfile");

        foreach my $strip (@stripfile) {
            chomp $strip;
            my @newgen;
            for(@actual) {
                eval $strip;
                if($_) {
                    push @newgen, $_;
                }
            }
            # this is to get rid of array entries that vanished (zero
            # length) because of replacements
            @actual = @newgen;
        }

        # variable-replace in the stderr we have from the test case file
        @validstderr = fixarray(@validstderr);

        # get all attributes
        my %hash = getpartattr("verify", "stderr");

        # get the mode attribute
        my $filemode=$hash{'mode'};
        if($filemode && ($filemode eq "text") && $has_textaware) {
            # text mode when running on windows: fix line endings
            map s/\r\n/\n/g, @validstderr;
            map s/\n/\r\n/g, @validstderr;
        }

        if($hash{'nonewline'}) {
            # Yes, we must cut off the final newline from the final line
            # of the protocol data
            chomp($validstderr[$#validstderr]);
        }

        $res = compare($testnum, $testname, "stderr", \@actual, \@validstderr);
        if($res) {
            return 1;
        }
        $ok .= "r";
    }
    else {
        $ok .= "-"; # stderr not checked
    }

    if(@protocol) {
        # Verify the sent request
        my @out = loadarray($SERVERIN);

        # what to cut off from the live protocol sent by curl
        my @strip = getpart("verify", "strip");

        my @protstrip=@protocol;

        # check if there's any attributes on the verify/protocol section
        my %hash = getpartattr("verify", "protocol");

        if($hash{'nonewline'}) {
            # Yes, we must cut off the final newline from the final line
            # of the protocol data
            chomp($protstrip[$#protstrip]);
        }

        for(@strip) {
            # strip off all lines that match the patterns from both arrays
            chomp $_;
            @out = striparray( $_, \@out);
            @protstrip= striparray( $_, \@protstrip);
        }

        # what parts to cut off from the protocol
        my @strippart = getpart("verify", "strippart");
        my $strip;
        @strippart = fixarray(@strippart);
        for $strip (@strippart) {
            chomp $strip;
            for(@out) {
                eval $strip;
            }
        }

        $res = compare($testnum, $testname, "protocol", \@out, \@protstrip);
        if($res) {
            return 1;
        }

        $ok .= "p";

    }
    else {
        $ok .= "-"; # protocol not checked
    }

    if(!$replyattr{'nocheck'} && (@reply || $replyattr{'sendzero'})) {
        # verify the received data
        my @out = loadarray($CURLOUT);
        $res = compare($testnum, $testname, "data", \@out, \@reply);
        if ($res) {
            return 1;
        }
        $ok .= "d";
    }
    else {
        $ok .= "-"; # data not checked
    }

    if(@upload) {
        # verify uploaded data
        my @out = loadarray("$LOGDIR/upload.$testnum");

        # what parts to cut off from the upload
        my @strippart = getpart("verify", "strippart");
        my $strip;
        for $strip (@strippart) {
            chomp $strip;
            for(@out) {
                eval $strip;
            }
        }

        $res = compare($testnum, $testname, "upload", \@out, \@upload);
        if ($res) {
            return 1;
        }
        $ok .= "u";
    }
    else {
        $ok .= "-"; # upload not checked
    }

    if(@proxyprot) {
        # Verify the sent proxy request
        my @out = loadarray($PROXYIN);

        # what to cut off from the live protocol sent by curl, we use the
        # same rules as for <protocol>
        my @strip = getpart("verify", "strip");

        my @protstrip=@proxyprot;

        # check if there's any attributes on the verify/protocol section
        my %hash = getpartattr("verify", "proxy");

        if($hash{'nonewline'}) {
            # Yes, we must cut off the final newline from the final line
            # of the protocol data
            chomp($protstrip[$#protstrip]);
        }

        for(@strip) {
            # strip off all lines that match the patterns from both arrays
            chomp $_;
            @out = striparray( $_, \@out);
            @protstrip= striparray( $_, \@protstrip);
        }

        # what parts to cut off from the protocol
        my @strippart = getpart("verify", "strippart");
        my $strip;
        for $strip (@strippart) {
            chomp $strip;
            for(@out) {
                eval $strip;
            }
        }

        $res = compare($testnum, $testname, "proxy", \@out, \@protstrip);
        if($res) {
            return 1;
        }

        $ok .= "P";

    }
    else {
        $ok .= "-"; # protocol not checked
    }

    my $outputok;
    for my $partsuffix (('', '1', '2', '3', '4')) {
        my @outfile=getpart("verify", "file".$partsuffix);
        if(@outfile || partexists("verify", "file".$partsuffix) ) {
            # we're supposed to verify a dynamically generated file!
            my %hash = getpartattr("verify", "file".$partsuffix);

            my $filename=$hash{'name'};
            if(!$filename) {
                logmsg "ERROR: section verify=>file$partsuffix ".
                       "has no name attribute\n";
                stopservers($verbose);
                # timestamp test result verification end
                $timevrfyend{$testnum} = Time::HiRes::time();
                return -1;
            }
            my @generated=loadarray($filename);

            # what parts to cut off from the file
            my @stripfile = getpart("verify", "stripfile".$partsuffix);

            my $filemode=$hash{'mode'};
            if($filemode && ($filemode eq "text") && $has_textaware) {
                # text mode when running on windows: fix line endings
                map s/\r\n/\n/g, @outfile;
                map s/\n/\r\n/g, @outfile;
            }

            my $strip;
            for $strip (@stripfile) {
                chomp $strip;
                my @newgen;
                for(@generated) {
                    eval $strip;
                    if($_) {
                        push @newgen, $_;
                    }
                }
                # this is to get rid of array entries that vanished (zero
                # length) because of replacements
                @generated = @newgen;
            }

            @outfile = fixarray(@outfile);

            $res = compare($testnum, $testname, "output ($filename)",
                           \@generated, \@outfile);
            if($res) {
                return 1;
            }

            $outputok = 1; # output checked
        }
    }
    $ok .= ($outputok) ? "o" : "-"; # output checked or not

    # accept multiple comma-separated error codes
    my @splerr = split(/ *, */, $errorcode);
    my $errok;
    foreach my $e (@splerr) {
        if($e == $cmdres) {
            # a fine error code
            $errok = 1;
            last;
        }
    }

    if($errok) {
        $ok .= "e";
    }
    else {
        if(!$short) {
            logmsg sprintf("\n%s returned $cmdres, when expecting %s\n",
                           (!$tool)?"curl":$tool, $errorcode);
        }
        logmsg " exit FAILED\n";
        # timestamp test result verification end
        $timevrfyend{$testnum} = Time::HiRes::time();
        return 1;
    }

    if($has_memory_tracking) {
        if(! -f $memdump) {
            logmsg "\n** ALERT! memory tracking with no output file?\n"
                if(!$cmdtype eq "perl");
        }
        else {
            my @memdata=`$memanalyze $memdump`;
            my $leak=0;
            for(@memdata) {
                if($_ ne "") {
                    # well it could be other memory problems as well, but
                    # we call it leak for short here
                    $leak=1;
                }
            }
            if($leak) {
                logmsg "\n** MEMORY FAILURE\n";
                logmsg @memdata;
                # timestamp test result verification end
                $timevrfyend{$testnum} = Time::HiRes::time();
                return 1;
            }
            else {
                $ok .= "m";
            }
        }
    }
    else {
        $ok .= "-"; # memory not checked
    }

    if($valgrind) {
        if($usevalgrind) {
            unless(opendir(DIR, "$LOGDIR")) {
                logmsg "ERROR: unable to read $LOGDIR\n";
                # timestamp test result verification end
                $timevrfyend{$testnum} = Time::HiRes::time();
                return 1;
            }
            my @files = readdir(DIR);
            closedir(DIR);
            my $vgfile;
            foreach my $file (@files) {
                if($file =~ /^valgrind$testnum(\..*|)$/) {
                    $vgfile = $file;
                    last;
                }
            }
            if(!$vgfile) {
                logmsg "ERROR: valgrind log file missing for test $testnum\n";
                # timestamp test result verification end
                $timevrfyend{$testnum} = Time::HiRes::time();
                return 1;
            }
            my @e = valgrindparse("$LOGDIR/$vgfile");
            if(@e && $e[0]) {
                if($automakestyle) {
                    logmsg "FAIL: $testnum - $testname - valgrind\n";
                }
                else {
                    logmsg " valgrind ERROR ";
                    logmsg @e;
                }
                # timestamp test result verification end
                $timevrfyend{$testnum} = Time::HiRes::time();
                return 1;
            }
            $ok .= "v";
        }
        else {
            if(!$short && !$disablevalgrind) {
                logmsg " valgrind SKIPPED\n";
            }
            $ok .= "-"; # skipped
        }
    }
    else {
        $ok .= "-"; # valgrind not checked
    }
    # add 'E' for event-based
    $ok .= $evbased ? "E" : "-";

    logmsg "$ok " if(!$short);

    # timestamp test result verification end
    $timevrfyend{$testnum} = Time::HiRes::time();

    my $sofar= time()-$start;
    my $esttotal = $sofar/$count * $total;
    my $estleft = $esttotal - $sofar;
    my $left=sprintf("remaining: %02d:%02d",
                     $estleft/60,
                     $estleft%60);
    my $took = $timevrfyend{$testnum} - $timeprepini{$testnum};
    my $duration = sprintf("duration: %02d:%02d",
                           $sofar/60, $sofar%60);
    if(!$automakestyle) {
        logmsg sprintf("OK (%-3d out of %-3d, %s, took %.1fs, %s)\n",
                       $count, $total, $left, $took, $duration);
    }
    else {
        logmsg "PASS: $testnum - $testname\n";
    }


    return 0;
}

#######################################################################
# Stop all running test servers
#
sub stopservers {
    my $verbose = $_[0];
    #
    # kill sockfilter processes for all pingpong servers
    #
    killallsockfilters($verbose);
    #
    # kill all server pids from %run hash clearing them
    #
    my $pidlist;
    foreach my $server (keys %run) {
        if($run{$server}) {
            if($verbose) {
                my $prev = 0;
                my $pids = $run{$server};
                foreach my $pid (split(' ', $pids)) {
                    if($pid != $prev) {
                        logmsg sprintf("* kill pid for %s => %d\n",
                            $server, $pid);
                        $prev = $pid;
                    }
                }
            }
            $pidlist .= "$run{$server} ";
            $run{$server} = 0;
        }
        $runcert{$server} = 0 if($runcert{$server});
    }
    killpid($verbose, $pidlist);
    #
    # cleanup all server pid files
    #
    foreach my $server (keys %serverpidfile) {
        my $pidfile = $serverpidfile{$server};
        my $pid = processexists($pidfile);
        if($pid > 0) {
            logmsg "Warning: $server server unexpectedly alive\n";
            killpid($verbose, $pid);
        }
        unlink($pidfile) if(-f $pidfile);
    }
}

#######################################################################
# startservers() starts all the named servers
#
# Returns: string with error reason or blank for success
#
sub startservers {
    my @what = @_;
    my ($pid, $pid2);
    for(@what) {
        my (@whatlist) = split(/\s+/,$_);
        my $what = lc($whatlist[0]);
        $what =~ s/[^a-z0-9\/-]//g;

        my $certfile;
        if($what =~ /^(ftp|http|imap|pop3|smtp)s((\d*)(-ipv6|-unix|))$/) {
            $certfile = ($whatlist[1]) ? $whatlist[1] : 'stunnel.pem';
        }

        if(($what eq "pop3") ||
           ($what eq "ftp") ||
           ($what eq "imap") ||
           ($what eq "smtp")) {
            if($torture && $run{$what} &&
               !responsive_pingpong_server($what, "", $verbose)) {
                stopserver($what);
            }
            if(!$run{$what}) {
                ($pid, $pid2) = runpingpongserver($what, "", $verbose);
                if($pid <= 0) {
                    return "failed starting ". uc($what) ." server";
                }
                printf ("* pid $what => %d %d\n", $pid, $pid2) if($verbose);
                $run{$what}="$pid $pid2";
            }
        }
        elsif($what eq "ftp2") {
            if($torture && $run{'ftp2'} &&
               !responsive_pingpong_server("ftp", "2", $verbose)) {
                stopserver('ftp2');
            }
            if(!$run{'ftp2'}) {
                ($pid, $pid2) = runpingpongserver("ftp", "2", $verbose);
                if($pid <= 0) {
                    return "failed starting FTP2 server";
                }
                printf ("* pid ftp2 => %d %d\n", $pid, $pid2) if($verbose);
                $run{'ftp2'}="$pid $pid2";
            }
        }
        elsif($what eq "ftp-ipv6") {
            if($torture && $run{'ftp-ipv6'} &&
               !responsive_pingpong_server("ftp", "", $verbose, "ipv6")) {
                stopserver('ftp-ipv6');
            }
            if(!$run{'ftp-ipv6'}) {
                ($pid, $pid2) = runpingpongserver("ftp", "", $verbose, "ipv6");
                if($pid <= 0) {
                    return "failed starting FTP-IPv6 server";
                }
                logmsg sprintf("* pid ftp-ipv6 => %d %d\n", $pid,
                       $pid2) if($verbose);
                $run{'ftp-ipv6'}="$pid $pid2";
            }
        }
        elsif($what eq "gopher") {
            if($torture && $run{'gopher'} &&
               !responsive_http_server("gopher", $verbose, 0, $GOPHERPORT)) {
                stopserver('gopher');
            }
            if(!$run{'gopher'}) {
                ($pid, $pid2) = runhttpserver("gopher", $verbose, 0,
                                              $GOPHERPORT);
                if($pid <= 0) {
                    return "failed starting GOPHER server";
                }
                logmsg sprintf ("* pid gopher => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'gopher'}="$pid $pid2";
            }
        }
        elsif($what eq "gopher-ipv6") {
            if($torture && $run{'gopher-ipv6'} &&
               !responsive_http_server("gopher", $verbose, "ipv6",
                                       $GOPHER6PORT)) {
                stopserver('gopher-ipv6');
            }
            if(!$run{'gopher-ipv6'}) {
                ($pid, $pid2) = runhttpserver("gopher", $verbose, "ipv6",
                                              $GOPHER6PORT);
                if($pid <= 0) {
                    return "failed starting GOPHER-IPv6 server";
                }
                logmsg sprintf("* pid gopher-ipv6 => %d %d\n", $pid,
                               $pid2) if($verbose);
                $run{'gopher-ipv6'}="$pid $pid2";
            }
        }
        elsif($what eq "http/2") {
            if(!$run{'http/2'}) {
                ($pid, $pid2) = runhttp2server($verbose, $HTTP2PORT);
                if($pid <= 0) {
                    return "failed starting HTTP/2 server";
                }
                logmsg sprintf ("* pid http/2 => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'http/2'}="$pid $pid2";
            }
        }
        elsif($what eq "http") {
            if($torture && $run{'http'} &&
               !responsive_http_server("http", $verbose, 0, $HTTPPORT)) {
                stopserver('http');
            }
            if(!$run{'http'}) {
                ($pid, $pid2) = runhttpserver("http", $verbose, 0,
                                              $HTTPPORT);
                if($pid <= 0) {
                    return "failed starting HTTP server";
                }
                logmsg sprintf ("* pid http => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'http'}="$pid $pid2";
            }
        }
        elsif($what eq "http-proxy") {
            if($torture && $run{'http-proxy'} &&
               !responsive_http_server("http", $verbose, "proxy",
                                       $HTTPPROXYPORT)) {
                stopserver('http-proxy');
            }
            if(!$run{'http-proxy'}) {
                ($pid, $pid2) = runhttpserver("http", $verbose, "proxy",
                                              $HTTPPROXYPORT);
                if($pid <= 0) {
                    return "failed starting HTTP-proxy server";
                }
                logmsg sprintf ("* pid http-proxy => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'http-proxy'}="$pid $pid2";
            }
        }
        elsif($what eq "http-ipv6") {
            if($torture && $run{'http-ipv6'} &&
               !responsive_http_server("http", $verbose, "ipv6", $HTTP6PORT)) {
                stopserver('http-ipv6');
            }
            if(!$run{'http-ipv6'}) {
                ($pid, $pid2) = runhttpserver("http", $verbose, "ipv6",
                                              $HTTP6PORT);
                if($pid <= 0) {
                    return "failed starting HTTP-IPv6 server";
                }
                logmsg sprintf("* pid http-ipv6 => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'http-ipv6'}="$pid $pid2";
            }
        }
        elsif($what eq "rtsp") {
            if($torture && $run{'rtsp'} &&
               !responsive_rtsp_server($verbose)) {
                stopserver('rtsp');
            }
            if(!$run{'rtsp'}) {
                ($pid, $pid2) = runrtspserver($verbose);
                if($pid <= 0) {
                    return "failed starting RTSP server";
                }
                printf ("* pid rtsp => %d %d\n", $pid, $pid2) if($verbose);
                $run{'rtsp'}="$pid $pid2";
            }
        }
        elsif($what eq "rtsp-ipv6") {
            if($torture && $run{'rtsp-ipv6'} &&
               !responsive_rtsp_server($verbose, "ipv6")) {
                stopserver('rtsp-ipv6');
            }
            if(!$run{'rtsp-ipv6'}) {
                ($pid, $pid2) = runrtspserver($verbose, "ipv6");
                if($pid <= 0) {
                    return "failed starting RTSP-IPv6 server";
                }
                logmsg sprintf("* pid rtsp-ipv6 => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'rtsp-ipv6'}="$pid $pid2";
            }
        }
        elsif($what eq "ftps") {
            if(!$stunnel) {
                # we can't run ftps tests without stunnel
                return "no stunnel";
            }
            if($runcert{'ftps'} && ($runcert{'ftps'} ne $certfile)) {
                # stop server when running and using a different cert
                stopserver('ftps');
            }
            if($torture && $run{'ftp'} &&
               !responsive_pingpong_server("ftp", "", $verbose)) {
                stopserver('ftp');
            }
            if(!$run{'ftp'}) {
                ($pid, $pid2) = runpingpongserver("ftp", "", $verbose);
                if($pid <= 0) {
                    return "failed starting FTP server";
                }
                printf ("* pid ftp => %d %d\n", $pid, $pid2) if($verbose);
                $run{'ftp'}="$pid $pid2";
            }
            if(!$run{'ftps'}) {
                ($pid, $pid2) = runftpsserver($verbose, "", $certfile);
                if($pid <= 0) {
                    return "failed starting FTPS server (stunnel)";
                }
                logmsg sprintf("* pid ftps => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'ftps'}="$pid $pid2";
            }
        }
        elsif($what eq "file") {
            # we support it but have no server!
        }
        elsif($what eq "https") {
            if(!$stunnel) {
                # we can't run https tests without stunnel
                return "no stunnel";
            }
            if($runcert{'https'} && ($runcert{'https'} ne $certfile)) {
                # stop server when running and using a different cert
                stopserver('https');
            }
            if($torture && $run{'http'} &&
               !responsive_http_server("http", $verbose, 0, $HTTPPORT)) {
                stopserver('http');
            }
            if(!$run{'http'}) {
                ($pid, $pid2) = runhttpserver("http", $verbose, 0,
                                              $HTTPPORT);
                if($pid <= 0) {
                    return "failed starting HTTP server";
                }
                printf ("* pid http => %d %d\n", $pid, $pid2) if($verbose);
                $run{'http'}="$pid $pid2";
            }
            if(!$run{'https'}) {
                ($pid, $pid2) = runhttpsserver($verbose, "", $certfile);
                if($pid <= 0) {
                    return "failed starting HTTPS server (stunnel)";
                }
                logmsg sprintf("* pid https => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'https'}="$pid $pid2";
            }
        }
        elsif($what eq "httptls") {
            if(!$httptlssrv) {
                # for now, we can't run http TLS-EXT tests without gnutls-serv
                return "no gnutls-serv";
            }
            if($torture && $run{'httptls'} &&
               !responsive_httptls_server($verbose, "IPv4")) {
                stopserver('httptls');
            }
            if(!$run{'httptls'}) {
                ($pid, $pid2) = runhttptlsserver($verbose, "IPv4");
                if($pid <= 0) {
                    return "failed starting HTTPTLS server (gnutls-serv)";
                }
                logmsg sprintf("* pid httptls => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'httptls'}="$pid $pid2";
            }
        }
        elsif($what eq "httptls-ipv6") {
            if(!$httptlssrv) {
                # for now, we can't run http TLS-EXT tests without gnutls-serv
                return "no gnutls-serv";
            }
            if($torture && $run{'httptls-ipv6'} &&
               !responsive_httptls_server($verbose, "ipv6")) {
                stopserver('httptls-ipv6');
            }
            if(!$run{'httptls-ipv6'}) {
                ($pid, $pid2) = runhttptlsserver($verbose, "ipv6");
                if($pid <= 0) {
                    return "failed starting HTTPTLS-IPv6 server (gnutls-serv)";
                }
                logmsg sprintf("* pid httptls-ipv6 => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'httptls-ipv6'}="$pid $pid2";
            }
        }
        elsif($what eq "tftp") {
            if($torture && $run{'tftp'} &&
               !responsive_tftp_server("", $verbose)) {
                stopserver('tftp');
            }
            if(!$run{'tftp'}) {
                ($pid, $pid2) = runtftpserver("", $verbose);
                if($pid <= 0) {
                    return "failed starting TFTP server";
                }
                printf ("* pid tftp => %d %d\n", $pid, $pid2) if($verbose);
                $run{'tftp'}="$pid $pid2";
            }
        }
        elsif($what eq "tftp-ipv6") {
            if($torture && $run{'tftp-ipv6'} &&
               !responsive_tftp_server("", $verbose, "ipv6")) {
                stopserver('tftp-ipv6');
            }
            if(!$run{'tftp-ipv6'}) {
                ($pid, $pid2) = runtftpserver("", $verbose, "ipv6");
                if($pid <= 0) {
                    return "failed starting TFTP-IPv6 server";
                }
                printf("* pid tftp-ipv6 => %d %d\n", $pid, $pid2) if($verbose);
                $run{'tftp-ipv6'}="$pid $pid2";
            }
        }
        elsif($what eq "sftp" || $what eq "scp") {
            if(!$run{'ssh'}) {
                ($pid, $pid2) = runsshserver("", $verbose);
                if($pid <= 0) {
                    return "failed starting SSH server";
                }
                printf ("* pid ssh => %d %d\n", $pid, $pid2) if($verbose);
                $run{'ssh'}="$pid $pid2";
            }
        }
        elsif($what eq "socks4" || $what eq "socks5" ) {
            if(!$run{'socks'}) {
                ($pid, $pid2) = runsocksserver("", $verbose);
                if($pid <= 0) {
                    return "failed starting socks server";
                }
                printf ("* pid socks => %d %d\n", $pid, $pid2) if($verbose);
                $run{'socks'}="$pid $pid2";
            }
        }
        elsif($what eq "http-unix") {
            if($torture && $run{'http-unix'} &&
               !responsive_http_server("http", $verbose, "unix", $HTTPUNIXPATH)) {
                stopserver('http-unix');
            }
            if(!$run{'http-unix'}) {
                ($pid, $pid2) = runhttpserver("http", $verbose, "unix",
                                              $HTTPUNIXPATH);
                if($pid <= 0) {
                    return "failed starting HTTP-unix server";
                }
                logmsg sprintf("* pid http-unix => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'http-unix'}="$pid $pid2";
            }
        }
        elsif($what eq "dict") {
            if(!$run{'dict'}) {
                ($pid, $pid2) = rundictserver($verbose, "", $DICTPORT);
                if($pid <= 0) {
                    return "failed starting DICT server";
                }
                logmsg sprintf ("* pid DICT => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'dict'}="$pid $pid2";
            }
        }
        elsif($what eq "smb") {
            if(!$run{'smb'}) {
                ($pid, $pid2) = runsmbserver($verbose, "", $SMBPORT);
                if($pid <= 0) {
                    return "failed starting SMB server";
                }
                logmsg sprintf ("* pid SMB => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'dict'}="$pid $pid2";
            }
        }
        elsif($what eq "telnet") {
            if(!$run{'telnet'}) {
                ($pid, $pid2) = runnegtelnetserver($verbose,
                                                   "",
                                                   $NEGTELNETPORT);
                if($pid <= 0) {
                    return "failed starting neg TELNET server";
                }
                logmsg sprintf ("* pid neg TELNET => %d %d\n", $pid, $pid2)
                    if($verbose);
                $run{'dict'}="$pid $pid2";
            }
        }
        elsif($what eq "none") {
            logmsg "* starts no server\n" if ($verbose);
        }
        else {
            warn "we don't support a server for $what";
            return "no server for $what";
        }
    }
    return 0;
}

##############################################################################
# This function makes sure the right set of server is running for the
# specified test case. This is a useful design when we run single tests as not
# all servers need to run then!
#
# Returns: a string, blank if everything is fine or a reason why it failed
#
sub serverfortest {
    my ($testnum)=@_;

    my @what = getpart("client", "server");

    if(!$what[0]) {
        warn "Test case $testnum has no server(s) specified";
        return "no server specified";
    }

    for(my $i = scalar(@what) - 1; $i >= 0; $i--) {
        my $srvrline = $what[$i];
        chomp $srvrline if($srvrline);
        if($srvrline =~ /^(\S+)((\s*)(.*))/) {
            my $server = "${1}";
            my $lnrest = "${2}";
            my $tlsext;
            if($server =~ /^(httptls)(\+)(ext|srp)(\d*)(-ipv6|)$/) {
                $server = "${1}${4}${5}";
                $tlsext = uc("TLS-${3}");
            }
            if(! grep /^\Q$server\E$/, @protocols) {
                if(substr($server,0,5) ne "socks") {
                    if($tlsext) {
                        return "curl lacks $tlsext support";
                    }
                    else {
                        return "curl lacks $server server support";
                    }
                }
            }
            $what[$i] = "$server$lnrest" if($tlsext);
        }
    }

    return &startservers(@what);
}

#######################################################################
# runtimestats displays test-suite run time statistics
#
sub runtimestats {
    my $lasttest = $_[0];

    return if(not $timestats);

    logmsg "\nTest suite total running time breakdown per task...\n\n";

    my @timesrvr;
    my @timeprep;
    my @timetool;
    my @timelock;
    my @timevrfy;
    my @timetest;
    my $timesrvrtot = 0.0;
    my $timepreptot = 0.0;
    my $timetooltot = 0.0;
    my $timelocktot = 0.0;
    my $timevrfytot = 0.0;
    my $timetesttot = 0.0;
    my $counter;

    for my $testnum (1 .. $lasttest) {
        if($timesrvrini{$testnum}) {
            $timesrvrtot += $timesrvrend{$testnum} - $timesrvrini{$testnum};
            $timepreptot +=
                (($timetoolini{$testnum} - $timeprepini{$testnum}) -
                 ($timesrvrend{$testnum} - $timesrvrini{$testnum}));
            $timetooltot += $timetoolend{$testnum} - $timetoolini{$testnum};
            $timelocktot += $timesrvrlog{$testnum} - $timetoolend{$testnum};
            $timevrfytot += $timevrfyend{$testnum} - $timesrvrlog{$testnum};
            $timetesttot += $timevrfyend{$testnum} - $timeprepini{$testnum};
            push @timesrvr, sprintf("%06.3f  %04d",
                $timesrvrend{$testnum} - $timesrvrini{$testnum}, $testnum);
            push @timeprep, sprintf("%06.3f  %04d",
                ($timetoolini{$testnum} - $timeprepini{$testnum}) -
                ($timesrvrend{$testnum} - $timesrvrini{$testnum}), $testnum);
            push @timetool, sprintf("%06.3f  %04d",
                $timetoolend{$testnum} - $timetoolini{$testnum}, $testnum);
            push @timelock, sprintf("%06.3f  %04d",
                $timesrvrlog{$testnum} - $timetoolend{$testnum}, $testnum);
            push @timevrfy, sprintf("%06.3f  %04d",
                $timevrfyend{$testnum} - $timesrvrlog{$testnum}, $testnum);
            push @timetest, sprintf("%06.3f  %04d",
                $timevrfyend{$testnum} - $timeprepini{$testnum}, $testnum);
        }
    }

    {
        no warnings 'numeric';
        @timesrvr = sort { $b <=> $a } @timesrvr;
        @timeprep = sort { $b <=> $a } @timeprep;
        @timetool = sort { $b <=> $a } @timetool;
        @timelock = sort { $b <=> $a } @timelock;
        @timevrfy = sort { $b <=> $a } @timevrfy;
        @timetest = sort { $b <=> $a } @timetest;
    }

    logmsg "Spent ". sprintf("%08.3f ", $timesrvrtot) .
           "seconds starting and verifying test harness servers.\n";
    logmsg "Spent ". sprintf("%08.3f ", $timepreptot) .
           "seconds reading definitions and doing test preparations.\n";
    logmsg "Spent ". sprintf("%08.3f ", $timetooltot) .
           "seconds actually running test tools.\n";
    logmsg "Spent ". sprintf("%08.3f ", $timelocktot) .
           "seconds awaiting server logs lock removal.\n";
    logmsg "Spent ". sprintf("%08.3f ", $timevrfytot) .
           "seconds verifying test results.\n";
    logmsg "Spent ". sprintf("%08.3f ", $timetesttot) .
           "seconds doing all of the above.\n";

    $counter = 25;
    logmsg "\nTest server starting and verification time per test ".
        sprintf("(%s)...\n\n", (not $fullstats)?"top $counter":"full");
    logmsg "-time-  test\n";
    logmsg "------  ----\n";
    foreach my $txt (@timesrvr) {
        last if((not $fullstats) && (not $counter--));
        logmsg "$txt\n";
    }

    $counter = 10;
    logmsg "\nTest definition reading and preparation time per test ".
        sprintf("(%s)...\n\n", (not $fullstats)?"top $counter":"full");
    logmsg "-time-  test\n";
    logmsg "------  ----\n";
    foreach my $txt (@timeprep) {
        last if((not $fullstats) && (not $counter--));
        logmsg "$txt\n";
    }

    $counter = 25;
    logmsg "\nTest tool execution time per test ".
        sprintf("(%s)...\n\n", (not $fullstats)?"top $counter":"full");
    logmsg "-time-  test\n";
    logmsg "------  ----\n";
    foreach my $txt (@timetool) {
        last if((not $fullstats) && (not $counter--));
        logmsg "$txt\n";
    }

    $counter = 15;
    logmsg "\nTest server logs lock removal time per test ".
        sprintf("(%s)...\n\n", (not $fullstats)?"top $counter":"full");
    logmsg "-time-  test\n";
    logmsg "------  ----\n";
    foreach my $txt (@timelock) {
        last if((not $fullstats) && (not $counter--));
        logmsg "$txt\n";
    }

    $counter = 10;
    logmsg "\nTest results verification time per test ".
        sprintf("(%s)...\n\n", (not $fullstats)?"top $counter":"full");
    logmsg "-time-  test\n";
    logmsg "------  ----\n";
    foreach my $txt (@timevrfy) {
        last if((not $fullstats) && (not $counter--));
        logmsg "$txt\n";
    }

    $counter = 50;
    logmsg "\nTotal time per test ".
        sprintf("(%s)...\n\n", (not $fullstats)?"top $counter":"full");
    logmsg "-time-  test\n";
    logmsg "------  ----\n";
    foreach my $txt (@timetest) {
        last if((not $fullstats) && (not $counter--));
        logmsg "$txt\n";
    }

    logmsg "\n";
}

# globally disabled tests
disabledtests("$TESTDIR/DISABLED");

# locally disabled tests, ignored by git etc
disabledtests("$TESTDIR/DISABLED.local");

#######################################################################
# Check options to this test program
#

my $number=0;
my $fromnum=-1;
my @testthis;
while(@ARGV) {
    if ($ARGV[0] eq "-v") {
        # verbose output
        $verbose=1;
    }
    elsif($ARGV[0] =~ /^-b(.*)/) {
        my $portno=$1;
        if($portno =~ s/(\d+)$//) {
            $base = int $1;
        }
    }
    elsif ($ARGV[0] eq "-c") {
        # use this path to curl instead of default
        $DBGCURL=$CURL="\"$ARGV[1]\"";
        shift @ARGV;
    }
    elsif ($ARGV[0] eq "-vc") {
        # use this path to a curl used to verify servers

        # Particularly useful when you introduce a crashing bug somewhere in
        # the development version as then it won't be able to run any tests
        # since it can't verify the servers!

        $VCURL="\"$ARGV[1]\"";
        shift @ARGV;
    }
    elsif ($ARGV[0] eq "-d") {
        # have the servers display protocol output
        $debugprotocol=1;
    }
    elsif($ARGV[0] eq "-e") {
        # run the tests cases event based if possible
        $run_event_based=1;
    }
    elsif ($ARGV[0] eq "-g") {
        # run this test with gdb
        $gdbthis=1;
    }
    elsif ($ARGV[0] eq "-gw") {
        # run this test with windowed gdb
        $gdbthis=1;
        $gdbxwin=1;
    }
    elsif($ARGV[0] eq "-s") {
        # short output
        $short=1;
    }
    elsif($ARGV[0] eq "-am") {
        # automake-style output
        $short=1;
        $automakestyle=1;
    }
    elsif($ARGV[0] eq "-n") {
        # no valgrind
        undef $valgrind;
    }
    elsif ($ARGV[0] eq "-R") {
        # execute in scrambled order
        $scrambleorder=1;
    }
    elsif($ARGV[0] =~ /^-t(.*)/) {
        # torture
        $torture=1;
        my $xtra = $1;

        if($xtra =~ s/(\d+)$//) {
            $tortalloc = $1;
        }
    }
    elsif($ARGV[0] eq "-a") {
        # continue anyway, even if a test fail
        $anyway=1;
    }
    elsif($ARGV[0] eq "-p") {
        $postmortem=1;
    }
    elsif($ARGV[0] eq "-l") {
        # lists the test case names only
        $listonly=1;
    }
    elsif($ARGV[0] eq "-k") {
        # keep stdout and stderr files after tests
        $keepoutfiles=1;
    }
    elsif($ARGV[0] eq "-r") {
        # run time statistics needs Time::HiRes
        if($Time::HiRes::VERSION) {
            keys(%timeprepini) = 1000;
            keys(%timesrvrini) = 1000;
            keys(%timesrvrend) = 1000;
            keys(%timetoolini) = 1000;
            keys(%timetoolend) = 1000;
            keys(%timesrvrlog) = 1000;
            keys(%timevrfyend) = 1000;
            $timestats=1;
            $fullstats=0;
        }
    }
    elsif($ARGV[0] eq "-rf") {
        # run time statistics needs Time::HiRes
        if($Time::HiRes::VERSION) {
            keys(%timeprepini) = 1000;
            keys(%timesrvrini) = 1000;
            keys(%timesrvrend) = 1000;
            keys(%timetoolini) = 1000;
            keys(%timetoolend) = 1000;
            keys(%timesrvrlog) = 1000;
            keys(%timevrfyend) = 1000;
            $timestats=1;
            $fullstats=1;
        }
    }
    elsif(($ARGV[0] eq "-h") || ($ARGV[0] eq "--help")) {
        # show help text
        print <<EOHELP
Usage: runtests.pl [options] [test selection(s)]
  -a       continue even if a test fails
  -bN      use base port number N for test servers (default $base)
  -c path  use this curl executable
  -d       display server debug info
  -e       event-based execution
  -g       run the test case with gdb
  -gw      run the test case with gdb as a windowed application
  -h       this help text
  -k       keep stdout and stderr files present after tests
  -l       list all test case names/descriptions
  -n       no valgrind
  -p       print log file contents when a test fails
  -R       scrambled order
  -r       run time statistics
  -rf      full run time statistics
  -s       short output
  -am      automake style output PASS/FAIL: [number] [name]
  -t[N]    torture (simulate function failures); N means fail Nth function
  -v       verbose output
  -vc path use this curl only to verify the existing servers
  [num]    like "5 6 9" or " 5 to 22 " to run those tests only
  [!num]   like "!5 !6 !9" to disable those tests
  [keyword] like "IPv6" to select only tests containing the key word
  [!keyword] like "!cookies" to disable any tests containing the key word
EOHELP
    ;
        exit;
    }
    elsif($ARGV[0] =~ /^(\d+)/) {
        $number = $1;
        if($fromnum >= 0) {
            for my $n ($fromnum .. $number) {
                if($disabled{$n}) {
                    # skip disabled test cases
                    my $why = "configured as DISABLED";
                    $skipped++;
                    $skipped{$why}++;
                    $teststat[$n]=$why; # store reason for this test case
                }
                else {
                    push @testthis, $n;
                }
            }
            $fromnum = -1;
        }
        else {
            push @testthis, $1;
        }
    }
    elsif($ARGV[0] =~ /^to$/i) {
        $fromnum = $number+1;
    }
    elsif($ARGV[0] =~ /^!(\d+)/) {
        $fromnum = -1;
        $disabled{$1}=$1;
    }
    elsif($ARGV[0] =~ /^!(.+)/) {
        $disabled_keywords{lc($1)}=$1;
    }
    elsif($ARGV[0] =~ /^([-[{a-zA-Z].*)/) {
        $enabled_keywords{lc($1)}=$1;
    }
    else {
        print "Unknown option: $ARGV[0]\n";
        exit;
    }
    shift @ARGV;
}

if(@testthis && ($testthis[0] ne "")) {
    $TESTCASES=join(" ", @testthis);
}

if($valgrind) {
    # we have found valgrind on the host, use it

    # verify that we can invoke it fine
    my $code = runclient("valgrind >/dev/null 2>&1");

    if(($code>>8) != 1) {
        #logmsg "Valgrind failure, disable it\n";
        undef $valgrind;
    } else {

        # since valgrind 2.1.x, '--tool' option is mandatory
        # use it, if it is supported by the version installed on the system
        runclient("valgrind --help 2>&1 | grep -- --tool > /dev/null 2>&1");
        if (($? >> 8)==0) {
            $valgrind_tool="--tool=memcheck";
        }
        open(C, "<$CURL");
        my $l = <C>;
        if($l =~ /^\#\!/) {
            # A shell script. This is typically when built with libtool,
            $valgrind="../libtool --mode=execute $valgrind";
        }
        close(C);

        # valgrind 3 renamed the --logfile option to --log-file!!!
        my $ver=join(' ', runclientoutput("valgrind --version"));
        # cut off all but digits and dots
        $ver =~ s/[^0-9.]//g;

        if($ver =~ /^(\d+)/) {
            $ver = $1;
            if($ver >= 3) {
                $valgrind_logfile="--log-file";
            }
        }
    }
}

if ($gdbthis) {
    # open the executable curl and read the first 4 bytes of it
    open(CHECK, "<$CURL");
    my $c;
    sysread CHECK, $c, 4;
    close(CHECK);
    if($c eq "#! /") {
        # A shell script. This is typically when built with libtool,
        $libtool = 1;
        $gdb = "../libtool --mode=execute gdb";
    }
}

$HTTPPORT        = $base++; # HTTP server port
$HTTPSPORT       = $base++; # HTTPS (stunnel) server port
$FTPPORT         = $base++; # FTP server port
$FTPSPORT        = $base++; # FTPS (stunnel) server port
$HTTP6PORT       = $base++; # HTTP IPv6 server port
$FTP2PORT        = $base++; # FTP server 2 port
$FTP6PORT        = $base++; # FTP IPv6 port
$TFTPPORT        = $base++; # TFTP (UDP) port
$TFTP6PORT       = $base++; # TFTP IPv6 (UDP) port
$SSHPORT         = $base++; # SSH (SCP/SFTP) port
$SOCKSPORT       = $base++; # SOCKS port
$POP3PORT        = $base++; # POP3 server port
$POP36PORT       = $base++; # POP3 IPv6 server port
$IMAPPORT        = $base++; # IMAP server port
$IMAP6PORT       = $base++; # IMAP IPv6 server port
$SMTPPORT        = $base++; # SMTP server port
$SMTP6PORT       = $base++; # SMTP IPv6 server port
$RTSPPORT        = $base++; # RTSP server port
$RTSP6PORT       = $base++; # RTSP IPv6 server port
$GOPHERPORT      = $base++; # Gopher IPv4 server port
$GOPHER6PORT     = $base++; # Gopher IPv6 server port
$HTTPTLSPORT     = $base++; # HTTP TLS (non-stunnel) server port
$HTTPTLS6PORT    = $base++; # HTTP TLS (non-stunnel) IPv6 server port
$HTTPPROXYPORT   = $base++; # HTTP proxy port, when using CONNECT
$HTTP2PORT       = $base++; # HTTP/2 port
$DICTPORT        = $base++; # DICT port
$SMBPORT         = $base++; # SMB port
$SMBSPORT        = $base++; # SMBS port
$NEGTELNETPORT   = $base++; # TELNET port with negotiation
$HTTPUNIXPATH    = 'http.sock'; # HTTP server Unix domain socket path

#######################################################################
# clear and create logging directory:
#

cleardir($LOGDIR);
mkdir($LOGDIR, 0777);

#######################################################################
# initialize some variables
#

get_disttests();
init_serverpidfile_hash();

#######################################################################
# Output curl version and host info being tested
#

if(!$listonly) {
    checksystem();
}

#######################################################################
# Fetch all disabled tests, if there are any
#

sub disabledtests {
    my ($file) = @_;

    if(open(D, "<$file")) {
        while(<D>) {
            if(/^ *\#/) {
                # allow comments
                next;
            }
            if($_ =~ /(\d+)/) {
                $disabled{$1}=$1; # disable this test number
            }
        }
        close(D);
    }
}

#######################################################################
# If 'all' tests are requested, find out all test numbers
#

if ( $TESTCASES eq "all") {
    # Get all commands and find out their test numbers
    opendir(DIR, $TESTDIR) || die "can't opendir $TESTDIR: $!";
    my @cmds = grep { /^test([0-9]+)$/ && -f "$TESTDIR/$_" } readdir(DIR);
    closedir(DIR);

    $TESTCASES=""; # start with no test cases

    # cut off everything but the digits
    for(@cmds) {
        $_ =~ s/[a-z\/\.]*//g;
    }
    # sort the numbers from low to high
    foreach my $n (sort { $a <=> $b } @cmds) {
        if($disabled{$n}) {
            # skip disabled test cases
            my $why = "configured as DISABLED";
            $skipped++;
            $skipped{$why}++;
            $teststat[$n]=$why; # store reason for this test case
            next;
        }
        $TESTCASES .= " $n";
    }
}
else {
    my $verified="";
    map {
        if (-e "$TESTDIR/test$_") {
            $verified.="$_ ";
        }
    } split(" ", $TESTCASES);
    if($verified eq "") {
        print "No existing test cases were specified\n";
        exit;
    }
    $TESTCASES = $verified;
}

if($scrambleorder) {
    # scramble the order of the test cases
    my @rand;
    while($TESTCASES) {
        my @all = split(/ +/, $TESTCASES);
        if(!$all[0]) {
            # if the first is blank, shift away it
            shift @all;
        }
        my $r = rand @all;
        push @rand, $all[$r];
        $all[$r]="";
        $TESTCASES = join(" ", @all);
    }
    $TESTCASES = join(" ", @rand);
}

#######################################################################
# Start the command line log
#
open(CMDLOG, ">$CURLLOG") ||
    logmsg "can't log command lines to $CURLLOG\n";

#######################################################################

# Display the contents of the given file.  Line endings are canonicalized
# and excessively long files are elided
sub displaylogcontent {
    my ($file)=@_;
    if(open(SINGLE, "<$file")) {
        my $linecount = 0;
        my $truncate;
        my @tail;
        while(my $string = <SINGLE>) {
            $string =~ s/\r\n/\n/g;
            $string =~ s/[\r\f\032]/\n/g;
            $string .= "\n" unless ($string =~ /\n$/);
            $string =~ tr/\n//;
            for my $line (split("\n", $string)) {
                $line =~ s/\s*\!$//;
                if ($truncate) {
                    push @tail, " $line\n";
                } else {
                    logmsg " $line\n";
                }
                $linecount++;
                $truncate = $linecount > 1000;
            }
        }
        if(@tail) {
            my $tailshow = 200;
            my $tailskip = 0;
            my $tailtotal = scalar @tail;
            if($tailtotal > $tailshow) {
                $tailskip = $tailtotal - $tailshow;
                logmsg "=== File too long: $tailskip lines omitted here\n";
            }
            for($tailskip .. $tailtotal-1) {
                logmsg "$tail[$_]";
            }
        }
        close(SINGLE);
    }
}

sub displaylogs {
    my ($testnum)=@_;
    opendir(DIR, "$LOGDIR") ||
        die "can't open dir: $!";
    my @logs = readdir(DIR);
    closedir(DIR);

    logmsg "== Contents of files in the $LOGDIR/ dir after test $testnum\n";
    foreach my $log (sort @logs) {
        if($log =~ /\.(\.|)$/) {
            next; # skip "." and ".."
        }
        if($log =~ /^\.nfs/) {
            next; # skip ".nfs"
        }
        if(($log eq "memdump") || ($log eq "core")) {
            next; # skip "memdump" and  "core"
        }
        if((-d "$LOGDIR/$log") || (! -s "$LOGDIR/$log")) {
            next; # skip directory and empty files
        }
        if(($log =~ /^stdout\d+/) && ($log !~ /^stdout$testnum/)) {
            next; # skip stdoutNnn of other tests
        }
        if(($log =~ /^stderr\d+/) && ($log !~ /^stderr$testnum/)) {
            next; # skip stderrNnn of other tests
        }
        if(($log =~ /^upload\d+/) && ($log !~ /^upload$testnum/)) {
            next; # skip uploadNnn of other tests
        }
        if(($log =~ /^curl\d+\.out/) && ($log !~ /^curl$testnum\.out/)) {
            next; # skip curlNnn.out of other tests
        }
        if(($log =~ /^test\d+\.txt/) && ($log !~ /^test$testnum\.txt/)) {
            next; # skip testNnn.txt of other tests
        }
        if(($log =~ /^file\d+\.txt/) && ($log !~ /^file$testnum\.txt/)) {
            next; # skip fileNnn.txt of other tests
        }
        if(($log =~ /^netrc\d+/) && ($log !~ /^netrc$testnum/)) {
            next; # skip netrcNnn of other tests
        }
        if(($log =~ /^trace\d+/) && ($log !~ /^trace$testnum/)) {
            next; # skip traceNnn of other tests
        }
        if(($log =~ /^valgrind\d+/) && ($log !~ /^valgrind$testnum(\..*|)$/)) {
            next; # skip valgrindNnn of other tests
        }
        logmsg "=== Start of file $log\n";
        displaylogcontent("$LOGDIR/$log");
        logmsg "=== End of file $log\n";
    }
}

#######################################################################
# The main test-loop
#

my $failed;
my $testnum;
my $ok=0;
my $total=0;
my $lasttest=0;
my @at = split(" ", $TESTCASES);
my $count=0;

$start = time();

foreach $testnum (@at) {

    $lasttest = $testnum if($testnum > $lasttest);
    $count++;

    my $error = singletest($run_event_based, $testnum, $count, scalar(@at));
    if($error < 0) {
        # not a test we can run
        next;
    }

    $total++; # number of tests we've run

    if($error>0) {
        $failed.= "$testnum ";
        if($postmortem) {
            # display all files in log/ in a nice way
            displaylogs($testnum);
        }
        if(!$anyway) {
            # a test failed, abort
            logmsg "\n - abort tests\n";
            last;
        }
    }
    elsif(!$error) {
        $ok++; # successful test counter
    }

    # loop for next test
}

my $sofar = time() - $start;

#######################################################################
# Close command log
#
close(CMDLOG);

# Tests done, stop the servers
stopservers($verbose);

my $all = $total + $skipped;

runtimestats($lasttest);

if($total) {
    logmsg sprintf("TESTDONE: $ok tests out of $total reported OK: %d%%\n",
                   $ok/$total*100);

    if($ok != $total) {
        logmsg "TESTFAIL: These test cases failed: $failed\n";
    }
}
else {
    logmsg "TESTFAIL: No tests were performed\n";
}

if($all) {
    logmsg "TESTDONE: $all tests were considered during ".
        sprintf("%.0f", $sofar) ." seconds.\n";
}

if($skipped && !$short) {
    my $s=0;
    logmsg "TESTINFO: $skipped tests were skipped due to these restraints:\n";

    for(keys %skipped) {
        my $r = $_;
        printf "TESTINFO: \"%s\" %d times (", $r, $skipped{$_};

        # now show all test case numbers that had this reason for being
        # skipped
        my $c=0;
        my $max = 9;
        for(0 .. scalar @teststat) {
            my $t = $_;
            if($teststat[$_] && ($teststat[$_] eq $r)) {
                if($c < $max) {
                    logmsg ", " if($c);
                    logmsg $_;
                }
                $c++;
            }
        }
        if($c > $max) {
            logmsg " and ".($c-$max)." more";
        }
        logmsg ")\n";
    }
}

if($total && ($ok != $total)) {
    exit 1;
}
