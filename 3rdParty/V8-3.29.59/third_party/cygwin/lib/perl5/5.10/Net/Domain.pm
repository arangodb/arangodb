# Net::Domain.pm
#
# Copyright (c) 1995-1998 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package Net::Domain;

require Exporter;

use Carp;
use strict;
use vars qw($VERSION @ISA @EXPORT_OK);
use Net::Config;

@ISA       = qw(Exporter);
@EXPORT_OK = qw(hostname hostdomain hostfqdn domainname);

$VERSION = "2.20";

my ($host, $domain, $fqdn) = (undef, undef, undef);

# Try every conceivable way to get hostname.


sub _hostname {

  # we already know it
  return $host
    if (defined $host);

  if ($^O eq 'MSWin32') {
    require Socket;
    my ($name, $alias, $type, $len, @addr) = gethostbyname($ENV{'COMPUTERNAME'} || 'localhost');
    while (@addr) {
      my $a = shift(@addr);
      $host = gethostbyaddr($a, Socket::AF_INET());
      last if defined $host;
    }
    if (defined($host) && index($host, '.') > 0) {
      $fqdn = $host;
      ($host, $domain) = $fqdn =~ /^([^\.]+)\.(.*)$/;
    }
    return $host;
  }
  elsif ($^O eq 'MacOS') {
    chomp($host = `hostname`);
  }
  elsif ($^O eq 'VMS') {    ## multiple varieties of net s/w makes this hard
    $host = $ENV{'UCX$INET_HOST'}      if defined($ENV{'UCX$INET_HOST'});
    $host = $ENV{'MULTINET_HOST_NAME'} if defined($ENV{'MULTINET_HOST_NAME'});
    if (index($host, '.') > 0) {
      $fqdn = $host;
      ($host, $domain) = $fqdn =~ /^([^\.]+)\.(.*)$/;
    }
    return $host;
  }
  else {
    local $SIG{'__DIE__'};

    # syscall is preferred since it avoids tainting problems
    eval {
      my $tmp = "\0" x 256;    ## preload scalar
      eval {
        package main;
        require "syscall.ph";
        defined(&main::SYS_gethostname);
        }
        || eval {
        package main;
        require "sys/syscall.ph";
        defined(&main::SYS_gethostname);
        }
        and $host =
        (syscall(&main::SYS_gethostname, $tmp, 256) == 0)
        ? $tmp
        : undef;
      }

      # POSIX
      || eval {
      require POSIX;
      $host = (POSIX::uname())[1];
      }

      # trusty old hostname command
      || eval {
      chop($host = `(hostname) 2>/dev/null`);    # BSD'ish
      }

      # sysV/POSIX uname command (may truncate)
      || eval {
      chop($host = `uname -n 2>/dev/null`);      ## SYSV'ish && POSIX'ish
      }

      # Apollo pre-SR10
      || eval { $host = (split(/[:\. ]/, `/com/host`, 6))[0]; }

      || eval { $host = ""; };
  }

  # remove garbage
  $host =~ s/[\0\r\n]+//go;
  $host =~ s/(\A\.+|\.+\Z)//go;
  $host =~ s/\.\.+/\./go;

  $host;
}


