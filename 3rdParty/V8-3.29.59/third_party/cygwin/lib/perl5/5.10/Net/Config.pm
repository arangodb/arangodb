# Net::Config.pm
#
# Copyright (c) 2000 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package Net::Config;

require Exporter;
use vars qw(@ISA @EXPORT %NetConfig $VERSION $CONFIGURE $LIBNET_CFG);
use Socket qw(inet_aton inet_ntoa);
use strict;

@EXPORT  = qw(%NetConfig);
@ISA     = qw(Net::LocalCfg Exporter);
$VERSION = "1.11";

eval { local $SIG{__DIE__}; require Net::LocalCfg };

%NetConfig = (
  nntp_hosts      => [],
  snpp_hosts      => [],
  pop3_hosts      => [],
  smtp_hosts      => [],
  ph_hosts        => [],
  daytime_hosts   => [],
  time_hosts      => [],
  inet_domain     => undef,
  ftp_firewall    => undef,
  ftp_ext_passive => 1,
  ftp_int_passive => 1,
  test_hosts      => 1,
  test_exist      => 1,
);

#
# Try to get as much configuration info as possible from InternetConfig
#
$^O eq 'MacOS' and eval <<TRY_INTERNET_CONFIG;
use Mac::InternetConfig;

{
my %nc = (
    nntp_hosts      => [ \$InternetConfig{ kICNNTPHost() } ],
    pop3_hosts      => [ \$InternetConfig{ kICMailAccount() } =~ /\@(.*)/ ],
    smtp_hosts      => [ \$InternetConfig{ kICSMTPHost() } ],
    ftp_testhost    => \$InternetConfig{ kICFTPHost() } ? \$InternetConfig{ kICFTPHost()} : undef,
    ph_hosts        => [ \$InternetConfig{ kICPhHost() }   ],
    ftp_ext_passive => \$InternetConfig{"646F676F\xA5UsePassiveMode"} || 0,
    ftp_int_passive => \$InternetConfig{"646F676F\xA5UsePassiveMode"} || 0,
    socks_hosts     => 
    	\$InternetConfig{ kICUseSocks() }    ? [ \$InternetConfig{ kICSocksHost() }    ] : [],
    ftp_firewall    => 
    	\$InternetConfig{ kICUseFTPProxy() } ? [ \$InternetConfig{ kICFTPProxyHost() } ] : [],
);
\@NetConfig{keys %nc} = values %nc;
}
TRY_INTERNET_CONFIG

my $file = __FILE__;
my $ref;
$file =~ s/Config.pm/libnet.cfg/;
if (-f $file) {
  $ref = eval { local $SIG{__DIE__}; do $file };
  if (ref($ref) eq 'HASH') {
    %NetConfig = (%NetConfig, %{$ref});
    $LIBNET_CFG = $file;
  }
}
if ($< == $> and !$CONFIGURE) {
  my $home = eval { local $SIG{__DIE__}; (getpwuid($>))[7] } || $ENV{HOME};
  $home ||= $ENV{HOMEDRIVE} . ($ENV{HOMEPATH} || '') if defined $ENV{HOMEDRIVE};
  if (defined $home) {
    $file      = $home . "/.libnetrc";
    $ref       = eval { local $SIG{__DIE__}; do $file } if -f $file;
    %NetConfig = (%NetConfig, %{$ref})
      if ref($ref) eq 'HASH';
  }
}
my ($k, $v);
while (($k, $v) = each %NetConfig) {
  $NetConfig{$k} = [$v]
    if ($k =~ /_hosts$/ and $k ne "test_hosts" and defined($v) and !ref($v));
}

# Take a hostname and determine if it is inside the firewall


