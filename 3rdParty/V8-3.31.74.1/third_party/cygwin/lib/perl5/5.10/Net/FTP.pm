# Net::FTP.pm
#
# Copyright (c) 1995-2004 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.
#
# Documentation (at end) improved 1996 by Nathan Torkington <gnat@frii.com>.

package Net::FTP;

require 5.001;

use strict;
use vars qw(@ISA $VERSION);
use Carp;

use Socket 1.3;
use IO::Socket;
use Time::Local;
use Net::Cmd;
use Net::Config;
use Fcntl qw(O_WRONLY O_RDONLY O_APPEND O_CREAT O_TRUNC);

$VERSION = '2.77';
@ISA     = qw(Exporter Net::Cmd IO::Socket::INET);

# Someday I will "use constant", when I am not bothered to much about
# compatability with older releases of perl

use vars qw($TELNET_IAC $TELNET_IP $TELNET_DM);
($TELNET_IAC, $TELNET_IP, $TELNET_DM) = (255, 244, 242);


BEGIN {

  # make a constant so code is fast'ish
  my $is_os390 = $^O eq 'os390';
  *trEBCDIC = sub () {$is_os390}
}


sub new {
  my $pkg = shift;
  my ($peer, %arg);
  if (@_ % 2) {
    $peer = shift;
    %arg  = @_;
  }
  else {
    %arg  = @_;
    $peer = delete $arg{Host};
  }

  my $host      = $peer;
  my $fire      = undef;
  my $fire_type = undef;

  if (exists($arg{Firewall}) || Net::Config->requires_firewall($peer)) {
         $fire = $arg{Firewall}
      || $ENV{FTP_FIREWALL}
      || $NetConfig{ftp_firewall}
      || undef;

    if (defined $fire) {
      $peer = $fire;
      delete $arg{Port};
           $fire_type = $arg{FirewallType}
        || $ENV{FTP_FIREWALL_TYPE}
        || $NetConfig{firewall_type}
        || undef;
    }
  }

  my $ftp = $pkg->SUPER::new(
    PeerAddr  => $peer,
    PeerPort  => $arg{Port} || 'ftp(21)',
    LocalAddr => $arg{'LocalAddr'},
    Proto     => 'tcp',
    Timeout   => defined $arg{Timeout}
    ? $arg{Timeout}
    : 120
    )
    or return undef;

  ${*$ftp}{'net_ftp_host'}    = $host;                             # Remote hostname
  ${*$ftp}{'net_ftp_type'}    = 'A';                               # ASCII/binary/etc mode
  ${*$ftp}{'net_ftp_blksize'} = abs($arg{'BlockSize'} || 10240);

  ${*$ftp}{'net_ftp_localaddr'} = $arg{'LocalAddr'};

  ${*$ftp}{'net_ftp_firewall'} = $fire
    if (defined $fire);
  ${*$ftp}{'net_ftp_firewall_type'} = $fire_type
    if (defined $fire_type);

  ${*$ftp}{'net_ftp_passive'} =
      int exists $arg{Passive} ? $arg{Passive}
    : exists $ENV{FTP_PASSIVE} ? $ENV{FTP_PASSIVE}
    : defined $fire            ? $NetConfig{ftp_ext_passive}
    : $NetConfig{ftp_int_passive};    # Whew! :-)

  $ftp->hash(exists $arg{Hash} ? $arg{Hash} : 0, 1024);

  $ftp->autoflush(1);

  $ftp->debug(exists $arg{Debug} ? $arg{Debug} : undef);

  unless ($ftp->response() == CMD_OK) {
    $ftp->close();
    $@ = $ftp->message;
    undef $ftp;
  }

  $ftp;
}

##
## User interface methods
##


sub host {
  my $me = shift;
  ${*$me}{'net_ftp_host'};
}


sub hash {
  my $ftp = shift;    # self

  my ($h, $b) = @_;
  unless ($h) {
    delete ${*$ftp}{'net_ftp_hash'};
    return [\*STDERR, 0];
  }
  ($h, $b) = (ref($h) ? $h : \*STDERR, $b || 1024);
  select((select($h), $| = 1)[0]);
  $b = 512 if $b < 512;
  ${*$ftp}{'net_ftp_hash'} = [$h, $b];
}


sub quit {
  my $ftp = shift;

  $ftp->_QUIT;
  $ftp->close;
}


sub DESTROY { }


sub ascii  { shift->type('A', @_); }
sub binary { shift->type('I', @_); }


sub ebcdic {
  carp "TYPE E is unsupported, shall default to I";
  shift->type('E', @_);
}


sub byte {
  carp "TYPE L is unsupported, shall default to I";
  shift->type('L', @_);
}

# Allow the user to send a command directly, BE CAREFUL !!


sub quot {
  my $ftp = shift;
  my $cmd = shift;

  $ftp->command(uc $cmd, @_);
  $ftp->response();
}


sub site {
  my $ftp = shift;

  $ftp->command("SITE", @_);
  $ftp->response();
}


sub mdtm {
  my $ftp  = shift;
  my $file = shift;

  # Server Y2K bug workaround
  #
  # sigh; some idiotic FTP servers use ("19%d",tm.tm_year) instead of
  # ("%d",tm.tm_year+1900).  This results in an extra digit in the
  # string returned. To account for this we allow an optional extra
  # digit in the year. Then if the first two digits are 19 we use the
  # remainder, otherwise we subtract 1900 from the whole year.

  $ftp->_MDTM($file)
    && $ftp->message =~ /((\d\d)(\d\d\d?))(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)/
    ? timegm($8, $7, $6, $5, $4 - 1, $2 eq '19' ? $3 : ($1 - 1900))
    : undef;
}


sub size {
  my $ftp  = shift;
  my $file = shift;
  my $io;
  if ($ftp->supported("SIZE")) {
    return $ftp->_SIZE($file)
      ? ($ftp->message =~ /(\d+)\s*(bytes?\s*)?$/)[0]
      : undef;
  }
  elsif ($ftp->supported("STAT")) {
    my @msg;
    return undef
      unless $ftp->_STAT($file) && (@msg = $ftp->message) == 3;
    my $line;
    foreach $line (@msg) {
      return (split(/\s+/, $line))[4]
        if $line =~ /^[-rwxSsTt]{10}/;
    }
  }
  else {
    my @files = $ftp->dir($file);
    if (@files) {
      return (split(/\s+/, $1))[4]
        if $files[0] =~ /^([-rwxSsTt]{10}.*)$/;
    }
  }
  undef;
}


