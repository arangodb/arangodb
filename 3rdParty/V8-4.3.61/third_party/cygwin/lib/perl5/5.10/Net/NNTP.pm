# Net::NNTP.pm
#
# Copyright (c) 1995-1997 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package Net::NNTP;

use strict;
use vars qw(@ISA $VERSION $debug);
use IO::Socket;
use Net::Cmd;
use Carp;
use Time::Local;
use Net::Config;

$VERSION = "2.24";
@ISA     = qw(Net::Cmd IO::Socket::INET);


sub new {
  my $self = shift;
  my $type = ref($self) || $self;
  my ($host, %arg);
  if (@_ % 2) {
    $host = shift;
    %arg  = @_;
  }
  else {
    %arg  = @_;
    $host = delete $arg{Host};
  }
  my $obj;

  $host ||= $ENV{NNTPSERVER} || $ENV{NEWSHOST};

  my $hosts = defined $host ? [$host] : $NetConfig{nntp_hosts};

  @{$hosts} = qw(news)
    unless @{$hosts};

  my $h;
  foreach $h (@{$hosts}) {
    $obj = $type->SUPER::new(
      PeerAddr => ($host = $h),
      PeerPort => $arg{Port} || 'nntp(119)',
      Proto => 'tcp',
      Timeout => defined $arg{Timeout}
      ? $arg{Timeout}
      : 120
      )
      and last;
  }

  return undef
    unless defined $obj;

  ${*$obj}{'net_nntp_host'} = $host;

  $obj->autoflush(1);
  $obj->debug(exists $arg{Debug} ? $arg{Debug} : undef);

  unless ($obj->response() == CMD_OK) {
    $obj->close;
    return undef;
  }

  my $c = $obj->code;
  my @m = $obj->message;

  unless (exists $arg{Reader} && $arg{Reader} == 0) {

    # if server is INN and we have transfer rights the we are currently
    # talking to innd not nnrpd
    if ($obj->reader) {

      # If reader suceeds the we need to consider this code to determine postok
      $c = $obj->code;
    }
    else {

      # I want to ignore this failure, so restore the previous status.
      $obj->set_status($c, \@m);
    }
  }

  ${*$obj}{'net_nntp_post'} = $c == 200 ? 1 : 0;

  $obj;
}


sub host {
  my $me = shift;
  ${*$me}{'net_nntp_host'};
}


sub debug_text {
  my $nntp  = shift;
  my $inout = shift;
  my $text  = shift;

  if ( (ref($nntp) and $nntp->code == 350 and $text =~ /^(\S+)/)
    || ($text =~ /^(authinfo\s+pass)/io))
  {
    $text = "$1 ....\n";
  }

  $text;
}


sub postok {
  @_ == 1 or croak 'usage: $nntp->postok()';
  my $nntp = shift;
  ${*$nntp}{'net_nntp_post'} || 0;
}


sub article {
  @_ >= 1 && @_ <= 3 or croak 'usage: $nntp->article( [ MSGID ], [ FH ] )';
  my $nntp = shift;
  my @fh;

  @fh = (pop) if @_ == 2 || (@_ && (ref($_[0]) || ref(\$_[0]) eq 'GLOB'));

  $nntp->_ARTICLE(@_)
    ? $nntp->read_until_dot(@fh)
    : undef;
}


sub articlefh {
  @_ >= 1 && @_ <= 2 or croak 'usage: $nntp->articlefh( [ MSGID ] )';
  my $nntp = shift;

  return unless $nntp->_ARTICLE(@_);
  return $nntp->tied_fh;
}


sub authinfo {
  @_ == 3 or croak 'usage: $nntp->authinfo( USER, PASS )';
  my ($nntp, $user, $pass) = @_;

  $nntp->_AUTHINFO("USER",      $user) == CMD_MORE
    && $nntp->_AUTHINFO("PASS", $pass) == CMD_OK;
}


