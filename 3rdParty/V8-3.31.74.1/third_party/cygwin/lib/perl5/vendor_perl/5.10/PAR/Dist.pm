package PAR::Dist;
require Exporter;
use vars qw/$VERSION @ISA @EXPORT @EXPORT_OK/;

$VERSION    = '0.29';
@ISA	    = 'Exporter';
@EXPORT	    = qw/
  blib_to_par
  install_par
  uninstall_par
  sign_par
  verify_par
  merge_par
  remove_man
  get_meta
  generate_blib_stub
/;

@EXPORT_OK = qw/
  parse_dist_name
  contains_binaries
/;

use strict;
use Carp qw/carp croak/;
use File::Spec;

=head1 NAME

PAR::Dist - Create and manipulate PAR distributions

=head1 VERSION

This document describes version 0.28 of PAR::Dist, released Feb  5, 2008.

=head1 SYNOPSIS

As a shell command:

    % perl -MPAR::Dist -eblib_to_par

In programs:

    use PAR::Dist;

    my $dist = blib_to_par();	# make a PAR file using ./blib/
    install_par($dist);		# install it into the system
    uninstall_par($dist);	# uninstall it from the system
    sign_par($dist);		# sign it using Module::Signature
    verify_par($dist);		# verify it using Module::Signature

    install_par("http://foo.com/DBI-1.37-MSWin32-5.8.0.par"); # works too
    install_par("http://foo.com/DBI-1.37"); # auto-appends archname + perlver
    install_par("cpan://SMUELLER/PAR-Packer-0.975"); # uses CPAN author directory

=head1 DESCRIPTION

This module creates and manipulates I<PAR distributions>.  They are
architecture-specific B<PAR> files, containing everything under F<blib/>
of CPAN distributions after their C<make> or C<Build> stage, a
F<META.yml> describing metadata of the original CPAN distribution, 
and a F<MANIFEST> detailing all files within it.  Digitally signed PAR
distributions will also contain a F<SIGNATURE> file.

The naming convention for such distributions is:

    $NAME-$VERSION-$ARCH-$PERL_VERSION.par

For example, C<PAR-Dist-0.01-i386-freebsd-5.8.0.par> corresponds to the
0.01 release of C<PAR-Dist> on CPAN, built for perl 5.8.0 running on
C<i386-freebsd>.

=head1 FUNCTIONS

Several functions are exported by default.  Unless otherwise noted,
they can take either a hash of
named arguments, a single argument (taken as C<$path> by C<blib_to_par>
and C<$dist> by other functions), or no arguments (in which case
the first PAR file in the current directory is used).

Therefore, under a directory containing only a single F<test.par>, all
invocations below are equivalent:

    % perl -MPAR::Dist -e"install_par( dist => 'test.par' )"
    % perl -MPAR::Dist -e"install_par( 'test.par' )"
    % perl -MPAR::Dist -einstall_par;