sub login {
  my ($ftp, $user, $pass, $acct) = @_;
  my ($ok, $ruser, $fwtype);

  unless (defined $user) {
    require Net::Netrc;

    my $rc = Net::Netrc->lookup(${*$ftp}{'net_ftp_host'});

    ($user, $pass, $acct) = $rc->lpa()
      if ($rc);
  }

  $user ||= "anonymous";
  $ruser = $user;

  $fwtype = ${*$ftp}{'net_ftp_firewall_type'}
    || $NetConfig{'ftp_firewall_type'}
    || 0;

  if ($fwtype && defined ${*$ftp}{'net_ftp_firewall'}) {
    if ($fwtype == 1 || $fwtype == 7) {
      $user .= '@' . ${*$ftp}{'net_ftp_host'};
    }
    else {
      require Net::Netrc;

      my $rc = Net::Netrc->lookup(${*$ftp}{'net_ftp_firewall'});

      my ($fwuser, $fwpass, $fwacct) = $rc ? $rc->lpa() : ();

      if ($fwtype == 5) {
        $user = join('@', $user, $fwuser, ${*$ftp}{'net_ftp_host'});
        $pass = $pass . '@' . $fwpass;
      }
      else {
        if ($fwtype == 2) {
          $user .= '@' . ${*$ftp}{'net_ftp_host'};
        }
        elsif ($fwtype == 6) {
          $fwuser .= '@' . ${*$ftp}{'net_ftp_host'};
        }

        $ok = $ftp->_USER($fwuser);

        return 0 unless $ok == CMD_OK || $ok == CMD_MORE;

        $ok = $ftp->_PASS($fwpass || "");

        return 0 unless $ok == CMD_OK || $ok == CMD_MORE;

        $ok = $ftp->_ACCT($fwacct)
          if defined($fwacct);

        if ($fwtype == 3) {
          $ok = $ftp->command("SITE", ${*$ftp}{'net_ftp_host'})->response;
        }
        elsif ($fwtype == 4) {
          $ok = $ftp->command("OPEN", ${*$ftp}{'net_ftp_host'})->response;
        }

        return 0 unless $ok == CMD_OK || $ok == CMD_MORE;
      }
    }
  }

  $ok = $ftp->_USER($user);

  # Some dumb firewalls don't prefix the connection messages
  $ok = $ftp->response()
    if ($ok == CMD_OK && $ftp->code == 220 && $user =~ /\@/);

  if ($ok == CMD_MORE) {
    unless (defined $pass) {
      require Net::Netrc;

      my $rc = Net::Netrc->lookup(${*$ftp}{'net_ftp_host'}, $ruser);

      ($ruser, $pass, $acct) = $rc->lpa()
        if ($rc);

      $pass = '-anonymous@'
        if (!defined $pass && (!defined($ruser) || $ruser =~ /^anonymous/o));
    }

    $ok = $ftp->_PASS($pass || "");
  }

  $ok = $ftp->_ACCT($acct)
    if (defined($acct) && ($ok == CMD_MORE || $ok == CMD_OK));

  if ($fwtype == 7 && $ok == CMD_OK && defined ${*$ftp}{'net_ftp_firewall'}) {
    my ($f, $auth, $resp) = _auth_id($ftp);
    $ftp->authorize($auth, $resp) if defined($resp);
  }

  $ok == CMD_OK;
}


sub account {
  @_ == 2 or croak 'usage: $ftp->account( ACCT )';
  my $ftp  = shift;
  my $acct = shift;
  $ftp->_ACCT($acct) == CMD_OK;
}


sub _auth_id {
  my ($ftp, $auth, $resp) = @_;

  unless (defined $resp) {
    require Net::Netrc;

    $auth ||= eval { (getpwuid($>))[0] } || $ENV{NAME};

    my $rc = Net::Netrc->lookup(${*$ftp}{'net_ftp_firewall'}, $auth)
      || Net::Netrc->lookup(${*$ftp}{'net_ftp_firewall'});

    ($auth, $resp) = $rc->lpa()
      if ($rc);
  }
  ($ftp, $auth, $resp);
}


sub authorize {
  @_ >= 1 || @_ <= 3 or croak 'usage: $ftp->authorize( [AUTH [, RESP]])';

  my ($ftp, $auth, $resp) = &_auth_id;

  my $ok = $ftp->_AUTH($auth || "");

  $ok = $ftp->_RESP($resp || "")
    if ($ok == CMD_MORE);

  $ok == CMD_OK;
}


sub rename {
  @_ == 3 or croak 'usage: $ftp->rename(FROM, TO)';

  my ($ftp, $from, $to) = @_;

  $ftp->_RNFR($from)
    && $ftp->_RNTO($to);
}


sub type {
  my $ftp    = shift;
  my $type   = shift;
  my $oldval = ${*$ftp}{'net_ftp_type'};

  return $oldval
    unless (defined $type);

  return undef
    unless ($ftp->_TYPE($type, @_));

  ${*$ftp}{'net_ftp_type'} = join(" ", $type, @_);

  $oldval;
}


sub alloc {
  my $ftp    = shift;
  my $size   = shift;
  my $oldval = ${*$ftp}{'net_ftp_allo'};

  return $oldval
    unless (defined $size);

  return undef
    unless ($ftp->_ALLO($size, @_));

  ${*$ftp}{'net_ftp_allo'} = join(" ", $size, @_);

  $oldval;
}


sub abort {
  my $ftp = shift;

  send($ftp, pack("CCC", $TELNET_IAC, $TELNET_IP, $TELNET_IAC), MSG_OOB);

  $ftp->command(pack("C", $TELNET_DM) . "ABOR");

  ${*$ftp}{'net_ftp_dataconn'}->close()
    if defined ${*$ftp}{'net_ftp_dataconn'};

  $ftp->response();

  $ftp->status == CMD_OK;
}


