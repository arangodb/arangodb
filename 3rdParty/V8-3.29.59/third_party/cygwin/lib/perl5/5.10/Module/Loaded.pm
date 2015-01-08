package Module::Loaded;

use strict;
use Carp qw[carp];

BEGIN { use base 'Exporter';
        use vars qw[@EXPORT $VERSION];
        
        $VERSION = '0.01';
        @EXPORT  = qw[mark_as_loaded mark_as_unloaded is_loaded];
}

=head1 NAME 

Module::Loaded - mark modules as loaded or unloaded

=head1 SYNOPSIS

    use Module::Loaded;
    
    $bool = mark_as_loaded('Foo');   # Foo.pm is now marked as loaded
    $loc  = is_loaded('Foo');        # location of Foo.pm set to the 
                                     # loaders location
    eval "require 'Foo'";            # is now a no-op

    $bool = mark_as_unloaded('Foo'); # Foo.pm no longer marked as loaded
    eval "require 'Foo'";            # Will try to find Foo.pm in @INC

=head1 DESCRIPTION

When testing applications, often you find yourself needing to provide
functionality in your test environment that would usually be provided
by external modules. Rather than munging the C<%INC> by hand to mark
these external modules as loaded, so they are not attempted to be loaded
by perl, this module offers you a very simple way to mark modules as
loaded and/or unloaded.

=head1 FUNCTIONS

=head2 $bool = mark_as_loaded( PACKAGE );

Marks the package as loaded to perl. C<PACKAGE> can be a bareword or
string.

If the module is already loaded, C<mark_as_loaded> will carp about
this and tell you from where the C<PACKAGE> has been loaded already.

=cut

sub mark_as_loaded (*) {
    my $pm      = shift;
    my $file    = __PACKAGE__->_pm_to_file( $pm ) or return;
    my $who     = [caller]->[1];
    
    my $where   = is_loaded( $pm );
    if ( defined $where ) {
        carp "'$pm' already marked as loaded ('$where')";
    
    } else {
        $INC{$file} = $who;
    }
    
    return 1;
}

=head2 $bool = mark_as_unloaded( PACKAGE );

Marks the package as unloaded to perl, which is the exact opposite 
of C<mark_as_loaded>. C<PACKAGE> can be a bareword or string.

If the module is already unloaded, C<mark_as_unloaded> will carp about
this and tell you the C<PACKAGE> has been unloaded already.

=cut

sub mark_as_unloaded (*) { 
    my $pm      = shift;
    my $file    = __PACKAGE__->_pm_to_file( $pm ) or return;

    unless( defined is_loaded( $pm ) ) {
        carp "'$pm' already marked as unloaded";

    } else {
        delete $INC{ $file };
    }
    
    return 1;
}

=head2 $loc = is_loaded( PACKAGE );

C<is_loaded> tells you if C<PACKAGE> has been marked as loaded yet.
C<PACKAGE> can be a bareword or string.

It returns falls if C<PACKAGE> has not been loaded yet and the location 
from where it is said to be loaded on success.

=cut

sub is_loaded (*) { 
    my $pm      = shift;
    my $file    = __PACKAGE__->_pm_to_file( $pm ) or return;

    return $INC{$file} if exists $INC{$file};
    
    return;
}


sub _pm_to_file {
    my $pkg = shift;
    my $pm  = shift or return;
    
    my $file = join '/', split '::', $pm;
    $file .= '.pm';
    
    return $file;
}    

=head1 AUTHOR

This module by
Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 COPYRIGHT

This module is
copyright (c) 2004-2005 Jos Boumans E<lt>kane@cpan.orgE<gt>.
All rights reserved.

This library is free software;
you may redistribute and/or modify it under the same
terms as Perl itself.

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

1;