sub authinfo_simple {
  @_ == 3 or croak 'usage: $nntp->authinfo( USER, PASS )';
  my ($nntp, $user, $pass) = @_;

  $nntp->_AUTHINFO('SIMPLE') == CMD_MORE
    && $nntp->command($user, $pass)->response == CMD_OK;
}


sub body {
  @_ >= 1 && @_ <= 3 or croak 'usage: $nntp->body( [ MSGID ], [ FH ] )';
  my $nntp = shift;
  my @fh;

  @fh = (pop) if @_ == 2 || (@_ && ref($_[0]) || ref(\$_[0]) eq 'GLOB');

  $nntp->_BODY(@_)
    ? $nntp->read_until_dot(@fh)
    : undef;
}


sub bodyfh {
  @_ >= 1 && @_ <= 2 or croak 'usage: $nntp->bodyfh( [ MSGID ] )';
  my $nntp = shift;
  return unless $nntp->_BODY(@_);
  return $nntp->tied_fh;
}


sub head {
  @_ >= 1 && @_ <= 3 or croak 'usage: $nntp->head( [ MSGID ], [ FH ] )';
  my $nntp = shift;
  my @fh;

  @fh = (pop) if @_ == 2 || (@_ && ref($_[0]) || ref(\$_[0]) eq 'GLOB');

  $nntp->_HEAD(@_)
    ? $nntp->read_until_dot(@fh)
    : undef;
}


sub headfh {
  @_ >= 1 && @_ <= 2 or croak 'usage: $nntp->headfh( [ MSGID ] )';
  my $nntp = shift;
  return unless $nntp->_HEAD(@_);
  return $nntp->tied_fh;
}


sub nntpstat {
  @_ == 1 || @_ == 2 or croak 'usage: $nntp->nntpstat( [ MSGID ] )';
  my $nntp = shift;

  $nntp->_STAT(@_) && $nntp->message =~ /(<[^>]+>)/o
    ? $1
    : undef;
}


