##
## Generic data connection package
##

package Net::FTP::dataconn;

use Carp;
use vars qw(@ISA $timeout $VERSION);
use Net::Cmd;
use Errno;

$VERSION = '0.11';
@ISA     = qw(IO::Socket::INET);


sub reading {
  my $data = shift;
  ${*$data}{'net_ftp_bytesread'} = 0;
}


sub abort {
  my $data = shift;
  my $ftp  = ${*$data}{'net_ftp_cmd'};

  # no need to abort if we have finished the xfer
  return $data->close
    if ${*$data}{'net_ftp_eof'};

  # for some reason if we continously open RETR connections and not
  # read a single byte, then abort them after a while the server will
  # close our connection, this prevents the unexpected EOF on the
  # command channel -- GMB
  if (exists ${*$data}{'net_ftp_bytesread'}
    && (${*$data}{'net_ftp_bytesread'} == 0))
  {
    my $buf     = "";
    my $timeout = $data->timeout;
    $data->can_read($timeout) && sysread($data, $buf, 1);
  }

  ${*$data}{'net_ftp_eof'} = 1;    # fake

  $ftp->abort;                     # this will close me
}


sub _close {
  my $data = shift;
  my $ftp  = ${*$data}{'net_ftp_cmd'};

  $data->SUPER::close();

  delete ${*$ftp}{'net_ftp_dataconn'}
    if exists ${*$ftp}{'net_ftp_dataconn'}
    && $data == ${*$ftp}{'net_ftp_dataconn'};
}


sub close {
  my $data = shift;
  my $ftp  = ${*$data}{'net_ftp_cmd'};

  if (exists ${*$data}{'net_ftp_bytesread'} && !${*$data}{'net_ftp_eof'}) {
    my $junk;
    $data->read($junk, 1, 0);
    return $data->abort unless ${*$data}{'net_ftp_eof'};
  }

  $data->_close;

  $ftp->response() == CMD_OK
    && $ftp->message =~ /unique file name:\s*(\S*)\s*\)/
    && (${*$ftp}{'net_ftp_unique'} = $1);

  $ftp->status == CMD_OK;
}


sub _select {
  my ($data, $timeout, $do_read) = @_;
  my ($rin, $rout, $win, $wout, $tout, $nfound);

  vec($rin = '', fileno($data), 1) = 1;

  ($win, $rin) = ($rin, $win) unless $do_read;

  while (1) {
    $nfound = select($rout = $rin, $wout = $win, undef, $tout = $timeout);

    last if $nfound >= 0;

    croak "select: $!"
      unless $!{EINTR};
  }

  $nfound;
}


sub can_read {
  _select(@_[0, 1], 1);
}


sub can_write {
  _select(@_[0, 1], 0);
}


sub cmd {
  my $ftp = shift;

  ${*$ftp}{'net_ftp_cmd'};
}


sub bytes_read {
  my $ftp = shift;

  ${*$ftp}{'net_ftp_bytesread'} || 0;
}

1;