sub requires_firewall {
  shift;    # ignore package
  my $host = shift;

  return 0 unless defined $NetConfig{'ftp_firewall'};

  $host = inet_aton($host) or return -1;
  $host = inet_ntoa($host);

  if (exists $NetConfig{'local_netmask'}) {
    my $quad = unpack("N", pack("C*", split(/\./, $host)));
    my $list = $NetConfig{'local_netmask'};
    $list = [$list] unless ref($list);
    foreach (@$list) {
      my ($net, $bits) = (m#^(\d+\.\d+\.\d+\.\d+)/(\d+)$#) or next;
      my $mask = ~0 << (32 - $bits);
      my $addr = unpack("N", pack("C*", split(/\./, $net)));

      return 0 if (($addr & $mask) == ($quad & $mask));
    }
    return 1;
  }

  return 0;
}

use vars qw(*is_external);
*is_external = \&requires_firewall;

1;

__END__

=head1 NAME

Net::Config - Local configuration data for libnet

=head1 SYNOPSYS

    use Net::Config qw(%NetConfig);

=head1 DESCRIPTION

C<Net::Config> holds configuration data for the modules in the libnet
distribution. During installation you will be asked for these values.

The configuration data is held globally in a file in the perl installation
tree, but a user may override any of these values by providing their own. This
can be done by having a C<.libnetrc> file in their home directory. This file
should return a reference to a HASH containing the keys described below.
For example

    # .libnetrc
    {
        nntp_hosts => [ "my_preferred_host" ],
	ph_hosts   => [ "my_ph_server" ],
    }
    __END__

=head1 METHODS

C<Net::Config> defines the following methods. They are methods as they are
invoked as class methods. This is because C<Net::Config> inherits from
C<Net::LocalCfg> so you can override these methods if you want.

=over 4

=item requires_firewall HOST

Attempts to determine if a given host is outside your firewall. Possible
return values are.

  -1  Cannot lookup hostname
   0  Host is inside firewall (or there is no ftp_firewall entry)
   1  Host is outside the firewall

This is done by using hostname lookup and the C<local_netmask> entry in
the configuration data.

=back

=head1 NetConfig VALUES

=over 4

=item nntp_hosts

=item snpp_hosts

=item pop3_hosts

=item smtp_hosts

=item ph_hosts

=item daytime_hosts

=item time_hosts

Each is a reference to an array of hostnames (in order of preference),
which should be used for the given protocol

=item inet_domain

Your internet domain name

=item ftp_firewall

If you have an FTP proxy firewall (B<NOT> an HTTP or SOCKS firewall)
then this value should be set to the firewall hostname. If your firewall
does not listen to port 21, then this value should be set to
C<"hostname:port"> (eg C<"hostname:99">)

=item ftp_firewall_type

There are many different ftp firewall products available. But unfortunately
there is no standard for how to traverse a firewall.  The list below shows the
sequence of commands that Net::FTP will use

  user        Username for remote host
  pass        Password for remote host
  fwuser      Username for firewall
  fwpass      Password for firewall
  remote.host The hostname of the remote ftp server

=over 4

=item 0

There is no firewall

=item 1

     USER user@remote.host
     PASS pass

=item 2

     USER fwuser
     PASS fwpass
     USER user@remote.host
     PASS pass

=item 3

     USER fwuser
     PASS fwpass
     SITE remote.site
     USER user
     PASS pass

=item 4

     USER fwuser
     PASS fwpass
     OPEN remote.site
     USER user
     PASS pass

=item 5

     USER user@fwuser@remote.site
     PASS pass@fwpass

=item 6

     USER fwuser@remote.site
     PASS fwpass
     USER user
     PASS pass

=item 7

     USER user@remote.host
     PASS pass
     AUTH fwuser
     RESP fwpass

=back

=item ftp_ext_passive

=item ftp_int_passive

FTP servers can work in passive or active mode. Active mode is when
you want to transfer data you have to tell the server the address and
port to connect to.  Passive mode is when the server provide the
address and port and you establish the connection.
 
With some firewalls active mode does not work as the server cannot
connect to your machine (because you are behind a firewall) and the firewall
does not re-write the command. In this case you should set C<ftp_ext_passive>
to a I<true> value.

Some servers are configured to only work in passive mode. If you have
one of these you can force C<Net::FTP> to always transfer in passive
mode; when not going via a firewall, by setting C<ftp_int_passive> to
a I<true> value.

=item local_netmask

A reference to a list of netmask strings in the form C<"134.99.4.0/24">.
These are used by the C<requires_firewall> function to determine if a given
host is inside or outside your firewall.

=back

The following entries are used during installation & testing on the
libnet package

=over 4

=item test_hosts

If true then C<make test> may attempt to connect to hosts given in the
configuration.

=item test_exists

If true then C<Configure> will check each hostname given that it exists

=back

=cut
