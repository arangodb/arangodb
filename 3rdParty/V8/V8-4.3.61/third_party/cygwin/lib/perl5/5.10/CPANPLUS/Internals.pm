package CPANPLUS::Internals;

### we /need/ perl5.6.1 or higher -- we use coderefs in @INC,
### and 5.6.0 is just too buggy
use 5.006001;

use strict;
use Config;


use CPANPLUS::Error;

use CPANPLUS::Selfupdate;

use CPANPLUS::Internals::Source;
use CPANPLUS::Internals::Extract;
use CPANPLUS::Internals::Fetch;
use CPANPLUS::Internals::Utils;
use CPANPLUS::Internals::Constants;
use CPANPLUS::Internals::Search;
use CPANPLUS::Internals::Report;

use Cwd                         qw[cwd];
use Params::Check               qw[check];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

use Object::Accessor;


local $Params::Check::VERBOSE = 1;

use vars qw[@ISA $VERSION];

@ISA = qw[
            CPANPLUS::Internals::Source
            CPANPLUS::Internals::Extract
            CPANPLUS::Internals::Fetch
            CPANPLUS::Internals::Utils
            CPANPLUS::Internals::Search
            CPANPLUS::Internals::Report
        ];

$VERSION = "0.84";

=pod

=head1 NAME

CPANPLUS::Internals

=head1 SYNOPSIS

    my $internals   = CPANPLUS::Internals->_init( _conf => $conf );
    my $backend     = CPANPLUS::Internals->_retrieve_id( $ID );

=head1 DESCRIPTION

This module is the guts of CPANPLUS -- it inherits from all other
modules in the CPANPLUS::Internals::* namespace, thus defying normal
rules of OO programming -- but if you're reading this, you already
know what's going on ;)

Please read the C<CPANPLUS::Backend> documentation for the normal API.

=head1 ACCESSORS

=over 4

=item _conf

Get/set the configure object

=item _id

Get/set the id

=item _lib

Get/set the current @INC path -- @INC is reset to this after each
install.

=item _perl5lib

Get/set the current PERL5LIB environment variable -- $ENV{PERL5LIB}
is reset to this after each install.

=cut

### autogenerate accessors ###
for my $key ( qw[_conf _id _lib _perl5lib _modules _hosts _methods _status
                 _callbacks _selfupdate]
) {
    no strict 'refs';
    *{__PACKAGE__."::$key"} = sub {
        $_[0]->{$key} = $_[1] if @_ > 1;
        return $_[0]->{$key};
    }
}

=pod

=back

=head1 METHODS

=head2 $internals = CPANPLUS::Internals->_init( _conf => CONFIG_OBJ )

C<_init> creates a new CPANPLUS::Internals object.

You have to pass it a valid C<CPANPLUS::Configure> object.

Returns the object on success, or dies on failure.

