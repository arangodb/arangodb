
require 5;
package Pod::Perldoc::ToNroff;
use strict;
use warnings;

# This is unlike ToMan.pm in that it emits the raw nroff source!

use base qw(Pod::Perldoc::BaseTo);

sub is_pageable        { 1 }  # well, if you ask for it...
sub write_with_binmode { 0 }
sub output_extension   { 'man' }

use Pod::Man ();

sub center          { shift->_perldoc_elem('center'         , @_) }
sub date            { shift->_perldoc_elem('date'           , @_) }
sub fixed           { shift->_perldoc_elem('fixed'          , @_) }
sub fixedbold       { shift->_perldoc_elem('fixedbold'      , @_) }
sub fixeditalic     { shift->_perldoc_elem('fixeditalic'    , @_) }
sub fixedbolditalic { shift->_perldoc_elem('fixedbolditalic', @_) }
sub quotes          { shift->_perldoc_elem('quotes'         , @_) }
sub release         { shift->_perldoc_elem('release'        , @_) }
sub section         { shift->_perldoc_elem('section'        , @_) }

sub new { return bless {}, ref($_[0]) || $_[0] }

sub parse_from_file {
  my $self = shift;
  my $file = $_[0];
  
  my @options =
    map {; $_, $self->{$_} }
      grep !m/^_/s,
        keys %$self
  ;
  
  defined(&Pod::Perldoc::DEBUG)
   and Pod::Perldoc::DEBUG()
   and print "About to call new Pod::Man ",
    $Pod::Man::VERSION ? "(v$Pod::Man::VERSION) " : '',
    "with options: ",
    @options ? "[@options]" : "(nil)", "\n";
  ;

  Pod::Man->new(@options)->parse_from_file(@_);
}

1;
__END__

=head1 NAME

Pod::Perldoc::ToNroff - let Perldoc convert Pod to nroff

=head1 SYNOPSIS

  perldoc -o nroff -d something.3 Some::Modulename

=head1 DESCRIPTION

This is a "plug-in" class that allows Perldoc to use
Pod::Man as a formatter class.

The following options are supported:  center, date, fixed, fixedbold,
fixeditalic, fixedbolditalic, quotes, release, section

Those options are explained in L<Pod::Man>.

For example:

  perldoc -o nroff -w center:Pod -d something.3 Some::Modulename

=head1 CAVEAT

This module may change to use a different pod-to-nroff formatter class
in the future, and this may change what options are supported.

=head1 SEE ALSO

L<Pod::Man>, L<Pod::Perldoc>, L<Pod::Perldoc::ToMan>

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