If C<$dist> resembles a URL, C<LWP::Simple::mirror> is called to mirror it
locally under C<$ENV{PAR_TEMP}> (or C<$TEMP/par/> if unspecified), and the
function will act on the fetched local file instead.  If the URL begins
with C<cpan://AUTHOR/>, it will be expanded automatically to the author's CPAN
directory (e.g. C<http://www.cpan.org/modules/by-authors/id/A/AU/AUTHOR/>).

If C<$dist> does not have a file extension beginning with a letter or
underscore, a dash and C<$suffix> ($ARCH-$PERL_VERSION.par by default)
will be appended to it.

=head2 blib_to_par

Takes key/value pairs as parameters or a single parameter indicating the
path that contains the F<blib/> subdirectory.

Builds a PAR distribution from the F<blib/> subdirectory under C<path>, or
under the current directory if unspecified.  If F<blib/> does not exist,
it automatically runs F<Build>, F<make>, F<Build.PL> or F<Makefile.PL> to
create it.

Returns the filename or the generated PAR distribution.

Valid parameters are:

=over 2

=item path

Sets the path which contains the F<blib/> subdirectory from which the PAR
distribution will be generated.

=item name, version, suffix

These attributes set the name, version and platform specific suffix
of the distribution. Name and version can be automatically
determined from the distributions F<META.yml> or F<Makefile.PL> files.

The suffix is generated from your architecture name and your version of
perl by default.

=item dist

The output filename for the PAR distribution.

=back

=cut

sub blib_to_par {
    @_ = (path => @_) if @_ == 1;

    my %args = @_;
    require Config;


    # don't use 'my $foo ... if ...' it creates a static variable!
    my $dist;
    my $path	= $args{path};
    $dist	= File::Spec->rel2abs($args{dist}) if $args{dist};
    my $name	= $args{name};
    my $version	= $args{version};
    my $suffix	= $args{suffix} || "$Config::Config{archname}-$Config::Config{version}.par";
    my $cwd;

    if (defined $path) {
        require Cwd;
        $cwd = Cwd::cwd();
        chdir $path;
    }

    _build_blib() unless -d "blib";

    my @files;
    open MANIFEST, ">", File::Spec->catfile("blib", "MANIFEST") or die $!;
    open META, ">", File::Spec->catfile("blib", "META.yml") or die $!;
    
    require File::Find;
    File::Find::find( sub {
        next unless $File::Find::name;
        (-r && !-d) and push ( @files, substr($File::Find::name, 5) );
    } , 'blib' );

    print MANIFEST join(
	"\n",
	'    <!-- accessible as jar:file:///NAME.par!/MANIFEST in compliant browsers -->',
	(sort @files),
	q(    # <html><body onload="var X=document.body.innerHTML.split(/\n/);var Y='<iframe src=&quot;META.yml&quot; style=&quot;float:right;height:40%;width:40%&quot;></iframe><ul>';for(var x in X){if(!X[x].match(/^\s*#/)&&X[x].length)Y+='<li><a href=&quot;'+X[x]+'&quot;>'+X[x]+'</a>'}document.body.innerHTML=Y">)
    );
    close MANIFEST;

    if (open(OLD_META, "META.yml")) {
        while (<OLD_META>) {
            if (/^distribution_type:/) {
                print META "distribution_type: par\n";
            }
            else {
                print META $_;
            }

            if (/^name:\s+(.*)/) {
                $name ||= $1;
                $name =~ s/::/-/g;
            }
            elsif (/^version:\s+.*Module::Build::Version/) {
                while (<OLD_META>) {
                    /^\s+original:\s+(.*)/ or next;
                    $version ||= $1;
                    last;
                }
            }
            elsif (/^version:\s+(.*)/) {
                $version ||= $1;
            }
        }
        close OLD_META;
        close META;
    }
    
    if ((!$name or !$version) and open(MAKEFILE, "Makefile")) {
        while (<MAKEFILE>) {
            if (/^DISTNAME\s+=\s+(.*)$/) {
                $name ||= $1;
            }
            elsif (/^VERSION\s+=\s+(.*)$/) {
                $version ||= $1;
            }
        }
    }

    if (not defined($name) or not defined($version)) {
        # could not determine name or version. Error.
        my $what;
        if (not defined $name) {
            $what = 'name';
            $what .= ' and version' if not defined $version;
        }
        elsif (not defined $version) {
            $what = 'version';
        }
        
        carp("I was unable to determine the $what of the PAR distribution. Please create a Makefile or META.yml file from which we can infer the information or just specify the missing information as an option to blib_to_par.");
        return();
    }
    
    $name =~ s/\s+$//;
    $version =~ s/\s+$//;

    my $file = "$name-$version-$suffix";
    unlink $file if -f $file;

    print META << "YAML" if fileno(META);
name: $name
version: $version
build_requires: {}
conflicts: {}
dist_name: $file
distribution_type: par
dynamic_config: 0
generated_by: 'PAR::Dist version $PAR::Dist::VERSION'
license: unknown
YAML
    close META;

    mkdir('blib', 0777);
    chdir('blib');
    _zip(dist => File::Spec->catfile(File::Spec->updir, $file)) or die $!;
    chdir(File::Spec->updir);

    unlink File::Spec->catfile("blib", "MANIFEST");
    unlink File::Spec->catfile("blib", "META.yml");

    $dist ||= File::Spec->catfile($cwd, $file) if $cwd;

    if ($dist and $file ne $dist) {
        rename( $file => $dist );
        $file = $dist;
    }

    my $pathname = File::Spec->rel2abs($file);
    if ($^O eq 'MSWin32') {
        $pathname =~ s!\\!/!g;
        $pathname =~ s!:!|!g;
    };
    print << ".";
Successfully created binary distribution '$file'.
Its contents are accessible in compliant browsers as:
    jar:file://$pathname!/MANIFEST
.

    chdir $cwd if $cwd;
    return $file;
}

sub _build_blib {
    if (-e 'Build') {
        system($^X, "Build");
    }
    elsif (-e 'Makefile') {
        system($Config::Config{make});
    }
    elsif (-e 'Build.PL') {
        system($^X, "Build.PL");
        system($^X, "Build");
    }
    elsif (-e 'Makefile.PL') {
        system($^X, "Makefile.PL");
        system($Config::Config{make});
    }
}

=head2 install_par

Installs a PAR distribution into the system, using
C<ExtUtils::Install::install_default>.

Valid parameters are:

=over 2

=item dist

The .par file to install. The heuristics outlined in the B<FUNCTIONS>
section above apply.

=item prefix

This string will be prepended to all installation paths.
If it isn't specified, the environment variable
C<PERL_INSTALL_ROOT> is used as a prefix.

=back

Additionally, you can use several parameters to change the default
installation destinations. You don't usually have to worry about this
unless you are installing into a user-local directory.
The following section outlines the parameter names and default settings:

  Parameter         From          To
  inst_lib          blib/lib      $Config{installsitelib} (*)
  inst_archlib      blib/arch     $Config{installsitearch}
  inst_script       blib/script   $Config{installscript}
  inst_bin          blib/bin      $Config{installbin}
  inst_man1dir      blib/man1     $Config{installman1dir}
  inst_man3dir      blib/man3     $Config{installman3dir}
  packlist_read                   $Config{sitearchexp}/auto/$name/.packlist
  packlist_write                  $Config{installsitearch}/auto/$name/.packlist

The C<packlist_write> parameter is used to control where the F<.packlist>
file is written to. (Necessary for uninstallation.)
The C<packlist_read> parameter specifies a .packlist file to merge in if
it exists. By setting any of the above installation targets to C<undef>,
you can remove that target altogether. For example, passing
C<inst_man1dir => undef, inst_man3dir => undef> means that the contained
manual pages won't be installed. This is not available for the packlists.

Finally, you may specify a C<custom_targets> parameter. Its value should be
a reference to a hash of custom installation targets such as

  custom_targets => { 'blib/my_data' => '/some/path/my_data' }

You can use this to install the F<.par> archives contents to arbitrary
locations.

If only a single parameter is given, it is treated as the C<dist>
parameter.

=cut

sub install_par {
    my %args = &_args;
    _install_or_uninstall(%args, action => 'install');
}

=head2 uninstall_par

Uninstalls all previously installed contents of a PAR distribution,
using C<ExtUtils::Install::uninstall>.

Takes almost the same parameters as C<install_par>, but naturally,
the installation target parameters do not apply. The only exception
to this is the C<packlist_read> parameter which specifies the
F<.packlist> file to read the list of installed files from.
It defaults to C<$Config::Config{installsitearch}/auto/$name/.packlist>.

=cut

sub uninstall_par {
    my %args = &_args;
    _install_or_uninstall(%args, action => 'uninstall');
}

sub _install_or_uninstall {
    my %args = &_args;
    my $name = $args{name};
    my $action = $args{action};

    my %ENV_copy = %ENV;
    $ENV{PERL_INSTALL_ROOT} = $args{prefix} if defined $args{prefix};

    require Cwd;
    my $old_dir = Cwd::cwd();
    
    my ($dist, $tmpdir) = _unzip_to_tmpdir( dist => $args{dist}, subdir => 'blib' );

    if ( open (META, File::Spec->catfile('blib', 'META.yml')) ) {
        while (<META>) {
            next unless /^name:\s+(.*)/;
            $name = $1;
            $name =~ s/\s+$//;
            last;
        }
        close META;
    }
    return if not defined $name or $name eq '';

    if (-d 'script') {
        require ExtUtils::MY;
        foreach my $file (glob("script/*")) {
            next unless -T $file;
            ExtUtils::MY->fixin($file);
            chmod(0555, $file);
        }
    }

    $name =~ s{::|-}{/}g;
    require ExtUtils::Install;

    my $rv;
    if ($action eq 'install') {
        my $target = _installation_target( File::Spec->curdir, $name, \%args );
        my $custom_targets = $args{custom_targets} || {};
        $target->{$_} = $custom_targets->{$_} foreach keys %{$custom_targets};
        
        $rv = ExtUtils::Install::install($target, 1, 0, 0);
    }
    elsif ($action eq 'uninstall') {
        require Config;
        $rv = ExtUtils::Install::uninstall(
            $args{packlist_read}||"$Config::Config{installsitearch}/auto/$name/.packlist"
        );
    }

    %ENV = %ENV_copy;

    chdir($old_dir);
    File::Path::rmtree([$tmpdir]);
    return $rv;
}

# Returns the default installation target as used by
# ExtUtils::Install::install(). First parameter should be the base
# directory containing the blib/ we're installing from.
# Second parameter should be the name of the distribution for the packlist
# paths. Third parameter may be a hash reference with user defined keys for
# the target hash. In fact, any contents that do not start with 'inst_' are
# skipped.
sub _installation_target {
    require Config;
    my $dir = shift;
    my $name = shift;
    my $user = shift || {};

    # accepted sources (and user overrides)
    my %sources = (
      inst_lib => File::Spec->catdir($dir,"blib","lib"),
      inst_archlib => File::Spec->catdir($dir,"blib","arch"),
      inst_bin => File::Spec->catdir($dir,'blib','bin'),
      inst_script => File::Spec->catdir($dir,'blib','script'),
      inst_man1dir => File::Spec->catdir($dir,'blib','man1'),
      inst_man3dir => File::Spec->catdir($dir,'blib','man3'),
      packlist_read => 'read',
      packlist_write => 'write',
    );


    # default targets
    my $target = {
       read => $Config::Config{sitearchexp}."/auto/$name/.packlist",
       write => $Config::Config{installsitearch}."/auto/$name/.packlist",
       $sources{inst_lib}
            => (_directory_not_empty($sources{inst_archlib}))
            ? $Config::Config{installsitearch}
            : $Config::Config{installsitelib},
       $sources{inst_archlib}   => $Config::Config{installsitearch},
       $sources{inst_bin}       => $Config::Config{installbin} ,
       $sources{inst_script}    => $Config::Config{installscript},
       $sources{inst_man1dir}   => $Config::Config{installman1dir},
       $sources{inst_man3dir}   => $Config::Config{installman3dir},
    };
    
    # Included for future support for ${flavour}perl external lib installation
#    if ($Config::Config{flavour_perl}) {
#        my $ext = File::Spec->catdir($dir, 'blib', 'ext');
#        # from => to
#        $sources{inst_external_lib}    = File::Spec->catdir($ext, 'lib');
#        $sources{inst_external_bin}    = File::Spec->catdir($ext, 'bin');
#        $sources{inst_external_include} = File::Spec->catdir($ext, 'include');
#        $sources{inst_external_src}    = File::Spec->catdir($ext, 'src');
#        $target->{ $sources{inst_external_lib} }     = $Config::Config{flavour_install_lib};
#        $target->{ $sources{inst_external_bin} }     = $Config::Config{flavour_install_bin};
#        $target->{ $sources{inst_external_include} } = $Config::Config{flavour_install_include};
#        $target->{ $sources{inst_external_src} }     = $Config::Config{flavour_install_src};
#    }
    
    # insert user overrides
    foreach my $key (keys %$user) {
        my $value = $user->{$key};
        if (not defined $value and $key ne 'packlist_read' and $key ne 'packlist_write') {
          # undef means "remove"
          delete $target->{ $sources{$key} };
        }
        elsif (exists $sources{$key}) {
          # overwrite stuff, don't let the user create new entries
          $target->{ $sources{$key} } = $value;
        }
    }

    return $target;
}

sub _directory_not_empty {
    require File::Find;
    my($dir) = @_;
    my $files = 0;
    File::Find::find(sub {
	    return if $_ eq ".exists";
        if (-f) {
            $File::Find::prune++;
            $files = 1;
            }
    }, $dir);
    return $files;
}

=head2 sign_par

Digitally sign a PAR distribution using C<gpg> or B<Crypt::OpenPGP>,
via B<Module::Signature>.

=cut

sub sign_par {
    my %args = &_args;
    _verify_or_sign(%args, action => 'sign');
}

=head2 verify_par

Verify the digital signature of a PAR distribution using C<gpg> or
B<Crypt::OpenPGP>, via B<Module::Signature>.

Returns a boolean value indicating whether verification passed; C<$!>
is set to the return code of C<Module::Signature::verify>.

=cut

sub verify_par {
    my %args = &_args;
    $! = _verify_or_sign(%args, action => 'verify');
    return ( $! == Module::Signature::SIGNATURE_OK() );
}

=head2 merge_par

Merge two or more PAR distributions into one. First argument must
be the name of the distribution you want to merge all others into.
Any following arguments will be interpreted as the file names of
further PAR distributions to merge into the first one.

  merge_par('foo.par', 'bar.par', 'baz.par')

This will merge the distributions C<foo.par>, C<bar.par> and C<baz.par>
into the distribution C<foo.par>. C<foo.par> will be overwritten!
The original META.yml of C<foo.par> is retained.

=cut

sub merge_par {
    my $base_par = shift;
    my @additional_pars = @_;
    require Cwd;
    require File::Copy;
    require File::Path;
    require File::Find;

    # parameter checking
    if (not defined $base_par) {
        croak "First argument to merge_par() must be the .par archive to modify.";
    }

    if (not -f $base_par or not -r _ or not -w _) {
        croak "'$base_par' is not a file or you do not have enough permissions to read and modify it.";
    }
    
    foreach (@additional_pars) {
        if (not -f $_ or not -r _) {
            croak "'$_' is not a file or you do not have enough permissions to read it.";
        }
    }

    # The unzipping will change directories. Remember old dir.
    my $old_cwd = Cwd::cwd();
    
    # Unzip the base par to a temp. dir.
    (undef, my $base_dir) = _unzip_to_tmpdir(
        dist => $base_par, subdir => 'blib'
    );
    my $blibdir = File::Spec->catdir($base_dir, 'blib');

    # move the META.yml to the (main) temp. dir.
    File::Copy::move(
        File::Spec->catfile($blibdir, 'META.yml'),
        File::Spec->catfile($base_dir, 'META.yml')
    );
    # delete (incorrect) MANIFEST
    unlink File::Spec->catfile($blibdir, 'MANIFEST');

    # extract additional pars and merge    
    foreach my $par (@additional_pars) {
        # restore original directory because the par path
        # might have been relative!
        chdir($old_cwd);
        (undef, my $add_dir) = _unzip_to_tmpdir(
            dist => $par
        );
        my @files;
        my @dirs;
        # I hate File::Find
        # And I hate writing portable code, too.
        File::Find::find(
            {wanted =>sub {
                my $file = $File::Find::name;
                push @files, $file if -f $file;
                push @dirs, $file if -d _;
            }},
            $add_dir
        );
        my ($vol, $subdir, undef) = File::Spec->splitpath( $add_dir, 1);
        my @dir = File::Spec->splitdir( $subdir );
    
        # merge directory structure
        foreach my $dir (@dirs) {
            my ($v, $d, undef) = File::Spec->splitpath( $dir, 1 );
            my @d = File::Spec->splitdir( $d );
            shift @d foreach @dir; # remove tmp dir from path
            my $target = File::Spec->catdir( $blibdir, @d );
            mkdir($target);
        }

        # merge files
        foreach my $file (@files) {
            my ($v, $d, $f) = File::Spec->splitpath( $file );
            my @d = File::Spec->splitdir( $d );
            shift @d foreach @dir; # remove tmp dir from path
            my $target = File::Spec->catfile(
                File::Spec->catdir( $blibdir, @d ),
                $f
            );
            File::Copy::copy($file, $target)
              or die "Could not copy '$file' to '$target': $!";
            
        }
        chdir($old_cwd);
        File::Path::rmtree([$add_dir]);
    }
    
    # delete (copied) MANIFEST and META.yml
    unlink File::Spec->catfile($blibdir, 'MANIFEST');
    unlink File::Spec->catfile($blibdir, 'META.yml');
    
    chdir($base_dir);
    my $resulting_par_file = Cwd::abs_path(blib_to_par());
    chdir($old_cwd);
    File::Copy::move($resulting_par_file, $base_par);
    
    File::Path::rmtree([$base_dir]);
}


=head2 remove_man

Remove the man pages from a PAR distribution. Takes one named
parameter: I<dist> which should be the name (and path) of the
PAR distribution file. The calling conventions outlined in
the C<FUNCTIONS> section above apply.

The PAR archive will be
extracted, stripped of all C<man\d?> and C<html> subdirectories
and then repackaged into the original file.

=cut

sub remove_man {
    my %args = &_args;
    my $par = $args{dist};
    require Cwd;
    require File::Copy;
    require File::Path;
    require File::Find;

    # parameter checking
    if (not defined $par) {
        croak "First argument to remove_man() must be the .par archive to modify.";
    }

    if (not -f $par or not -r _ or not -w _) {
        croak "'$par' is not a file or you do not have enough permissions to read and modify it.";
    }
    
    # The unzipping will change directories. Remember old dir.
    my $old_cwd = Cwd::cwd();
    
    # Unzip the base par to a temp. dir.
    (undef, my $base_dir) = _unzip_to_tmpdir(
        dist => $par, subdir => 'blib'
    );
    my $blibdir = File::Spec->catdir($base_dir, 'blib');

    # move the META.yml to the (main) temp. dir.
    File::Copy::move(
        File::Spec->catfile($blibdir, 'META.yml'),
        File::Spec->catfile($base_dir, 'META.yml')
    );
    # delete (incorrect) MANIFEST
    unlink File::Spec->catfile($blibdir, 'MANIFEST');

    opendir DIRECTORY, 'blib' or die $!;
    my @dirs = grep { /^blib\/(?:man\d*|html)$/ }
               grep { -d $_ }
               map  { File::Spec->catfile('blib', $_) }
               readdir DIRECTORY;
    close DIRECTORY;
    
    File::Path::rmtree(\@dirs);
    
    chdir($base_dir);
    my $resulting_par_file = Cwd::abs_path(blib_to_par());
    chdir($old_cwd);
    File::Copy::move($resulting_par_file, $par);
    
    File::Path::rmtree([$base_dir]);
}


=head2 get_meta

Opens a PAR archive and extracts the contained META.yml file.
Returns the META.yml file as a string.

Takes one named parameter: I<dist>. If only one parameter is
passed, it is treated as the I<dist> parameter. (Have a look
at the description in the C<FUNCTIONS> section above.)

Returns undef if no PAR archive or no META.yml within the
archive were found.

=cut

sub get_meta {
    my %args = &_args;
    my $dist = $args{dist};
    return undef if not defined $dist or not -r $dist;
    require Cwd;
    require File::Path;

    # The unzipping will change directories. Remember old dir.
    my $old_cwd = Cwd::cwd();
    
    # Unzip the base par to a temp. dir.
    (undef, my $base_dir) = _unzip_to_tmpdir(
        dist => $dist, subdir => 'blib'
    );
    my $blibdir = File::Spec->catdir($base_dir, 'blib');

    my $meta = File::Spec->catfile($blibdir, 'META.yml');

    if (not -r $meta) {
        return undef;
    }
    
    open FH, '<', $meta
      or die "Could not open file '$meta' for reading: $!";
    
    local $/ = undef;
    my $meta_text = <FH>;
    close FH;
    
    chdir($old_cwd);
    
    File::Path::rmtree([$base_dir]);
    
    return $meta_text;
}



sub _unzip {
    my %args = &_args;
    my $dist = $args{dist};
    my $path = $args{path} || File::Spec->curdir;
    return unless -f $dist;

    # Try fast unzipping first
    if (eval { require Archive::Unzip::Burst; 1 }) {
        my $return = Archive::Unzip::Burst::unzip($dist, $path);
        return if $return; # true return value == error (a la system call)
    }
    # Then slow unzipping
    if (eval { require Archive::Zip; 1 }) {
        my $zip = Archive::Zip->new;
        local %SIG;
        $SIG{__WARN__} = sub { print STDERR $_[0] unless $_[0] =~ /\bstat\b/ };
        return unless $zip->read($dist) == Archive::Zip::AZ_OK()
                  and $zip->extractTree('', "$path/") == Archive::Zip::AZ_OK();
    }
    # Then fall back to the system
    else {
        return if system(unzip => $dist, '-d', $path);
    }

    return 1;
}

sub _zip {
    my %args = &_args;
    my $dist = $args{dist};

    if (eval { require Archive::Zip; 1 }) {
        my $zip = Archive::Zip->new;
        $zip->addTree( File::Spec->curdir, '' );
        $zip->writeToFileNamed( $dist ) == Archive::Zip::AZ_OK() or die $!;
    }
    else {
        system(qw(zip -r), $dist, File::Spec->curdir) and die $!;
    }
}


# This sub munges the arguments to most of the PAR::Dist functions
# into a hash. On the way, it downloads PAR archives as necessary, etc.
sub _args {
    # default to the first .par in the CWD
    if (not @_) {
        @_ = (glob('*.par'))[0];
    }

    # single argument => it's a distribution file name or URL
    @_ = (dist => @_) if @_ == 1;

    my %args = @_;
    $args{name} ||= $args{dist};

    # If we are installing from an URL, we want to munge the
    # distribution name so that it is in form "Module-Name"
    if (defined $args{name}) {
        $args{name} =~ s/^\w+:\/\///;
        my @elems = parse_dist_name($args{name});
        # @elems is name, version, arch, perlversion
        if (defined $elems[0]) {
            $args{name} = $elems[0];
        }
        else {
            $args{name} =~ s/^.*\/([^\/]+)$/$1/;
            $args{name} =~ s/^([0-9A-Za-z_-]+)-\d+\..+$/$1/;
        }
    }

    # append suffix if there is none
    if ($args{dist} and not $args{dist} =~ /\.[a-zA-Z_][^.]*$/) {
        require Config;
        my $suffix = $args{suffix};
        $suffix ||= "$Config::Config{archname}-$Config::Config{version}.par";
        $args{dist} .= "-$suffix";
    }

    # download if it's an URL
    if ($args{dist} and $args{dist} =~ m!^\w+://!) {
        $args{dist} = _fetch(dist => $args{dist})
    }

    return %args;
}


# Download PAR archive, but only if necessary (mirror!)
my %escapes;
sub _fetch {
    my %args = @_;

    if ($args{dist} =~ s/^file:\/\///) {
      return $args{dist} if -e $args{dist};
      return;
    }
    require LWP::Simple;

    $ENV{PAR_TEMP} ||= File::Spec->catdir(File::Spec->tmpdir, 'par');
    mkdir $ENV{PAR_TEMP}, 0777;
    %escapes = map { chr($_) => sprintf("%%%02X", $_) } 0..255 unless %escapes;

    $args{dist} =~ s{^cpan://((([a-zA-Z])[a-zA-Z])[-_a-zA-Z]+)/}
		    {http://www.cpan.org/modules/by-authors/id/\U$3/$2/$1\E/};

    my $file = $args{dist};
    $file =~ s/([^\w\.])/$escapes{$1}/g;
    $file = File::Spec->catfile( $ENV{PAR_TEMP}, $file);
    my $rc = LWP::Simple::mirror( $args{dist}, $file );

    if (!LWP::Simple::is_success($rc) and $rc != 304) {
        die "Error $rc: ", LWP::Simple::status_message($rc), " ($args{dist})\n";
    }

    return $file if -e $file;
    return;
}

sub _verify_or_sign {
    my %args = &_args;

    require File::Path;
    require Module::Signature;
    die "Module::Signature version 0.25 required"
      unless Module::Signature->VERSION >= 0.25;

    require Cwd;
    my $cwd = Cwd::cwd();
    my $action = $args{action};
    my ($dist, $tmpdir) = _unzip_to_tmpdir($args{dist});
    $action ||= (-e 'SIGNATURE' ? 'verify' : 'sign');

    if ($action eq 'sign') {
        open FH, '>SIGNATURE' unless -e 'SIGNATURE';
        open FH, 'MANIFEST' or die $!;

        local $/;
        my $out = <FH>;
        if ($out !~ /^SIGNATURE(?:\s|$)/m) {
            $out =~ s/^(?!\s)/SIGNATURE\n/m;
            open FH, '>MANIFEST' or die $!;
            print FH $out;
        }
        close FH;

        $args{overwrite} = 1 unless exists $args{overwrite};
        $args{skip}      = 0 unless exists $args{skip};
    }

    my $rv = Module::Signature->can($action)->(%args);
    _zip(dist => $dist) if $action eq 'sign';
    File::Path::rmtree([$tmpdir]);

    chdir($cwd);
    return $rv;
}

sub _unzip_to_tmpdir {
    my %args = &_args;

    require File::Temp;

    my $dist   = File::Spec->rel2abs($args{dist});
    my $tmpdirname = File::Spec->catdir(File::Spec->tmpdir, "parXXXXX");
    my $tmpdir = File::Temp::mkdtemp($tmpdirname)        
      or die "Could not create temporary directory from template '$tmpdirname': $!";
    my $path = $tmpdir;
    $path = File::Spec->catdir($tmpdir, $args{subdir}) if defined $args{subdir};
    _unzip(dist => $dist, path => $path);

    chdir $tmpdir;
    return ($dist, $tmpdir);
}



=head2 parse_dist_name

First argument must be a distribution file name. The file name
is parsed into I<distribution name>, I<distribution version>,
I<architecture name>, and I<perl version>.

Returns the results as a list in the above order.
If any or all of the above cannot be determined, returns undef instead
of the undetermined elements.

Supported formats are:

Math-Symbolic-0.502-x86_64-linux-gnu-thread-multi-5.8.7

Math-Symbolic-0.502

The ".tar.gz" or ".par" extensions as well as any
preceding paths are stripped before parsing. Starting with C<PAR::Dist>
0.22, versions containing a preceding C<v> are parsed correctly.

This function is not exported by default.

=cut

sub parse_dist_name {
	my $file = shift;
	return(undef, undef, undef, undef) if not defined $file;

	(undef, undef, $file) = File::Spec->splitpath($file);
	
	my $version = qr/v?(?:\d+(?:_\d+)?|\d*(?:\.\d+(?:_\d+)?)+)/;
	$file =~ s/\.(?:par|tar\.gz|tar)$//i;
	my @elem = split /-/, $file;
	my (@dn, $dv, @arch, $pv);
	while (@elem) {
		my $e = shift @elem;
		if (
            $e =~ /^$version$/o
            and not(# if not next token also a version
                    # (assumes an arch string doesnt start with a version...)
                @elem and $elem[0] =~ /^$version$/o
            )
        ) {
            
			$dv = $e;
			last;
		}
		push @dn, $e;
	}
	
	my $dn;
	$dn = join('-', @dn) if @dn;

	if (not @elem) {
		return( $dn, $dv, undef, undef);
	}

	while (@elem) {
		my $e = shift @elem;
		if ($e =~ /^$version|any_version$/) {
			$pv = $e;
			last;
		}
		push @arch, $e;
	}

	my $arch;
	$arch = join('-', @arch) if @arch;

	return($dn, $dv, $arch, $pv);
}

=head2 generate_blib_stub

Creates a F<blib/lib> subdirectory in the current directory
and prepares a F<META.yml> with meta information for a
new PAR distribution. First argument should be the name of the
PAR distribution in a format understood by C<parse_dist_name()>.
Alternatively, named arguments resembling those of
C<blib_to_par> are accepted.

After running C<generate_blib_stub> and injecting files into
the F<blib> directory, you can create a PAR distribution
using C<blib_to_par>.
This function is useful for creating custom PAR distributions
from scratch. (I.e. not from an unpacked CPAN distribution)
Example:

  use PAR::Dist;
  use File::Copy 'copy';
  
  generate_blib_stub(
    name => 'MyApp', version => '1.00'
  );
  copy('MyApp.pm', 'blib/lib/MyApp.pm');
  blib_to_par(); # generates the .par file!

C<generate_blib_stub> will not overwrite existing files.

=cut

sub generate_blib_stub {
    my %args = &_args;
    my $dist = $args{dist};
    require Config;
    
    my $name	= $args{name};
    my $version	= $args{version};
    my $suffix	= $args{suffix};

    my ($parse_name, $parse_version, $archname, $perlversion)
      = parse_dist_name($dist);
    
    $name ||= $parse_name;
    $version ||= $parse_version;
    $suffix = "$archname-$perlversion"
      if (not defined $suffix or $suffix eq '')
         and $archname and $perlversion;
    
    $suffix ||= "$Config::Config{archname}-$Config::Config{version}";
    if ( grep { not defined $_ } ($name, $version, $suffix) ) {
        warn "Could not determine distribution meta information from distribution name '$dist'";
        return();
    }
    $suffix =~ s/\.par$//;

    if (not -f 'META.yml') {
        open META, '>', 'META.yml'
          or die "Could not open META.yml file for writing: $!";
        print META << "YAML" if fileno(META);
name: $name
version: $version
build_requires: {}
conflicts: {}
dist_name: $name-$version-$suffix.par
distribution_type: par
dynamic_config: 0
generated_by: 'PAR::Dist version $PAR::Dist::VERSION'
license: unknown
YAML
        close META;
    }

    mkdir('blib');
    mkdir(File::Spec->catdir('blib', 'lib'));
    mkdir(File::Spec->catdir('blib', 'script'));

    return 1;
}


=head2 contains_binaries

This function is not exported by default.

Opens a PAR archive tries to determine whether that archive
contains platform-specific binary code.

Takes one named parameter: I<dist>. If only one parameter is
passed, it is treated as the I<dist> parameter. (Have a look
at the description in the C<FUNCTIONS> section above.)

Throws a fatal error if the PAR archive could not be found.

Returns one if the PAR was found to contain binary code
and zero otherwise.

=cut

sub contains_binaries {
    require File::Find;
    my %args = &_args;
    my $dist = $args{dist};
    return undef if not defined $dist or not -r $dist;
    require Cwd;
    require File::Path;

    # The unzipping will change directories. Remember old dir.
    my $old_cwd = Cwd::cwd();
    
    # Unzip the base par to a temp. dir.
    (undef, my $base_dir) = _unzip_to_tmpdir(
        dist => $dist, subdir => 'blib'
    );
    my $blibdir = File::Spec->catdir($base_dir, 'blib');
    my $archdir = File::Spec->catdir($blibdir, 'arch');

    my $found = 0;

    File::Find::find(
      sub {
        $found++ if -f $_ and not /^\.exists$/;
      },
      $archdir
    );

    chdir($old_cwd);
    
    File::Path::rmtree([$base_dir]);
    
    return $found ? 1 : 0;
}

1;

=head1 SEE ALSO

L<PAR>, L<ExtUtils::Install>, L<Module::Signature>, L<LWP::Simple>

=head1 AUTHORS

Audrey Tang E<lt>cpan@audreyt.orgE<gt> 2003-2007

Steffen Mueller E<lt>smueller@cpan.orgE<gt> 2005-2007

PAR has a mailing list, E<lt>par@perl.orgE<gt>, that you can write to;
send an empty mail to E<lt>par-subscribe@perl.orgE<gt> to join the list
and participate in the discussion.

Please send bug reports to E<lt>bug-par@rt.cpan.orgE<gt>.

=head1 COPYRIGHT

Copyright 2003-2007 by Audrey Tang E<lt>autrijus@autrijus.orgE<gt>.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut
