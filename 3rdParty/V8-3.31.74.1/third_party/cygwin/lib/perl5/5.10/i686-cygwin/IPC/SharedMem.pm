################################################################################
#
#  $Revision: 2 $
#  $Author: mhx $
#  $Date: 2007/10/14 05:16:08 +0200 $
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

package IPC::SharedMem;

use IPC::SysV qw(IPC_STAT IPC_RMID shmat shmdt memread memwrite);
use strict;
use vars qw($VERSION);
use Carp;

$VERSION = do { my @r = '$Snapshot: /IPC-SysV/2.00 $' =~ /(\d+\.\d+(?:_\d+)?)/; @r ? $r[0] : '9.99' };
$VERSION = eval $VERSION;

# Figure out if we have support for native sized types
my $N = do { my $foo = eval { pack "L!", 0 }; $@ ? '' : '!' };

{
    package IPC::SharedMem::stat;

    use Class::Struct qw(struct);

    struct 'IPC::SharedMem::stat' => [
	uid	=> '$',
	gid	=> '$',
	cuid	=> '$',
	cgid	=> '$',
	mode	=> '$',
	segsz	=> '$',
	lpid	=> '$',
	cpid	=> '$',
	nattch	=> '$',
	atime	=> '$',
	dtime	=> '$',
	ctime	=> '$',
    ];
}

sub new
{
  @_ == 4 or croak 'IPC::SharedMem->new(KEY, SIZE, FLAGS)';
  my($class, $key, $size, $flags) = @_;

  my $id = shmget $key, $size, $flags or return undef;

  bless { _id => $id, _addr => undef, _isrm => 0 }, $class
}

sub id
{
  my $self = shift;
  $self->{_id};
}

sub addr
{
  my $self = shift;
  $self->{_addr};
}

sub stat
{
  my $self = shift;
  my $data = '';
  shmctl $self->id, IPC_STAT, $data or return undef;
  IPC::SharedMem::stat->new->unpack($data);
}

sub attach
{
  @_ >= 1 && @_ <= 2 or croak '$shm->attach([FLAG])';
  my($self, $flag) = @_;
  defined $self->addr and return undef;
  $self->{_addr} = shmat($self->id, undef, $flag || 0);
  defined $self->addr;
}

sub detach
{
  my $self = shift;
  defined $self->addr or return undef;
  my $rv = defined shmdt($self->addr);
  undef $self->{_addr} if $rv;
  $rv;
}

sub remove
{
  my $self = shift;
  return undef if $self->is_removed;
  my $rv = shmctl $self->id, IPC_RMID, 0;
  $self->{_isrm} = 1 if $rv;
  return $rv;
}

sub is_removed
{
  my $self = shift;
  $self->{_isrm};
}

sub read
{
  @_ == 3 or croak '$shm->read(POS, SIZE)';
  my($self, $pos, $size) = @_;
  my $buf = '';
  if (defined $self->addr) {
    memread($self->addr, $buf, $pos, $size) or return undef;
  }
  else {
    shmread($self->id, $buf, $pos, $size) or return undef;
  }
  $buf;
}

sub write
{
  @_ == 4 or croak '$shm->write(STRING, POS, SIZE)';
  my($self, $str, $pos, $size) = @_;
  if (defined $self->addr) {
    return memwrite($self->addr, $str, $pos, $size);
  }
  else {
    return shmwrite($self->id, $str, $pos, $size);
  }
}

1;

__END__

=head1 NAME

IPC::SharedMem - SysV Shared Memory IPC object class

=head1 SYNOPSIS

    use IPC::SysV qw(IPC_PRIVATE S_IRUSR S_IWUSR);
    use IPC::SharedMem;

    $shm = IPC::SharedMem->new(IPC_PRIVATE, 8, S_IRWXU);

    $shm->write(pack("S", 4711), 2, 2);

    $data = $shm->read(0, 2);

    $ds = $shm->stat;

    $shm->remove;

=head1 DESCRIPTION

A class providing an object based interface to SysV IPC shared memory.

=head1 METHODS

=over 4

=item new ( KEY , SIZE , FLAGS )

Creates a new shared memory segment associated with C<KEY>. A new
segment is created if

=over 4

=item *

C<KEY> is equal to C<IPC_PRIVATE>

=item *

C<KEY> does not already have a shared memory segment associated
with it, and C<I<FLAGS> & IPC_CREAT> is true.

=back

On creation of a new shared memory segment C<FLAGS> is used to
set the permissions.  Be careful not to set any flags that the
Sys V IPC implementation does not allow: in some systems setting
execute bits makes the operations fail.

=item id

Returns the shared memory identifier.

=item read ( POS, SIZE )

Read C<SIZE> bytes from the shared memory segment at C<POS>. Returns
the string read, or C<undef> if there was an error. The return value
becomes tainted. See L<shmread>.

=item write ( STRING, POS, SIZE )

Write C<SIZE> bytes to the shared memory segment at C<POS>. Returns
true if successful, or false if there is an error. See L<shmwrite>.

=item remove

Remove the shared memory segment from the system or mark it as
removed as long as any processes are still attached to it.

=item is_removed

Returns true if the shared memory segment has been removed or
marked for removal.

=item stat

Returns an object of type C<IPC::SharedMem::stat> which is a sub-class
of C<Class::Struct>. It provides the following fields. For a description
of these fields see you system documentation.

    uid
    gid
    cuid
    cgid
    mode
    segsz
    lpid
    cpid
    nattach
    atime
    dtime
    ctime

=item attach ( [FLAG] )

Permanently attach to the shared memory segment. When a C<IPC::SharedMem>
object is attached, it will use L<memread> and L<memwrite> instead of
L<shmread> and L<shmwrite> for accessing the shared memory segment.
Returns true if successful, or false on error. See L<shmat>.

=item detach

Detach from the shared memory segment that previously has been attached
to. Returns true if successful, or false on error. See L<shmdt>.

=item addr

Returns the address of the shared memory that has been attached to in a
format suitable for use with C<pack('P')>. Returns C<undef> if the shared
memory has not been attached.

=back

=head1 SEE ALSO

L<IPC::SysV>, L<Class::Struct>

=head1 AUTHORS

Marcus Holland-Moritz <mhx@cpan.org>

=head1 COPYRIGHT

Version 2.x, Copyright (C) 2007, Marcus Holland-Moritz.

Version 1.x, Copyright (c) 1997, Graham Barr.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut

