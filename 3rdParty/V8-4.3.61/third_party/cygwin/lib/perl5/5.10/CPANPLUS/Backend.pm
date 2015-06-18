package CPANPLUS::Backend;

use strict;


use CPANPLUS::Error;
use CPANPLUS::Configure;
use CPANPLUS::Internals;
use CPANPLUS::Internals::Constants;
use CPANPLUS::Module;
use CPANPLUS::Module::Author;
use CPANPLUS::Backend::RV;

use FileHandle;
use File::Spec                  ();
use File::Spec::Unix            ();
use Params::Check               qw[check];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

$Params::Check::VERBOSE = 1;

use vars qw[@ISA $VERSION];

@ISA     = qw[CPANPLUS::Internals];
$VERSION = $CPANPLUS::Internals::VERSION;

### mark that we're running under CPANPLUS to spawned processes
$ENV{'PERL5_CPANPLUS_IS_RUNNING'} = $$;

### XXX version.pm MAY format this version, if it's in use... :(
### so for consistency, just call ->VERSION ourselves as well.
$ENV{'PERL5_CPANPLUS_IS_VERSION'} = __PACKAGE__->VERSION;

=pod

=head1 NAME

CPANPLUS::Backend

=head1 SYNOPSIS

    my $cb      = CPANPLUS::Backend->new;
    my $conf    = $cb->configure_object;

    my $author  = $cb->author_tree('KANE');
    my $mod     = $cb->module_tree('Some::Module');
    my $mod     = $cb->parse_module( module => 'Some::Module' );

    my @objs    = $cb->search(  type    => TYPE,
                                allow   => [...] );

    $cb->flush('all');
    $cb->reload_indices;
    $cb->local_mirror;


=head1 DESCRIPTION

This module provides the programmer's interface to the C<CPANPLUS>
libraries.

=head1 ENVIRONMENT

When C<CPANPLUS::Backend> is loaded, which is necessary for just
about every <CPANPLUS> operation, the environment variable
C<PERL5_CPANPLUS_IS_RUNNING> is set to the current process id.

Additionally, the environment variable C<PERL5_CPANPLUS_IS_VERSION> 
will be set to the version of C<CPANPLUS::Backend>.

This information might be useful somehow to spawned processes.

=head1 METHODS

=head2 $cb = CPANPLUS::Backend->new( [CONFIGURE_OBJ] )

This method returns a new C<CPANPLUS::Backend> object.
This also initialises the config corresponding to this object.
You have two choices in this:

=over 4

=item Provide a valid C<CPANPLUS::Configure> object

This will be used verbatim.

=item No arguments

Your default config will be loaded and used.

=back

New will return a C<CPANPLUS::Backend> object on success and die on
failure.

=cut

sub new {
    my $class   = shift;
    my $conf;

    if( $_[0] && IS_CONFOBJ->( conf => $_[0] ) ) {
        $conf = shift;
    } else {
        $conf = CPANPLUS::Configure->new() or return;
    }

    my $self = $class->SUPER::_init( _conf => $conf );

    return $self;
}

=pod

=head2 $href = $cb->module_tree( [@modules_names_list] )

Returns a reference to the CPANPLUS module tree.

If you give it any arguments, they will be treated as module names
and C<module_tree> will try to look up these module names and
return the corresponding module objects instead.

See L<CPANPLUS::Module> for the operations you can perform on a
module object.

=cut

sub module_tree {
    my $self    = shift;
    my $modtree = $self->_module_tree;

    if( @_ ) {
        my @rv;
        for my $name ( grep { defined } @_) {

            ### From John Malmberg: This is failing on VMS 
            ### because ODS-2 does not retain the case of 
            ### filenames that are created.
            ### The problem is the filename is being converted 
            ### to a module name and then looked up in the 
            ### %$modtree hash.
            ### 
            ### As a fix, we do a search on VMS instead --
            ### more cpu cycles, but it gets around the case
            ### problem --kane
            my ($modobj) = do {
                ON_VMS
                    ? $self->search(
                          type    => 'module',
                          allow   => [qr/^$name$/i],
                      )
                    : $modtree->{$name}
            };
            
            push @rv, $modobj || '';
        }
        return @rv == 1 ? $rv[0] : @rv;
    } else {
        return $modtree;
    }
}

