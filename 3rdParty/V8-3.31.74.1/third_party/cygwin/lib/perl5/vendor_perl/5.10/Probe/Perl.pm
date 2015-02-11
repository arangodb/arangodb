package Probe::Perl;

use vars qw( $VERSION );
$VERSION = '0.01';

use strict;

# TODO: cache values derived from launching an external perl process
# TODO: docs refer to Config.pm and $self->{config}


use Config;
use File::Spec;

sub new {
  my $class = shift;
  my $data  = shift || {};
  return bless( $data, $class );
}

sub config {
  my ($self, $key) = (shift, shift);
  if (@_) {
    unless (ref $self) {
      die "Can't set config values via $self->config().  Use $self->new() to create a local view";
    }
    $self->{$key} = shift;
  }
  return ref($self) && exists $self->{$key} ? $self->{$key} : $Config{$key};
}

sub config_revert {
  my $self = shift;
  die "Can't use config_revert() as a class method" unless ref($self);
  
  delete $self->{$_} foreach @_;
}

sub perl_version {
  my $self = shift;
  # Check the current perl interpreter
  # It's much more convenient to use $] here than $^V, but 'man
  # perlvar' says I'm not supposed to.  Bloody tyrant.
  return $^V ? $self->perl_version_to_float(sprintf( "%vd", $^V )) : $];
}

sub perl_version_to_float {
  my ($self, $version) = @_;
  $version =~ s/\./../;  # Double up the first dot so the output has one dot remaining
  $version =~ s/\.(\d+)/sprintf( '%03d', $1 )/eg;
  return $version;
}

sub perl_is_same {
  my ($self, $perl) = @_;
  return `$perl -MConfig=myconfig -e print -e myconfig` eq Config->myconfig;
}

sub find_perl_interpreter {
  my $self = shift;

  return $^X if File::Spec->file_name_is_absolute($^X);

  my $exe = $self->config('exe_ext');

  my $thisperl = $^X;
  if ($self->os_type eq 'VMS') {
    # VMS might have a file version at the end
    $thisperl .= $exe unless $thisperl =~ m/$exe(;\d+)?$/i;
  } elsif (defined $exe) {
    $thisperl .= $exe unless $thisperl =~ m/$exe$/i;
  }

  foreach my $perl ( $self->config('perlpath'),
		     map( File::Spec->catfile($_, $thisperl),
			  File::Spec->path() )
		   ) {
    return $perl if -f $perl and $self->perl_is_same($perl);
  }
  return;
}

# Determine the default @INC for this Perl
sub perl_inc {
  my $self = shift;

  local $ENV{PERL5LIB};  # this is not considered part of the default.

  my $perl = $self->find_perl_interpreter();

  my @inc = `$perl -l -e print -e for -e \@INC`;
  chomp @inc;

  return @inc;
}


{
  my %OSTYPES = qw(
		   aix       Unix
		   bsdos     Unix
		   dgux      Unix
		   dynixptx  Unix
		   freebsd   Unix
		   linux     Unix
		   hpux      Unix
		   irix      Unix
		   darwin    Unix
		   machten   Unix
		   next      Unix
		   openbsd   Unix
		   netbsd    Unix
		   dec_osf   Unix
		   svr4      Unix
		   svr5      Unix
		   sco_sv    Unix
		   unicos    Unix
		   unicosmk  Unix
		   solaris   Unix
		   sunos     Unix
		   cygwin    Unix
		   os2       Unix

		   dos       Windows
		   MSWin32   Windows

		   os390     EBCDIC
		   os400     EBCDIC
		   posix-bc  EBCDIC
		   vmesa     EBCDIC

		   MacOS     MacOS
		   VMS       VMS
		   VOS       VOS
		   riscos    RiscOS
		   amigaos   Amiga
		   mpeix     MPEiX
		  );


  sub os_type {
    my $class  = shift;
    return $OSTYPES{shift || $^O};
  }
}


1;

__END__


=head1 NAME

Probe::Perl - Information about the currently running perl

=head1 SYNOPSIS

 use Probe::Perl;
 $p = Probe::Perl->new();
 
 # Version of this perl as a floating point number
 $ver = $p->perl_version();
 $ver = Probe::Perl->perl_version();
 
 # Convert a multi-dotted string to a floating point number
 $ver = $p->perl_version_to_float($ver);
 $ver = Probe::Perl->perl_version_to_float($ver);
 
 # Check if the given perl is the same as the one currently running
 $bool = $p->perl_is_same($perl_path);
 $bool = Probe::Perl->perl_is_same($perl_path);
 
 # Find a path to the currently-running perl
 $path = $p->find_perl_interpreter();
 $path = Probe::Perl->find_perl_interpreter();
 
 # Get @INC before run-time additions
 @paths = $p->perl_inc();
 @paths = Probe::Perl->perl_inc();
 
 # Get the general type of operating system
 $type = $p->os_type();
 $type = Probe::Perl->os_type();
 
 # Access Config.pm values
 $val = $p->config('foo');
 $val = Probe::Perl->config('foo');
 $p->config('foo' => 'bar');  # Set locally
 $p->config_revert('foo');  # Revert

=head1 DESCRIPTION

This module provides methods for obtaining information about the
currently running perl interpreter.  It originally began life as code
in the C<Module::Build> project, but has been externalized here for
general use.

=head1 METHODS

=over 4

=item new()

Creates a new Probe::Perl object and returns it.  Most methods in
the Probe::Perl packages are available as class methods, so you
don't always need to create a new object.  But if you want to create a
mutable view of the C<Config.pm> data, it's necessary to create an
object to store the values in.

=item config( $key [, $value] )

Returns the C<Config.pm> value associated with C<$key>.  If C<$value>
is also specified, then the value is set to C<$value> for this view of
the data.  In this case, C<config()> must be called as an object
method, not a class method.

=item config_revert( $key )

Removes any user-assigned value in this view of the C<Config.pm> data.

=item find_perl_interpreter( )

Returns the absolute path of this perl interpreter.  This is actually
sort of a tricky thing to discover sometimes - in these cases we use
C<perl_is_same()> to verify.

=item perl_version( )

Returns the version of this perl interpreter as a perl-styled version
number using C<perl_version_to_float()>.  Uses C<$^V> if your perl is
recent enough, otherwise uses C<$]>.

=item perl_version_to_float( $version )

Formats C<$version> as a perl-styled version number like C<5.008001>.

=item perl_is_same( $perl )

Given the name of a perl interpreter, this method determines if it has
the same configuration as the one represented by the current perl
instance.  Usually this means it's exactly the same

=item perl_inc( )

Returns a list of directories in this perl's C<@INC> path, I<before>
any entries from C<use lib>, C<$ENV{PERL5LIB}>, or C<-I> switches are
added.

=item os_type( [$osname] )

Returns a generic OS type (e.g. "Unix", "Windows", "MacOS") for the
given OS name. If no OS name is given it uses the value in $^O, which
is the same as $Config{osname}.

=back

=head1 AUTHOR

Randy W. Sims <randys@thepierianspring.org>

Based partly on code from the Module::Build project, by Ken Williams
<kwilliams@cpan.org> and others.

=head1 COPYRIGHT

Copyright 2005 Ken Williams and Randy Sims.  All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
