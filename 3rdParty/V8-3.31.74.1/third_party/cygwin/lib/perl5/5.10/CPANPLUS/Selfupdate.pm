package CPANPLUS::Selfupdate;

use strict;
use Params::Check               qw[check];
use IPC::Cmd                    qw[can_run];
use CPANPLUS::Error             qw[error msg];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

use CPANPLUS::Internals::Constants;

$Params::Check::VERBOSE = 1;

=head1 NAME

CPANPLUS::Selfupdate

=head1 SYNOPSIS

    $su     = $cb->selfupdate_object;
    
    @feats  = $su->list_features;
    @feats  = $su->list_enabled_features;
    
    @mods   = map { $su->modules_for_feature( $_ ) } @feats;
    @mods   = $su->list_core_dependencies;
    @mods   = $su->list_core_modules;
    
    for ( @mods ) {
        print $_->name " should be version " . $_->version_required;
        print "Installed version is not uptodate!" 
            unless $_->is_installed_version_sufficient;
    }
    
    $ok     = $su->selfupdate( update => 'all', latest => 0 );

=cut

### a config has describing our deps etc
{

    my $Modules = {
        dependencies => {
            'File::Fetch'               => '0.13_04', # win32 & VMS file://
            'File::Spec'                => '0.82',
            'IPC::Cmd'                  => '0.36', # 5.6.2 compat: 2-arg open
            'Locale::Maketext::Simple'  => '0.01',
            'Log::Message'              => '0.01',
            'Module::Load'              => '0.10',
            'Module::Load::Conditional' => '0.18', # Better parsing: #23995,
                                                   # uses version.pm for <=>
            'version'                   => '0.73', # needed for M::L::C
                                                   # addresses #24630 and 
                                                   # #24675
                                                   # Address ~0 overflow issue
            'Params::Check'             => '0.22',
            'Package::Constants'        => '0.01',
            'Term::UI'                  => '0.18', # option parsing
            'Test::Harness'             => '2.62', # due to bug #19505
                                                   # only 2.58 and 2.60 are bad
            'Test::More'                => '0.47', # to run our tests
            'Archive::Extract'          => '0.16', # ./Dir bug fix
            'Archive::Tar'              => '1.23',
            'IO::Zlib'                  => '1.04', # needed for Archive::Tar
            'Object::Accessor'          => '0.32', # overloaded stringification
            'Module::CoreList'          => '2.09',
            'Module::Pluggable'         => '2.4',
            'Module::Loaded'            => '0.01',
        },
    
        features => {
            # config_key_name => [
            #     sub { } to list module key/value pairs
            #     sub { } to check if feature is enabled
            # ]
            prefer_makefile => [
                sub {
                    my $cb = shift;
                    $cb->configure_object->get_conf('prefer_makefile') 
                        ? { }
                        : { 'CPANPLUS::Dist::Build' => '0.04'  };
                },
                sub { return 1 },   # always enabled
            ],            
            cpantest        => [
                {
                    'YAML::Tiny'     => '0.0',
                    'Test::Reporter' => '1.34',
                },
                sub { 
                    my $cb = shift;
                    return $cb->configure_object->get_conf('cpantest');
                },
            ],                
            dist_type => [
                sub { 
                    my $cb      = shift;
                    my $dist    = $cb->configure_object->get_conf('dist_type');
                    return { $dist => '0.0' } if $dist;
                    return;
                },            
                sub { 
                    my $cb = shift;
                    return $cb->configure_object->get_conf('dist_type');
                },
            ],

            md5 => [
                {
                    'Digest::MD5'   => '0.0',
                },            
                sub { 
                    my $cb = shift;
                    return $cb->configure_object->get_conf('md5');
                },
            ],
            shell => [
                sub { 
                    my $cb      = shift;
                    my $dist    = $cb->configure_object->get_conf('shell');
                    
                    ### we bundle these shells, so don't bother having a dep
                    ### on them... If we don't do this, CPAN.pm actually detects
                    ### a recursive dependency and breaks (see #26077).
                    ### This is not an issue for CPANPLUS itself, it handles
                    ### it smartly.
                    return if $dist eq SHELL_DEFAULT or $dist eq SHELL_CLASSIC;
                    return { $dist => '0.0' } if $dist;
                    return;
                },            
                sub { return 1 },
            ],                
            signature => [
                sub {
                    my $cb      = shift;
                    return {
                        'Module::Signature' => '0.06',
                    } if can_run('gpg');
                    ### leave this out -- Crypt::OpenPGP is fairly
                    ### painful to install, and broken on some platforms
                    ### so we'll just always fall back to gpg. It may
                    ### issue a warning or 2, but that's about it.
                    ### this change due to this ticket: #26914
                    # and $cb->configure_object->get_conf('prefer_bin');

                    return { 
                        'Crypt::OpenPGP'    => '0.0', 
                        'Module::Signature' => '0.06',
                    };
                },            
                sub {
                    my $cb = shift;
                    return $cb->configure_object->get_conf('signature');
                },
            ],
            storable => [
                { 'Storable' => '0.0' },         
                sub { 
                    my $cb = shift;
                    return $cb->configure_object->get_conf('storable');
                },
            ],
        },
        core => {
            'CPANPLUS' => '0.0',
        },
    };

    sub _get_config { return $Modules }
}