sub _hostdomain {

  # we already know it
  return $domain
    if (defined $domain);

  local $SIG{'__DIE__'};

  return $domain = $NetConfig{'inet_domain'}
    if defined $NetConfig{'inet_domain'};

  # try looking in /etc/resolv.conf
  # putting this here and assuming that it is correct, eliminates
  # calls to gethostbyname, and therefore DNS lookups. This helps
  # those on dialup systems.

  local *RES;
  local ($_);

  if (open(RES, "/etc/resolv.conf")) {
    while (<RES>) {
      $domain = $1
        if (/\A\s*(?:domain|search)\s+(\S+)/);
    }
    close(RES);

    return $domain
      if (defined $domain);
  }

  # just try hostname and system calls

  my $host = _hostname();
  my (@hosts);

  @hosts = ($host, "localhost");

  unless (defined($host) && $host =~ /\./) {
    my $dom = undef;
    eval {
      my $tmp = "\0" x 256;    ## preload scalar
      eval {
        package main;
        require "syscall.ph";
        }
        || eval {
        package main;
        require "sys/syscall.ph";
        }
        and $dom =
        (syscall(&main::SYS_getdomainname, $tmp, 256) == 0)
        ? $tmp
        : undef;
    };

    if ($^O eq 'VMS') {
      $dom ||= $ENV{'TCPIP$INET_DOMAIN'}
        || $ENV{'UCX$INET_DOMAIN'};
    }

    chop($dom = `domainname 2>/dev/null`)
      unless (defined $dom || $^O =~ /^(?:cygwin|MSWin32)/);

    if (defined $dom) {
      my @h = ();
      $dom =~ s/^\.+//;
      while (length($dom)) {
        push(@h, "$host.$dom");
        $dom =~ s/^[^.]+.+// or last;
      }
      unshift(@hosts, @h);
    }
  }

  # Attempt to locate FQDN

  foreach (grep { defined $_ } @hosts) {
    my @info = gethostbyname($_);

    next unless @info;

    # look at real name & aliases
    my $site;
    foreach $site ($info[0], split(/ /, $info[1])) {
      if (rindex($site, ".") > 0) {

        # Extract domain from FQDN

        ($domain = $site) =~ s/\A[^\.]+\.//;
        return $domain;
      }
    }
  }

  # Look for environment variable

  $domain ||= $ENV{LOCALDOMAIN} || $ENV{DOMAIN};

  if (defined $domain) {
    $domain =~ s/[\r\n\0]+//g;
    $domain =~ s/(\A\.+|\.+\Z)//g;
    $domain =~ s/\.\.+/\./g;
  }

  $domain;
}


sub domainname {

  return $fqdn
    if (defined $fqdn);

  _hostname();
  _hostdomain();

  # Assumption: If the host name does not contain a period
  # and the domain name does, then assume that they are correct
  # this helps to eliminate calls to gethostbyname, and therefore
  # eleminate DNS lookups

  return $fqdn = $host . "." . $domain
    if (defined $host
    and defined $domain
    and $host !~ /\./
    and $domain =~ /\./);

  # For hosts that have no name, just an IP address
  return $fqdn = $host if defined $host and $host =~ /^\d+(\.\d+){3}$/;

  my @host   = defined $host   ? split(/\./, $host)   : ('localhost');
  my @domain = defined $domain ? split(/\./, $domain) : ();
  my @fqdn   = ();

  # Determine from @host & @domain the FQDN

  my @d = @domain;

LOOP:
  while (1) {
    my @h = @host;
    while (@h) {
      my $tmp = join(".", @h, @d);
      if ((gethostbyname($tmp))[0]) {
        @fqdn = (@h, @d);
        $fqdn = $tmp;
        last LOOP;
      }
      pop @h;
    }
    last unless shift @d;
  }

  if (@fqdn) {
    $host = shift @fqdn;
    until ((gethostbyname($host))[0]) {
      $host .= "." . shift @fqdn;
    }
    $domain = join(".", @fqdn);
  }
  else {
    undef $host;
    undef $domain;
    undef $fqdn;
  }

  $fqdn;
}


sub hostfqdn { domainname() }


sub hostname {
  domainname()
    unless (defined $host);
  return $host;
}


sub hostdomain {
  domainname()
    unless (defined $domain);
  return $domain;
}

1;    # Keep require happy

__END__

=head1 NAME

Net::Domain - Attempt to evaluate the current host's internet name and domain

=head1 SYNOPSIS

    use Net::Domain qw(hostname hostfqdn hostdomain domainname);

=head1 DESCRIPTION

Using various methods B<attempt> to find the Fully Qualified Domain Name (FQDN)
of the current host. From this determine the host-name and the host-domain.

Each of the functions will return I<undef> if the FQDN cannot be determined.

=over 4

=item hostfqdn ()

Identify and return the FQDN of the current host.

=item domainname ()

An alias for hostfqdn ().

=item hostname ()

Returns the smallest part of the FQDN which can be used to identify the host.

=item hostdomain ()

Returns the remainder of the FQDN after the I<hostname> has been removed.

=back

=head1 AUTHOR

Graham Barr <gbarr@pobox.com>.
Adapted from Sys::Hostname by David Sundstrom <sunds@asictest.sc.ti.com>

=head1 COPYRIGHT

Copyright (c) 1995-1998 Graham Barr. All rights reserved.
This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
