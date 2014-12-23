package CPANPLUS::Dist;

use strict;


use CPANPLUS::Error;
use CPANPLUS::Internals::Constants;

use Params::Check               qw[check];
use Module::Load::Conditional   qw[can_load check_install];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';
use Object::Accessor;

local $Params::Check::VERBOSE = 1;

my @methods = qw[status parent];
for my $key ( @methods ) {
    no strict 'refs';
    *{__PACKAGE__."::$key"} = sub {
        my $self = shift;
        $self->{$key} = $_[0] if @_;
        return $self->{$key};
    }
}

=pod

=head1 NAME

CPANPLUS::Dist

=head1 SYNOPSIS

    my $dist = CPANPLUS::Dist->new(
                                format  => 'build',
                                module  => $modobj,
                            );

=head1 DESCRIPTION

C<CPANPLUS::Dist> is a base class for C<CPANPLUS::Dist::MM>
and C<CPANPLUS::Dist::Build>. Developers of other C<CPANPLUS::Dist::*>
plugins should look at C<CPANPLUS::Dist::Base>.

=head1 ACCESSORS

=over 4

=item parent()

Returns the C<CPANPLUS::Module> object that parented this object.

=item status()

Returns the C<Object::Accessor> object that keeps the status for
this module.

=back

=head1 STATUS ACCESSORS

All accessors can be accessed as follows:
    $deb->status->ACCESSOR

=over 4

=item created()

Boolean indicating whether the dist was created successfully.
Explicitly set to C<0> when failed, so a value of C<undef> may be
interpreted as C<not yet attempted>.

=item installed()

Boolean indicating whether the dist was installed successfully.
Explicitly set to C<0> when failed, so a value of C<undef> may be
interpreted as C<not yet attempted>.

=item uninstalled()

Boolean indicating whether the dist was uninstalled successfully.
Explicitly set to C<0> when failed, so a value of C<undef> may be
interpreted as C<not yet attempted>.

=item dist()

The location of the final distribution. This may be a file or
directory, depending on how your distribution plug in of choice
works. This will be set upon a successful create.

=cut

=back

=head2 $dist = CPANPLUS::Dist->new( module => MODOBJ, [format => DIST_TYPE] );

Create a new C<CPANPLUS::Dist> object based on the provided C<MODOBJ>.
The optional argument C<format> is used to indicate what type of dist
you would like to create (like C<makemaker> for a C<CPANPLUS::Dist::MM>
object, C<build> for a C<CPANPLUS::Dist::Build> object, and so on ).
If not provided, will default to the setting as specified by your
config C<dist_type>.

Returns a C<CPANPLUS::Dist> object on success and false on failure.

=cut

sub new {
    my $self = shift;
    my %hash = @_;

    local $Params::Check::ALLOW_UNKNOWN = 1;

    ### first verify we got a module object ###
    my $mod;
    my $tmpl = {
        module  => { required => 1, allow => IS_MODOBJ, store => \$mod },
    };
    check( $tmpl, \%hash ) or return;

    ### get the conf object ###
    my $conf = $mod->parent->configure_object();

    ### figure out what type of dist object to create ###
    my $format;
    my $tmpl2 = {
        format  => {    default => $conf->get_conf('dist_type'),
                        allow   => [ __PACKAGE__->dist_types ],
                        store   => \$format  },
    };
    check( $tmpl2, \%hash ) or return;


    unless( can_load( modules => { $format => '0.0' }, verbose => 1 ) ) {
        error(loc("'%1' not found -- you need '%2' version '%3' or higher ".
                    "to detect plugins", $format, 'Module::Pluggable','2.4'));
        return;
    }

    ### bless the object in the child class ###
    my $obj = bless { parent => $mod }, $format;

    ### check if the format is available in this environment ###
    if( $conf->_get_build('sanity_check') and not $obj->format_available ) {
        error( loc( "Format '%1' is not available",$format) );
        return;
    }

    ### create a status object ###
    {   my $acc = Object::Accessor->new;
        $obj->status($acc);

        ### add minimum supported accessors
        $acc->mk_accessors( qw[prepared created installed uninstalled 
                               distdir dist] );
    }

    ### now initialize it or admit failure
    unless( $obj->init ) {
        error(loc("Dist initialization of '%1' failed for '%2'",
                    $format, $mod->module));
        return;
    }

    ### return the object
    return $obj;
}

=head2 @dists = CPANPLUS::Dist->dist_types;

Returns a list of the CPANPLUS::Dist::* classes available

=cut