=pod

=head2 $href = $cb->author_tree( [@author_names_list] )

Returns a reference to the CPANPLUS author tree.

If you give it any arguments, they will be treated as author names
and C<author_tree> will try to look up these author names and
return the corresponding author objects instead.

See L<CPANPLUS::Module::Author> for the operations you can perform on
an author object.

=cut

sub author_tree {
    my $self        = shift;
    my $authtree    = $self->_author_tree;

    if( @_ ) {
        my @rv;
        for my $name (@_) {
            push @rv, $authtree->{$name} || '';
        }
        return @rv == 1 ? $rv[0] : @rv;
    } else {
        return $authtree;
    }
}

=pod

=head2 $conf = $cb->configure_object;

Returns a copy of the C<CPANPLUS::Configure> object.

See L<CPANPLUS::Configure> for operations you can perform on a
configure object.

=cut

sub configure_object { return shift->_conf() };

=head2 $su = $cb->selfupdate_object;

Returns a copy of the C<CPANPLUS::Selfupdate> object.

See the L<CPANPLUS::Selfupdate> manpage for the operations
you can perform on the selfupdate object.

=cut

sub selfupdate_object { return shift->_selfupdate() };

=pod

=head2 @mods = $cb->search( type => TYPE, allow => AREF, [data => AREF, verbose => BOOL] )

C<search> enables you to search for either module or author objects,
based on their data. The C<type> you can specify is any of the
accessors specified in C<CPANPLUS::Module::Author> or
C<CPANPLUS::Module>. C<search> will determine by the C<type> you
specified whether to search by author object or module object.

You have to specify an array reference of regular expressions or
strings to match against. The rules used for this array ref are the
same as in C<Params::Check>, so read that manpage for details.

The search is an C<or> search, meaning that if C<any> of the criteria
match, the search is considered to be successful.

You can specify the result of a previous search as C<data> to limit
the new search to these module or author objects, rather than the
entire module or author tree.  This is how you do C<and> searches.

Returns a list of module or author objects on success and false
on failure.

See L<CPANPLUS::Module> for the operations you can perform on a
module object.
See L<CPANPLUS::Module::Author> for the operations you can perform on
an author object.

=cut

sub search {
    my $self = shift;
    my $conf = $self->configure_object;
    my %hash = @_;

    my ($type);
    my $args = do {
        local $Params::Check::NO_DUPLICATES = 0;
        local $Params::Check::ALLOW_UNKNOWN = 1;

        my $tmpl = {
            type    => { required => 1, allow => [CPANPLUS::Module->accessors(),
                            CPANPLUS::Module::Author->accessors()], store => \$type },
            allow   => { required => 1, default => [ ], strict_type => 1 },
        };

        check( $tmpl, \%hash )
    } or return;

    ### figure out whether it was an author or a module search
    ### when ambiguous, it'll be an author search.
    my $aref;
    if( grep { $type eq $_ } CPANPLUS::Module::Author->accessors() ) {
        $aref = $self->_search_author_tree( %$args );
    } else {
        $aref = $self->_search_module_tree( %$args );
    }

    return @$aref if $aref;
    return;
}

=pod

=head2 $backend_rv = $cb->fetch( modules => \@mods )

Fetches a list of modules. C<@mods> can be a list of distribution
names, module names or module objects--basically anything that
L<parse_module> can understand.

See the equivalent method in C<CPANPLUS::Module> for details on
other options you can pass.

Since this is a multi-module method call, the return value is
implemented as a C<CPANPLUS::Backend::RV> object. Please consult
that module's documentation on how to interpret the return value.

=head2 $backend_rv = $cb->extract( modules => \@mods )

Extracts a list of modules. C<@mods> can be a list of distribution
names, module names or module objects--basically anything that
L<parse_module> can understand.

See the equivalent method in C<CPANPLUS::Module> for details on
other options you can pass.

