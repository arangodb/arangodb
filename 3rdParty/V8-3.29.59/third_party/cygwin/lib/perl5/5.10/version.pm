#!perl -w
package version;

use 5.005_04;
use strict;

use vars qw(@ISA $VERSION $CLASS *qv);

$VERSION = 0.74;

$CLASS = 'version';

# Preloaded methods go here.
sub import {
    my ($class) = @_;
    my $callpkg = caller();
    no strict 'refs';
    
    *{$callpkg."::qv"} = 
	    sub {return bless version::qv(shift), $class }
	unless defined (&{"$callpkg\::qv"});

}

1;
