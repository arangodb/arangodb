package CPANPLUS::Configure;
use strict;


use CPANPLUS::Internals::Constants;
use CPANPLUS::Error;
use CPANPLUS::Config;

use Log::Message;
use Module::Load                qw[load];
use Params::Check               qw[check];
use File::Basename              qw[dirname];
use Module::Loaded              ();
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

use vars                        qw[$AUTOLOAD $VERSION $MIN_CONFIG_VERSION];
use base                        qw[CPANPLUS::Internals::Utils];

local $Params::Check::VERBOSE = 1;

### require, avoid circular use ###
require CPANPLUS::Internals;
$VERSION = $CPANPLUS::Internals::VERSION = $CPANPLUS::Internals::VERSION;

### can't use O::A as we're using our own AUTOLOAD to get to
### the config options.
for my $meth ( qw[conf]) {
    no strict 'refs';
    
    *$meth = sub {
        my $self = shift;
        $self->{'_'.$meth} = $_[0] if @_;
        return $self->{'_'.$meth};
    }     
}


=pod

=head1 NAME

CPANPLUS::Configure

=head1 SYNOPSIS

    $conf   = CPANPLUS::Configure->new( );

    $bool   = $conf->can_save;
    $bool   = $conf->save( $where );

    @opts   = $conf->options( $type );

    $make       = $conf->get_program('make');
    $verbose    = $conf->set_conf( verbose => 1 );

=head1 DESCRIPTION

This module deals with all the configuration issues for CPANPLUS.
Users can use objects created by this module to alter the behaviour
of CPANPLUS.

Please refer to the C<CPANPLUS::Backend> documentation on how to
obtain a C<CPANPLUS::Configure> object.

=head1 METHODS

=head2 $Configure = CPANPLUS::Configure->new( load_configs => BOOL )

This method returns a new object. Normal users will never need to
invoke the C<new> method, but instead retrieve the desired object via
a method call on a C<CPANPLUS::Backend> object.

The C<load_configs> parameter controls wether or not additional
user configurations are to be loaded or not. Defaults to C<true>.

=cut

### store teh CPANPLUS::Config object in a closure, so we only
### initialize it once.. otherwise, on a 2nd ->new, settings
### from configs on top of this one will be reset
{   my $Config;

    sub new {
        my $class   = shift;
        my %hash    = @_;
        
        ### XXX pass on options to ->init() like rescan?
        my ($load);
        my $tmpl    = {
            load_configs    => { default => 1, store => \$load },
        };
        
        check( $tmpl, \%hash ) or (
            warn Params::Check->last_error, return
        );
        
        $Config     ||= CPANPLUS::Config->new;
        my $self    = bless {}, $class;
        $self->conf( $Config );
    
        ### you want us to load other configs?
        ### these can override things in the default config
        $self->init if $load;
    
        return $self;
    }
}

=head2 $bool = $Configure->init( [rescan => BOOL])

Initialize the configure with other config files than just
the default 'CPANPLUS::Config'.

Called from C<new()> to load user/system configurations

If the C<rescan> option is provided, your disk will be
examined again to see if there are new config files that
could be read. Defaults to C<false>.

Returns true on success, false on failure.

=cut

### move the Module::Pluggable detection to runtime, rather
### than compile time, so that a simple 'require CPANPLUS'
### doesn't start running over your filesystem for no good
### reason. Make sure we only do the M::P call once though.
### we use $loaded to mark it
{   my $loaded;
    my $warned;
    sub init {
        my $self    = shift;
        my $obj     = $self->conf;
        my %hash    = @_;
        
        my ($rescan);
        my $tmpl    = {
            rescan  => { default => 0, store => \$rescan },
        };
        
        check( $tmpl, \%hash ) or (
            warn Params::Check->last_error, return
        );        
        
        ### warn if we find an old style config specified
        ### via environment variables
        {   my $env = ENV_CPANPLUS_CONFIG;
            if( $ENV{$env} and not $warned ) {
                $warned++;
                error(loc("Specifying a config file in your environment " .
                          "using %1 is obsolete.\nPlease follow the ".
                          "directions outlined in %2 or use the '%3' command\n".
                          "in the default shell to use custom config files.",
                          $env, "CPANPLUS::Configure->save", 's save'));
            }
        }            
        
        ### make sure that the homedir is included now
        local @INC = ( CONFIG_USER_LIB_DIR->(), @INC );
        
        ### only set it up once
        if( !$loaded++ or $rescan ) {   
            ### find plugins & extra configs
            ### check $home/.cpanplus/lib as well
            require Module::Pluggable;
            
            Module::Pluggable->import(
                search_path => ['CPANPLUS::Config'],
                search_dirs => [ CONFIG_USER_LIB_DIR ],
                except      => qr/::SUPER$/,
                sub_name    => 'configs'
            );
        }
        
        
        ### do system config, user config, rest.. in that order
        ### apparently, on a 2nd invocation of -->configs, a
        ### ::ISA::CACHE package can appear.. that's bad...
        my %confs = map  { $_ => $_ } 
                    grep { $_ !~ /::ISA::/ } __PACKAGE__->configs;
        my @confs = grep { defined } 
                    map  { delete $confs{$_} } CONFIG_SYSTEM, CONFIG_USER;
        push @confs, sort keys %confs;                    
    
        for my $plugin ( @confs ) {
            msg(loc("Found config '%1'", $plugin),0);
            
            ### if we already did this the /last/ time around dont 
            ### run the setup agian.
            if( my $loc = Module::Loaded::is_loaded( $plugin ) ) {
                msg(loc("  Already loaded '%1' (%2)", $plugin, $loc), 0);
                next;
            } else {
                msg(loc("  Loading config '%1'", $plugin),0);
            
                eval { load $plugin };
                msg(loc("  Loaded '%1' (%2)", 
                        $plugin, Module::Loaded::is_loaded( $plugin ) ), 0);
            }                   
            
            if( $@ ) {
                error(loc("Could not load '%1': %2", $plugin, $@));
                next;
            }     
            
            my $sub = $plugin->can('setup');
            $sub->( $self ) if $sub;
        }
        
        ### clean up the paths once more, just in case
        $obj->_clean_up_paths;
    
        return 1;
    }
}
=pod

