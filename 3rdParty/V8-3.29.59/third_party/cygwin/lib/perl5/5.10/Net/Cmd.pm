# Net::Cmd.pm
#
# Copyright (c) 1995-2006 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package Net::Cmd;

require 5.001;
require Exporter;

use strict;
use vars qw(@ISA @EXPORT $VERSION);
use Carp;
use Symbol 'gensym';

BEGIN {
  if ($^O eq 'os390') {
    require Convert::EBCDIC;

    #    Convert::EBCDIC->import;
  }
}

BEGIN {
  if (!eval { require utf8 }) {
    *is_utf8 = sub { 0 };
  }
  elsif (eval { utf8::is_utf8(undef); 1 }) {
    *is_utf8 = \&utf8::is_utf8;
  }
  elsif (eval { require Encode; Encode::is_utf8(undef); 1 }) {
    *is_utf8 = \&Encode::is_utf8;
  }
  else {
    *is_utf8 = sub { $_[0] =~ /[^\x00-\xff]/ };
  }
}

$VERSION = "2.29";
@ISA     = qw(Exporter);
@EXPORT  = qw(CMD_INFO CMD_OK CMD_MORE CMD_REJECT CMD_ERROR CMD_PENDING);


sub CMD_INFO    {1}
sub CMD_OK      {2}
sub CMD_MORE    {3}
sub CMD_REJECT  {4}
sub CMD_ERROR   {5}
sub CMD_PENDING {0}

my %debug = ();

my $tr = $^O eq 'os390' ? Convert::EBCDIC->new() : undef;


sub toebcdic {
  my $cmd = shift;

  unless (exists ${*$cmd}{'net_cmd_asciipeer'}) {
    my $string    = $_[0];
    my $ebcdicstr = $tr->toebcdic($string);
    ${*$cmd}{'net_cmd_asciipeer'} = $string !~ /^\d+/ && $ebcdicstr =~ /^\d+/;
  }

  ${*$cmd}{'net_cmd_asciipeer'}
    ? $tr->toebcdic($_[0])
    : $_[0];
}


sub toascii {
  my $cmd = shift;
  ${*$cmd}{'net_cmd_asciipeer'}
    ? $tr->toascii($_[0])
    : $_[0];
}


sub _print_isa {
  no strict qw(refs);

  my $pkg = shift;
  my $cmd = $pkg;

  $debug{$pkg} ||= 0;

  my %done = ();
  my @do   = ($pkg);
  my %spc  = ($pkg, "");

  while ($pkg = shift @do) {
    next if defined $done{$pkg};

    $done{$pkg} = 1;

    my $v =
      defined ${"${pkg}::VERSION"}
      ? "(" . ${"${pkg}::VERSION"} . ")"
      : "";

    my $spc = $spc{$pkg};
    $cmd->debug_print(1, "${spc}${pkg}${v}\n");

    if (@{"${pkg}::ISA"}) {
      @spc{@{"${pkg}::ISA"}} = ("  " . $spc{$pkg}) x @{"${pkg}::ISA"};
      unshift(@do, @{"${pkg}::ISA"});
    }
  }
}


sub debug {
  @_ == 1 or @_ == 2 or croak 'usage: $obj->debug([LEVEL])';

  my ($cmd, $level) = @_;
  my $pkg    = ref($cmd) || $cmd;
  my $oldval = 0;

  if (ref($cmd)) {
    $oldval = ${*$cmd}{'net_cmd_debug'} || 0;
  }
  else {
    $oldval = $debug{$pkg} || 0;
  }

  return $oldval
    unless @_ == 2;

  $level = $debug{$pkg} || 0
    unless defined $level;

  _print_isa($pkg)
    if ($level && !exists $debug{$pkg});

  if (ref($cmd)) {
    ${*$cmd}{'net_cmd_debug'} = $level;
  }
  else {
    $debug{$pkg} = $level;
  }

  $oldval;
}


sub message {
  @_ == 1 or croak 'usage: $obj->message()';

  my $cmd = shift;

  wantarray
    ? @{${*$cmd}{'net_cmd_resp'}}
    : join("", @{${*$cmd}{'net_cmd_resp'}});
}