### returns a list of dist_types we support
### will get overridden by Module::Pluggable if loaded
### XXX add support for 'plugin' dir in config as well
{   my $Loaded;
    my @Dists   = (INSTALLER_MM);
    my @Ignore  = ();

    ### backdoor method to add more dist types
    sub _add_dist_types     { my $self = shift; push @Dists,  @_ };
    
    ### backdoor method to exclude dist types
    sub _ignore_dist_types  { my $self = shift; push @Ignore, @_ };

    ### locally add the plugins dir to @INC, so we can find extra plugins
    #local @INC = @INC, File::Spec->catdir(
    #                        $conf->get_conf('base'),
    #                        $conf->_get_build('plugins') );

    ### load any possible plugins
    sub dist_types {

        if ( !$Loaded++ and check_install(  module  => 'Module::Pluggable',
                                            version => '2.4')
        ) {
            require Module::Pluggable;

            my $only_re = __PACKAGE__ . '::\w+$';

            Module::Pluggable->import(
                            sub_name    => '_dist_types',
                            search_path => __PACKAGE__,
                            only        => qr/$only_re/,
                            except      => [ INSTALLER_MM, 
                                             INSTALLER_SAMPLE,
                                             INSTALLER_BASE,
                                        ]
                        );
            my %ignore = map { $_ => $_ } @Ignore;                        
                        
            push @Dists, grep { not $ignore{$_}  } __PACKAGE__->_dist_types;
        }

        return @Dists;
    }
}

=head2 prereq_satisfied( modobj => $modobj, version => $version_spec )

Returns true if this prereq is satisfied.  Returns false if it's not.
Also issues an error if it seems "unsatisfiable," i.e. if it can't be
found on CPAN or the latest CPAN version doesn't satisfy it.

=cut

sub prereq_satisfied {
    my $dist = shift;
    my $cb   = $dist->parent->parent;
    my %hash = @_;
  
    my($mod,$ver);
    my $tmpl = {
        version => { required => 1, store => \$ver },
        modobj  => { required => 1, store => \$mod, allow => IS_MODOBJ },
    };
    
    check( $tmpl, \%hash ) or return;
  
    return 1 if $mod->is_uptodate( version => $ver );
  
    if ( $cb->_vcmp( $ver, $mod->version ) > 0 ) {

        error(loc(  
                "This distribution depends on %1, but the latest version".
                " of %2 on CPAN (%3) doesn't satisfy the specific version".
                " dependency (%4). You may have to resolve this dependency ".
                "manually.", 
                $mod->module, $mod->module, $mod->version, $ver ));
  
    }

    return;
}

=head2 _resolve_prereqs

Makes sure prerequisites are resolved

XXX Need docs, internal use only

=cut