=head1 METHODS

=head2 $self = CPANPLUS::Selfupdate->new( $backend_object );

Sets up a new selfupdate object. Called automatically when
a new backend object is created.

=cut

sub new {
    my $class = shift;
    my $cb    = shift or return;
    return bless sub { $cb }, $class;
}    


{   ### cache to find the relevant modules
    my $cache = {
        core 
            => sub { my $self = shift;
                     core => [ $self->list_core_modules ]   },
 
        dependencies        
            => sub { my $self = shift;
                     dependencies => [ $self->list_core_dependencies ] },

        enabled_features    
            => sub { my $self = shift;
                     map { $_ => [ $self->modules_for_feature( $_ ) ] }
                        $self->list_enabled_features 
                   },
        features
            => sub { my $self = shift;
                     map { $_ => [ $self->modules_for_feature( $_ ) ] }
                        $self->list_features   
                   },
            ### make sure to do 'core' first, in case
            ### we are out of date ourselves
        all => [ qw|core dependencies enabled_features| ],
    };
    
    
=head2 @cat = $self->list_categories

Returns a list of categories that the C<selfupdate> method accepts.

See C<selfupdate> for details.

=cut

    sub list_categories { return sort keys %$cache }

=head2 %list = $self->list_modules_to_update( update => "core|dependencies|enabled_features|features|all", [latest => BOOL] )
    
List which modules C<selfupdate> would upgrade. You can update either 
the core (CPANPLUS itself), the core dependencies, all features you have
currently turned on, or all features available, or everything.

The C<latest> option determines whether it should update to the latest
version on CPAN, or if the minimal required version for CPANPLUS is
good enough.
    
Returns a hash of feature names and lists of module objects to be
upgraded based on the category you provided. For example:

    %list = $self->list_modules_to_update( update => 'core' );
    
Would return:

    ( core => [ $module_object_for_cpanplus ] );
    
=cut    
    
    sub list_modules_to_update {
        my $self = shift;
        my $cb   = $self->();
        my $conf = $cb->configure_object;
        my %hash = @_;
        
        my($type, $latest);
        my $tmpl = {
            update => { required => 1, store => \$type,
                         allow   => [ keys %$cache ], },
            latest => { default  => 0, store => \$latest, allow => BOOLEANS },                     
        };    
    
        {   local $Params::Check::ALLOW_UNKNOWN = 1;
            check( $tmpl, \%hash ) or return;
        }
    
        my $ref     = $cache->{$type};

        ### a list of ( feature1 => \@mods, feature2 => \@mods, etc )        
        my %list    = UNIVERSAL::isa( $ref, 'ARRAY' )
                            ? map { $cache->{$_}->( $self ) } @$ref
                            : $ref->( $self );

        ### filter based on whether we need the latest ones or not
        for my $aref ( values %list ) {              
              $aref = [ $latest 
                        ? grep { !$_->is_uptodate } @$aref
                        : grep { !$_->is_installed_version_sufficient } @$aref
                      ];
        }
        
        return %list;
    }
    
=head2 $bool = $self->selfupdate( update => "core|dependencies|enabled_features|features|all", [latest => BOOL, force => BOOL] )

Selfupdate CPANPLUS. You can update either the core (CPANPLUS itself),
the core dependencies, all features you have currently turned on, or
all features available, or everything.

The C<latest> option determines whether it should update to the latest
version on CPAN, or if the minimal required version for CPANPLUS is
good enough.

Returns true on success, false on error.

=cut

    sub selfupdate {
        my $self = shift;
        my $cb   = $self->();
        my $conf = $cb->configure_object;
        my %hash = @_;
    
        my $force;
        my $tmpl = {
            force  => { default => $conf->get_conf('force'), store => \$force },
        };    
    
        {   local $Params::Check::ALLOW_UNKNOWN = 1;
            check( $tmpl, \%hash ) or return;
        }
    
        my %list = $self->list_modules_to_update( %hash ) or return;

        ### just the modules please
        my @mods = map { @$_ } values %list;
        
        my $flag;
        for my $mod ( @mods ) {
            unless( $mod->install( force => $force ) ) {
                $flag++;
                error(loc("Failed to update module '%1'", $mod->name));
            }
        }
        
        return if $flag;
        return 1;
    }    

}

=head2 @features = $self->list_features

Returns a list of features that are supported by CPANPLUS.

=cut

sub list_features {
    my $self = shift;
    return keys %{ $self->_get_config->{'features'} };
}

=head2 @features = $self->list_enabled_features