sub get {
  my ($ftp, $remote, $local, $where) = @_;

  my ($loc, $len, $buf, $resp, $data);
  local *FD;

  my $localfd = ref($local) || ref(\$local) eq "GLOB";

  ($local = $remote) =~ s#^.*/##
    unless (defined $local);

  croak("Bad remote filename '$remote'\n")
    if $remote =~ /[\r\n]/s;

  ${*$ftp}{'net_ftp_rest'} = $where if defined $where;
  my $rest = ${*$ftp}{'net_ftp_rest'};

  delete ${*$ftp}{'net_ftp_port'};
  delete ${*$ftp}{'net_ftp_pasv'};

  $data = $ftp->retr($remote)
    or return undef;

  if ($localfd) {
    $loc = $local;
  }
  else {
    $loc = \*FD;

    unless (sysopen($loc, $local, O_CREAT | O_WRONLY | ($rest ? O_APPEND: O_TRUNC))) {
      carp "Cannot open Local file $local: $!\n";
      $data->abort;
      return undef;
    }
  }

  if ($ftp->type eq 'I' && !binmode($loc)) {
    carp "Cannot binmode Local file $local: $!\n";
    $data->abort;
    close($loc) unless $localfd;
    return undef;
  }

  $buf = '';
  my ($count, $hashh, $hashb, $ref) = (0);

  ($hashh, $hashb) = @$ref
    if ($ref = ${*$ftp}{'net_ftp_hash'});

  my $blksize = ${*$ftp}{'net_ftp_blksize'};
  local $\;    # Just in case

  while (1) {
    last unless $len = $data->read($buf, $blksize);

    if (trEBCDIC && $ftp->type ne 'I') {
      $buf = $ftp->toebcdic($buf);
      $len = length($buf);
    }

    if ($hashh) {
      $count += $len;
      print $hashh "#" x (int($count / $hashb));
      $count %= $hashb;
    }
    unless (print $loc $buf) {
      carp "Cannot write to Local file $local: $!\n";
      $data->abort;
      close($loc)
        unless $localfd;
      return undef;
    }
  }

  print $hashh "\n" if $hashh;

  unless ($localfd) {
    unless (close($loc)) {
      carp "Cannot close file $local (perhaps disk space) $!\n";
      return undef;
    }
  }

  unless ($data->close())    # implied $ftp->response
  {
    carp "Unable to close datastream";
    return undef;
  }

  return $local;
}


sub cwd {
  @_ == 1 || @_ == 2 or croak 'usage: $ftp->cwd( [ DIR ] )';

  my ($ftp, $dir) = @_;

  $dir = "/" unless defined($dir) && $dir =~ /\S/;

  $dir eq ".."
    ? $ftp->_CDUP()
    : $ftp->_CWD($dir);
}


sub cdup {
  @_ == 1 or croak 'usage: $ftp->cdup()';
  $_[0]->_CDUP;
}


sub pwd {
  @_ == 1 || croak 'usage: $ftp->pwd()';
  my $ftp = shift;

  $ftp->_PWD();
  $ftp->_extract_path;
}

# rmdir( $ftp, $dir, [ $recurse ] )
#
# Removes $dir on remote host via FTP.
# $ftp is handle for remote host
#
# If $recurse is TRUE, the directory and deleted recursively.
# This means all of its contents and subdirectories.
#
# Initial version contributed by Dinkum Software
#
sub rmdir {
  @_ == 2 || @_ == 3 or croak('usage: $ftp->rmdir( DIR [, RECURSE ] )');

  # Pick off the args
  my ($ftp, $dir, $recurse) = @_;
  my $ok;

  return $ok
    if $ok = $ftp->_RMD($dir)
    or !$recurse;

  # Try to delete the contents
  # Get a list of all the files in the directory
  my @filelist = grep { !/^\.{1,2}$/ } $ftp->ls($dir);

  return undef
    unless @filelist;    # failed, it is probably not a directory

  # Go thru and delete each file or the directory
  my $file;
  foreach $file (map { m,/, ? $_ : "$dir/$_" } @filelist) {
    next                 # successfully deleted the file
      if $ftp->delete($file);

    # Failed to delete it, assume its a directory
    # Recurse and ignore errors, the final rmdir() will
    # fail on any errors here
    return $ok
      unless $ok = $ftp->rmdir($file, 1);
  }

  # Directory should be empty
  # Try to remove the directory again
  # Pass results directly to caller
  # If any of the prior deletes failed, this
  # rmdir() will fail because directory is not empty
  return $ftp->_RMD($dir);
}


sub restart {
  @_ == 2 || croak 'usage: $ftp->restart( BYTE_OFFSET )';

  my ($ftp, $where) = @_;

  ${*$ftp}{'net_ftp_rest'} = $where;

  return undef;
}


sub mkdir {
  @_ == 2 || @_ == 3 or croak 'usage: $ftp->mkdir( DIR [, RECURSE ] )';

  my ($ftp, $dir, $recurse) = @_;

  $ftp->_MKD($dir) || $recurse
    or return undef;

  my $path = $dir;

  unless ($ftp->ok) {
    my @path = split(m#(?=/+)#, $dir);

    $path = "";

    while (@path) {
      $path .= shift @path;

      $ftp->_MKD($path);

      $path = $ftp->_extract_path($path);
    }

    # If the creation of the last element was not successful, see if we
    # can cd to it, if so then return path

    unless ($ftp->ok) {
      my ($status, $message) = ($ftp->status, $ftp->message);
      my $pwd = $ftp->pwd;

      if ($pwd && $ftp->cwd($dir)) {
        $path = $dir;
        $ftp->cwd($pwd);
      }
      else {
        undef $path;
      }
      $ftp->set_status($status, $message);
    }
  }

  $path;
}


sub delete {
  @_ == 2 || croak 'usage: $ftp->delete( FILENAME )';

  $_[0]->_DELE($_[1]);
}


sub put        { shift->_store_cmd("stor", @_) }
sub put_unique { shift->_store_cmd("stou", @_) }
sub append     { shift->_store_cmd("appe", @_) }


sub nlst { shift->_data_cmd("NLST", @_) }
sub list { shift->_data_cmd("LIST", @_) }
sub retr { shift->_data_cmd("RETR", @_) }
sub stor { shift->_data_cmd("STOR", @_) }
sub stou { shift->_data_cmd("STOU", @_) }
sub appe { shift->_data_cmd("APPE", @_) }


sub _store_cmd {
  my ($ftp, $cmd, $local, $remote) = @_;
  my ($loc, $sock, $len, $buf);
  local *FD;

  my $localfd = ref($local) || ref(\$local) eq "GLOB";

  unless (defined $remote) {
    croak 'Must specify remote filename with stream input'
      if $localfd;

    require File::Basename;
    $remote = File::Basename::basename($local);
  }
  if (defined ${*$ftp}{'net_ftp_allo'}) {
    delete ${*$ftp}{'net_ftp_allo'};
  }
  else {

    # if the user hasn't already invoked the alloc method since the last
    # _store_cmd call, figure out if the local file is a regular file(not
    # a pipe, or device) and if so get the file size from stat, and send
    # an ALLO command before sending the STOR, STOU, or APPE command.
    my $size = do { local $^W; -f $local && -s _ };    # no ALLO if sending data from a pipe
    $ftp->_ALLO($size) if $size;
  }
  croak("Bad remote filename '$remote'\n")
    if $remote =~ /[\r\n]/s;

  if ($localfd) {
    $loc = $local;
  }
  else {
    $loc = \*FD;

    unless (sysopen($loc, $local, O_RDONLY)) {
      carp "Cannot open Local file $local: $!\n";
      return undef;
    }
  }

  if ($ftp->type eq 'I' && !binmode($loc)) {
    carp "Cannot binmode Local file $local: $!\n";
    return undef;
  }

  delete ${*$ftp}{'net_ftp_port'};
  delete ${*$ftp}{'net_ftp_pasv'};

  $sock = $ftp->_data_cmd($cmd, $remote)
    or return undef;

  $remote = ($ftp->message =~ /FILE:\s*(.*)/)[0]
    if 'STOU' eq uc $cmd;

  my $blksize = ${*$ftp}{'net_ftp_blksize'};

  my ($count, $hashh, $hashb, $ref) = (0);

  ($hashh, $hashb) = @$ref
    if ($ref = ${*$ftp}{'net_ftp_hash'});

  while (1) {
    last unless $len = read($loc, $buf = "", $blksize);

    if (trEBCDIC && $ftp->type ne 'I') {
      $buf = $ftp->toascii($buf);
      $len = length($buf);
    }

    if ($hashh) {
      $count += $len;
      print $hashh "#" x (int($count / $hashb));
      $count %= $hashb;
    }

    my $wlen;
    unless (defined($wlen = $sock->write($buf, $len)) && $wlen == $len) {
      $sock->abort;
      close($loc)
        unless $localfd;
      print $hashh "\n" if $hashh;
      return undef;
    }
  }

  print $hashh "\n" if $hashh;

  close($loc)
    unless $localfd;

  $sock->close()
    or return undef;

  if ('STOU' eq uc $cmd and $ftp->message =~ m/unique\s+file\s*name\s*:\s*(.*)\)|"(.*)"/) {
    require File::Basename;
    $remote = File::Basename::basename($+);
  }

  return $remote;
}