Since this is a multi-module method call, the return value is
implemented as a C<CPANPLUS::Backend::RV> object. Please consult
that module's documentation on how to interpret the return value.

=head2 $backend_rv = $cb->install( modules => \@mods )

Installs a list of modules. C<@mods> can be a list of distribution
names, module names or module objects--basically anything that
L<parse_module> can understand.

See the equivalent method in C<CPANPLUS::Module> for details on
other options you can pass.

Since this is a multi-module method call, the return value is
implemented as a C<CPANPLUS::Backend::RV> object. Please consult
that module's documentation on how to interpret the return value.

=head2 $backend_rv = $cb->readme( modules => \@mods )

Fetches the readme for a list of modules. C<@mods> can be a list of
distribution names, module names or module objects--basically
anything that L<parse_module> can understand.

See the equivalent method in C<CPANPLUS::Module> for details on
other options you can pass.

Since this is a multi-module method call, the return value is
implemented as a C<CPANPLUS::Backend::RV> object. Please consult
that module's documentation on how to interpret the return value.

=head2 $backend_rv = $cb->files( modules => \@mods )

Returns a list of files used by these modules if they are installed.
C<@mods> can be a list of distribution names, module names or module
objects--basically anything that L<parse_module> can understand.

See the equivalent method in C<CPANPLUS::Module> for details on
other options you can pass.

Since this is a multi-module method call, the return value is
implemented as a C<CPANPLUS::Backend::RV> object. Please consult
that module's documentation on how to interpret the return value.

=head2 $backend_rv = $cb->distributions( modules => \@mods )

Returns a list of module objects representing all releases for this
module on success.
C<@mods> can be a list of distribution names, module names or module
objects, basically anything that L<parse_module> can understand.

See the equivalent method in C<CPANPLUS::Module> for details on
other options you can pass.

Since this is a multi-module method call, the return value is
implemented as a C<CPANPLUS::Backend::RV> object. Please consult
that module's documentation on how to interpret the return value.

=cut

### XXX add direcotry_tree, packlist etc? or maybe remove files? ###
for my $func (qw[fetch extract install readme files distributions]) {
    no strict 'refs';

    *$func = sub {
        my $self = shift;
        my $conf = $self->configure_object;
        my %hash = @_;

        local $Params::Check::NO_DUPLICATES = 1;
        local $Params::Check::ALLOW_UNKNOWN = 1;

        my ($mods);
        my $tmpl = {
            modules     => { default  => [],    strict_type => 1,
                             required => 1,     store => \$mods },
        };

        my $args = check( $tmpl, \%hash ) or return;

        ### make them all into module objects ###
        my %mods = map {$_ => $self->parse_module(module => $_) || ''} @$mods;

        my $flag; my $href;
        while( my($name,$obj) = each %mods ) {
            $href->{$name} = IS_MODOBJ->( mod => $obj )
                                ? $obj->$func( %$args )
                                : undef;

            $flag++ unless $href->{$name};
        }

        return CPANPLUS::Backend::RV->new(
                    function    => $func,
                    ok          => !$flag,
                    rv          => $href,
                    args        => \%hash,
                );
    }
}

=pod

=head2 $mod_obj = $cb->parse_module( module => $modname|$distname|$modobj|URI )

C<parse_module> tries to find a C<CPANPLUS::Module> object that
matches your query. Here's a list of examples you could give to
C<parse_module>;

=over 4

=item Text::Bastardize

=item Text-Bastardize

=item Text-Bastardize-1.06

=item AYRNIEU/Text-Bastardize

=item AYRNIEU/Text-Bastardize-1.06

=item AYRNIEU/Text-Bastardize-1.06.tar.gz

=item http://example.com/Text-Bastardize-1.06.tar.gz

=item file:///tmp/Text-Bastardize-1.06.tar.gz

=back

These items would all come up with a C<CPANPLUS::Module> object for
C<Text::Bastardize>. The ones marked explicitly as being version 1.06
would give back a C<CPANPLUS::Module> object of that version.
Even if the version on CPAN is currently higher.

If C<parse_module> is unable to actually find the module you are looking
for in its module tree, but you supplied it with an author, module
and version part in a distribution name or URI, it will create a fake
C<CPANPLUS::Module> object for you, that you can use just like the
real thing.