sub debug_text { $_[2] }


sub debug_print {
  my ($cmd, $out, $text) = @_;
  print STDERR $cmd, ($out ? '>>> ' : '<<< '), $cmd->debug_text($out, $text);
}


sub code {
  @_ == 1 or croak 'usage: $obj->code()';

  my $cmd = shift;

  ${*$cmd}{'net_cmd_code'} = "000"
    unless exists ${*$cmd}{'net_cmd_code'};

  ${*$cmd}{'net_cmd_code'};
}


sub status {
  @_ == 1 or croak 'usage: $obj->status()';

  my $cmd = shift;

  substr(${*$cmd}{'net_cmd_code'}, 0, 1);
}


sub set_status {
  @_ == 3 or croak 'usage: $obj->set_status(CODE, MESSAGE)';

  my $cmd = shift;
  my ($code, $resp) = @_;

  $resp = [$resp]
    unless ref($resp);

  (${*$cmd}{'net_cmd_code'}, ${*$cmd}{'net_cmd_resp'}) = ($code, $resp);

  1;
}


sub command {
  my $cmd = shift;

  unless (defined fileno($cmd)) {
    $cmd->set_status("599", "Connection closed");
    return $cmd;
  }


  $cmd->dataend()
    if (exists ${*$cmd}{'net_cmd_last_ch'});

  if (scalar(@_)) {
    local $SIG{PIPE} = 'IGNORE' unless $^O eq 'MacOS';

    my $str = join(
      " ",
      map {
        /\n/
          ? do { my $n = $_; $n =~ tr/\n/ /; $n }
          : $_;
        } @_
    );
    $str = $cmd->toascii($str) if $tr;
    $str .= "\015\012";

    my $len = length $str;
    my $swlen;

    $cmd->close
      unless (defined($swlen = syswrite($cmd, $str, $len)) && $swlen == $len);

    $cmd->debug_print(1, $str)
      if ($cmd->debug);

    ${*$cmd}{'net_cmd_resp'} = [];       # the response
    ${*$cmd}{'net_cmd_code'} = "000";    # Made this one up :-)
  }

  $cmd;
}


sub ok {
  @_ == 1 or croak 'usage: $obj->ok()';

  my $code = $_[0]->code;
  0 < $code && $code < 400;
}


sub unsupported {
  my $cmd = shift;

  ${*$cmd}{'net_cmd_resp'} = ['Unsupported command'];
  ${*$cmd}{'net_cmd_code'} = 580;
  0;
}


sub getline {
  my $cmd = shift;

  ${*$cmd}{'net_cmd_lines'} ||= [];

  return shift @{${*$cmd}{'net_cmd_lines'}}
    if scalar(@{${*$cmd}{'net_cmd_lines'}});

  my $partial = defined(${*$cmd}{'net_cmd_partial'}) ? ${*$cmd}{'net_cmd_partial'} : "";
  my $fd      = fileno($cmd);

  return undef
    unless defined $fd;

  my $rin = "";
  vec($rin, $fd, 1) = 1;

  my $buf;

  until (scalar(@{${*$cmd}{'net_cmd_lines'}})) {
    my $timeout = $cmd->timeout || undef;
    my $rout;

    my $select_ret = select($rout = $rin, undef, undef, $timeout);
    if ($select_ret > 0) {
      unless (sysread($cmd, $buf = "", 1024)) {
        carp(ref($cmd) . ": Unexpected EOF on command channel")
          if $cmd->debug;
        $cmd->close;
        return undef;
      }

      substr($buf, 0, 0) = $partial;    ## prepend from last sysread

      my @buf = split(/\015?\012/, $buf, -1);    ## break into lines

      $partial = pop @buf;

      push(@{${*$cmd}{'net_cmd_lines'}}, map {"$_\n"} @buf);

    }
    else {
      my $msg = $select_ret ? "Error or Interrupted: $!" : "Timeout";
      carp("$cmd: $msg") if ($cmd->debug);
      return undef;
    }
  }

  ${*$cmd}{'net_cmd_partial'} = $partial;

  if ($tr) {
    foreach my $ln (@{${*$cmd}{'net_cmd_lines'}}) {
      $ln = $cmd->toebcdic($ln);
    }
  }

  shift @{${*$cmd}{'net_cmd_lines'}};
}


