package CPANPLUS::inc;

=head1 NAME

CPANPLUS::inc

=head1 DESCRIPTION

OBSOLETE

=cut

sub original_perl5opt   { $ENV{PERL5OPT}    };
sub original_perl5lib   { $ENV{PERL5LIB}    };
sub original_inc        { @INC              };

1;

__END__

use strict;
use vars        qw[$DEBUG $VERSION $ENABLE_INC_HOOK %LIMIT $QUIET];
use File::Spec  ();
use Config      ();

### 5.6.1. nags about require + bareword otherwise ###
use lib ();

$QUIET              = 0;
$DEBUG              = 0;
%LIMIT              = ();

=pod

=head1 NAME

CPANPLUS::inc - runtime inclusion of privately bundled modules

=head1 SYNOPSIS

    ### set up CPANPLUS::inc to do it's thing ###
    BEGIN { use CPANPLUS::inc };

    ### enable debugging ###
    use CPANPLUS::inc qw[DEBUG];

=head1 DESCRIPTION

This module enables the use of the bundled modules in the
C<CPANPLUS/inc> directory of this package. These modules are bundled
to make sure C<CPANPLUS> is able to bootstrap itself. It will do the
following things:

=over 4

=item Put a coderef at the beginning of C<@INC>

This allows us to decide which module to load, and where to find it.
For details on what we do, see the C<INTERESTING MODULES> section below.
Also see the C<CAVEATS> section.

