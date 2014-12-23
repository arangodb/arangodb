
require 5;
package Pod::Simple::Debug;
use strict;

sub import {
  my($value,$variable);
  
  if(@_ == 2) {
    $value = $_[1];
  } elsif(@_ == 3) {
    ($variable, $value) = @_[1,2];
    
    ($variable, $value) = ($value, $variable)
       if     defined $value    and ref($value)    eq 'SCALAR'
      and not(defined $variable and ref($variable) eq 'SCALAR')
    ; # tolerate getting it backwards
    
    unless( defined $variable and ref($variable) eq 'SCALAR') {
      require Carp;
      Carp::croak("Usage:\n use Pod::Simple::Debug (NUMVAL)\nor"
                . "\n use Pod::Simple::Debug (\\\$var, STARTNUMVAL)\nAborting");
    }
  } else {
    require Carp;
    Carp::croak("Usage:\n use Pod::Simple::Debug (NUMVAL)\nor"
                    . "\n use Pod::Simple::Debug (\\\$var, STARTNUMVAL)\nAborting");
  }

  if( defined &Pod::Simple::DEBUG ) {
    require Carp;
    Carp::croak("It's too late to call Pod::Simple::Debug -- "
              . "Pod::Simple has already loaded\nAborting");
  }
  
  $value = 0 unless defined $value;

  unless($value =~ m/^-?\d+$/) {
    require Carp;
    Carp::croak( "$value isn't a numeric value."
            . "\nUsage:\n use Pod::Simple::Debug (NUMVAL)\nor"
                    . "\n use Pod::Simple::Debug (\\\$var, STARTNUMVAL)\nAborting");
  }

  if( defined $variable ) {
    # make a not-really-constant
    *Pod::Simple::DEBUG = sub () { $$variable } ;
    $$variable = $value;
    print "# Starting Pod::Simple::DEBUG = non-constant $variable with val $value\n";
  } else {
    *Pod::Simple::DEBUG = eval " sub () { $value } ";
    print "# Starting Pod::Simple::DEBUG = $value\n";
  }
  
  require Pod::Simple;
  return;
}

1;


__END__

=head1 NAME

Pod::Simple::Debug -- put Pod::Simple into trace/debug mode

=head1 SYNOPSIS

 use Pod::Simple::Debug (5);  # or some integer

Or:

 my $debuglevel;
 use Pod::Simple::Debug (\$debuglevel, 0);
 ...some stuff that uses Pod::Simple to do stuff, but which
  you don't want debug output from...

 $debug_level = 4;
 ...some stuff that uses Pod::Simple to do stuff, but which
  you DO want debug output from...

 $debug_level = 0;

=head1 DESCRIPTION

This is an internal module for controlling the debug level (a.k.a. trace
level) of Pod::Simple.  This is of interest only to Pod::Simple
developers.


=head1 CAVEATS

Note that you should load this module I<before> loading Pod::Simple (or
any Pod::Simple-based class).  If you try loading Pod::Simple::Debug
after &Pod::Simple::DEBUG is already defined, Pod::Simple::Debug will
throw a fatal error to the effect that
"it's s too late to call Pod::Simple::Debug".

Note that the C<use Pod::Simple::Debug (\$x, I<somenum>)> mode will make
Pod::Simple (et al) run rather slower, since &Pod::Simple::DEBUG won't
be a constant sub anymore, and so Pod::Simple (et al) won't compile with
constant-folding.


=head1 GUTS

Doing this:

  use Pod::Simple::Debug (5);  # or some integer

is basically equivalent to:

  BEGIN { sub Pod::Simple::DEBUG () {5} }  # or some integer
  use Pod::Simple ();

And this:

  use Pod::Simple::Debug (\$debug_level,0);  # or some integer

is basically equivalent to this:

  my $debug_level;
  BEGIN { $debug_level = 0 }
  BEGIN { sub Pod::Simple::DEBUG () { $debug_level }
  use Pod::Simple ();

=head1 SEE ALSO

L<Pod::Simple>

The article "Constants in Perl", in I<The Perl Journal> issue
21.  See L<http://www.sysadminmag.com/tpj/issues/vol5_5/>

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