sub ungetline {
  my ($cmd, $str) = @_;

  ${*$cmd}{'net_cmd_lines'} ||= [];
  unshift(@{${*$cmd}{'net_cmd_lines'}}, $str);
}


sub parse_response {
  return ()
    unless $_[1] =~ s/^(\d\d\d)(.?)//o;
  ($1, $2 eq "-");
}


sub response {
  my $cmd = shift;
  my ($code, $more) = (undef) x 2;

  ${*$cmd}{'net_cmd_resp'} ||= [];

  while (1) {
    my $str = $cmd->getline();

    return CMD_ERROR
      unless defined($str);

    $cmd->debug_print(0, $str)
      if ($cmd->debug);

    ($code, $more) = $cmd->parse_response($str);
    unless (defined $code) {
      $cmd->ungetline($str);
      last;
    }

    ${*$cmd}{'net_cmd_code'} = $code;

    push(@{${*$cmd}{'net_cmd_resp'}}, $str);

    last unless ($more);
  }

  substr($code, 0, 1);
}


sub read_until_dot {
  my $cmd = shift;
  my $fh  = shift;
  my $arr = [];

  while (1) {
    my $str = $cmd->getline() or return undef;

    $cmd->debug_print(0, $str)
      if ($cmd->debug & 4);

    last if ($str =~ /^\.\r?\n/o);

    $str =~ s/^\.\././o;

    if (defined $fh) {
      print $fh $str;
    }
    else {
      push(@$arr, $str);
    }
  }

  $arr;
}


sub datasend {
  my $cmd  = shift;
  my $arr  = @_ == 1 && ref($_[0]) ? $_[0] : \@_;
  my $line = join("", @$arr);

  # encode to individual utf8 bytes if
  # $line is a string (in internal UTF-8)
  utf8::encode($line) if is_utf8($line);

  return 0 unless defined(fileno($cmd));

  my $last_ch = ${*$cmd}{'net_cmd_last_ch'};
  $last_ch = ${*$cmd}{'net_cmd_last_ch'} = "\012" unless defined $last_ch;

  return 1 unless length $line;

  if ($cmd->debug) {
    foreach my $b (split(/\n/, $line)) {
      $cmd->debug_print(1, "$b\n");
    }
  }

  $line =~ tr/\r\n/\015\012/ unless "\r" eq "\015";

  my $first_ch = '';

  if ($last_ch eq "\015") {
    $first_ch = "\012" if $line =~ s/^\012//;
  }
  elsif ($last_ch eq "\012") {
    $first_ch = "." if $line =~ /^\./;
  }

  $line =~ s/\015?\012(\.?)/\015\012$1$1/sg;

  substr($line, 0, 0) = $first_ch;

  ${*$cmd}{'net_cmd_last_ch'} = substr($line, -1, 1);

  my $len    = length($line);
  my $offset = 0;
  my $win    = "";
  vec($win, fileno($cmd), 1) = 1;
  my $timeout = $cmd->timeout || undef;

  local $SIG{PIPE} = 'IGNORE' unless $^O eq 'MacOS';

  while ($len) {
    my $wout;
    my $s = select(undef, $wout = $win, undef, $timeout);
    if ((defined $s and $s > 0) or -f $cmd)    # -f for testing on win32
    {
      my $w = syswrite($cmd, $line, $len, $offset);
      unless (defined($w)) {
        carp("$cmd: $!") if $cmd->debug;
        return undef;
      }
      $len -= $w;
      $offset += $w;
    }
    else {
      carp("$cmd: Timeout") if ($cmd->debug);
      return undef;
    }
  }

  1;
}