See L<CPANPLUS::Module> for the operations you can perform on a
module object.

If even this fancy guessing doesn't enable C<parse_module> to create
a fake module object for you to use, it will warn about an error and
return false.

=cut

sub parse_module {
    my $self = shift;
    my $conf = $self->configure_object;
    my %hash = @_;

    my $mod;
    my $tmpl = {
        module  => { required => 1, store => \$mod },
    };

    my $args = check( $tmpl, \%hash ) or return;

    return $mod if IS_MODOBJ->( module => $mod );

    ### ok, so it's not a module object, but a ref nonetheless?
    ### what are you smoking?
    if( ref $mod ) {
        error(loc("Can not parse module string from reference '%1'", $mod ));
        return;
    }
    
    ### check only for allowed characters in a module name
    unless( $mod =~ /[^\w:]/ ) {

        ### perhaps we can find it in the module tree?
        my $maybe = $self->module_tree($mod);
        return $maybe if IS_MODOBJ->( module => $maybe );
    }

    ### ok, so it looks like a distribution then?
    my @parts   = split '/', $mod;
    my $dist    = pop @parts;

    ### ah, it's a URL
    if( $mod =~ m|\w+://.+| ) {
        my $modobj = CPANPLUS::Module::Fake->new(
                        module  => $dist,
                        version => 0,
                        package => $dist,
                        path    => File::Spec::Unix->catdir(
                                        $conf->_get_mirror('base'),
                                        UNKNOWN_DL_LOCATION ),
                        author  => CPANPLUS::Module::Author::Fake->new
                    );
        
        ### set the fetch_from accessor so we know to by pass the
        ### usual mirrors
        $modobj->status->_fetch_from( $mod );
        
        ### better guess for the version
        $modobj->version( $modobj->package_version ) 
            if defined $modobj->package_version;
        
        ### better guess at module name, if possible
        if ( my $pkgname = $modobj->package_name ) {
            $pkgname =~ s/-/::/g;
        
            ### no sense replacing it unless we changed something
            $modobj->module( $pkgname ) 
                if ($pkgname ne $modobj->package_name) || $pkgname !~ /-/;
        }                
        
        return $modobj;      
    }
    
    ### perhaps we can find it's a third party module?
    {   my $modobj = CPANPLUS::Module::Fake->new(
                        module  => $mod,
                        version => 0,
                        package => $dist,
                        path    => File::Spec::Unix->catdir(
                                        $conf->_get_mirror('base'),
                                        UNKNOWN_DL_LOCATION ),
                        author  => CPANPLUS::Module::Author::Fake->new
                    );
        if( $modobj->is_third_party ) {
            my $info = $modobj->third_party_information;
            
            $modobj->author->author( $info->{author}     );
            $modobj->author->email(  $info->{author_url} );
            $modobj->description(    $info->{url} );

            return $modobj;
        }
    }

    unless( $dist ) {
        error( loc("%1 is not a proper distribution name!", $mod) );
        return;
    }
    
    ### there's wonky uris out there, like this:
    ### E/EY/EYCK/Net/Lite/Net-Lite-FTP-0.091
    ### compensate for that
    my $author;
    ### you probably have an A/AB/ABC/....../Dist.tgz type uri
    if( (defined $parts[0] and length $parts[0] == 1) and 
        (defined $parts[1] and length $parts[1] == 2) and
        $parts[2] =~ /^$parts[0]/i and $parts[2] =~ /^$parts[1]/i
    ) {   
        splice @parts, 0, 2;    # remove the first 2 entries from the list
        $author = shift @parts; # this is the actual author name then    

    ### we''ll assume a ABC/..../Dist.tgz
    } else {
        $author = shift @parts || '';
    }
    
    my($pkg, $version, $ext) = 
        $self->_split_package_string( package => $dist );
    
    ### translate a distribution into a module name ###
    my $guess = $pkg; 
    $guess =~ s/-/::/g if $guess; 

    my $maybe = $self->module_tree( $guess );
    if( IS_MODOBJ->( module => $maybe ) ) {

        ### maybe you asked for a package instead
        if ( $maybe->package eq $mod ) {
            return $maybe;

        ### perhaps an outdated version instead?
        } elsif ( $version ) {
            my $auth_obj; my $path;

            ### did you give us an author part? ###
            if( $author ) {
                $auth_obj   = CPANPLUS::Module::Author::Fake->new(
                                    _id     => $maybe->_id,
                                    cpanid  => uc $author,
                                    author  => uc $author,
                                );
                $path       = File::Spec::Unix->catdir(
                                    $conf->_get_mirror('base'),
                                    substr(uc $author, 0, 1),
                                    substr(uc $author, 0, 2),
                                    uc $author,
                                    @parts,     #possible sub dirs
                                );
            } else {
                $auth_obj   = $maybe->author;
                $path       = $maybe->path;
            }        
        
            if( $maybe->package_name eq $pkg ) {
    
                my $modobj = CPANPLUS::Module::Fake->new(
                    module  => $maybe->module,
                    version => $version,
                    package => $pkg . '-' . $version . '.' .
                                    $maybe->package_extension,
                    path    => $path,
                    author  => $auth_obj,
                    _id     => $maybe->_id
                );
                return $modobj;

            ### you asked for a specific version?
            ### assume our $maybe is the one you wanted,
            ### and fix up the version.. 
            } else {
    
                my $modobj = $maybe->clone;
                $modobj->version( $version );
                $modobj->package( 
                        $maybe->package_name .'-'. 
                        $version .'.'. 
                        $maybe->package_extension 
                );
                
                ### you wanted a specific author, but it's not the one
                ### from the module tree? we'll fix it up
                if( $author and $author ne $modobj->author->cpanid ) {
                    $modobj->author( $auth_obj );
                    $modobj->path( $path );
                }
                
                return $modobj;
            }
        
        ### you didn't care about a version, so just return the object then
        } elsif ( !$version ) {
            return $maybe;
        }

    ### ok, so we can't find it, and it's not an outdated dist either
    ### perhaps we can fake one based on the author name and so on
    } elsif ( $author and $version ) {

        ### be extra friendly and pad the .tar.gz suffix where needed
        ### it's just a guess of course, but most dists are .tar.gz
        $dist .= '.tar.gz' unless $dist =~ /\.[A-Za-z]+$/;

        ### XXX duplication from above for generating author obj + path...
        my $modobj = CPANPLUS::Module::Fake->new(
            module  => $guess,
            version => $version,
            package => $dist,
            author  => CPANPLUS::Module::Author::Fake->new(
                            author  => uc $author,
                            cpanid  => uc $author,
                            _id     => $self->_id,
                        ),
            path    => File::Spec::Unix->catdir(
                            $conf->_get_mirror('base'),
                            substr(uc $author, 0, 1),
                            substr(uc $author, 0, 2),
                            uc $author,
                            @parts,         #possible subdirs
                        ),
            _id     => $self->_id,
        );

        return $modobj;

    ### face it, we have /no/ idea what he or she wants...
    ### let's start putting the blame somewhere
    } else {

        unless( $author ) {
            error( loc( "'%1' does not contain an author part", $mod ) );
        }

        error( loc( "Cannot find '%1' in the module tree", $mod ) );
    }

    return;
}