=item Add the full path to the C<CPANPLUS/inc> directory to C<$ENV{PERL5LIB>.

This allows us to find our bundled modules even if we spawn off a new
process. Although it's not able to do the selective loading as the
coderef in C<@INC> could, it's a good fallback.

=back

=head1 METHODS

=head2 CPANPLUS::inc->inc_path()

Returns the full path to the C<CPANPLUS/inc> directory.

=head2 CPANPLUS::inc->my_path()

Returns the full path to be added to C<@INC> to load
C<CPANPLUS::inc> from.

=head2 CPANPLUS::inc->installer_path()

Returns the full path to the C<CPANPLUS/inc/installers> directory.

=cut

{   my $ext     = '.pm';
    my $file    = (join '/', split '::', __PACKAGE__) . $ext;

    ### os specific file path, if you're not on unix
    my $osfile  = File::Spec->catfile( split('::', __PACKAGE__) ) . $ext;

    ### this returns a unixy path, compensate if you're on non-unix
    my $path    = File::Spec->rel2abs(
                        File::Spec->catfile( split '/', $INC{$file} )
                    );

    ### don't forget to quotemeta; win32 paths are special
    my $qm_osfile = quotemeta $osfile;
    my $path_to_me          = $path; $path_to_me    =~ s/$qm_osfile$//i;
    my $path_to_inc         = $path; $path_to_inc   =~ s/$ext$//i;
    my $path_to_installers  = File::Spec->catdir( $path_to_inc, 'installers' );

    sub inc_path        { return $path_to_inc  }
    sub my_path         { return $path_to_me   }
    sub installer_path  { return $path_to_installers }
}

=head2 CPANPLUS::inc->original_perl5lib

Returns the value of $ENV{PERL5LIB} the way it was when C<CPANPLUS::inc>
got loaded.

=head2 CPANPLUS::inc->original_perl5opt

Returns the value of $ENV{PERL5OPT} the way it was when C<CPANPLUS::inc>
got loaded.

=head2 CPANPLUS::inc->original_inc

Returns the value of @INC the way it was when C<CPANPLUS::inc> got
loaded.

=head2 CPANPLUS::inc->limited_perl5opt(@modules);

Returns a string you can assign to C<$ENV{PERL5OPT}> to have a limited
include facility from C<CPANPLUS::inc>. It will roughly look like:

    -I/path/to/cpanplus/inc -MCPANPLUS::inc=module1,module2

=cut

{   my $org_opt = $ENV{PERL5OPT};
    my $org_lib = $ENV{PERL5LIB};
    my @org_inc = @INC;

    sub original_perl5opt   { $org_opt || ''};
    sub original_perl5lib   { $org_lib || ''};
    sub original_inc        { @org_inc, __PACKAGE__->my_path };

    sub limited_perl5opt    {
        my $pkg = shift;
        my $lim = join ',', @_ or return;

        ### -Icp::inc -Mcp::inc=mod1,mod2,mod3
        my $opt =   '-I' . __PACKAGE__->my_path . ' ' .
                    '-M' . __PACKAGE__ . "=$lim";

        $opt .=     $Config::Config{'path_sep'} .
                    CPANPLUS::inc->original_perl5opt
                if  CPANPLUS::inc->original_perl5opt;

        return $opt;
    }
}

=head2 CPANPLUS::inc->interesting_modules()

Returns a hashref with modules we're interested in, and the minimum
version we need to find.

It would looks something like this:

    {   File::Fetch             => 0.06,
        IPC::Cmd                => 0.22,
        ....
    }

=cut

{
    my $map = {
        ### used to have 0.80, but not it was never released by coral
        ### 0.79 *should* be good enough for now... asked coral to 
        ### release 0.80 on 10/3/2006
        'IPC::Run'                  => '0.79', 
        'File::Fetch'               => '0.07',
        #'File::Spec'                => '0.82', # can't, need it ourselves...
        'IPC::Cmd'                  => '0.24',
        'Locale::Maketext::Simple'  => 0,
        'Log::Message'              => 0,
        'Module::Load'              => '0.10',
        'Module::Load::Conditional' => '0.07',
        'Params::Check'             => '0.22',
        'Term::UI'                  => '0.05',
        'Archive::Extract'          => '0.07',
        'Archive::Tar'              => '1.23',
        'IO::Zlib'                  => '1.04',
        'Object::Accessor'          => '0.03',
        'Module::CoreList'          => '1.97',
        'Module::Pluggable'         => '2.4',
        'Module::Loaded'            => 0,
        #'Config::Auto'             => 0,   # not yet, not using it yet
    };

    sub interesting_modules { return $map; }
}


=head1 INTERESTING MODULES

C<CPANPLUS::inc> doesn't even bother to try find and find a module
it's not interested in. A list of I<interesting modules> can be
obtained using the C<interesting_modules> method described above.

Note that all subclassed modules of an C<interesting module> will
also be attempted to be loaded, but a version will not be checked.

When it however does encounter a module it is interested in, it will
do the following things:

=over 4

=item Loop over your @INC

And for every directory it finds there (skipping all non directories
-- see the C<CAVEATS> section), see if the module requested can be
found there.

=item Check the version on every suitable module found in @INC

After a list of modules has been gathered, the version of each of them
is checked to find the one with the highest version, and return that as
the module to C<use>.

This enables us to use a recent enough version from our own bundled
modules, but also to use a I<newer> module found in your path instead,
if it is present. Thus having access to bugfixed versions as they are
released.

If for some reason no satisfactory version could be found, a warning
will be emitted. See the C<DEBUG> section for more details on how to
find out exactly what C<CPANPLUS::inc> is doing.

=back

=cut

{   my $Loaded;
    my %Cache;


    ### returns the path to a certain module we found
    sub path_to {
        my $self    = shift;
        my $mod     = shift or return;

        ### find the directory
        my $path    = $Cache{$mod}->[0][2] or return;

        ### probe them explicitly for a special file, because the
        ### dir we found the file in vs our own paths may point to the
        ### same location, but might not pass an 'eq' test.

        ### it's our inc-path
        return __PACKAGE__->inc_path
                if -e File::Spec->catfile( $path, '.inc' );

        ### it's our installer path
        return __PACKAGE__->installer_path
                if -e File::Spec->catfile( $path, '.installers' );

        ### it's just some dir...
        return $path;
    }

    ### just a debug method
    sub _show_cache { return \%Cache };

    sub import {
        my $pkg = shift;

        ### filter DEBUG, and toggle the global
        map { $LIMIT{$_} = 1 }  
            grep {  /DEBUG/ ? ++$DEBUG && 0 : 
                    /QUIET/ ? ++$QUIET && 0 :
                    1 
            } @_;
        
        ### only load once ###
        return 1 if $Loaded++;

        ### first, add our own private dir to the end of @INC:
        {
            push @INC,  __PACKAGE__->my_path, __PACKAGE__->inc_path,
                        __PACKAGE__->installer_path;

            ### XXX stop doing this, there's no need for it anymore;
            ### none of the shell outs need to have this set anymore
#             ### add the path to this module to PERL5OPT in case
#             ### we spawn off some programs...
#             ### then add this module to be loaded in PERL5OPT...
#             {   local $^W;
#                 $ENV{'PERL5LIB'} .= $Config::Config{'path_sep'}
#                                  . __PACKAGE__->my_path
#                                  . $Config::Config{'path_sep'}
#                                  . __PACKAGE__->inc_path;
#
#                 $ENV{'PERL5OPT'} = '-M'. __PACKAGE__ . ' '
#                                  . ($ENV{'PERL5OPT'} || '');
#             }
        }

        ### next, find the highest version of a module that
        ### we care about. very basic check, but will
        ### have to do for now.
        lib->import( sub {
            my $path    = pop();                    # path to the pm
            my $module  = $path or return;          # copy of the path, to munge
            my @parts   = split qr!\\|/!, $path;    # dirs + file name; could be
                                                    # win32 paths =/
            my $file    = pop @parts;               # just the file name
            my $map     = __PACKAGE__->interesting_modules;

            ### translate file name to module name 
            ### could contain win32 paths delimiters
            $module =~ s!/|\\!::!g; $module =~ s/\.pm//i;

            my $check_version; my $try;
            ### does it look like a module we care about?
            my ($interesting) = grep { $module =~ /^$_/ } keys %$map;
            ++$try if $interesting;

            ### do we need to check the version too?
            ++$check_version if exists $map->{$module};

            ### we don't care ###
            unless( $try ) {
                warn __PACKAGE__ .": Not interested in '$module'\n" if $DEBUG;
                return;

            ### we're not allowed
            } elsif ( $try and keys %LIMIT ) {
                unless( grep { $module =~ /^$_/ } keys %LIMIT  ) {
                    warn __PACKAGE__ .": Limits active, '$module' not allowed ".
                                        "to be loaded" if $DEBUG;
                    return;
                }
            }

            ### found filehandles + versions ###
            my @found;
            DIR: for my $dir (@INC) {
                next DIR unless -d $dir;

                ### get the full path to the module ###
                my $pm = File::Spec->catfile( $dir, @parts, $file );

                ### open the file if it exists ###
                if( -e $pm ) {
                    my $fh;
                    unless( open $fh, "$pm" ) {
                        warn __PACKAGE__ .": Could not open '$pm': $!\n"
                            if $DEBUG;
                        next DIR;
                    }

                    my $found;
                    ### XXX stolen from module::load::conditional ###
                    while (local $_ = <$fh> ) {

                        ### the following regexp comes from the
                        ### ExtUtils::MakeMaker documentation.
                        if ( /([\$*])(([\w\:\']*)\bVERSION)\b.*\=/ ) {

                            ### this will eval the version in to $VERSION if it
                            ### was declared as $VERSION in the module.
                            ### else the result will be in $res.
                            ### this is a fix on skud's Module::InstalledVersion

                            local $VERSION;
                            my $res = eval $_;

                            ### default to '0.0' if there REALLY is no version
                            ### all to satisfy warnings
                            $found = $VERSION || $res || '0.0';

                            ### found what we came for
                            last if $found;
                        }
                    }

                    ### no version defined at all? ###
                    $found ||= '0.0';

                    warn __PACKAGE__ .": Found match for '$module' in '$dir' "
                                     ."with version '$found'\n" if $DEBUG;

                    ### reset the position of the filehandle ###
                    seek $fh, 0, 0;

                    ### store the found version + filehandle it came from ###
                    push @found, [ $found, $fh, $dir, $pm ];
                }

            } # done looping over all the dirs

            ### nothing found? ###
            unless (@found) {
                warn __PACKAGE__ .": Unable to find any module named "
                                    . "'$module'\n" if $DEBUG;
                return;
            }

            ### find highest version
            ### or the one in the same dir as a base module already loaded
            ### or otherwise, the one not bundled
            ### or otherwise the newest
            my @sorted = sort {
                            _vcmp($b->[0], $a->[0])                  ||
                            ($Cache{$interesting}
                                ?($b->[2] eq $Cache{$interesting}->[0][2]) <=>
                                 ($a->[2] eq $Cache{$interesting}->[0][2])
                                : 0 )                               ||
                            (($a->[2] eq __PACKAGE__->inc_path) <=>
                             ($b->[2] eq __PACKAGE__->inc_path))    ||
                            (-M $a->[3] <=> -M $b->[3])
                          } @found;

            warn __PACKAGE__ .": Best match for '$module' is found in "
                             ."'$sorted[0][2]' with version '$sorted[0][0]'\n"
                    if $DEBUG;

            if( $check_version and 
                not (_vcmp($sorted[0][0], $map->{$module}) >= 0) 
            ) {
                warn __PACKAGE__ .": Cannot find high enough version for "
                                 ."'$module' -- need '$map->{$module}' but "
                                 ."only found '$sorted[0][0]'. Returning "
                                 ."highest found version but this may cause "
                                 ."problems\n" unless $QUIET;
            };

            ### right, so that damn )#$(*@#)(*@#@ Module::Build makes
            ### assumptions about the environment (especially its own tests)
            ### and blows up badly if it's loaded via CP::inc :(
            ### so, if we find a newer version on disk (which would happen when
            ### upgrading or having upgraded, just pretend we didn't find it,
            ### let it be loaded via the 'normal' way.
            ### can't even load the *proper* one via our CP::inc, as it will
            ### get upset just over the fact it's loaded via a non-standard way
            if( $module =~ /^Module::Build/ and
                $sorted[0][2] ne __PACKAGE__->inc_path and
                $sorted[0][2] ne __PACKAGE__->installer_path
            ) {
                warn __PACKAGE__ .": Found newer version of 'Module::Build::*' "
                                 ."elsewhere in your path. Pretending to not "
                                 ."have found it\n" if $DEBUG;
                return;
            }

            ### store what we found for this module
            $Cache{$module} = \@sorted;

            ### best matching filehandle ###
            return $sorted[0][1];
        } );
    }
}

### XXX copied from C::I::Utils, so there's no circular require here!
sub _vcmp {
    my ($x, $y) = @_;
    s/_//g foreach $x, $y;
    return $x <=> $y;
}

=pod

=head1 DEBUG

Since this module does C<Clever Things> to your search path, it might
be nice sometimes to figure out what it's doing, if things don't work
as expected. You can enable a debug trace by calling the module like
this:

    use CPANPLUS::inc 'DEBUG';

This will show you what C<CPANPLUS::inc> is doing, which might look
something like this:

    CPANPLUS::inc: Found match for 'Params::Check' in
    '/opt/lib/perl5/site_perl/5.8.3' with version '0.07'
    CPANPLUS::inc: Found match for 'Params::Check' in
    '/my/private/lib/CPANPLUS/inc' with version '0.21'
    CPANPLUS::inc: Best match for 'Params::Check' is found in
    '/my/private/lib/CPANPLUS/inc' with version '0.21'

=head1 CAVEATS

This module has 2 major caveats, that could lead to unexpected
behaviour. But currently I don't know how to fix them, Suggestions
are much welcomed.

=over 4

=item On multiple C<use lib> calls, our coderef may not be the first in @INC

If this happens, although unlikely in most situations and not happening
when calling the shell directly, this could mean that a lower (too low)
versioned module is loaded, which might cause failures in the
application.

=item Non-directories in @INC

Non-directories are right now skipped by CPANPLUS::inc. They could of
course lead us to newer versions of a module, but it's too tricky to
verify if they would. Therefor they are skipped. In the worst case
scenario we'll find the sufficing version bundled with CPANPLUS.


=cut

1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