sub rawdatasend {
  my $cmd  = shift;
  my $arr  = @_ == 1 && ref($_[0]) ? $_[0] : \@_;
  my $line = join("", @$arr);

  return 0 unless defined(fileno($cmd));

  return 1
    unless length($line);

  if ($cmd->debug) {
    my $b = "$cmd>>> ";
    print STDERR $b, join("\n$b", split(/\n/, $line)), "\n";
  }

  my $len    = length($line);
  my $offset = 0;
  my $win    = "";
  vec($win, fileno($cmd), 1) = 1;
  my $timeout = $cmd->timeout || undef;

  local $SIG{PIPE} = 'IGNORE' unless $^O eq 'MacOS';
  while ($len) {
    my $wout;
    if (select(undef, $wout = $win, undef, $timeout) > 0) {
      my $w = syswrite($cmd, $line, $len, $offset);
      unless (defined($w)) {
        carp("$cmd: $!") if $cmd->debug;
        return undef;
      }
      $len -= $w;
      $offset += $w;
    }
    else {
      carp("$cmd: Timeout") if ($cmd->debug);
      return undef;
    }
  }

  1;
}


sub dataend {
  my $cmd = shift;

  return 0 unless defined(fileno($cmd));

  my $ch = ${*$cmd}{'net_cmd_last_ch'};
  my $tosend;

  if (!defined $ch) {
    return 1;
  }
  elsif ($ch ne "\012") {
    $tosend = "\015\012";
  }

  $tosend .= ".\015\012";

  local $SIG{PIPE} = 'IGNORE' unless $^O eq 'MacOS';

  $cmd->debug_print(1, ".\n")
    if ($cmd->debug);

  syswrite($cmd, $tosend, length $tosend);

  delete ${*$cmd}{'net_cmd_last_ch'};

  $cmd->response() == CMD_OK;
}

# read and write to tied filehandle
sub tied_fh {
  my $cmd = shift;
  ${*$cmd}{'net_cmd_readbuf'} = '';
  my $fh = gensym();
  tie *$fh, ref($cmd), $cmd;
  return $fh;
}

# tie to myself
sub TIEHANDLE {
  my $class = shift;
  my $cmd   = shift;
  return $cmd;
}

# Tied filehandle read.  Reads requested data length, returning
# end-of-file when the dot is encountered.
sub READ {
  my $cmd = shift;
  my ($len, $offset) = @_[1, 2];
  return unless exists ${*$cmd}{'net_cmd_readbuf'};
  my $done = 0;
  while (!$done and length(${*$cmd}{'net_cmd_readbuf'}) < $len) {
    ${*$cmd}{'net_cmd_readbuf'} .= $cmd->getline() or return;
    $done++ if ${*$cmd}{'net_cmd_readbuf'} =~ s/^\.\r?\n\Z//m;
  }

  $_[0] = '';
  substr($_[0], $offset + 0) = substr(${*$cmd}{'net_cmd_readbuf'}, 0, $len);
  substr(${*$cmd}{'net_cmd_readbuf'}, 0, $len) = '';
  delete ${*$cmd}{'net_cmd_readbuf'} if $done;

  return length $_[0];
}


sub READLINE {
  my $cmd = shift;

  # in this context, we use the presence of readbuf to
  # indicate that we have not yet reached the eof
  return unless exists ${*$cmd}{'net_cmd_readbuf'};
  my $line = $cmd->getline;
  return if $line =~ /^\.\r?\n/;
  $line;
}


sub PRINT {
  my $cmd = shift;
  my ($buf, $len, $offset) = @_;
  $len ||= length($buf);
  $offset += 0;
  return unless $cmd->datasend(substr($buf, $offset, $len));
  ${*$cmd}{'net_cmd_sending'}++;    # flag that we should call dataend()
  return $len;
}


sub CLOSE {
  my $cmd = shift;
  my $r = exists(${*$cmd}{'net_cmd_sending'}) ? $cmd->dataend : 1;
  delete ${*$cmd}{'net_cmd_readbuf'};
  delete ${*$cmd}{'net_cmd_sending'};
  $r;
}

1;

__END__


=head1 NAME

Net::Cmd - Network Command class (as used by FTP, SMTP etc)

=head1 SYNOPSIS

    use Net::Cmd;

    @ISA = qw(Net::Cmd);

=head1 DESCRIPTION

C<Net::Cmd> is a collection of methods that can be inherited by a sub class
of C<IO::Handle>. These methods implement the functionality required for a
command based protocol, for example FTP and SMTP.

