################################################################################
#
#  $Revision: 23 $
#  $Author: mhx $
#  $Date: 2007/10/19 20:46:32 +0200 $
#
################################################################################
#
#  Version 2.x, Copyright (C) 2007, Marcus Holland-Moritz <mhx@cpan.org>.
#  Version 1.x, Copyright (C) 1997, Graham Barr <gbarr@pobox.com>.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

package IPC::SysV;

use strict;
use vars qw(@ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $VERSION $XS_VERSION $AUTOLOAD);
use Carp;
use Config;

require Exporter;
@ISA = qw(Exporter);

$VERSION = do { my @r = '$Snapshot: /IPC-SysV/2.00 $' =~ /(\d+\.\d+(?:_\d+)?)/; @r ? $r[0] : '9.99' };
$XS_VERSION = $VERSION;
$VERSION = eval $VERSION;

# To support new constants, just add them to @EXPORT_OK
# and the C/XS code will be generated automagically.
@EXPORT_OK = (qw(

  GETALL GETNCNT GETPID GETVAL GETZCNT

  IPC_ALLOC IPC_CREAT IPC_EXCL IPC_GETACL IPC_INFO IPC_LOCKED
  IPC_M IPC_NOERROR IPC_NOWAIT IPC_PRIVATE IPC_R IPC_RMID
  IPC_SET IPC_SETACL IPC_SETLABEL IPC_STAT IPC_W IPC_WANTED

  MSG_EXCEPT MSG_FWAIT MSG_INFO MSG_LOCKED MSG_MWAIT MSG_NOERROR
  MSG_QWAIT MSG_R MSG_RWAIT MSG_STAT MSG_W MSG_WAIT MSG_WWAIT

  SEM_A SEM_ALLOC SEM_DEST SEM_ERR SEM_INFO SEM_ORDER SEM_R
  SEM_STAT SEM_UNDO

  SETALL SETVAL

  SHMLBA

  SHM_A SHM_CLEAR SHM_COPY SHM_DCACHE SHM_DEST SHM_ECACHE
  SHM_FMAP SHM_HUGETLB SHM_ICACHE SHM_INFO SHM_INIT SHM_LOCK
  SHM_LOCKED SHM_MAP SHM_NORESERVE SHM_NOSWAP SHM_R SHM_RDONLY
  SHM_REMAP SHM_REMOVED SHM_RND SHM_SHARE_MMU SHM_SHATTR
  SHM_SIZE SHM_STAT SHM_UNLOCK SHM_W

  S_IRUSR S_IWUSR S_IXUSR S_IRWXU
  S_IRGRP S_IWGRP S_IXGRP S_IRWXG
  S_IROTH S_IWOTH S_IXOTH S_IRWXO

  ENOSPC ENOSYS

), qw(

  ftok shmat shmdt memread memwrite

));

sub AUTOLOAD
{
  my $constname = $AUTOLOAD;
  $constname =~ s/.*:://;
  die "&IPC::SysV::_constant not defined" if $constname eq '_constant';
  my ($error, $val) = _constant($constname);
  if ($error) {
    my (undef, $file, $line) = caller;
    die "$error at $file line $line.\n";
  }
  {
    no strict 'refs';
    *$AUTOLOAD = sub { $val };
  }
  goto &$AUTOLOAD;
}

BOOT_XS: {
  # If I inherit DynaLoader then I inherit AutoLoader and I DON'T WANT TO
  require DynaLoader;

  # DynaLoader calls dl_load_flags as a static method.
  *dl_load_flags = DynaLoader->can('dl_load_flags');

  do {
    __PACKAGE__->can('bootstrap') || \&DynaLoader::bootstrap
  }->(__PACKAGE__, $XS_VERSION);
}

1;

__END__

=head1 NAME

IPC::SysV - System V IPC constants and system calls

=head1 SYNOPSIS

  use IPC::SysV qw(IPC_STAT IPC_PRIVATE);

=head1 DESCRIPTION

C<IPC::SysV> defines and conditionally exports all the constants
defined in your system include files which are needed by the SysV
IPC calls.  Common ones include

  IPC_CREATE IPC_EXCL IPC_NOWAIT IPC_PRIVATE IPC_RMID IPC_SET IPC_STAT
  GETVAL SETVAL GETPID GETNCNT GETZCNT GETALL SETALL
  SEM_A SEM_R SEM_UNDO
  SHM_RDONLY SHM_RND SHMLBA

and auxiliary ones

  S_IRUSR S_IWUSR S_IRWXU
  S_IRGRP S_IWGRP S_IRWXG
  S_IROTH S_IWOTH S_IRWXO

but your system might have more.

=over 4

=item ftok( PATH )

=item ftok( PATH, ID )

Return a key based on PATH and ID, which can be used as a key for
C<msgget>, C<semget> and C<shmget>. See L<ftok>.

If ID is omitted, it defaults to C<1>. If a single character is
given for ID, the numeric value of that character is used.

=item shmat( ID, ADDR, FLAG )

Attach the shared memory segment identified by ID to the address
space of the calling process. See L<shmat>.

ADDR should be C<undef> unless you really know what you're doing.

=item shmdt( ADDR )

Detach the shared memory segment located at the address specified
by ADDR from the address space of the calling process. See L<shmdt>.

=item memread( ADDR, VAR, POS, SIZE )

Reads SIZE bytes from a memory segment at ADDR starting at position POS.
VAR must be a variable that will hold the data read. Returns true if
successful, or false if there is an error. memread() taints the variable.

=item memwrite( ADDR, STRING, POS, SIZE )

Writes SIZE bytes from STRING to a memory segment at ADDR starting at
position POS. If STRING is too long, only SIZE bytes are used; if STRING
is too short, nulls are written to fill out SIZE bytes. Returns true if
successful, or false if there is an error.

=back

=head1 SEE ALSO

L<IPC::Msg>, L<IPC::Semaphore>, L<IPC::SharedMem>, L<ftok>, L<shmat>, L<shmdt>

=head1 AUTHORS

Graham Barr <gbarr@pobox.com>,
Jarkko Hietaniemi <jhi@iki.fi>,
Marcus Holland-Moritz <mhx@cpan.org>

=head1 COPYRIGHT

Version 2.x, Copyright (C) 2007, Marcus Holland-Moritz.

Version 1.x, Copyright (c) 1997, Graham Barr.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut

