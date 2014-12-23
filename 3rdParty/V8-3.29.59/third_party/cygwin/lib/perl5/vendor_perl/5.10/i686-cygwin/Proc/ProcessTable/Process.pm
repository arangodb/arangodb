package Proc::ProcessTable::Process;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $AUTOLOAD);

require Exporter;
require AutoLoader;

@ISA = qw(Exporter AutoLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	
);
$VERSION = '0.02';


# Preloaded methods go here.
use Carp;
use File::Basename;

sub AUTOLOAD {
  my $self = shift;
  my $type = ref($self)
    or croak "$self is not an object";
  
  my $name = $AUTOLOAD;
  $name =~ s/.*://;		# strip fully-qualified portion
  
  unless (exists $self->{$name} ) {
    croak "Can't access `$name' field in class $type";
  }
  
  if (@_) {
    return $self->{$name} = shift;
  } else {
    return $self->{$name};
  }
}

########################################################
# Kill; just a wrapper for perl's kill at the moment
########################################################
sub kill {
  my ($self, $signal) = @_;
  return( kill($signal, $self->pid) );
}

########################################################
# Get/set accessors for priority and process group 
# (everything else is just a get, so handled by autoload)
#########################################################

# Hmmm... These could use the perl functions to get if not stored on the object
sub priority {
  my ($self, $priority) = @_;
  if( defined($priority) ){
    setpriority(0, $self->pid, $priority);
    if( getpriority(0, $self->pid) == $priority ){  # Yuck; getpriority doesn't return a status
      $self->{priority} = $priority;
    }
  }
  return $self->{priority};
}

sub pgrp {
  my ($self, $pgrp) = @_;
  if( defined($pgrp) ){
    setpgrp($self->pid, $pgrp);
    if( getpgrp($self->pid) == $pgrp ){ # Ditto setpgrp
      $self->{pgrp} = $pgrp;
    }
  }
  return $self->{pgrp};
}


# Apparently needed for mod_perl
sub DESTROY {}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

Proc::ProcessTable::Process - Perl process objects

=head1 SYNOPSIS

 $process->kill(9);
 $process->priority(19);
 $process->pgrp(500);
 $uid = $process->uid;
 ...

=head1 DESCRIPTION

This is a stub module to provide OO process attribute access for
Proc::ProcessTable. Proc::ProcessTable::Process objects are
constructed directly by Proc::ProcessTable; there is no constructor
method, only accessors.

=head1 METHODS

=over 4

=item kill

Sends a signal to the process; just an aesthetic wrapper for perl's
kill. Takes the signal (name or number) as an argument. Returns number
of processes signalled.

=item priority

Get/set accessor; if called with a numeric argument, attempts to reset
the process's priority to that number using perl's <B>setpriority
function. Returns the process priority.

=item pgrp

Same as above for the process group.

=item all other methods...

are simple accessors that retrieve the process attributes for which
they are named. Currently supported are:

  uid         UID of process
  gid         GID of process
  euid        effective UID of process           (Solaris only)
  egid        effective GID of process           (Solaris only)
  pid         process ID
  ppid        parent process ID
  spid        sprod ID                           (IRIX only)
  pgrp        process group
  sess        session ID
  cpuid       CPU ID of processor running on     (IRIX only)
  priority    priority of process
  ttynum      tty number of process
  flags       flags of process
  minflt      minor page faults                  (Linux only)
  cminflt     child minor page faults            (Linux only)
  majflt      major page faults                  (Linux only)
  cmajflt     child major page faults            (Linux only)
  utime       user mode time (1/100s of seconds) (Linux only)
  stime       kernel mode time                   (Linux only)
  cutime      child utime                        (Linux only)
  cstime      child stime                        (Linux only)
  time        user + system time                 
  ctime       child user + system time
  timensec    user + system nanoseconds part	 (Solaris only)
  ctimensec   child user + system nanoseconds    (Solaris only)
  qtime       cumulative cpu time                (IRIX only)
  size        virtual memory size (bytes)
  rss         resident set size (bytes)
  wchan       address of current system call 
  fname       file name
  start       start time (seconds since the epoch)
  pctcpu      percent cpu used since process started
  state       state of process
  pctmem      percent memory			 
  cmndline    full command line of process
  ttydev      path of process's tty
  clname      scheduling class name              (IRIX only)

See the "README.osname" files in the distribution for more
up-to-date information. 

=back

=head1 AUTHOR

D. Urist, durist@frii.com

=head1 SEE ALSO

Proc::ProcessTable.pm, perl(1).

=cut