sub _resolve_prereqs {
    my $dist = shift;
    my $self = $dist->parent;
    my $cb   = $self->parent;
    my $conf = $cb->configure_object;
    my %hash = @_;

    my ($prereqs, $format, $verbose, $target, $force, $prereq_build);
    my $tmpl = {
        ### XXX perhaps this should not be required, since it may not be
        ### packaged, just installed...
        ### Let it be empty as well -- that means the $modobj->install
        ### routine will figure it out, which is fine if we didn't have any
        ### very specific wishes (it will even detect the favourite
        ### dist_type).
        format          => { required => 1, store => \$format,
                                allow => ['',__PACKAGE__->dist_types], },
        prereqs         => { required => 1, default => { },
                                strict_type => 1, store => \$prereqs },
        verbose         => { default => $conf->get_conf('verbose'),
                                store => \$verbose },
        force           => { default => $conf->get_conf('force'),
                                store => \$force },
                        ### make sure allow matches with $mod->install's list
        target          => { default => '', store => \$target,
                                allow => ['',qw[create ignore install]] },
        prereq_build    => { default => 0, store => \$prereq_build },
    };

    check( $tmpl, \%hash ) or return;

    ### so there are no prereqs? then don't even bother
    return 1 unless keys %$prereqs;

    ### so you didn't provide an explicit target.
    ### maybe your config can tell us what to do.
    $target ||= {
        PREREQ_ASK,     TARGET_INSTALL, # we'll bail out if the user says no
        PREREQ_BUILD,   TARGET_CREATE,
        PREREQ_IGNORE,  TARGET_IGNORE,
        PREREQ_INSTALL, TARGET_INSTALL,
    }->{ $conf->get_conf('prereqs') } || '';
    
    ### XXX BIG NASTY HACK XXX FIXME at some point.
    ### when installing Bundle::CPANPLUS::Dependencies, we want to
    ### install all packages matching 'cpanplus' to be installed last,
    ### as all CPANPLUS' prereqs are being installed as well, but are
    ### being loaded for bootstrapping purposes. This means CPANPLUS
    ### can find them, but for example cpanplus::dist::build won't,
    ### which gets messy FAST. So, here we sort our prereqs only IF
    ### the parent module is Bundle::CPANPLUS::Dependencies.
    ### Really, we would wnat some sort of sorted prereq mechanism,
    ### but Bundle:: doesn't support it, and we flatten everything
    ### to a hash internally. A sorted hash *might* do the trick if
    ### we got a transparent implementation.. that would mean we would
    ### just have to remove the 'sort' here, and all will be well
    my @sorted_prereqs;
    
    ### use regex, could either be a module name, or a package name
    if( $self->module =~ /^Bundle(::|-)CPANPLUS(::|-)Dependencies/ ) {
        my (@first, @last);
        for my $mod ( sort keys %$prereqs ) {
            $mod =~ /CPANPLUS/
                ? push @last,  $mod
                : push @first, $mod;
        }
        @sorted_prereqs = (@first, @last);
    } else {
        @sorted_prereqs = sort keys %$prereqs;
    }

    ### first, transfer this key/value pairing into a
    ### list of module objects + desired versions
    my @install_me;
    
    for my $mod ( @sorted_prereqs ) {
        my $version = $prereqs->{$mod};
        my $modobj  = $cb->module_tree($mod);

        #### XXX we ignore the version, and just assume that the latest
        #### version from cpan will meet your requirements... dodgy =/
        unless( $modobj ) {
            error( loc( "No such module '%1' found on CPAN", $mod ) );
            next;
        }

        ### it's not uptodate, we need to install it
        if( !$dist->prereq_satisfied(modobj => $modobj, version => $version)) {
            msg(loc("Module '%1' requires '%2' version '%3' to be installed ",
                    $self->module, $modobj->module, $version), $verbose );

            push @install_me, [$modobj, $version];

        ### it's not an MM or Build format, that means it's a package
        ### manager... we'll need to install it as well, via the PM
        } elsif ( INSTALL_VIA_PACKAGE_MANAGER->($format) and
                    !$modobj->package_is_perl_core and
                    ($target ne TARGET_IGNORE)
        ) {
            msg(loc("Module '%1' depends on '%2', may need to build a '%3' ".
                    "package for it as well", $self->module, $modobj->module,
                    $format));
            push @install_me, [$modobj, $version];
        }
    }



    ### so you just want to ignore prereqs? ###
    if( $target eq TARGET_IGNORE ) {

        ### but you have modules you need to install
        if( @install_me ) {
            msg(loc("Ignoring prereqs, this may mean your install will fail"),
                $verbose);
            msg(loc("'%1' listed the following dependencies:", $self->module),
                $verbose);

            for my $aref (@install_me) {
                my ($mod,$version) = @$aref;

                my $str = sprintf "\t%-35s %8s\n", $mod->module, $version;
                msg($str,$verbose);
            }

            return;

        ### ok, no problem, you have all needed prereqs anyway
        } else {
            return 1;
        }
    }

    my $flag;
    for my $aref (@install_me) {
        my($modobj,$version) = @$aref;

        ### another prereq may have already installed this one...
        ### so dont ask again if the module turns out to be uptodate
        ### see bug [#11840]
        ### if either force or prereq_build are given, the prereq
        ### should be built anyway
        next if (!$force and !$prereq_build) && 
                $dist->prereq_satisfied(modobj => $modobj, version => $version);

        ### either we're told to ignore the prereq,
        ### or the user wants us to ask him
        if( ( $conf->get_conf('prereqs') == PREREQ_ASK and not
              $cb->_callbacks->install_prerequisite->($self, $modobj)
            )
        ) {
            msg(loc("Will not install prerequisite '%1' -- Note " .
                    "that the overall install may fail due to this",
                    $modobj->module), $verbose);
            next;
        }

        ### value set and false -- means failure ###
        if( defined $modobj->status->installed
            && !$modobj->status->installed
        ) {
            error( loc( "Prerequisite '%1' failed to install before in " .
                        "this session", $modobj->module ) );
            $flag++;
            last;
        }

        ### part of core?
        if( $modobj->package_is_perl_core ) {
            error(loc("Prerequisite '%1' is perl-core (%2) -- not ".
                      "installing that. Aborting install",
                      $modobj->module, $modobj->package ) );
            $flag++;
            last;
        }

        ### circular dependency code ###
        my $pending = $cb->_status->pending_prereqs || {};

        ### recursive dependency ###
        if ( $pending->{ $modobj->module } ) {
            error( loc( "Recursive dependency detected (%1) -- skipping",
                        $modobj->module ) );
            next;
        }

        ### register this dependency as pending ###
        $pending->{ $modobj->module } = $modobj;
        $cb->_status->pending_prereqs( $pending );


        ### call $modobj->install rather than doing
        ### CPANPLUS::Dist->new and the like ourselves,
        ### since ->install will take care of fetch &&
        ### extract as well
        my $pa = $dist->status->_prepare_args   || {};
        my $ca = $dist->status->_create_args    || {};
        my $ia = $dist->status->_install_args   || {};

        unless( $modobj->install(   %$pa, %$ca, %$ia,
                                    force   => $force,
                                    verbose => $verbose,
                                    format  => $format,
                                    target  => $target )
        ) {
            error(loc("Failed to install '%1' as prerequisite " .
                      "for '%2'", $modobj->module, $self->module ) );
            $flag++;
        }

        ### unregister the pending dependency ###
        $pending->{ $modobj->module } = 0;
        $cb->_status->pending_prereqs( $pending );

        last if $flag;

        ### don't want us to install? ###
        if( $target ne TARGET_INSTALL ) {
            my $dir = $modobj->status->extract
                        or error(loc("No extraction dir for '%1' found ".
                                     "-- weird", $modobj->module));

            $modobj->add_to_includepath();
            
            next;
        }
    }

    ### reset the $prereqs iterator, in case we bailed out early ###
    keys %$prereqs;

    return 1 unless $flag;
    return;
}

1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
