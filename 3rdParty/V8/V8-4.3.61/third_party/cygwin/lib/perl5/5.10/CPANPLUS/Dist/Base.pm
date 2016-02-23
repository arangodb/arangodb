package CPANPLUS::Dist::Base;

use strict;

use vars    qw[@ISA $VERSION];
@ISA =      qw[CPANPLUS::Dist];
$VERSION =  '0.01';

=head1 NAME

CPANPLUS::Dist::Base - Base class for custom distribution classes

=head1 SYNOPSIS

    package CPANPLUS::Dist::MY_IMPLEMENTATION

    use base 'CPANPLUS::Dist::Base';

    sub prepare {
        my $dist = shift;
        
        ### do the 'standard' things
        $dist->SUPER::prepare( @_ ) or return;
    
        ### do MY_IMPLEMENTATION specific things
        ...
        
        ### don't forget to set the status!
        return $dist->status->prepared( $SUCCESS ? 1 : 0 );
    }


=head1 DESCRIPTION

CPANPLUS::Dist::Base functions as a base class for all custom
distribution implementations. It does all the mundane work 
CPANPLUS would have done without a custom distribution, so you
can override just the parts you need to make your own implementation
work.

=head1 FLOW

Below is a brief outline when and in which order methods in this
class are called:

    $Class->format_available;   # can we use this class on this system?

    $dist->init;                # set up custom accessors, etc
    $dist->prepare;             # find/write meta information
    $dist->create;              # write the distribution file
    $dist->install;             # install the distribution file
    
    $dist->uninstall;           # remove the distribution (OPTIONAL)

=head1 METHODS

=cut


=head2 $bool = $Class->format_available

This method is called when someone requests a module to be installed
via the superclass. This gives you the opportunity to check if all
the needed requirements to build and install this distribution have
been met.

For example, you might need a command line program, or a certain perl
module installed to do your job. Now is the time to check.

Simply return true if the request can proceed and false if it can not.

The C<CPANPLUS::Dist::Base> implementation always returns true.

=cut 

sub format_available { return 1 }


=head2 $bool = $dist->init

This method is called just after the new dist object is set up and
before the C<prepare> method is called. This is the time to set up
the object so it can be used with your class. 

For example, you might want to add extra accessors to the C<status>
object, which you might do as follows:

    $dist->status->mk_accessors( qw[my_implementation_accessor] );
    
The C<status> object is implemented as an instance of the 
C<Object::Accessor> class. Please refer to it's documentation for 
details.
    
Return true if the initialization was successul, and false if it was
not.
    
The C<CPANPLUS::Dist::Base> implementation does not alter your object 
and always returns true.

=cut

sub init { return 1; }

=head2 $bool = $dist->prepare

This runs the preparation step of your distribution. This step is meant
to set up the environment so the C<create> step can create the actual
distribution(file). 
A C<prepare> call in the standard C<ExtUtils::MakeMaker> distribution 
would, for example, run C<perl Makefile.PL> to find the dependencies
for a distribution. For a C<debian> distribution, this is where you 
would write all the metafiles required for the C<dpkg-*> tools.

The C<CPANPLUS::Dist::Base> implementation simply calls the underlying
distribution class (Typically C<CPANPLUS::Dist::MM> or 
C<CPANPLUS::Dist::Build>).

Sets C<< $dist->status->prepared >> to the return value of this function.
If you override this method, you should make sure to set this value.

=cut

sub prepare { 
    ### just in case you already did a create call for this module object
    ### just via a different dist object
    my $dist        = shift;
    my $self        = $dist->parent;
    my $dist_cpan   = $self->status->dist_cpan;

    my $cb   = $self->parent;
    my $conf = $cb->configure_object;

    $dist->status->prepared( $dist_cpan->prepare( @_ ) );
}

=head2 $bool = $dist->create

This runs the creation step of your distribution. This step is meant
to follow up on the C<prepare> call, that set up your environment so 
the C<create> step can create the actual distribution(file). 
A C<create> call in the standard C<ExtUtils::MakeMaker> distribution 
would, for example, run C<make> and C<make test> to build and test
a distribution. For a C<debian> distribution, this is where you 
would create the actual C<.deb> file using C<dpkg>.

The C<CPANPLUS::Dist::Base> implementation simply calls the underlying
distribution class (Typically C<CPANPLUS::Dist::MM> or 
C<CPANPLUS::Dist::Build>).

Sets C<< $dist->status->dist >> to the location of the created 
distribution.
If you override this method, you should make sure to set this value.

Sets C<< $dist->status->created >> to the return value of this function.
If you override this method, you should make sure to set this value.

=cut

sub create { 
    ### just in case you already did a create call for this module object
    ### just via a different dist object
    my $dist        = shift;
    my $self        = $dist->parent;
    my $dist_cpan   = $self->status->dist_cpan;
    $dist           = $self->status->dist   if      $self->status->dist;
    $self->status->dist( $dist )            unless  $self->status->dist;

    my $cb      = $self->parent;
    my $conf    = $cb->configure_object;
    my $format  = ref $dist;

    ### make sure to set this variable, if the caller hasn't yet
    ### just so we have some clue where the dist left off.
    $dist->status->dist( $dist_cpan->status->distdir )
        unless defined $dist->status->dist;

    $dist->status->created( $dist_cpan->create(prereq_format => $format, @_) );
}

=head2 $bool = $dist->install

This runs the install step of your distribution. This step is meant
to follow up on the C<create> call, which prepared a distribution(file)
to install.
A C<create> call in the standard C<ExtUtils::MakeMaker> distribution 
would, for example, run C<make install> to copy the distribution files
to their final destination. For a C<debian> distribution, this is where 
you would run C<dpkg --install> on the created C<.deb> file.

The C<CPANPLUS::Dist::Base> implementation simply calls the underlying
distribution class (Typically C<CPANPLUS::Dist::MM> or 
C<CPANPLUS::Dist::Build>).

Sets C<< $dist->status->installed >> to the return value of this function.
If you override this method, you should make sure to set this value.

=cut

sub install { 
    ### just in case you already did a create call for this module object
    ### just via a different dist object
    my $dist        = shift;
    my $self        = $dist->parent;
    my $dist_cpan   = $self->status->dist_cpan;    

    my $cb   = $self->parent;
    my $conf = $cb->configure_object;

    $dist->status->installed( $dist_cpan->install( @_ ) );
}

=head2 $bool = $dist->uninstall

This runs the uninstall step of your distribution. This step is meant
to remove the distribution from the file system. 
A C<uninstall> call in the standard C<ExtUtils::MakeMaker> distribution 
would, for example, run C<make uninstall> to remove the distribution 
files the file system. For a C<debian> distribution, this is where you 
would run C<dpkg --uninstall PACKAGE>.

The C<CPANPLUS::Dist::Base> implementation simply calls the underlying
distribution class (Typically C<CPANPLUS::Dist::MM> or 
C<CPANPLUS::Dist::Build>).

Sets C<< $dist->status->uninstalled >> to the return value of this function.
If you override this method, you should make sure to set this value.

=cut

sub uninstall { 
    ### just in case you already did a create call for this module object
    ### just via a different dist object
    my $dist        = shift;
    my $self        = $dist->parent;
    my $dist_cpan   = $self->status->dist_cpan;    

    my $cb   = $self->parent;
    my $conf = $cb->configure_object;

    $dist->status->uninstalled( $dist_cpan->uninstall( @_ ) );
}

1;              

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