=head2 can_save( [$config_location] )

Check if we can save the configuration to the specified file.
If no file is provided, defaults to your personal config.

Returns true if the file can be saved, false otherwise.

=cut

sub can_save {
    my $self = shift;
    my $file = shift || CONFIG_USER_FILE->();
    
    return 1 unless -e $file;

    chmod 0644, $file;
    return (-w $file);
}

=pod

=head2 $file = $conf->save( [$package_name] )

Saves the configuration to the package name you provided.
If this package is not C<CPANPLUS::Config::System>, it will
be saved in your C<.cpanplus> directory, otherwise it will
be attempted to be saved in the system wide directory.

If no argument is provided, it will default to your personal
config.

Returns the full path to the file if the config was saved, 
false otherwise.

=cut

sub _config_pm_to_file {
    my $self = shift;
    my $pm   = shift or return;
    my $dir  = shift || CONFIG_USER_LIB_DIR->();

    ### only 3 types of files know: home, system and 'other'
    ### so figure out where to save them based on their type
    my $file;
    if( $pm eq CONFIG_USER ) {
        $file = CONFIG_USER_FILE->();   

    } elsif ( $pm eq CONFIG_SYSTEM ) {
        $file = CONFIG_SYSTEM_FILE->();
        
    ### third party file        
    } else {
        my $cfg_pkg = CONFIG . '::';
        unless( $pm =~ /^$cfg_pkg/ ) {
            error(loc(
                "WARNING: Your config package '%1' is not in the '%2' ".
                "namespace and will not be automatically detected by %3",
                $pm, $cfg_pkg, 'CPANPLUS'
            ));        
        }                        
    
        $file = File::Spec->catfile(
            $dir,
            split( '::', $pm )
        ) . '.pm';        
    }

    return $file;
}


sub save {
    my $self    = shift;
    my $pm      = shift || CONFIG_USER;
    my $savedir = shift || '';
    
    my $file = $self->_config_pm_to_file( $pm, $savedir ) or return;
    my $dir  = dirname( $file );
    
    unless( -d $dir ) {
        $self->_mkdir( dir => $dir ) or (
            error(loc("Can not create directory '%1' to save config to",$dir)),
            return
        )
    }       
    return unless $self->can_save($file);

    ### find only accesors that are not private
    my @acc = sort grep { $_ !~ /^_/ } $self->conf->ls_accessors;

    ### for dumping the values
    use Data::Dumper;
    
    my @lines;
    for my $acc ( @acc ) {
        
        push @lines, "### $acc section", $/;
        
        for my $key ( $self->conf->$acc->ls_accessors ) {
            my $val = Dumper( $self->conf->$acc->$key );
        
            $val =~ s/\$VAR1\s+=\s+//;
            $val =~ s/;\n//;
        
            push @lines, '$'. "conf->set_${acc}( $key => $val );", $/;
        }
        push @lines, $/,$/;

    }

    my $str = join '', map { "    $_" } @lines;

    ### use a variable to make sure the pod parser doesn't snag it
    my $is      = '=';
    my $time    = gmtime;
   
    
    my $msg     = <<_END_OF_CONFIG_;
###############################################
###                                         
###  Configuration structure for $pm        
###                                         
###############################################

#last changed: $time GMT

### minimal pod, so you can find it with perldoc -l, etc
${is}pod

${is}head1 NAME

$pm

${is}head1 DESCRIPTION

This is a CPANPLUS configuration file. Editing this
config changes the way CPANPLUS will behave

${is}cut

package $pm;

use strict;

sub setup {
    my \$conf = shift;
    
$str

    return 1;    
} 

1;

_END_OF_CONFIG_

    $self->_move( file => $file, to => "$file~" ) if -f $file;

    my $fh = new FileHandle;
    $fh->open(">$file")
        or (error(loc("Could not open '%1' for writing: %2", $file, $!)),
            return );

    $fh->print($msg);
    $fh->close;

    return $file;
}