=cut
{   ### NOTE:
    ### if extra callbacks are added, don't forget to update the
    ### 02-internals.t test script with them!
    my $callback_map = {
        ### name                default value    
        install_prerequisite    => 1,   # install prereqs when 'ask' is set?
        edit_test_report        => 0,   # edit the prepared test report?
        send_test_report        => 1,   # send the test report?
                                        # munge the test report
        munge_test_report       => sub { return $_[1] },
                                        # filter out unwanted prereqs
        filter_prereqs          => sub { return $_[1] },
                                        # continue if 'make test' fails?
        proceed_on_test_failure => sub { return 0 },
        munge_dist_metafile     => sub { return $_[1] },
    };
    
    my $status = Object::Accessor->new;
    $status->mk_accessors(qw[pending_prereqs]);

    my $callback = Object::Accessor->new;
    $callback->mk_accessors(keys %$callback_map);

    my $conf;
    my $Tmpl = {
        _conf       => { required => 1, store => \$conf,
                            allow => IS_CONFOBJ },
        _id         => { default => '',                 no_override => 1 },
        _lib        => { default => [ @INC ],           no_override => 1 },
        _perl5lib   => { default => $ENV{'PERL5LIB'},   no_override => 1 },
        _authortree => { default => '',                 no_override => 1 },
        _modtree    => { default => '',                 no_override => 1 },
        _hosts      => { default => {},                 no_override => 1 },
        _methods    => { default => {},                 no_override => 1 },
        _status     => { default => '<empty>',          no_override => 1 },
        _callbacks  => { default => '<empty>',          no_override => 1 },
    };

    sub _init {
        my $class   = shift;
        my %hash    = @_;

        ### temporary warning until we fix the storing of multiple id's
        ### and their serialization:
        ### probably not going to happen --kane
        if( my $id = $class->_last_id ) {
            # make it a singleton.
            warn loc(q[%1 currently only supports one %2 object per ] .
                     qq[running program\n], 'CPANPLUS', $class);

            return $class->_retrieve_id( $id );
        }

        my $args = check($Tmpl, \%hash)
                    or die loc(qq[Could not initialize '%1' object], $class);

        bless $args, $class;

        $args->{'_id'}          = $args->_inc_id;
        $args->{'_status'}      = $status;
        $args->{'_callbacks'}   = $callback;

        ### initialize callbacks to default state ###
        for my $name ( $callback->ls_accessors ) {
            my $rv = ref $callback_map->{$name} ? 'sub return value' :
                         $callback_map->{$name} ? 'true' : 'false';
        
            $args->_callbacks->$name(
                sub { msg(loc("DEFAULT '%1' HANDLER RETURNING '%2'",
                              $name, $rv), $args->_conf->get_conf('debug')); 
                      return ref $callback_map->{$name} 
                                ? $callback_map->{$name}->( @_ )
                                : $callback_map->{$name};
                } 
            );
        }

        ### create a selfupdate object
        $args->_selfupdate( CPANPLUS::Selfupdate->new( $args ) );

        ### initalize it as an empty hashref ###
        $args->_status->pending_prereqs( {} );

        ### allow for dirs to be added to @INC at runtime,
        ### rather then compile time
        push @INC, @{$conf->get_conf('lib')};

        ### add any possible new dirs ###
        $args->_lib( [@INC] );

        $conf->_set_build( startdir => cwd() ),
            or error( loc("couldn't locate current dir!") );

        $ENV{FTP_PASSIVE} = 1, if $conf->get_conf('passive');

        my $id = $args->_store_id( $args );

        unless ( $id == $args->_id ) {
            error( loc("IDs do not match: %1 != %2. Storage failed!",
                        $id, $args->_id) );
        }

        return $args;
    }

=pod

=head2 $bool = $internals->_flush( list => \@caches )

Flushes the designated caches from the C<CPANPLUS> object.

Returns true on success, false if one or more caches could not be
be flushed.

=cut

    sub _flush {
        my $self = shift;
        my %hash = @_;

        my $aref;
        my $tmpl = {
            list    => { required => 1, default => [],
                            strict_type => 1, store => \$aref },
        };

        my $args = check( $tmpl, \%hash ) or return;

        my $flag = 0;
        for my $what (@$aref) {
            my $cache = '_' . $what;

            ### set the include paths back to their original ###
            if( $what eq 'lib' ) {
                $ENV{PERL5LIB}  = $self->_perl5lib || '';
                @INC            = @{$self->_lib};

            ### give all modules a new status object -- this is slightly
            ### costly, but the best way to make sure all statusses are
            ### forgotten --kane
            } elsif ( $what eq 'modules' ) {
                for my $modobj ( values %{$self->module_tree} ) {
                    $modobj->_flush;
                }

            ### blow away the methods cache... currently, that's only
            ### File::Fetch's method fail list
            } elsif ( $what eq 'methods' ) {

                ### still fucking p4 :( ###
                $File'Fetch::METHOD_FAIL = $File'Fetch::METHOD_FAIL = {};

            ### blow away the m::l::c cache, so modules can be (re)loaded
            ### again if they become available
            } elsif ( $what eq 'load' ) {
                undef $Module::Load::Conditional::CACHE;

            } else {
                unless ( exists $self->{$cache} && exists $Tmpl->{$cache} ) {
                    error( loc( "No such cache: '%1'", $what ) );
                    $flag++;
                    next;
                } else {
                    $self->$cache( {} );
                }
            }
        }
        return !$flag;
    }

### NOTE:
### if extra callbacks are added, don't forget to update the
### 02-internals.t test script with them!

=pod 

=head2 $bool = $internals->_register_callback( name => CALLBACK_NAME, code => CODEREF );

Registers a callback for later use by the internal libraries.

Here is a list of the currently used callbacks:

=over 4

=item install_prerequisite

Is called when the user wants to be C<asked> about what to do with
prerequisites. Should return a boolean indicating true to install
the prerequisite and false to skip it.

=item send_test_report

Is called when the user should be prompted if he wishes to send the
test report. Should return a boolean indicating true to send the 
test report and false to skip it.

=item munge_test_report

Is called when the test report message has been composed, giving
the user a chance to programatically alter it. Should return the 
(munged) message to be sent.

=item edit_test_report

Is called when the user should be prompted to edit test reports
about to be sent out by Test::Reporter. Should return a boolean 
indicating true to edit the test report in an editor and false 
to skip it.

=item proceed_on_test_failure

Is called when 'make test' or 'Build test' fails. Should return
a boolean indicating whether the install should continue even if
the test failed.

=item munge_dist_metafile

Is called when the C<CPANPLUS::Dist::*> metafile is created, like
C<control> for C<CPANPLUS::Dist::Deb>, giving the user a chance to
programatically alter it. Should return the (munged) text to be
written to the metafile.

=back

=cut

    sub _register_callback {
        my $self = shift or return;
        my %hash = @_;

        my ($name,$code);
        my $tmpl = {
            name    => { required => 1, store => \$name,
                         allow => [$callback->ls_accessors] },
            code    => { required => 1, allow => IS_CODEREF,
                         store => \$code },
        };

        check( $tmpl, \%hash ) or return;

        $self->_callbacks->$name( $code ) or return;

        return 1;
    }

# =head2 $bool = $internals->_add_callback( name => CALLBACK_NAME, code => CODEREF );
# 
# Adds a new callback to be used from anywhere in the system. If the callback
# is already known, an error is raised and false is returned. If the callback
# is not yet known, it is added, and the corresponding coderef is registered
# using the
# 
# =cut
# 
#     sub _add_callback {
#         my $self = shift or return;
#         my %hash = @_;
#         
#         my ($name,$code);
#         my $tmpl = {
#             name    => { required => 1, store => \$name, },
#             code    => { required => 1, allow => IS_CODEREF,
#                          store => \$code },
#         };
# 
#         check( $tmpl, \%hash ) or return;
# 
#         if( $callback->can( $name ) ) {
#             error(loc("Callback '%1' is already registered"));
#             return;
#         }
# 
#         $callback->mk_accessor( $name );
# 
#         $self->_register_callback( name => $name, code => $code ) or return;
# 
#         return 1;
#     }

}