=pod

=head2 $bool = $cb->reload_indices( [update_source => BOOL, verbose => BOOL] );

This method reloads the source files.

If C<update_source> is set to true, this will fetch new source files
from your CPAN mirror. Otherwise, C<reload_indices> will do its
usual cache checking and only update them if they are out of date.

By default, C<update_source> will be false.

The verbose setting defaults to what you have specified in your
config file.

Returns true on success and false on failure.

=cut

sub reload_indices {
    my $self    = shift;
    my %hash    = @_;
    my $conf    = $self->configure_object;

    my $tmpl = {
        update_source   => { default    => 0, allow => [qr/^\d$/] },
        verbose         => { default    => $conf->get_conf('verbose') },
    };

    my $args = check( $tmpl, \%hash ) or return;

    ### make a call to the internal _module_tree, so it triggers cache
    ### file age
    my $uptodate = $self->_check_trees( %$args );


    return 1 if $self->_build_trees(
                                uptodate    => $uptodate,
                                use_stored  => 0,
                                verbose     => $conf->get_conf('verbose'),
                            );

    error( loc( "Error rebuilding source trees!" ) );

    return;
}

=pod

=head2 $bool = $cb->flush(CACHE_NAME)

This method allows flushing of caches.
There are several things which can be flushed:

=over 4

