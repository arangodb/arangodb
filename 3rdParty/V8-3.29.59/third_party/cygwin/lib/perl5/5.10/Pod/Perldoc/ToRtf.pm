
require 5;
package Pod::Perldoc::ToRtf;
use strict;
use warnings;
use vars qw($VERSION);

use base qw( Pod::Simple::RTF );

$VERSION   # so that ->VERSION is happy
# stop CPAN from seeing this
 =
$Pod::Simple::RTF::VERSION;


sub is_pageable        { 0 }
sub write_with_binmode { 0 }
sub output_extension   { 'rtf' }

sub page_for_perldoc {
  my($self, $tempfile, $perldoc) = @_;
  return unless $perldoc->IS_MSWin32;
  
  my $rtf_pager = $ENV{'RTFREADER'} || 'write.exe';
  
  $perldoc->aside( "About to launch <\"$rtf_pager\" \"$tempfile\">\n" );
  
  return 1 if system( qq{"$rtf_pager"}, qq{"$tempfile"} ) == 0;
  return 0;
}

1;
__END__

=head1 NAME

Pod::Perldoc::ToRtf - let Perldoc render Pod as RTF

=head1 SYNOPSIS

  perldoc -o rtf Some::Modulename

=head1 DESCRIPTION

This is a "plug-in" class that allows Perldoc to use
Pod::Simple::RTF as a formatter class.

This is actually a Pod::Simple::RTF subclass, and inherits
all its options.

You have to have Pod::Simple::RTF installed (from the Pod::Simple dist),
or this module won't work.

If Perldoc is running under MSWin and uses this class as a formatter,
the output will be opened with F<write.exe> or whatever program is
specified in the environment variable C<RTFREADER>. For example, to
specify that RTF files should be opened the same as they are when you
double-click them, you would do C<set RTFREADER=start.exe> in your
F<autoexec.bat>.

Handy tip: put C<set PERLDOC=-ortf> in your F<autoexec.bat>
and that will set this class as the default formatter to run when
you do C<perldoc whatever>.

=head1 SEE ALSO

L<Pod::Simple::RTF>, L<Pod::Simple>, L<Pod::Perldoc>

=head1 COPYRIGHT AND DISCLAIMERS

Copyright (c) 2002 Sean M. Burke.  All rights reserved.

This library is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

This program is distributed in the hope that it will be useful, but
without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.

=head1 AUTHOR

Sean M. Burke C<sburke@cpan.org>

=cut