sub port {
  @_ == 1 || @_ == 2 or croak 'usage: $ftp->port([PORT])';

  my ($ftp, $port) = @_;
  my $ok;

  delete ${*$ftp}{'net_ftp_intern_port'};

  unless (defined $port) {

    # create a Listen socket at same address as the command socket

    ${*$ftp}{'net_ftp_listen'} ||= IO::Socket::INET->new(
      Listen    => 5,
      Proto     => 'tcp',
      Timeout   => $ftp->timeout,
      LocalAddr => $ftp->sockhost,
    );

    my $listen = ${*$ftp}{'net_ftp_listen'};

    my ($myport, @myaddr) = ($listen->sockport, split(/\./, $listen->sockhost));

    $port = join(',', @myaddr, $myport >> 8, $myport & 0xff);

    ${*$ftp}{'net_ftp_intern_port'} = 1;
  }

  $ok = $ftp->_PORT($port);

  ${*$ftp}{'net_ftp_port'} = $port;

  $ok;
}


sub ls  { shift->_list_cmd("NLST", @_); }
sub dir { shift->_list_cmd("LIST", @_); }


sub pasv {
  @_ == 1 or croak 'usage: $ftp->pasv()';

  my $ftp = shift;

  delete ${*$ftp}{'net_ftp_intern_port'};

  $ftp->_PASV && $ftp->message =~ /(\d+(,\d+)+)/
    ? ${*$ftp}{'net_ftp_pasv'} = $1
    : undef;
}


sub unique_name {
  my $ftp = shift;
  ${*$ftp}{'net_ftp_unique'} || undef;
}


sub supported {
  @_ == 2 or croak 'usage: $ftp->supported( CMD )';
  my $ftp  = shift;
  my $cmd  = uc shift;
  my $hash = ${*$ftp}{'net_ftp_supported'} ||= {};

  return $hash->{$cmd}
    if exists $hash->{$cmd};

  return $hash->{$cmd} = 0
    unless $ftp->_HELP($cmd);

  my $text = $ftp->message;
  if ($text =~ /following\s+commands/i) {
    $text =~ s/^.*\n//;
    while ($text =~ /(\*?)(\w+)(\*?)/sg) {
      $hash->{"\U$2"} = !length("$1$3");
    }
  }
  else {
    $hash->{$cmd} = $text !~ /unimplemented/i;
  }

  $hash->{$cmd} ||= 0;
}

##
## Deprecated methods
##


sub lsl {
  carp "Use of Net::FTP::lsl deprecated, use 'dir'"
    if $^W;
  goto &dir;
}


sub authorise {
  carp "Use of Net::FTP::authorise deprecated, use 'authorize'"
    if $^W;
  goto &authorize;
}


##
## Private methods
##


sub _extract_path {
  my ($ftp, $path) = @_;

  # This tries to work both with and without the quote doubling
  # convention (RFC 959 requires it, but the first 3 servers I checked
  # didn't implement it).  It will fail on a server which uses a quote in
  # the message which isn't a part of or surrounding the path.
  $ftp->ok
    && $ftp->message =~ /(?:^|\s)\"(.*)\"(?:$|\s)/
    && ($path = $1) =~ s/\"\"/\"/g;

  $path;
}

##
## Communication methods
##


sub _dataconn {
  my $ftp  = shift;
  my $data = undef;
  my $pkg  = "Net::FTP::" . $ftp->type;

  eval "require " . $pkg;

  $pkg =~ s/ /_/g;

  delete ${*$ftp}{'net_ftp_dataconn'};

  if (defined ${*$ftp}{'net_ftp_pasv'}) {
    my @port = map { 0 + $_ } split(/,/, ${*$ftp}{'net_ftp_pasv'});

    $data = $pkg->new(
      PeerAddr  => join(".", @port[0 .. 3]),
      PeerPort  => $port[4] * 256 + $port[5],
      LocalAddr => ${*$ftp}{'net_ftp_localaddr'},
      Proto     => 'tcp'
    );
  }
  elsif (defined ${*$ftp}{'net_ftp_listen'}) {
    $data = ${*$ftp}{'net_ftp_listen'}->accept($pkg);
    close(delete ${*$ftp}{'net_ftp_listen'});
  }

  if ($data) {
    ${*$data} = "";
    $data->timeout($ftp->timeout);
    ${*$ftp}{'net_ftp_dataconn'} = $data;
    ${*$data}{'net_ftp_cmd'}     = $ftp;
    ${*$data}{'net_ftp_blksize'} = ${*$ftp}{'net_ftp_blksize'};
  }

  $data;
}