=item * C<methods>

The return status of methods which have been attempted, such as
different ways of fetching files.  It is recommended that automatic
flushing be used instead.

=item * C<hosts>

The return status of URIs which have been attempted, such as
different hosts of fetching files.  It is recommended that automatic
flushing be used instead.

=item * C<modules>

Information about modules such as prerequisites and whether
installation succeeded, failed, or was not attempted.

=item * C<lib>

This resets PERL5LIB, which is changed to ensure that while installing
modules they are in our @INC.

=item * C<load>

This resets the cache of modules we've attempted to load, but failed.
This enables you to load them again after a failed load, if they 
somehow have become available.

=item * C<all>

Flush all of the aforementioned caches.

=back

Returns true on success and false on failure.

=cut

sub flush {
    my $self = shift;
    my $type = shift or return;

    my $cache = {
        methods => [ qw( methods load ) ],
        hosts   => [ qw( hosts ) ],
        modules => [ qw( modules lib) ],
        lib     => [ qw( lib ) ],
        load    => [ qw( load ) ],
        all     => [ qw( hosts lib modules methods load ) ],
    };

    my $aref = $cache->{$type}
                    or (
                        error( loc("No such cache '%1'", $type) ),
                        return
                    );

    return $self->_flush( list => $aref );
}

=pod

=head2 @mods = $cb->installed()

Returns a list of module objects of all your installed modules.
If an error occurs, it will return false.

See L<CPANPLUS::Module> for the operations you can perform on a
module object.

=cut

sub installed {
    my $self = shift;
    my $aref = $self->_all_installed;

    return @$aref if $aref;
    return;
}

=pod

=head2 $bool = $cb->local_mirror([path => '/dir/to/save/to', index_files => BOOL, force => BOOL, verbose => BOOL] )

Creates a local mirror of CPAN, of only the most recent sources in a
location you specify. If you set this location equal to a custom host
in your C<CPANPLUS::Config> you can use your local mirror to install
from.

It takes the following arguments:

=over 4

=item path

The location where to create the local mirror.

=item index_files

Enable/disable fetching of index files. You can disable fetching of the
index files if you don't plan to use the local mirror as your primary 
site, or if you'd like up-to-date index files be fetched from elsewhere.

Defaults to true.

=item force

Forces refetching of packages, even if they are there already.

Defaults to whatever setting you have in your C<CPANPLUS::Config>.

=item verbose

Prints more messages about what its doing.

Defaults to whatever setting you have in your C<CPANPLUS::Config>.

=back

Returns true on success and false on error.

=cut

sub local_mirror {
    my $self = shift;
    my $conf = $self->configure_object;
    my %hash = @_;

    my($path, $index, $force, $verbose);
    my $tmpl = {
        path        => { default => $conf->get_conf('base'),
                            store => \$path },
        index_files => { default => 1, store => \$index },
        force       => { default => $conf->get_conf('force'),
                            store => \$force },
        verbose     => { default => $conf->get_conf('verbose'),
                            store => \$verbose },
    };

    check( $tmpl, \%hash ) or return;

    unless( -d $path ) {
        $self->_mkdir( dir => $path )
                or( error( loc( "Could not create '%1', giving up", $path ) ),
                    return
                );
    } elsif ( ! -w _ ) {
        error( loc( "Could not write to '%1', giving up", $path ) );
        return;
    }

    my $flag;
    AUTHOR: {
    for my $auth (  sort { $a->cpanid cmp $b->cpanid }
                    values %{$self->author_tree}
    ) {

        MODULE: {
        my $i;
        for my $mod ( $auth->modules ) {
            my $fetchdir = File::Spec->catdir( $path, $mod->path );

            my %opts = (
                verbose     => $verbose,
                force       => $force,
                fetchdir    => $fetchdir,
            );

            ### only do this the for the first module ###
            unless( $i++ ) {
                $mod->_get_checksums_file(
                            %opts
                        ) or (
                            error( loc( "Could not fetch %1 file, " .
                                        "skipping author '%2'",
                                        CHECKSUMS, $auth->cpanid ) ),
                            $flag++, next AUTHOR
                        );
            }

            $mod->fetch( %opts )
                    or( error( loc( "Could not fetch '%1'", $mod->module ) ),
                        $flag++, next MODULE
                    );
        } }
    } }

    if( $index ) {
        for my $name (qw[auth dslip mod]) {
            $self->_update_source(
                        name    => $name,
                        verbose => $verbose,
                        path    => $path,
                    ) or ( $flag++, next );
        }
    }

    return !$flag;
}

