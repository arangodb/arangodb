################################################################################
#
#  $Revision: 18 $
#  $Author: mhx $
#  $Date: 2007/10/15 20:29:08 +0200 $
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

package IPC::Semaphore;

use IPC::SysV qw(GETNCNT GETZCNT GETVAL SETVAL GETPID GETALL SETALL
		 IPC_STAT IPC_SET IPC_RMID);
use strict;
use vars qw($VERSION);
use Carp;

$VERSION = do { my @r = '$Snapshot: /IPC-SysV/2.00 $' =~ /(\d+\.\d+(?:_\d+)?)/; @r ? $r[0] : '9.99' };
$VERSION = eval $VERSION;

# Figure out if we have support for native sized types
my $N = do { my $foo = eval { pack "L!", 0 }; $@ ? '' : '!' };

{
    package IPC::Semaphore::stat;

    use Class::Struct qw(struct);

    struct 'IPC::Semaphore::stat' => [
	uid	=> '$',
	gid	=> '$',
	cuid	=> '$',
	cgid	=> '$',
	mode	=> '$',
	ctime	=> '$',
	otime	=> '$',
	nsems	=> '$',
    ];
}

sub new {
    @_ == 4 || croak 'new ' . __PACKAGE__ . '( KEY, NSEMS, FLAGS )';
    my $class = shift;

    my $id = semget($_[0],$_[1],$_[2]);

    defined($id)
	? bless \$id, $class
	: undef;
}

sub id {
    my $self = shift;
    $$self;
}

sub remove {
    my $self = shift;
    (semctl($$self,0,IPC_RMID,0), undef $$self)[0];
}

sub getncnt {
    @_ == 2 || croak '$sem->getncnt( SEM )';
    my $self = shift;
    my $sem = shift;
    my $v = semctl($$self,$sem,GETNCNT,0);
    $v ? 0 + $v : undef;
}

sub getzcnt {
    @_ == 2 || croak '$sem->getzcnt( SEM )';
    my $self = shift;
    my $sem = shift;
    my $v = semctl($$self,$sem,GETZCNT,0);
    $v ? 0 + $v : undef;
}

sub getval {
    @_ == 2 || croak '$sem->getval( SEM )';
    my $self = shift;
    my $sem = shift;
    my $v = semctl($$self,$sem,GETVAL,0);
    $v ? 0 + $v : undef;
}

sub getpid {
    @_ == 2 || croak '$sem->getpid( SEM )';
    my $self = shift;
    my $sem = shift;
    my $v = semctl($$self,$sem,GETPID,0);
    $v ? 0 + $v : undef;
}

sub op {
    @_ >= 4 || croak '$sem->op( OPLIST )';
    my $self = shift;
    croak 'Bad arg count' if @_ % 3;
    my $data = pack("s$N*",@_);
    semop($$self,$data);
}

sub stat {
    my $self = shift;
    my $data = "";
    semctl($$self,0,IPC_STAT,$data)
	or return undef;
    IPC::Semaphore::stat->new->unpack($data);
}

sub set {
    my $self = shift;
    my $ds;

    if(@_ == 1) {
	$ds = shift;
    }
    else {
	croak 'Bad arg count' if @_ % 2;
	my %arg = @_;
	$ds = $self->stat
		or return undef;
	my($key,$val);
	$ds->$key($val)
	    while(($key,$val) = each %arg);
    }

    my $v = semctl($$self,0,IPC_SET,$ds->pack);
    $v ? 0 + $v : undef;
}

sub getall {
    my $self = shift;
    my $data = "";
    semctl($$self,0,GETALL,$data)
	or return ();
    (unpack("s$N*",$data));
}

sub setall {
    my $self = shift;
    my $data = pack("s$N*",@_);
    semctl($$self,0,SETALL,$data);
}

sub setval {
    @_ == 3 || croak '$sem->setval( SEM, VAL )';
    my $self = shift;
    my $sem = shift;
    my $val = shift;
    semctl($$self,$sem,SETVAL,$val);
}

1;

__END__

=head1 NAME

IPC::Semaphore - SysV Semaphore IPC object class

=head1 SYNOPSIS

    use IPC::SysV qw(IPC_PRIVATE S_IRUSR S_IWUSR IPC_CREAT);
    use IPC::Semaphore;

    $sem = IPC::Semaphore->new(IPC_PRIVATE, 10, S_IRUSR | S_IWUSR | IPC_CREAT);

    $sem->setall( (0) x 10);

    @sem = $sem->getall;

    $ncnt = $sem->getncnt;

    $zcnt = $sem->getzcnt;

    $ds = $sem->stat;

    $sem->remove;

=head1 DESCRIPTION

A class providing an object based interface to SysV IPC semaphores.

=head1 METHODS

=over 4

=item new ( KEY , NSEMS , FLAGS )

Create a new semaphore set associated with C<KEY>. C<NSEMS> is the number
of semaphores in the set. A new set is created if

=over 4

=item *

C<KEY> is equal to C<IPC_PRIVATE>

=item *

C<KEY> does not already have a semaphore identifier
associated with it, and C<I<FLAGS> & IPC_CREAT> is true.

=back

On creation of a new semaphore set C<FLAGS> is used to set the
permissions.  Be careful not to set any flags that the Sys V
IPC implementation does not allow: in some systems setting
execute bits makes the operations fail.

=item getall

Returns the values of the semaphore set as an array.

=item getncnt ( SEM )

Returns the number of processes waiting for the semaphore C<SEM> to
become greater than its current value

=item getpid ( SEM )

Returns the process id of the last process that performed an operation
on the semaphore C<SEM>.

=item getval ( SEM )

Returns the current value of the semaphore C<SEM>.

=item getzcnt ( SEM )

Returns the number of processes waiting for the semaphore C<SEM> to
become zero.

=item id

Returns the system identifier for the semaphore set.

=item op ( OPLIST )

C<OPLIST> is a list of operations to pass to C<semop>. C<OPLIST> is
a concatenation of smaller lists, each which has three values. The
first is the semaphore number, the second is the operation and the last
is a flags value. See L<semop> for more details. For example

    $sem->op(
	0, -1, IPC_NOWAIT,
	1,  1, IPC_NOWAIT
    );

=item remove

Remove and destroy the semaphore set from the system.

=item set ( STAT )

=item set ( NAME => VALUE [, NAME => VALUE ...] )

C<set> will set the following values of the C<stat> structure associated
with the semaphore set.

    uid
    gid
    mode (only the permission bits)

C<set> accepts either a stat object, as returned by the C<stat> method,
or a list of I<name>-I<value> pairs.

=item setall ( VALUES )

Sets all values in the semaphore set to those given on the C<VALUES> list.
C<VALUES> must contain the correct number of values.

=item setval ( N , VALUE )

Set the C<N>th value in the semaphore set to C<VALUE>

=item stat

Returns an object of type C<IPC::Semaphore::stat> which is a sub-class of
C<Class::Struct>. It provides the following fields. For a description
of these fields see your system documentation.

    uid
    gid
    cuid
    cgid
    mode
    ctime
    otime
    nsems

=back

=head1 SEE ALSO

L<IPC::SysV>, L<Class::Struct>, L<semget>, L<semctl>, L<semop> 

=head1 AUTHORS

Graham Barr <gbarr@pobox.com>,
Marcus Holland-Moritz <mhx@cpan.org>

=head1 COPYRIGHT

Version 2.x, Copyright (C) 2007, Marcus Holland-Moritz.

Version 1.x, Copyright (c) 1997, Graham Barr.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