sub _list_cmd {
  my $ftp = shift;
  my $cmd = uc shift;

  delete ${*$ftp}{'net_ftp_port'};
  delete ${*$ftp}{'net_ftp_pasv'};

  my $data = $ftp->_data_cmd($cmd, @_);

  return
    unless (defined $data);

  require Net::FTP::A;
  bless $data, "Net::FTP::A";    # Force ASCII mode

  my $databuf = '';
  my $buf     = '';
  my $blksize = ${*$ftp}{'net_ftp_blksize'};

  while ($data->read($databuf, $blksize)) {
    $buf .= $databuf;
  }

  my $list = [split(/\n/, $buf)];

  $data->close();

  if (trEBCDIC) {
    for (@$list) { $_ = $ftp->toebcdic($_) }
  }

  wantarray
    ? @{$list}
    : $list;
}


sub _data_cmd {
  my $ftp   = shift;
  my $cmd   = uc shift;
  my $ok    = 1;
  my $where = delete ${*$ftp}{'net_ftp_rest'} || 0;
  my $arg;

  for $arg (@_) {
    croak("Bad argument '$arg'\n")
      if $arg =~ /[\r\n]/s;
  }

  if ( ${*$ftp}{'net_ftp_passive'}
    && !defined ${*$ftp}{'net_ftp_pasv'}
    && !defined ${*$ftp}{'net_ftp_port'})
  {
    my $data = undef;

    $ok = defined $ftp->pasv;
    $ok = $ftp->_REST($where)
      if $ok && $where;

    if ($ok) {
      $ftp->command($cmd, @_);
      $data = $ftp->_dataconn();
      $ok   = CMD_INFO == $ftp->response();
      if ($ok) {
        $data->reading
          if $data && $cmd =~ /RETR|LIST|NLST/;
        return $data;
      }
      $data->_close
        if $data;
    }
    return undef;
  }

  $ok = $ftp->port
    unless (defined ${*$ftp}{'net_ftp_port'}
    || defined ${*$ftp}{'net_ftp_pasv'});

  $ok = $ftp->_REST($where)
    if $ok && $where;

  return undef
    unless $ok;

  $ftp->command($cmd, @_);

  return 1
    if (defined ${*$ftp}{'net_ftp_pasv'});

  $ok = CMD_INFO == $ftp->response();

  return $ok
    unless exists ${*$ftp}{'net_ftp_intern_port'};

  if ($ok) {
    my $data = $ftp->_dataconn();

    $data->reading
      if $data && $cmd =~ /RETR|LIST|NLST/;

    return $data;
  }


  close(delete ${*$ftp}{'net_ftp_listen'});

  return undef;
}

##
## Over-ride methods (Net::Cmd)
##


sub debug_text { $_[2] =~ /^(pass|resp|acct)/i ? "$1 ....\n" : $_[2]; }


sub command {
  my $ftp = shift;

  delete ${*$ftp}{'net_ftp_port'};
  $ftp->SUPER::command(@_);
}


sub response {
  my $ftp  = shift;
  my $code = $ftp->SUPER::response();

  delete ${*$ftp}{'net_ftp_pasv'}
    if ($code != CMD_MORE && $code != CMD_INFO);

  $code;
}


sub parse_response {
  return ($1, $2 eq "-")
    if $_[1] =~ s/^(\d\d\d)([- ]?)//o;

  my $ftp = shift;

  # Darn MS FTP server is a load of CRAP !!!!
  return ()
    unless ${*$ftp}{'net_cmd_code'} + 0;

  (${*$ftp}{'net_cmd_code'}, 1);
}

##
## Allow 2 servers to talk directly
##


sub pasv_xfer_unique {
  my ($sftp, $sfile, $dftp, $dfile) = @_;
  $sftp->pasv_xfer($sfile, $dftp, $dfile, 1);
}


sub pasv_xfer {
  my ($sftp, $sfile, $dftp, $dfile, $unique) = @_;

  ($dfile = $sfile) =~ s#.*/##
    unless (defined $dfile);

  my $port = $sftp->pasv
    or return undef;

  $dftp->port($port)
    or return undef;

  return undef
    unless ($unique ? $dftp->stou($dfile) : $dftp->stor($dfile));

  unless ($sftp->retr($sfile) && $sftp->response == CMD_INFO) {
    $sftp->retr($sfile);
    $dftp->abort;
    $dftp->response();
    return undef;
  }

  $dftp->pasv_wait($sftp);
}


sub pasv_wait {
  @_ == 2 or croak 'usage: $ftp->pasv_wait(NON_PASV_FTP)';

  my ($ftp, $non_pasv) = @_;
  my ($file, $rin, $rout);

  vec($rin = '', fileno($ftp), 1) = 1;
  select($rout = $rin, undef, undef, undef);

  $ftp->response();
  $non_pasv->response();

  return undef
    unless $ftp->ok() && $non_pasv->ok();

  return $1
    if $ftp->message =~ /unique file name:\s*(\S*)\s*\)/;

  return $1
    if $non_pasv->message =~ /unique file name:\s*(\S*)\s*\)/;

  return 1;
}


sub feature {
  @_ == 2 or croak 'usage: $ftp->feature( NAME )';
  my ($ftp, $feat) = @_;

  my $feature = ${*$ftp}{net_ftp_feature} ||= do {
    my @feat;

    # Example response
    # 211-Features:
    #  MDTM
    #  REST STREAM
    #  SIZE
    # 211 End

    @feat = map { /^\s+(.*\S)/ } $ftp->message
      if $ftp->_FEAT;

    \@feat;
  };

  return grep { /^\Q$feat\E\b/i } @$feature;
}


sub cmd { shift->command(@_)->response() }

########################################
#
# RFC959 commands
#


sub _ABOR { shift->command("ABOR")->response() == CMD_OK }
sub _ALLO { shift->command("ALLO", @_)->response() == CMD_OK }
sub _CDUP { shift->command("CDUP")->response() == CMD_OK }
sub _NOOP { shift->command("NOOP")->response() == CMD_OK }
sub _PASV { shift->command("PASV")->response() == CMD_OK }
sub _QUIT { shift->command("QUIT")->response() == CMD_OK }
sub _DELE { shift->command("DELE", @_)->response() == CMD_OK }
sub _CWD  { shift->command("CWD", @_)->response() == CMD_OK }
sub _PORT { shift->command("PORT", @_)->response() == CMD_OK }
sub _RMD  { shift->command("RMD", @_)->response() == CMD_OK }
sub _MKD  { shift->command("MKD", @_)->response() == CMD_OK }
sub _PWD  { shift->command("PWD", @_)->response() == CMD_OK }
sub _TYPE { shift->command("TYPE", @_)->response() == CMD_OK }
sub _RNTO { shift->command("RNTO", @_)->response() == CMD_OK }
sub _RESP { shift->command("RESP", @_)->response() == CMD_OK }
sub _MDTM { shift->command("MDTM", @_)->response() == CMD_OK }
sub _SIZE { shift->command("SIZE", @_)->response() == CMD_OK }
sub _HELP { shift->command("HELP", @_)->response() == CMD_OK }
sub _STAT { shift->command("STAT", @_)->response() == CMD_OK }
sub _FEAT { shift->command("FEAT", @_)->response() == CMD_OK }
sub _APPE { shift->command("APPE", @_)->response() == CMD_INFO }
sub _LIST { shift->command("LIST", @_)->response() == CMD_INFO }
sub _NLST { shift->command("NLST", @_)->response() == CMD_INFO }
sub _RETR { shift->command("RETR", @_)->response() == CMD_INFO }
sub _STOR { shift->command("STOR", @_)->response() == CMD_INFO }
sub _STOU { shift->command("STOU", @_)->response() == CMD_INFO }
sub _RNFR { shift->command("RNFR", @_)->response() == CMD_MORE }
sub _REST { shift->command("REST", @_)->response() == CMD_MORE }
sub _PASS { shift->command("PASS", @_)->response() }
sub _ACCT { shift->command("ACCT", @_)->response() }
sub _AUTH { shift->command("AUTH", @_)->response() }


sub _USER {
  my $ftp = shift;
  my $ok  = $ftp->command("USER", @_)->response();

  # A certain brain dead firewall :-)
  $ok = $ftp->command("user", @_)->response()
    unless $ok == CMD_MORE or $ok == CMD_OK;

  $ok;
}


sub _SMNT { shift->unsupported(@_) }
sub _MODE { shift->unsupported(@_) }
sub _SYST { shift->unsupported(@_) }
sub _STRU { shift->unsupported(@_) }
sub _REIN { shift->unsupported(@_) }

1;

__END__

=head1 NAME

Net::FTP - FTP Client class

=head1 SYNOPSIS

    use Net::FTP;

    $ftp = Net::FTP->new("some.host.name", Debug => 0)
      or die "Cannot connect to some.host.name: $@";

    $ftp->login("anonymous",'-anonymous@')
      or die "Cannot login ", $ftp->message;

    $ftp->cwd("/pub")
      or die "Cannot change working directory ", $ftp->message;

    $ftp->get("that.file")
      or die "get failed ", $ftp->message;

    $ftp->quit;

=head1 DESCRIPTION

C<Net::FTP> is a class implementing a simple FTP client in Perl as
described in RFC959.  It provides wrappers for a subset of the RFC959
commands.

=head1 OVERVIEW

FTP stands for File Transfer Protocol.  It is a way of transferring
files between networked machines.  The protocol defines a client
(whose commands are provided by this module) and a server (not
implemented in this module).  Communication is always initiated by the
client, and the server responds with a message and a status code (and
sometimes with data).

The FTP protocol allows files to be sent to or fetched from the
server.  Each transfer involves a B<local file> (on the client) and a
B<remote file> (on the server).  In this module, the same file name
will be used for both local and remote if only one is specified.  This
means that transferring remote file C</path/to/file> will try to put
that file in C</path/to/file> locally, unless you specify a local file
name.