Returns a list of features that are enabled in your current
CPANPLUS installation.

=cut

sub list_enabled_features {
    my $self = shift;
    my $cb   = $self->();
    
    my @enabled;
    for my $feat ( $self->list_features ) {
        my $ref = $self->_get_config->{'features'}->{$feat}->[1];
        push @enabled, $feat if $ref->($cb);
    }
    
    return @enabled;
}

=head2 @mods = $self->modules_for_feature( FEATURE [,AS_HASH] )

Returns a list of C<CPANPLUS::Selfupdate::Module> objects which 
represent the modules required to support this feature.

For a list of features, call the C<list_features> method.

If the C<AS_HASH> argument is provided, no module objects are
returned, but a hashref where the keys are names of the modules,
and values are their minimum versions.

=cut

sub modules_for_feature {
    my $self    = shift;
    my $feature = shift or return;
    my $as_hash = shift || 0;
    my $cb      = $self->();
    
    unless( exists $self->_get_config->{'features'}->{$feature} ) {
        error(loc("Unknown feature '%1'", $feature));
        return;
    }
    
    my $ref = $self->_get_config->{'features'}->{$feature}->[0];
    
    ### it's either a list of modules/versions or a subroutine that
    ### returns a list of modules/versions
    my $href = UNIVERSAL::isa( $ref, 'HASH' ) ? $ref : $ref->( $cb );
    
    return unless $href;    # nothing needed for the feature?

    return $href if $as_hash;
    return $self->_hashref_to_module( $href );
}


=head2 @mods = $self->list_core_dependencies( [AS_HASH] )

Returns a list of C<CPANPLUS::Selfupdate::Module> objects which 
represent the modules that comprise the core dependencies of CPANPLUS.

If the C<AS_HASH> argument is provided, no module objects are
returned, but a hashref where the keys are names of the modules,
and values are their minimum versions.

=cut

sub list_core_dependencies {
    my $self    = shift;
    my $as_hash = shift || 0;
    my $cb      = $self->();
    my $href    = $self->_get_config->{'dependencies'};

    return $href if $as_hash;
    return $self->_hashref_to_module( $href );
}

=head2 @mods = $self->list_core_modules( [AS_HASH] )

Returns a list of C<CPANPLUS::Selfupdate::Module> objects which 
represent the modules that comprise the core of CPANPLUS.

If the C<AS_HASH> argument is provided, no module objects are
returned, but a hashref where the keys are names of the modules,
and values are their minimum versions.

=cut

sub list_core_modules {
    my $self    = shift;
    my $as_hash = shift || 0;
    my $cb      = $self->();
    my $href    = $self->_get_config->{'core'};

    return $href if $as_hash;
    return $self->_hashref_to_module( $href );
}

sub _hashref_to_module {
    my $self = shift;
    my $cb   = $self->();
    my $href = shift or return;
    
    return map { 
            CPANPLUS::Selfupdate::Module->new(
                $cb->module_tree($_) => $href->{$_}
            )
        } keys %$href;
}        
    

=head1 CPANPLUS::Selfupdate::Module

C<CPANPLUS::Selfupdate::Module> extends C<CPANPLUS::Module> objects
by providing accessors to aid in selfupdating CPANPLUS.

These objects are returned by all methods of C<CPANPLUS::Selfupdate>
that return module objects.

=cut

{   package CPANPLUS::Selfupdate::Module;
    use base 'CPANPLUS::Module';
    
    ### stores module name -> cpanplus required version
    ### XXX only can deal with 1 pair!
    my %Cache = ();
    my $Acc   = 'version_required';
    
    sub new {
        my $class = shift;
        my $mod   = shift or return;
        my $ver   = shift;          return unless defined $ver;
        
        my $obj   = $mod->clone;    # clone the module object
        bless $obj, $class;         # rebless it to our class
        
        $obj->$Acc( $ver );
        
        return $obj;
    }

=head2 $version = $mod->version_required

Returns the version of this module required for CPANPLUS.

=cut
    
    sub version_required {
        my $self = shift;
        $Cache{ $self->name } = shift() if @_;
        return $Cache{ $self->name };
    }        

=head2 $bool = $mod->is_installed_version_sufficient

Returns true if the installed version of this module is sufficient
for CPANPLUS, or false if it is not.

=cut

    
    sub is_installed_version_sufficient {
        my $self = shift;
        return $self->is_uptodate( version => $self->$Acc );
    }

}    

1;

=pod

=head1 BUG REPORTS

Please report bugs or other issues to E<lt>bug-cpanplus@rt.cpan.org<gt>.

=head1 AUTHOR

This module by Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 COPYRIGHT

The CPAN++ interface (of which this module is a part of) is copyright (c) 
2001 - 2007, Jos Boumans E<lt>kane@cpan.orgE<gt>. All rights reserved.

This library is free software; you may redistribute and/or modify it 
under the same terms as Perl itself.

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
