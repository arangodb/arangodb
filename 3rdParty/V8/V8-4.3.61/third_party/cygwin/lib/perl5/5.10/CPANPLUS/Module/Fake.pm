package CPANPLUS::Module::Fake;


use CPANPLUS::Error;
use CPANPLUS::Module;
use CPANPLUS::Module::Author::Fake;
use CPANPLUS::Internals;

use strict;
use vars            qw[@ISA];
use Params::Check   qw[check];

@ISA = qw[CPANPLUS::Module];
$Params::Check::VERBOSE = 1;

=pod

=head1 NAME

CPANPLUS::Module::Fake

=head1 SYNOPSIS

    my $obj = CPANPLUS::Module::Fake->new(
                module  => 'Foo',
                path    => 'ftp/path/to/foo',
                author  => CPANPLUS::Module::Author::Fake->new,
                package => 'fake-1.1.tgz',
                _id     => $cpan->_id,
            );

=head1 DESCRIPTION

A class for creating fake module objects, for shortcut use internally
by CPANPLUS.

Inherits from C<CPANPLUS::Module>.

=head1 METHODS

=head2 new( module => $mod, path => $path, package => $pkg, [_id => DIGIT] )

Creates a dummy module object from the above parameters. It can
take more options (same as C<< CPANPLUS::Module->new >> but the above
are required.

=cut

sub new {
    my $class = shift;
    my %hash  = @_;
    
    local $Params::Check::ALLOW_UNKNOWN = 1;
    
    my $tmpl = {
        module  => { required => 1 },
        path    => { required => 1 },
        package => { required => 1 },
        _id     => { default => CPANPLUS::Internals->_last_id },
        author  => { default => '' },
    };
    
    my $args = check( $tmpl, \%hash ) or return;
    
    $args->{author} ||= CPANPLUS::Module::Author::Fake->new( 
                                                        _id => $args->{_id} );
    
    my $obj = CPANPLUS::Module->new( %$args ) or return;
    
    unless( $obj->_id ) {
        error(loc("No '%1' specified -- No CPANPLUS object associated!",'_id'));
        return;
    }        
    
    ### rebless object ###
    return bless $obj, $class;                                   
}    

1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:    