=pod

=head2 $file = $cb->autobundle([path => OUTPUT_PATH, force => BOOL, verbose => BOOL])

Writes out a snapshot of your current installation in C<CPAN> bundle
style. This can then be used to install the same modules for a
different or on a different machine.

It will, by default, write to an 'autobundle' directory under your
cpanplus homedirectory, but you can override that by supplying a
C<path> argument.

It will return the location of the output file on success and false on
failure.

=cut

sub autobundle {
    my $self = shift;
    my $conf = $self->configure_object;
    my %hash = @_;

    my($path,$force,$verbose);
    my $tmpl = {
        force   => { default => $conf->get_conf('force'), store => \$force },
        verbose => { default => $conf->get_conf('verbose'), store => \$verbose },
        path    => { default => File::Spec->catdir(
                                        $conf->get_conf('base'),
                                        $self->_perl_version( perl => $^X ),
                                        $conf->_get_build('distdir'),
                                        $conf->_get_build('autobundle') ),
                    store => \$path },
    };

    check($tmpl, \%hash) or return;

    unless( -d $path ) {
        $self->_mkdir( dir => $path )
                or( error(loc("Could not create directory '%1'", $path ) ),
                    return
                );
    }

    my $name; my $file;
    {   ### default filename for the bundle ###
        my($year,$month,$day) = (localtime)[5,4,3];
        $year += 1900; $month++;

        my $ext = 0;

        my $prefix  = $conf->_get_build('autobundle_prefix');
        my $format  = "${prefix}_%04d_%02d_%02d_%02d";

        BLOCK: {
            $name = sprintf( $format, $year, $month, $day, $ext);

            $file = File::Spec->catfile( $path, $name . '.pm' );

            -f $file ? ++$ext && redo BLOCK : last BLOCK;
        }
    }
    my $fh;
    unless( $fh = FileHandle->new( ">$file" ) ) {
        error( loc( "Could not open '%1' for writing: %2", $file, $! ) );
        return;
    }
    
    ### make sure we load the module tree *before* doing this, as it
    ### starts to chdir all over the place
    $self->module_tree;

    my $string = join "\n\n",
                    map {
                        join ' ',
                            $_->module,
                            ($_->installed_version(verbose => 0) || 'undef')
                    } sort {
                        $a->module cmp $b->module
                    }  $self->installed;

    my $now     = scalar localtime;
    my $head    = '=head1';
    my $pkg     = __PACKAGE__;
    my $version = $self->VERSION;
    my $perl_v  = join '', `$^X -V`;

    print $fh <<EOF;
package $name

\$VERSION = '0.01';

1;

__END__

$head NAME

$name - Snapshot of your installation at $now

$head SYNOPSIS

perl -MCPANPLUS -e "install $name"

$head CONTENTS

$string

$head CONFIGURATION

$perl_v

$head AUTHOR

This bundle has been generated autotomatically by
    $pkg $version

EOF

    close $fh;

    return $file;
}

### XXX these wrappers are not individually tested! only the underlying
### code through source.t and indirectly trought he CustomSource plugin.
=pod

=head1 CUSTOM MODULE SOURCES