=head1 USER METHODS

These methods provide a user interface to the C<Net::Cmd> object.

=over 4

=item debug ( VALUE )

Set the level of debug information for this object. If C<VALUE> is not given
then the current state is returned. Otherwise the state is changed to 
C<VALUE> and the previous state returned. 

Different packages
may implement different levels of debug but a non-zero value results in 
copies of all commands and responses also being sent to STDERR.

If C<VALUE> is C<undef> then the debug level will be set to the default
debug level for the class.

This method can also be called as a I<static> method to set/get the default
debug level for a given class.

=item message ()

Returns the text message returned from the last command

=item code ()

Returns the 3-digit code from the last command. If a command is pending
then the value 0 is returned

=item ok ()

Returns non-zero if the last code value was greater than zero and
less than 400. This holds true for most command servers. Servers
where this does not hold may override this method.

=item status ()

Returns the most significant digit of the current status code. If a command
is pending then C<CMD_PENDING> is returned.

=item datasend ( DATA )

Send data to the remote server, converting LF to CRLF. Any line starting
with a '.' will be prefixed with another '.'.
C<DATA> may be an array or a reference to an array.

=item dataend ()

End the sending of data to the remote server. This is done by ensuring that
the data already sent ends with CRLF then sending '.CRLF' to end the
transmission. Once this data has been sent C<dataend> calls C<response> and
returns true if C<response> returns CMD_OK.

=back

=head1 CLASS METHODS

These methods are not intended to be called by the user, but used or 
over-ridden by a sub-class of C<Net::Cmd>

=over 4

=item debug_print ( DIR, TEXT )

Print debugging information. C<DIR> denotes the direction I<true> being
data being sent to the server. Calls C<debug_text> before printing to
STDERR.

=item debug_text ( TEXT )

This method is called to print debugging information. TEXT is
the text being sent. The method should return the text to be printed

This is primarily meant for the use of modules such as FTP where passwords
are sent, but we do not want to display them in the debugging information.

=item command ( CMD [, ARGS, ... ])

Send a command to the command server. All arguments a first joined with
a space character and CRLF is appended, this string is then sent to the
command server.

Returns undef upon failure

=item unsupported ()

Sets the status code to 580 and the response text to 'Unsupported command'.
Returns zero.

=item response ()

Obtain a response from the server. Upon success the most significant digit
of the status code is returned. Upon failure, timeout etc., I<undef> is
returned.

=item parse_response ( TEXT )

This method is called by C<response> as a method with one argument. It should
return an array of 2 values, the 3-digit status code and a flag which is true
when this is part of a multi-line response and this line is not the list.

=item getline ()

Retrieve one line, delimited by CRLF, from the remote server. Returns I<undef>
upon failure.

B<NOTE>: If you do use this method for any reason, please remember to add
some C<debug_print> calls into your method.

=item ungetline ( TEXT )

Unget a line of text from the server.

=item rawdatasend ( DATA )

Send data to the remote server without performing any conversions. C<DATA>
is a scalar.

=item read_until_dot ()

Read data from the remote server until a line consisting of a single '.'.
Any lines starting with '..' will have one of the '.'s removed.

Returns a reference to a list containing the lines, or I<undef> upon failure.

=item tied_fh ()

Returns a filehandle tied to the Net::Cmd object.  After issuing a
command, you may read from this filehandle using read() or <>.  The
filehandle will return EOF when the final dot is encountered.
Similarly, you may write to the filehandle in order to send data to
the server after issuing a command that expects data to be written.

See the Net::POP3 and Net::SMTP modules for examples of this.

=back

=head1 EXPORTS

C<Net::Cmd> exports six subroutines, five of these, C<CMD_INFO>, C<CMD_OK>,
C<CMD_MORE>, C<CMD_REJECT> and C<CMD_ERROR>, correspond to possible results
of C<response> and C<status>. The sixth is C<CMD_PENDING>.

=head1 AUTHOR

Graham Barr <gbarr@pobox.com>

=head1 COPYRIGHT

Copyright (c) 1995-2006 Graham Barr. All rights reserved.
This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