=pod

=head2 options( type => TYPE )

Returns a list of all valid config options given a specific type
(like for example C<conf> of C<program>) or false if the type does
not exist

=cut

sub options {
    my $self = shift;
    my $conf = $self->conf;
    my %hash = @_;

    my $type;
    my $tmpl = {
        type    => { required       => 1, default   => '',
                     strict_type    => 1, store     => \$type },
    };

    check($tmpl, \%hash) or return;

    my %seen;
    return sort grep { !$seen{$_}++ }
                map { $_->$type->ls_accessors if $_->can($type)  } 
                $self->conf;
    return;
}

=pod

=head1 ACCESSORS

Accessors that start with a C<_> are marked private -- regular users
should never need to use these.

See the C<CPANPLUS::Config> documentation for what items can be
set and retrieved.

=head2 get_SOMETHING( ITEM, [ITEM, ITEM, ... ] );

The C<get_*> style accessors merely retrieves one or more desired
config options.

=head2 set_SOMETHING( ITEM => VAL, [ITEM => VAL, ITEM => VAL, ... ] );

The C<set_*> style accessors set the current value for one
or more config options and will return true upon success, false on
failure.

=head2 add_SOMETHING( ITEM => VAL, [ITEM => VAL, ITEM => VAL, ... ] );

The C<add_*> style accessor adds a new key to a config key.

Currently, the following accessors exist:

=over 4

=item set|get_conf

Simple configuration directives like verbosity and favourite shell.

=item set|get_program

Location of helper programs.

=item _set|_get_build

Locations of where to put what files for CPANPLUS.

=item _set|_get_source

Locations and names of source files locally.

=item _set|_get_mirror

Locations and names of source files remotely.

=item _set|_get_fetch

Special settings pertaining to the fetching of files.

=back

=cut

sub AUTOLOAD {
    my $self    = shift;
    my $conf    = $self->conf;

    my $name    = $AUTOLOAD;
    $name       =~ s/.+:://;

    my ($private, $action, $field) =
                $name =~ m/^(_)?((?:[gs]et|add))_([a-z]+)$/;

    my $type = '';
    $type .= '_'    if $private;
    $type .= $field if $field;

    unless ( $conf->can($type) ) {
        error( loc("Invalid method type: '%1'", $name) );
        return;
    }

    unless( scalar @_ ) {
        error( loc("No arguments provided!") );
        return;
    }

    ### retrieve a current value for an existing key ###
    if( $action eq 'get' ) {
        for my $key (@_) {
            my @list = ();

            ### get it from the user config first
            if( $conf->can($type) and $conf->$type->can($key) ) {
                push @list, $conf->$type->$key;

            ### XXX EU::AI compatibility hack to provide lookups like in
            ### cpanplus 0.04x; we renamed ->_get_build('base') to
            ### ->get_conf('base')
            } elsif ( $type eq '_build' and $key eq 'base' ) {
                return $self->get_conf($key);  
                
            } else {     
                error( loc(q[No such key '%1' in field '%2'], $key, $type) );
                return;
            }

            return wantarray ? @list : $list[0];
        }

    ### set an existing key to a new value ###
    } elsif ( $action eq 'set' ) {
        my %args = @_;

        while( my($key,$val) = each %args ) {

            if( $conf->can($type) and $conf->$type->can($key) ) {
                $conf->$type->$key( $val );
                
            } else {
                error( loc(q[No such key '%1' in field '%2'], $key, $type) );
                return;
            }
        }

        return 1;

    ### add a new key to the config ###
    } elsif ( $action eq 'add' ) {
        my %args = @_;

        while( my($key,$val) = each %args ) {

            if( $conf->$type->can($key) ) {
                error( loc( q[Key '%1' already exists for field '%2'],
                            $key, $type));
                return;
            } else {
                $conf->$type->mk_accessors( $key );
                $conf->$type->$key( $val );
            }
        }
        return 1;

    } else {

        error( loc(q[Unknown action '%1'], $action) );
        return;
    }
}

sub DESTROY { 1 };

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

=head1 SEE ALSO

L<CPANPLUS::Backend>, L<CPANPLUS::Configure::Setup>, L<CPANPLUS::Config>

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