Besides the sources as provided by the general C<CPAN> mirrors, it's 
possible to add your own sources list to your C<CPANPLUS> index.

The methodology behind this works much like C<Debian's apt-sources>.

The methods below show you how to make use of this functionality. Also
note that most of these methods are available through the default shell
plugin command C</cs>, making them available as shortcuts through the
shell and via the commandline.

=head2 %files = $cb->list_custom_sources

Returns a mapping of registered custom sources and their local indices
as follows:

    /full/path/to/local/index => http://remote/source

Note that any file starting with an C<#> is being ignored.

=cut

sub list_custom_sources {
    return shift->__list_custom_module_sources( @_ );
}

=head2 $local_index = $cb->add_custom_source( uri => URI, [verbose => BOOL] );

Adds an C<URI> to your own sources list and mirrors its index. See the 
documentation on C<< $cb->update_custom_source >> on how this is done.

Returns the full path to the local index on success, or false on failure.

Note that when adding a new C<URI>, the change to the in-memory tree is
not saved until you rebuild or save the tree to disk again. You can do 
this using the C<< $cb->reload_indices >> method.

=cut

sub add_custom_source {
    return shift->_add_custom_module_source( @_ );
}

=head2 $local_index = $cb->remove_custom_source( uri => URI, [verbose => BOOL] );

Removes an C<URI> from your own sources list and removes its index.

To find out what C<URI>s you have as part of your own sources list, use
the C<< $cb->list_custom_sources >> method.

Returns the full path to the deleted local index file on success, or false
on failure.

=cut

### XXX do clever dispatching based on arg number?
sub remove_custom_source {
    return shift->_remove_custom_module_source( @_ );
}

=head2 $bool = $cb->update_custom_source( [remote => URI] );

Updates the indexes for all your custom sources. It does this by fetching
a file called C<packages.txt> in the root of the custom sources's C<URI>.
If you provide the C<remote> argument, it will only update the index for
that specific C<URI>.

Here's an example of how custom sources would resolve into index files:

  file:///path/to/sources       =>  file:///path/to/sources/packages.txt
  http://example.com/sources    =>  http://example.com/sources/packages.txt
  ftp://example.com/sources     =>  ftp://example.com/sources/packages.txt
  
The file C<packages.txt> simply holds a list of packages that can be found
under the root of the C<URI>. This file can be automatically generated for
you when the remote source is a C<file:// URI>. For C<http://>, C<ftp://>,
and similar, the administrator of that repository should run the method
C<< $cb->write_custom_source_index >> on the repository to allow remote
users to index it.

For details, see the C<< $cb->write_custom_source_index >> method below.

All packages that are added via this mechanism will be attributed to the
author with C<CPANID> C<LOCAL>. You can use this id to search for all 
added packages.

=cut

sub update_custom_source {
    my $self = shift;
    
    ### if it mentions /remote/, the request is to update a single uri,
    ### not all the ones we have, so dispatch appropriately
    my $rv = grep( /remote/i, @_)
        ? $self->__update_custom_module_source( @_ )
        : $self->__update_custom_module_sources( @_ );

    return $rv;
}    

=head2 $file = $cb->write_custom_source_index( path => /path/to/package/root, [to => /path/to/index/file, verbose => BOOL] );

Writes the index for a custom repository root. Most users will not have to 
worry about this, but administrators of a repository will need to make sure
their indexes are up to date.

The index will be written to a file called C<packages.txt> in your repository
root, which you can specify with the C<path> argument. You can override this
location by specifying the C<to> argument, but in normal operation, that should
not be required.

Once the index file is written, users can then add the C<URI> pointing to 
the repository to their custom list of sources and start using it right away. See the C<< $cb->add_custom_source >> method for user details.

=cut

sub write_custom_source_index {
    return shift->__write_custom_module_index( @_ );
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

=head1 SEE ALSO

L<CPANPLUS::Configure>, L<CPANPLUS::Module>, L<CPANPLUS::Module::Author>, 
L<CPANPLUS::Selfupdate>

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

__END__

todo:
sub dist {          # not sure about this one -- probably already done
                      enough in Module.pm
sub reports {       # in Module.pm, wrapper here