=pod

=head2 $bool = $internals->_add_to_includepath( directories => \@dirs )

Adds a list of directories to the include path.
This means they get added to C<@INC> as well as C<$ENV{PERL5LIB}>.

Returns true on success, false on failure.

=cut

sub _add_to_includepath {
    my $self = shift;
    my %hash = @_;

    my $dirs;
    my $tmpl = {
        directories => { required => 1, default => [], store => \$dirs,
                         strict_type => 1 },
    };

    check( $tmpl, \%hash ) or return;

    for my $lib (@$dirs) {
        push @INC, $lib unless grep { $_ eq $lib } @INC;
    }

    {   local $^W;  ### it will be complaining if $ENV{PERL5LIB]
                    ### is not defined (yet).
        $ENV{'PERL5LIB'} .= join '', map { $Config{'path_sep'} . $_ } @$dirs;
    }

    return 1;
}

=pod

=head2 $id = CPANPLUS::Internals->_last_id

Return the id of the last object stored.

=head2 $id = CPANPLUS::Internals->_store_id( $internals )

Store this object; return its id.

=head2 $obj = CPANPLUS::Internals->_retrieve_id( $ID )

Retrieve an object based on its ID -- return false on error.

=head2 CPANPLUS::Internals->_remove_id( $ID )

Remove the object marked by $ID from storage.

=head2 @objs = CPANPLUS::Internals->_return_all_objects

Return all stored objects.

=cut


### code for storing multiple objects
### -- although we only support one right now
### XXX when support for multiple objects comes, saving source will have
### to change
{
    my $idref = {};
    my $count = 0;

    sub _inc_id { return ++$count; }

    sub _last_id { $count }

    sub _store_id {
        my $self    = shift;
        my $obj     = shift or return;

       unless( IS_INTERNALS_OBJ->($obj) ) {
            error( loc("The object you passed has the wrong ref type: '%1'",
                        ref $obj) );
            return;
        }

        $idref->{ $obj->_id } = $obj;
        return $obj->_id;
    }

    sub _retrieve_id {
        my $self    = shift;
        my $id      = shift or return;

        my $obj = $idref->{$id};
        return $obj;
    }

    sub _remove_id {
        my $self    = shift;
        my $id      = shift or return;

        return delete $idref->{$id};
    }

    sub _return_all_objects { return values %$idref }
}

1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