sub group {
  @_ == 1 || @_ == 2 or croak 'usage: $nntp->group( [ GROUP ] )';
  my $nntp = shift;
  my $grp  = ${*$nntp}{'net_nntp_group'} || undef;

  return $grp
    unless (@_ || wantarray);

  my $newgrp = shift;

  return wantarray ? () : undef
    unless $nntp->_GROUP($newgrp || $grp || "")
    && $nntp->message =~ /(\d+)\s+(\d+)\s+(\d+)\s+(\S+)/;

  my ($count, $first, $last, $group) = ($1, $2, $3, $4);

  # group may be replied as '(current group)'
  $group = ${*$nntp}{'net_nntp_group'}
    if $group =~ /\(/;

  ${*$nntp}{'net_nntp_group'} = $group;

  wantarray
    ? ($count, $first, $last, $group)
    : $group;
}


sub help {
  @_ == 1 or croak 'usage: $nntp->help()';
  my $nntp = shift;

  $nntp->_HELP
    ? $nntp->read_until_dot
    : undef;
}


sub ihave {
  @_ >= 2 or croak 'usage: $nntp->ihave( MESSAGE-ID [, MESSAGE ])';
  my $nntp = shift;
  my $mid  = shift;

  $nntp->_IHAVE($mid) && $nntp->datasend(@_)
    ? @_ == 0 || $nntp->dataend
    : undef;
}


sub last {
  @_ == 1 or croak 'usage: $nntp->last()';
  my $nntp = shift;

  $nntp->_LAST && $nntp->message =~ /(<[^>]+>)/o
    ? $1
    : undef;
}


sub list {
  @_ == 1 or croak 'usage: $nntp->list()';
  my $nntp = shift;

  $nntp->_LIST
    ? $nntp->_grouplist
    : undef;
}


sub newgroups {
  @_ >= 2 or croak 'usage: $nntp->newgroups( SINCE [, DISTRIBUTIONS ])';
  my $nntp = shift;
  my $time = _timestr(shift);
  my $dist = shift || "";

  $dist = join(",", @{$dist})
    if ref($dist);

  $nntp->_NEWGROUPS($time, $dist)
    ? $nntp->_grouplist
    : undef;
}


sub newnews {
  @_ >= 2 && @_ <= 4
    or croak 'usage: $nntp->newnews( SINCE [, GROUPS [, DISTRIBUTIONS ]])';
  my $nntp = shift;
  my $time = _timestr(shift);
  my $grp  = @_ ? shift: $nntp->group;
  my $dist = shift || "";

  $grp ||= "*";
  $grp = join(",", @{$grp})
    if ref($grp);

  $dist = join(",", @{$dist})
    if ref($dist);

  $nntp->_NEWNEWS($grp, $time, $dist)
    ? $nntp->_articlelist
    : undef;
}


sub next {
  @_ == 1 or croak 'usage: $nntp->next()';
  my $nntp = shift;

  $nntp->_NEXT && $nntp->message =~ /(<[^>]+>)/o
    ? $1
    : undef;
}


sub post {
  @_ >= 1 or croak 'usage: $nntp->post( [ MESSAGE ] )';
  my $nntp = shift;

  $nntp->_POST() && $nntp->datasend(@_)
    ? @_ == 0 || $nntp->dataend
    : undef;
}


sub postfh {
  my $nntp = shift;
  return unless $nntp->_POST();
  return $nntp->tied_fh;
}


sub quit {
  @_ == 1 or croak 'usage: $nntp->quit()';
  my $nntp = shift;

  $nntp->_QUIT;
  $nntp->close;
}


sub slave {
  @_ == 1 or croak 'usage: $nntp->slave()';
  my $nntp = shift;

  $nntp->_SLAVE;
}

##
## The following methods are not implemented by all servers
##


sub active {
  @_ == 1 || @_ == 2 or croak 'usage: $nntp->active( [ PATTERN ] )';
  my $nntp = shift;

  $nntp->_LIST('ACTIVE', @_)
    ? $nntp->_grouplist
    : undef;
}


sub active_times {
  @_ == 1 or croak 'usage: $nntp->active_times()';
  my $nntp = shift;

  $nntp->_LIST('ACTIVE.TIMES')
    ? $nntp->_grouplist
    : undef;
}


sub distributions {
  @_ == 1 or croak 'usage: $nntp->distributions()';
  my $nntp = shift;

  $nntp->_LIST('DISTRIBUTIONS')
    ? $nntp->_description
    : undef;
}


sub distribution_patterns {
  @_ == 1 or croak 'usage: $nntp->distributions()';
  my $nntp = shift;

  my $arr;
  local $_;

  $nntp->_LIST('DISTRIB.PATS')
    && ($arr = $nntp->read_until_dot)
    ? [grep { /^\d/ && (chomp, $_ = [split /:/]) } @$arr]
    : undef;
}


sub newsgroups {
  @_ == 1 || @_ == 2 or croak 'usage: $nntp->newsgroups( [ PATTERN ] )';
  my $nntp = shift;

  $nntp->_LIST('NEWSGROUPS', @_)
    ? $nntp->_description
    : undef;
}


sub overview_fmt {
  @_ == 1 or croak 'usage: $nntp->overview_fmt()';
  my $nntp = shift;

  $nntp->_LIST('OVERVIEW.FMT')
    ? $nntp->_articlelist
    : undef;
}


sub subscriptions {
  @_ == 1 or croak 'usage: $nntp->subscriptions()';
  my $nntp = shift;

  $nntp->_LIST('SUBSCRIPTIONS')
    ? $nntp->_articlelist
    : undef;
}


sub listgroup {
  @_ == 1 || @_ == 2 or croak 'usage: $nntp->listgroup( [ GROUP ] )';
  my $nntp = shift;

  $nntp->_LISTGROUP(@_)
    ? $nntp->_articlelist
    : undef;
}


sub reader {
  @_ == 1 or croak 'usage: $nntp->reader()';
  my $nntp = shift;

  $nntp->_MODE('READER');
}


sub xgtitle {
  @_ == 1 || @_ == 2 or croak 'usage: $nntp->xgtitle( [ PATTERN ] )';
  my $nntp = shift;

  $nntp->_XGTITLE(@_)
    ? $nntp->_description
    : undef;
}


sub xhdr {
  @_ >= 2 && @_ <= 4 or croak 'usage: $nntp->xhdr( HEADER, [ MESSAGE-SPEC ] )';
  my $nntp = shift;
  my $hdr  = shift;
  my $arg  = _msg_arg(@_);

  $nntp->_XHDR($hdr, $arg)
    ? $nntp->_description
    : undef;
}


sub xover {
  @_ == 2 || @_ == 3 or croak 'usage: $nntp->xover( MESSAGE-SPEC )';
  my $nntp = shift;
  my $arg  = _msg_arg(@_);

  $nntp->_XOVER($arg)
    ? $nntp->_fieldlist
    : undef;
}


sub xpat {
  @_ == 4 || @_ == 5 or croak '$nntp->xpat( HEADER, PATTERN, MESSAGE-SPEC )';
  my $nntp = shift;
  my $hdr  = shift;
  my $pat  = shift;
  my $arg  = _msg_arg(@_);

  $pat = join(" ", @$pat)
    if ref($pat);

  $nntp->_XPAT($hdr, $arg, $pat)
    ? $nntp->_description
    : undef;
}


sub xpath {
  @_ == 2 or croak 'usage: $nntp->xpath( MESSAGE-ID )';
  my ($nntp, $mid) = @_;

  return undef
    unless $nntp->_XPATH($mid);

  my $m;
  ($m = $nntp->message) =~ s/^\d+\s+//o;
  my @p = split /\s+/, $m;

  wantarray ? @p : $p[0];
}


sub xrover {
  @_ == 2 || @_ == 3 or croak 'usage: $nntp->xrover( MESSAGE-SPEC )';
  my $nntp = shift;
  my $arg  = _msg_arg(@_);

  $nntp->_XROVER($arg)
    ? $nntp->_description
    : undef;
}


sub date {
  @_ == 1 or croak 'usage: $nntp->date()';
  my $nntp = shift;

  $nntp->_DATE
    && $nntp->message =~ /(\d{4})(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)/
    ? timegm($6, $5, $4, $3, $2 - 1, $1 - 1900)
    : undef;
}


##
## Private subroutines
##


sub _msg_arg {
  my $spec = shift;
  my $arg  = "";

  if (@_) {
    carp "Depriciated passing of two message numbers, " . "pass a reference"
      if $^W;
    $spec = [$spec, $_[0]];
  }

  if (defined $spec) {
    if (ref($spec)) {
      $arg = $spec->[0];
      if (defined $spec->[1]) {
        $arg .= "-"
          if $spec->[1] != $spec->[0];
        $arg .= $spec->[1]
          if $spec->[1] > $spec->[0];
      }
    }
    else {
      $arg = $spec;
    }
  }

  $arg;
}


sub _timestr {
  my $time = shift;
  my @g    = reverse((gmtime($time))[0 .. 5]);
  $g[1] += 1;
  $g[0] %= 100;
  sprintf "%02d%02d%02d %02d%02d%02d GMT", @g;
}


sub _grouplist {
  my $nntp = shift;
  my $arr  = $nntp->read_until_dot
    or return undef;

  my $hash = {};
  my $ln;

  foreach $ln (@$arr) {
    my @a = split(/[\s\n]+/, $ln);
    $hash->{$a[0]} = [@a[1, 2, 3]];
  }

  $hash;
}


sub _fieldlist {
  my $nntp = shift;
  my $arr  = $nntp->read_until_dot
    or return undef;

  my $hash = {};
  my $ln;

  foreach $ln (@$arr) {
    my @a = split(/[\t\n]/, $ln);
    my $m = shift @a;
    $hash->{$m} = [@a];
  }

  $hash;
}


sub _articlelist {
  my $nntp = shift;
  my $arr  = $nntp->read_until_dot;

  chomp(@$arr)
    if $arr;

  $arr;
}


sub _description {
  my $nntp = shift;
  my $arr  = $nntp->read_until_dot
    or return undef;

  my $hash = {};
  my $ln;

  foreach $ln (@$arr) {
    chomp($ln);

    $hash->{$1} = $ln
      if $ln =~ s/^\s*(\S+)\s*//o;
  }

  $hash;

}

##
## The commands
##


sub _ARTICLE  { shift->command('ARTICLE',  @_)->response == CMD_OK }
sub _AUTHINFO { shift->command('AUTHINFO', @_)->response }
sub _BODY     { shift->command('BODY',     @_)->response == CMD_OK }
sub _DATE      { shift->command('DATE')->response == CMD_INFO }
sub _GROUP     { shift->command('GROUP', @_)->response == CMD_OK }
sub _HEAD      { shift->command('HEAD', @_)->response == CMD_OK }
sub _HELP      { shift->command('HELP', @_)->response == CMD_INFO }
sub _IHAVE     { shift->command('IHAVE', @_)->response == CMD_MORE }
sub _LAST      { shift->command('LAST')->response == CMD_OK }
sub _LIST      { shift->command('LIST', @_)->response == CMD_OK }
sub _LISTGROUP { shift->command('LISTGROUP', @_)->response == CMD_OK }
sub _NEWGROUPS { shift->command('NEWGROUPS', @_)->response == CMD_OK }
sub _NEWNEWS   { shift->command('NEWNEWS', @_)->response == CMD_OK }
sub _NEXT      { shift->command('NEXT')->response == CMD_OK }
sub _POST      { shift->command('POST', @_)->response == CMD_MORE }
sub _QUIT      { shift->command('QUIT', @_)->response == CMD_OK }
sub _SLAVE     { shift->command('SLAVE', @_)->response == CMD_OK }
sub _STAT      { shift->command('STAT', @_)->response == CMD_OK }
sub _MODE      { shift->command('MODE', @_)->response == CMD_OK }
sub _XGTITLE   { shift->command('XGTITLE', @_)->response == CMD_OK }
sub _XHDR      { shift->command('XHDR', @_)->response == CMD_OK }
sub _XPAT      { shift->command('XPAT', @_)->response == CMD_OK }
sub _XPATH     { shift->command('XPATH', @_)->response == CMD_OK }
sub _XOVER     { shift->command('XOVER', @_)->response == CMD_OK }
sub _XROVER    { shift->command('XROVER', @_)->response == CMD_OK }
sub _XTHREAD   { shift->unsupported }
sub _XSEARCH   { shift->unsupported }
sub _XINDEX    { shift->unsupported }

##
## IO/perl methods
##


sub DESTROY {
  my $nntp = shift;
  defined(fileno($nntp)) && $nntp->quit;
}


1;

__END__

=head1 NAME

Net::NNTP - NNTP Client class

=head1 SYNOPSIS

    use Net::NNTP;

    $nntp = Net::NNTP->new("some.host.name");
    $nntp->quit;

=head1 DESCRIPTION

C<Net::NNTP> is a class implementing a simple NNTP client in Perl as described
in RFC977. C<Net::NNTP> inherits its communication methods from C<Net::Cmd>

=head1 CONSTRUCTOR

=over 4

=item new ( [ HOST ] [, OPTIONS ])

This is the constructor for a new Net::NNTP object. C<HOST> is the
name of the remote host to which a NNTP connection is required. If not
given then it may be passed as the C<Host> option described below. If no host is passed
then two environment variables are checked, first C<NNTPSERVER> then
C<NEWSHOST>, then C<Net::Config> is checked, and if a host is not found
then C<news> is used.

C<OPTIONS> are passed in a hash like fashion, using key and value pairs.
Possible options are:

B<Host> - NNTP host to connect to. It may be a single scalar, as defined for
the C<PeerAddr> option in L<IO::Socket::INET>, or a reference to
an array with hosts to try in turn. The L</host> method will return the value
which was used to connect to the host.

B<Timeout> - Maximum time, in seconds, to wait for a response from the
NNTP server, a value of zero will cause all IO operations to block.
(default: 120)

B<Debug> - Enable the printing of debugging information to STDERR

B<Reader> - If the remote server is INN then initially the connection
will be to nnrpd, by default C<Net::NNTP> will issue a C<MODE READER> command
so that the remote server becomes innd. If the C<Reader> option is given
with a value of zero, then this command will not be sent and the
connection will be left talking to nnrpd.

=back

=head1 METHODS

Unless otherwise stated all methods return either a I<true> or I<false>
value, with I<true> meaning that the operation was a success. When a method
states that it returns a value, failure will be returned as I<undef> or an
empty list.

=over 4

=item article ( [ MSGID|MSGNUM ], [FH] )

Retrieve the header, a blank line, then the body (text) of the
specified article. 

If C<FH> is specified then it is expected to be a valid filehandle
and the result will be printed to it, on success a true value will be
returned. If C<FH> is not specified then the return value, on success,
will be a reference to an array containing the article requested, each
entry in the array will contain one line of the article.

If no arguments are passed then the current article in the currently
selected newsgroup is fetched.

C<MSGNUM> is a numeric id of an article in the current newsgroup, and
will change the current article pointer.  C<MSGID> is the message id of
an article as shown in that article's header.  It is anticipated that the
client will obtain the C<MSGID> from a list provided by the C<newnews>
command, from references contained within another article, or from the
message-id provided in the response to some other commands.

If there is an error then C<undef> will be returned.

=item body ( [ MSGID|MSGNUM ], [FH] )

Like C<article> but only fetches the body of the article.

=item head ( [ MSGID|MSGNUM ], [FH] )

Like C<article> but only fetches the headers for the article.

=item articlefh ( [ MSGID|MSGNUM ] )

=item bodyfh ( [ MSGID|MSGNUM ] )

=item headfh ( [ MSGID|MSGNUM ] )

These are similar to article(), body() and head(), but rather than
returning the requested data directly, they return a tied filehandle
from which to read the article.

=item nntpstat ( [ MSGID|MSGNUM ] )

The C<nntpstat> command is similar to the C<article> command except that no
text is returned.  When selecting by message number within a group,
the C<nntpstat> command serves to set the "current article pointer" without
sending text.

Using the C<nntpstat> command to
select by message-id is valid but of questionable value, since a
selection by message-id does B<not> alter the "current article pointer".

Returns the message-id of the "current article".

=item group ( [ GROUP ] )

Set and/or get the current group. If C<GROUP> is not given then information
is returned on the current group.

In a scalar context it returns the group name.

In an array context the return value is a list containing, the number
of articles in the group, the number of the first article, the number
of the last article and the group name.

=item ihave ( MSGID [, MESSAGE ])

The C<ihave> command informs the server that the client has an article
whose id is C<MSGID>.  If the server desires a copy of that
article, and C<MESSAGE> has been given the it will be sent.

Returns I<true> if the server desires the article and C<MESSAGE> was
successfully sent,if specified.

If C<MESSAGE> is not specified then the message must be sent using the
C<datasend> and C<dataend> methods from L<Net::Cmd>

C<MESSAGE> can be either an array of lines or a reference to an array.

=item last ()

Set the "current article pointer" to the previous article in the current
newsgroup.

Returns the message-id of the article.

=item date ()

Returns the date on the remote server. This date will be in a UNIX time
format (seconds since 1970)

=item postok ()

C<postok> will return I<true> if the servers initial response indicated
that it will allow posting.

=item authinfo ( USER, PASS )

Authenticates to the server (using AUTHINFO USER / AUTHINFO PASS)
using the supplied username and password.  Please note that the
password is sent in clear text to the server.  This command should not
be used with valuable passwords unless the connection to the server is
somehow protected.

=item list ()

Obtain information about all the active newsgroups. The results is a reference
to a hash where the key is a group name and each value is a reference to an
array. The elements in this array are:- the last article number in the group,
the first article number in the group and any information flags about the group.

=item newgroups ( SINCE [, DISTRIBUTIONS ])

C<SINCE> is a time value and C<DISTRIBUTIONS> is either a distribution
pattern or a reference to a list of distribution patterns.
The result is the same as C<list>, but the
groups return will be limited to those created after C<SINCE> and, if
specified, in one of the distribution areas in C<DISTRIBUTIONS>. 

=item newnews ( SINCE [, GROUPS [, DISTRIBUTIONS ]])

C<SINCE> is a time value. C<GROUPS> is either a group pattern or a reference
to a list of group patterns. C<DISTRIBUTIONS> is either a distribution
pattern or a reference to a list of distribution patterns.

Returns a reference to a list which contains the message-ids of all news posted
after C<SINCE>, that are in a groups which matched C<GROUPS> and a
distribution which matches C<DISTRIBUTIONS>.

=item next ()

Set the "current article pointer" to the next article in the current
newsgroup.

Returns the message-id of the article.

=item post ( [ MESSAGE ] )

Post a new article to the news server. If C<MESSAGE> is specified and posting
is allowed then the message will be sent.

If C<MESSAGE> is not specified then the message must be sent using the
C<datasend> and C<dataend> methods from L<Net::Cmd>

C<MESSAGE> can be either an array of lines or a reference to an array.

The message, either sent via C<datasend> or as the C<MESSAGE>
parameter, must be in the format as described by RFC822 and must
contain From:, Newsgroups: and Subject: headers.

=item postfh ()

Post a new article to the news server using a tied filehandle.  If
posting is allowed, this method will return a tied filehandle that you
can print() the contents of the article to be posted.  You must
explicitly close() the filehandle when you are finished posting the
article, and the return value from the close() call will indicate
whether the message was successfully posted.

=item slave ()

Tell the remote server that I am not a user client, but probably another
news server.

=item quit ()

Quit the remote server and close the socket connection.

=back

=head2 Extension methods

These methods use commands that are not part of the RFC977 documentation. Some
servers may not support all of them.

=over 4

=item newsgroups ( [ PATTERN ] )

Returns a reference to a hash where the keys are all the group names which
match C<PATTERN>, or all of the groups if no pattern is specified, and
each value contains the description text for the group.

=item distributions ()

Returns a reference to a hash where the keys are all the possible
distribution names and the values are the distribution descriptions.

=item subscriptions ()

Returns a reference to a list which contains a list of groups which
are recommended for a new user to subscribe to.

=item overview_fmt ()

Returns a reference to an array which contain the names of the fields returned
by C<xover>.

=item active_times ()

Returns a reference to a hash where the keys are the group names and each
value is a reference to an array containing the time the groups was created
and an identifier, possibly an Email address, of the creator.

=item active ( [ PATTERN ] )

Similar to C<list> but only active groups that match the pattern are returned.
C<PATTERN> can be a group pattern.

=item xgtitle ( PATTERN )

Returns a reference to a hash where the keys are all the group names which
match C<PATTERN> and each value is the description text for the group.

=item xhdr ( HEADER, MESSAGE-SPEC )

Obtain the header field C<HEADER> for all the messages specified. 

The return value will be a reference
to a hash where the keys are the message numbers and each value contains
the text of the requested header for that message.

=item xover ( MESSAGE-SPEC )

The return value will be a reference
to a hash where the keys are the message numbers and each value contains
a reference to an array which contains the overview fields for that
message.

The names of the fields can be obtained by calling C<overview_fmt>.

=item xpath ( MESSAGE-ID )

Returns the path name to the file on the server which contains the specified
message.

=item xpat ( HEADER, PATTERN, MESSAGE-SPEC)

The result is the same as C<xhdr> except the is will be restricted to
headers where the text of the header matches C<PATTERN>

=item xrover

The XROVER command returns reference information for the article(s)
specified.

Returns a reference to a HASH where the keys are the message numbers and the
values are the References: lines from the articles

=item listgroup ( [ GROUP ] )

Returns a reference to a list of all the active messages in C<GROUP>, or
the current group if C<GROUP> is not specified.

=item reader

Tell the server that you are a reader and not another server.

This is required by some servers. For example if you are connecting to
an INN server and you have transfer permission your connection will
be connected to the transfer daemon, not the NNTP daemon. Issuing
this command will cause the transfer daemon to hand over control
to the NNTP daemon.

Some servers do not understand this command, but issuing it and ignoring
the response is harmless.

=back

=head1 UNSUPPORTED

The following NNTP command are unsupported by the package, and there are
no plans to do so.

    AUTHINFO GENERIC
    XTHREAD
    XSEARCH
    XINDEX

=head1 DEFINITIONS

=over 4

=item MESSAGE-SPEC

C<MESSAGE-SPEC> is either a single message-id, a single message number, or
a reference to a list of two message numbers.

If C<MESSAGE-SPEC> is a reference to a list of two message numbers and the
second number in a range is less than or equal to the first then the range
represents all messages in the group after the first message number.

B<NOTE> For compatibility reasons only with earlier versions of Net::NNTP
a message spec can be passed as a list of two numbers, this is deprecated
and a reference to the list should now be passed

=item PATTERN

The C<NNTP> protocol uses the C<WILDMAT> format for patterns.
The WILDMAT format was first developed by Rich Salz based on
the format used in the UNIX "find" command to articulate
file names. It was developed to provide a uniform mechanism
for matching patterns in the same manner that the UNIX shell
matches filenames.

Patterns are implicitly anchored at the
beginning and end of each string when testing for a match.

There are five pattern matching operations other than a strict
one-to-one match between the pattern and the source to be
checked for a match.

The first is an asterisk C<*> to match any sequence of zero or more
characters.

The second is a question mark C<?> to match any single character. The
third specifies a specific set of characters.

The set is specified as a list of characters, or as a range of characters
where the beginning and end of the range are separated by a minus (or dash)
character, or as any combination of lists and ranges. The dash can
also be included in the set as a character it if is the beginning
or end of the set. This set is enclosed in square brackets. The
close square bracket C<]> may be used in a set if it is the first
character in the set.

The fourth operation is the same as the
logical not of the third operation and is specified the same
way as the third with the addition of a caret character C<^> at
the beginning of the test string just inside the open square
bracket.

The final operation uses the backslash character to
invalidate the special meaning of an open square bracket C<[>,
the asterisk, backslash or the question mark. Two backslashes in
sequence will result in the evaluation of the backslash as a
character with no special meaning.

=over 4

=item Examples

=item C<[^]-]>

matches any single character other than a close square
bracket or a minus sign/dash.

=item C<*bdc>

matches any string that ends with the string "bdc"
including the string "bdc" (without quotes).

=item C<[0-9a-zA-Z]>

matches any single printable alphanumeric ASCII character.

=item C<a??d>

matches any four character string which begins
with a and ends with d.

=back

=back

=head1 SEE ALSO

L<Net::Cmd>

=head1 AUTHOR

Graham Barr <gbarr@pobox.com>

=head1 COPYRIGHT

Copyright (c) 1995-1997 Graham Barr. All rights reserved.
This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
