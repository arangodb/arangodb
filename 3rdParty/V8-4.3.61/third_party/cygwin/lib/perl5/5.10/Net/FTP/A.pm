## 
## Package to read/write on ASCII data connections
##

package Net::FTP::A;
use strict;
use vars qw(@ISA $buf $VERSION);
use Carp;

require Net::FTP::dataconn;

@ISA     = qw(Net::FTP::dataconn);
$VERSION = "1.18";


sub read {
  my $data = shift;
  local *buf = \$_[0];
  shift;
  my $size = shift || croak 'read($buf,$size,[$offset])';
  my $timeout = @_ ? shift: $data->timeout;

  if (length(${*$data}) < $size && !${*$data}{'net_ftp_eof'}) {
    my $blksize = ${*$data}{'net_ftp_blksize'};
    $blksize = $size if $size > $blksize;

    my $l = 0;
    my $n;

  READ:
    {
      my $readbuf = defined(${*$data}{'net_ftp_cr'}) ? "\015" : '';

      $data->can_read($timeout)
        or croak "Timeout";

      if ($n = sysread($data, $readbuf, $blksize, length $readbuf)) {
        ${*$data}{'net_ftp_bytesread'} += $n;
        ${*$data}{'net_ftp_cr'} =
          substr($readbuf, -1) eq "\015"
          ? chop($readbuf)
          : undef;
      }
      else {
        return undef
          unless defined $n;

        ${*$data}{'net_ftp_eof'} = 1;
      }

      $readbuf =~ s/\015\012/\n/sgo;
      ${*$data} .= $readbuf;

      unless (length(${*$data})) {

        redo READ
          if ($n > 0);

        $size = length(${*$data})
          if ($n == 0);
      }
    }
  }

  $buf = substr(${*$data}, 0, $size);
  substr(${*$data}, 0, $size) = '';

  length $buf;
}


sub write {
  my $data = shift;
  local *buf = \$_[0];
  shift;
  my $size = shift || croak 'write($buf,$size,[$timeout])';
  my $timeout = @_ ? shift: $data->timeout;

  my $nr = (my $tmp = substr($buf, 0, $size)) =~ tr/\r\n/\015\012/;
  $tmp =~ s/([^\015])\012/$1\015\012/sg if $nr;
  $tmp =~ s/^\012/\015\012/ unless ${*$data}{'net_ftp_outcr'};
  ${*$data}{'net_ftp_outcr'} = substr($tmp, -1) eq "\015";

  # If the remote server has closed the connection we will be signal'd
  # when we write. This can happen if the disk on the remote server fills up

  local $SIG{PIPE} = 'IGNORE'
    unless ($SIG{PIPE} || '') eq 'IGNORE'
    or $^O eq 'MacOS';

  my $len   = length($tmp);
  my $off   = 0;
  my $wrote = 0;

  my $blksize = ${*$data}{'net_ftp_blksize'};

  while ($len) {
    $data->can_write($timeout)
      or croak "Timeout";

    $off += $wrote;
    $wrote = syswrite($data, substr($tmp, $off), $len > $blksize ? $blksize : $len);
    return undef
      unless defined($wrote);
    $len -= $wrote;
  }

  $size;
}

1;
