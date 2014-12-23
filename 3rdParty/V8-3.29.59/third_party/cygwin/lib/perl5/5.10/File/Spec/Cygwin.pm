package File::Spec::Cygwin;

use strict;
use vars qw(@ISA $VERSION);
require File::Spec::Unix;

$VERSION = '3.2702';

@ISA = qw(File::Spec::Unix);

=head1 NAME

File::Spec::Cygwin - methods for Cygwin file specs

=head1 SYNOPSIS

 require File::Spec::Cygwin; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See L<File::Spec> and L<File::Spec::Unix>.  This package overrides the
implementation of these methods, not the semantics.

This module is still in beta.  Cygwin-knowledgeable folks are invited
to offer patches and suggestions.

=cut

=pod

=over 4

=item canonpath

Any C<\> (backslashes) are converted to C</> (forward slashes),
and then File::Spec::Unix canonpath() is called on the result.

=cut

sub canonpath {
    my($self,$path) = @_;
    return unless defined $path;

    $path =~ s|\\|/|g;

    # Handle network path names beginning with double slash
    my $node = '';
    if ( $path =~ s@^(//[^/]+)(?:/|\z)@/@s ) {
        $node = $1;
    }
    return $node . $self->SUPER::canonpath($path);
}

sub catdir {
    my $self = shift;
    return unless @_;

    # Don't create something that looks like a //network/path
    if ($_[0] and ($_[0] eq '/' or $_[0] eq '\\')) {
        shift;
        return $self->SUPER::catdir('', @_);
    }

    $self->SUPER::catdir(@_);
}

=pod

=item file_name_is_absolute

True is returned if the file name begins with C<drive_letter:>,
and if not, File::Spec::Unix file_name_is_absolute() is called.

=cut


sub file_name_is_absolute {
    my ($self,$file) = @_;
    return 1 if $file =~ m{^([a-z]:)?[\\/]}is; # C:/test
    return $self->SUPER::file_name_is_absolute($file);
}

=item tmpdir (override)

Returns a string representation of the first existing directory
from the following list:

    $ENV{TMPDIR}
    /tmp
    $ENV{'TMP'}
    $ENV{'TEMP'}
    C:/temp

Since Perl 5.8.0, if running under taint mode, and if the environment
variables are tainted, they are not used.

=cut

my $tmpdir;
sub tmpdir {
    return $tmpdir if defined $tmpdir;
    $tmpdir = $_[0]->_tmpdir( $ENV{TMPDIR}, "/tmp", $ENV{'TMP'}, $ENV{'TEMP'}, 'C:/temp' );
}

=item case_tolerant

Override Unix. Cygwin case-tolerance depends on managed mount settings and
as with MSWin32 on GetVolumeInformation() $ouFsFlags == FS_CASE_SENSITIVE,
indicating the case significance when comparing file specifications.
Since XP FS_CASE_SENSITIVE is effectively disabled for the NT subsystem.
See http://cygwin.com/ml/cygwin/2007-07/msg00891.html
Accepts an optional drive-mount argument.
Default: 1

=cut

my %tmp_case_tolerant;
sub case_tolerant () {
  return 1 unless $^O eq 'cygwin'
    and defined &Cygwin::mount_flags;

  my $drive = shift;
  $drive = shift if $drive =~ /^File::Spec/;
  my $windrive;
  if (! $drive) {
      $windrive = $ENV{SYSTEMDRIVE} || substr($ENV{WINDIR}, 0, 2);
      $drive = Cygwin::win_to_posix_path($windrive."\\");
  }
  return $tmp_case_tolerant{$drive} if exists $tmp_case_tolerant{$drive};
  my $mntopts = Cygwin::mount_flags($drive);
  if ($mntopts and ($mntopts =~ /,managed/)) {
    $tmp_case_tolerant{$drive} = 0;
    return 0;
  }
  require File::Spec::Win32;
  $windrive = substr(Cygwin::posix_to_win_path($drive),0,2);
  $tmp_case_tolerant{$drive} = File::Spec::Win32::case_tolerant($windrive);
  return $tmp_case_tolerant{$drive};
}

=back

=head1 COPYRIGHT

Copyright (c) 2004,2007 by the Perl 5 Porters.  All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

1;