The protocol also defines several standard B<translations> which the
file can undergo during transfer.  These are ASCII, EBCDIC, binary,
and byte.  ASCII is the default type, and indicates that the sender of
files will translate the ends of lines to a standard representation
which the receiver will then translate back into their local
representation.  EBCDIC indicates the file being transferred is in
EBCDIC format.  Binary (also known as image) format sends the data as
a contiguous bit stream.  Byte format transfers the data as bytes, the
values of which remain the same regardless of differences in byte size
between the two machines (in theory - in practice you should only use
this if you really know what you're doing).

=head1 CONSTRUCTOR

=over 4

=item new ([ HOST ] [, OPTIONS ])

This is the constructor for a new Net::FTP object. C<HOST> is the
name of the remote host to which an FTP connection is required.

C<HOST> is optional. If C<HOST> is not given then it may instead be
passed as the C<Host> option described below. 

C<OPTIONS> are passed in a hash like fashion, using key and value pairs.
Possible options are:

B<Host> - FTP host to connect to. It may be a single scalar, as defined for
the C<PeerAddr> option in L<IO::Socket::INET>, or a reference to
an array with hosts to try in turn. The L</host> method will return the value
which was used to connect to the host.


B<Firewall> - The name of a machine which acts as an FTP firewall. This can be
overridden by an environment variable C<FTP_FIREWALL>. If specified, and the
given host cannot be directly connected to, then the
connection is made to the firewall machine and the string C<@hostname> is
appended to the login identifier. This kind of setup is also referred to
as an ftp proxy.

B<FirewallType> - The type of firewall running on the machine indicated by
B<Firewall>. This can be overridden by an environment variable
C<FTP_FIREWALL_TYPE>. For a list of permissible types, see the description of
ftp_firewall_type in L<Net::Config>.

B<BlockSize> - This is the block size that Net::FTP will use when doing
transfers. (defaults to 10240)

B<Port> - The port number to connect to on the remote machine for the
FTP connection

B<Timeout> - Set a timeout value (defaults to 120)

B<Debug> - debug level (see the debug method in L<Net::Cmd>)

B<Passive> - If set to a non-zero value then all data transfers will
be done using passive mode. If set to zero then data transfers will be
done using active mode.  If the machine is connected to the Internet
directly, both passive and active mode should work equally well.
Behind most firewall and NAT configurations passive mode has a better
chance of working.  However, in some rare firewall configurations,
active mode actually works when passive mode doesn't.  Some really old
FTP servers might not implement passive transfers.  If not specified,
then the transfer mode is set by the environment variable
C<FTP_PASSIVE> or if that one is not set by the settings done by the
F<libnetcfg> utility.  If none of these apply then passive mode is
used.

B<Hash> - If given a reference to a file handle (e.g., C<\*STDERR>),
print hash marks (#) on that filehandle every 1024 bytes.  This
simply invokes the C<hash()> method for you, so that hash marks
are displayed for all transfers.  You can, of course, call C<hash()>
explicitly whenever you'd like.

B<LocalAddr> - Local address to use for all socket connections, this
argument will be passed to L<IO::Socket::INET>

If the constructor fails undef will be returned and an error message will
be in $@

=back

=head1 METHODS

Unless otherwise stated all methods return either a I<true> or I<false>
value, with I<true> meaning that the operation was a success. When a method
states that it returns a value, failure will be returned as I<undef> or an
empty list.

=over 4

=item login ([LOGIN [,PASSWORD [, ACCOUNT] ] ])

Log into the remote FTP server with the given login information. If
no arguments are given then the C<Net::FTP> uses the C<Net::Netrc>
package to lookup the login information for the connected host.
If no information is found then a login of I<anonymous> is used.
If no password is given and the login is I<anonymous> then I<anonymous@>
will be used for password.

If the connection is via a firewall then the C<authorize> method will
be called with no arguments.

=item authorize ( [AUTH [, RESP]])

This is a protocol used by some firewall ftp proxies. It is used
to authorise the user to send data out.  If both arguments are not specified
then C<authorize> uses C<Net::Netrc> to do a lookup.

=item site (ARGS)

Send a SITE command to the remote server and wait for a response.

Returns most significant digit of the response code.

=item ascii

Transfer file in ASCII. CRLF translation will be done if required

=item binary

Transfer file in binary mode. No transformation will be done.

B<Hint>: If both server and client machines use the same line ending for
text files, then it will be faster to transfer all files in binary mode.

=item rename ( OLDNAME, NEWNAME )

Rename a file on the remote FTP server from C<OLDNAME> to C<NEWNAME>. This
is done by sending the RNFR and RNTO commands.

=item delete ( FILENAME )

Send a request to the server to delete C<FILENAME>.

=item cwd ( [ DIR ] )

Attempt to change directory to the directory given in C<$dir>.  If
C<$dir> is C<"..">, the FTP C<CDUP> command is used to attempt to
move up one directory. If no directory is given then an attempt is made
to change the directory to the root directory.

=item cdup ()

Change directory to the parent of the current directory.

=item pwd ()

Returns the full pathname of the current directory.

=item restart ( WHERE )

Set the byte offset at which to begin the next data transfer. Net::FTP simply
records this value and uses it when during the next data transfer. For this
reason this method will not return an error, but setting it may cause
a subsequent data transfer to fail.

=item rmdir ( DIR [, RECURSE ])

Remove the directory with the name C<DIR>. If C<RECURSE> is I<true> then
C<rmdir> will attempt to delete everything inside the directory.

=item mkdir ( DIR [, RECURSE ])

Create a new directory with the name C<DIR>. If C<RECURSE> is I<true> then
C<mkdir> will attempt to create all the directories in the given path.

Returns the full pathname to the new directory.

=item alloc ( SIZE [, RECORD_SIZE] )

The alloc command allows you to give the ftp server a hint about the size
of the file about to be transferred using the ALLO ftp command. Some storage
systems use this to make intelligent decisions about how to store the file.
The C<SIZE> argument represents the size of the file in bytes. The
C<RECORD_SIZE> argument indicates a maximum record or page size for files
sent with a record or page structure.

The size of the file will be determined, and sent to the server
automatically for normal files so that this method need only be called if
you are transferring data from a socket, named pipe, or other stream not
associated with a normal file.

=item ls ( [ DIR ] )

Get a directory listing of C<DIR>, or the current directory.

In an array context, returns a list of lines returned from the server. In
a scalar context, returns a reference to a list.

=item dir ( [ DIR ] )

Get a directory listing of C<DIR>, or the current directory in long format.

In an array context, returns a list of lines returned from the server. In
a scalar context, returns a reference to a list.

=item get ( REMOTE_FILE [, LOCAL_FILE [, WHERE]] )

Get C<REMOTE_FILE> from the server and store locally. C<LOCAL_FILE> may be
a filename or a filehandle. If not specified, the file will be stored in
the current directory with the same leafname as the remote file.

If C<WHERE> is given then the first C<WHERE> bytes of the file will
not be transferred, and the remaining bytes will be appended to
the local file if it already exists.

Returns C<LOCAL_FILE>, or the generated local file name if C<LOCAL_FILE>
is not given. If an error was encountered undef is returned.

=item put ( LOCAL_FILE [, REMOTE_FILE ] )

Put a file on the remote server. C<LOCAL_FILE> may be a name or a filehandle.
If C<LOCAL_FILE> is a filehandle then C<REMOTE_FILE> must be specified. If
C<REMOTE_FILE> is not specified then the file will be stored in the current
directory with the same leafname as C<LOCAL_FILE>.

Returns C<REMOTE_FILE>, or the generated remote filename if C<REMOTE_FILE>
is not given.

B<NOTE>: If for some reason the transfer does not complete and an error is
returned then the contents that had been transferred will not be remove
automatically.

=item put_unique ( LOCAL_FILE [, REMOTE_FILE ] )

Same as put but uses the C<STOU> command.

Returns the name of the file on the server.

=item append ( LOCAL_FILE [, REMOTE_FILE ] )

Same as put but appends to the file on the remote server.

Returns C<REMOTE_FILE>, or the generated remote filename if C<REMOTE_FILE>
is not given.

=item unique_name ()

Returns the name of the last file stored on the server using the
C<STOU> command.

=item mdtm ( FILE )

Returns the I<modification time> of the given file

=item size ( FILE )

Returns the size in bytes for the given file as stored on the remote server.

B<NOTE>: The size reported is the size of the stored file on the remote server.
If the file is subsequently transferred from the server in ASCII mode
and the remote server and local machine have different ideas about
"End Of Line" then the size of file on the local machine after transfer
may be different.

=item supported ( CMD )

Returns TRUE if the remote server supports the given command.

=item hash ( [FILEHANDLE_GLOB_REF],[ BYTES_PER_HASH_MARK] )

Called without parameters, or with the first argument false, hash marks
are suppressed.  If the first argument is true but not a reference to a 
file handle glob, then \*STDERR is used.  The second argument is the number
of bytes per hash mark printed, and defaults to 1024.  In all cases the
return value is a reference to an array of two:  the filehandle glob reference
and the bytes per hash mark.

=item feature ( NAME )

Determine if the server supports the specified feature. The return
value is a list of lines the server responded with to describe the
options that it supports for the given feature. If the feature is
unsupported then the empty list is returned.

  if ($ftp->feature( 'MDTM' )) {
    # Do something
  }

  if (grep { /\bTLS\b/ } $ftp->feature('AUTH')) {
    # Server supports TLS
  }

=back

The following methods can return different results depending on
how they are called. If the user explicitly calls either
of the C<pasv> or C<port> methods then these methods will
return a I<true> or I<false> value. If the user does not
call either of these methods then the result will be a
reference to a C<Net::FTP::dataconn> based object.

=over 4

=item nlst ( [ DIR ] )

Send an C<NLST> command to the server, with an optional parameter.

=item list ( [ DIR ] )

Same as C<nlst> but using the C<LIST> command

=item retr ( FILE )

Begin the retrieval of a file called C<FILE> from the remote server.

=item stor ( FILE )

Tell the server that you wish to store a file. C<FILE> is the
name of the new file that should be created.

=item stou ( FILE )

Same as C<stor> but using the C<STOU> command. The name of the unique
file which was created on the server will be available via the C<unique_name>
method after the data connection has been closed.

=item appe ( FILE )

Tell the server that we want to append some data to the end of a file
called C<FILE>. If this file does not exist then create it.

=back

If for some reason you want to have complete control over the data connection,
this includes generating it and calling the C<response> method when required,
then the user can use these methods to do so.

However calling these methods only affects the use of the methods above that
can return a data connection. They have no effect on methods C<get>, C<put>,
C<put_unique> and those that do not require data connections.

=over 4

=item port ( [ PORT ] )

Send a C<PORT> command to the server. If C<PORT> is specified then it is sent
to the server. If not, then a listen socket is created and the correct information
sent to the server.

=item pasv ()

Tell the server to go into passive mode. Returns the text that represents the
port on which the server is listening, this text is in a suitable form to
sent to another ftp server using the C<port> method.

=back

The following methods can be used to transfer files between two remote
servers, providing that these two servers can connect directly to each other.

=over 4

=item pasv_xfer ( SRC_FILE, DEST_SERVER [, DEST_FILE ] )

This method will do a file transfer between two remote ftp servers. If
C<DEST_FILE> is omitted then the leaf name of C<SRC_FILE> will be used.

=item pasv_xfer_unique ( SRC_FILE, DEST_SERVER [, DEST_FILE ] )

Like C<pasv_xfer> but the file is stored on the remote server using
the STOU command.

=item pasv_wait ( NON_PASV_SERVER )

This method can be used to wait for a transfer to complete between a passive
server and a non-passive server. The method should be called on the passive
server with the C<Net::FTP> object for the non-passive server passed as an
argument.

=item abort ()

Abort the current data transfer.

=item quit ()

Send the QUIT command to the remote FTP server and close the socket connection.

=back

=head2 Methods for the adventurous

C<Net::FTP> inherits from C<Net::Cmd> so methods defined in C<Net::Cmd> may
be used to send commands to the remote FTP server.

=over 4

=item quot (CMD [,ARGS])

Send a command, that Net::FTP does not directly support, to the remote
server and wait for a response.

Returns most significant digit of the response code.

B<WARNING> This call should only be used on commands that do not require
data connections. Misuse of this method can hang the connection.

=back

=head1 THE dataconn CLASS

Some of the methods defined in C<Net::FTP> return an object which will
be derived from this class.The dataconn class itself is derived from
the C<IO::Socket::INET> class, so any normal IO operations can be performed.
However the following methods are defined in the dataconn class and IO should
be performed using these.

=over 4

=item read ( BUFFER, SIZE [, TIMEOUT ] )

Read C<SIZE> bytes of data from the server and place it into C<BUFFER>, also
performing any <CRLF> translation necessary. C<TIMEOUT> is optional, if not
given, the timeout value from the command connection will be used.

Returns the number of bytes read before any <CRLF> translation.

=item write ( BUFFER, SIZE [, TIMEOUT ] )

Write C<SIZE> bytes of data from C<BUFFER> to the server, also
performing any <CRLF> translation necessary. C<TIMEOUT> is optional, if not
given, the timeout value from the command connection will be used.

Returns the number of bytes written before any <CRLF> translation.

=item bytes_read ()

Returns the number of bytes read so far.

=item abort ()

Abort the current data transfer.

=item close ()

Close the data connection and get a response from the FTP server. Returns
I<true> if the connection was closed successfully and the first digit of
the response from the server was a '2'.

=back

=head1 UNIMPLEMENTED

The following RFC959 commands have not been implemented:

=over 4

=item B<SMNT>

Mount a different file system structure without changing login or
accounting information.

=item B<HELP>

Ask the server for "helpful information" (that's what the RFC says) on
the commands it accepts.

=item B<MODE>

Specifies transfer mode (stream, block or compressed) for file to be
transferred.

=item B<SYST>

Request remote server system identification.

=item B<STAT>

Request remote server status.

=item B<STRU>

Specifies file structure for file to be transferred.

=item B<REIN>

Reinitialize the connection, flushing all I/O and account information.

=back

=head1 REPORTING BUGS

When reporting bugs/problems please include as much information as possible.
It may be difficult for me to reproduce the problem as almost every setup
is different.

A small script which yields the problem will probably be of help. It would
also be useful if this script was run with the extra options C<Debug => 1>
passed to the constructor, and the output sent with the bug report. If you
cannot include a small script then please include a Debug trace from a
run of your program which does yield the problem.

=head1 AUTHOR

Graham Barr <gbarr@pobox.com>

=head1 SEE ALSO

L<Net::Netrc>
L<Net::Cmd>

ftp(1), ftpd(8), RFC 959
http://www.cis.ohio-state.edu/htbin/rfc/rfc959.html

=head1 USE EXAMPLES

For an example of the use of Net::FTP see

=over 4

=item http://www.csh.rit.edu/~adam/Progs/

C<autoftp> is a program that can retrieve, send, or list files via
the FTP protocol in a non-interactive manner.

=back

=head1 CREDITS

Henry Gabryjelski <henryg@WPI.EDU> - for the suggestion of creating directories
recursively.

Nathan Torkington <gnat@frii.com> - for some input on the documentation.

Roderick Schertler <roderick@gate.net> - for various inputs

=head1 COPYRIGHT

Copyright (c) 1995-2004 Graham Barr. All rights reserved.
This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut
