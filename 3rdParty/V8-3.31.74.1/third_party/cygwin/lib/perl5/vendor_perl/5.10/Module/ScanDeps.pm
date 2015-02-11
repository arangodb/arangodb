package Module::ScanDeps;

use 5.006;
use strict;
use vars qw( $VERSION @EXPORT @EXPORT_OK $CurrentPackage @IncludeLibs $ScanFileRE );

$VERSION   = '0.82';
@EXPORT    = qw( scan_deps scan_deps_runtime );
@EXPORT_OK = qw( scan_line scan_chunk add_deps scan_deps_runtime path_to_inc_name );

use Config;
use Exporter;
use base 'Exporter';
use constant dl_ext  => ".$Config{dlext}";
use constant lib_ext => $Config{lib_ext};
use constant is_insensitive_fs => (
    -s $0 
        and (-s lc($0) || -1) == (-s uc($0) || -1)
        and (-s lc($0) || -1) == -s $0
);

use version;
use Cwd ();
use File::Path ();
use File::Temp ();
use File::Spec ();
use File::Basename ();
use FileHandle;
use Module::Build::ModuleInfo;

$ScanFileRE = qr/(?:^|\\|\/)(?:[^.]*|.*\.(?i:p[ml]|t|al))$/;

=head1 NAME

Module::ScanDeps - Recursively scan Perl code for dependencies

=head1 VERSION

This document describes version 0.82 of Module::ScanDeps, released
January 28, 2008.

=head1 SYNOPSIS

Via the command-line program L<scandeps.pl>:

    % scandeps.pl *.pm          # Print PREREQ_PM section for *.pm
    % scandeps.pl -e "use utf8" # Read script from command line
    % scandeps.pl -B *.pm       # Include core modules
    % scandeps.pl -V *.pm       # Show autoload/shared/data files

Used in a program;

    use Module::ScanDeps;

    # standard usage
    my $hash_ref = scan_deps(
        files   => [ 'a.pl', 'b.pl' ],
        recurse => 1,
    );

    # shorthand; assume recurse == 1
    my $hash_ref = scan_deps( 'a.pl', 'b.pl' );

    # App::Packer::Frontend compatible interface
    # see App::Packer::Frontend for the structure returned by get_files
    my $scan = Module::ScanDeps->new;
    $scan->set_file( 'a.pl' );
    $scan->set_options( add_modules => [ 'Test::More' ] );
    $scan->calculate_info;
    my $files = $scan->get_files;

=head1 DESCRIPTION

This module scans potential modules used by perl programs, and returns a
hash reference; its keys are the module names as appears in C<%INC>
(e.g. C<Test/More.pm>); the values are hash references with this structure:

    {
        file    => '/usr/local/lib/perl5/5.8.0/Test/More.pm',
        key     => 'Test/More.pm',
        type    => 'module',    # or 'autoload', 'data', 'shared'
        used_by => [ 'Test/Simple.pm', ... ],
        uses    => [ 'Test/Other.pm', ... ],
    }

One function, C<scan_deps>, is exported by default.  Other
functions such as (C<scan_line>, C<scan_chunk>, C<add_deps>, C<path_to_inc_name>)
are exported upon request.

Users of B<App::Packer> may also use this module as the dependency-checking
frontend, by tweaking their F<p2e.pl> like below:

    use Module::ScanDeps;
    ...
    my $packer = App::Packer->new( frontend => 'Module::ScanDeps' );
    ...

Please see L<App::Packer::Frontend> for detailed explanation on
the structure returned by C<get_files>.

=head2 B<scan_deps>

    $rv_ref = scan_deps(
        files   => \@files,     recurse => $recurse,
        rv      => \%rv,        skip    => \%skip,
        compile => $compile,    execute => $execute,
    );
    $rv_ref = scan_deps(@files); # shorthand, with recurse => 1

This function scans each file in C<@files>, registering their
dependencies into C<%rv>, and returns a reference to the updated
C<%rv>.  The meaning of keys and values are explained above.

If C<$recurse> is true, C<scan_deps> will call itself recursively,
to perform a breadth-first search on text files (as defined by the
-T operator) found in C<%rv>.

If the C<\%skip> is specified, files that exists as its keys are
skipped.  This is used internally to avoid infinite recursion.

If C<$compile> or C<$execute> is true, runs C<files> in either
compile-only or normal mode, then inspects their C<%INC> after
termination to determine additional runtime dependencies.

If C<$execute> is an array reference, runs the files contained
in it instead of C<@files>.

Additionally, an option C<warn_missing> is recognized. If set to true,
C<scan_deps> issues a warning to STDERR for every module file that the
scanned code depends but that wasn't found. Please note that this may
also report numerous false positives. That is why by default, the heuristic
silently drops all dependencies it cannot find.

=head2 B<scan_deps_runtime>

Like B<scan_deps>, but skips the static scanning part.

=head2 B<scan_line>

    @modules = scan_line($line);

Splits a line into chunks (currently with the semicolon characters), and
return the union of C<scan_chunk> calls of them.

If the line is C<__END__> or C<__DATA__>, a single C<__END__> element is
returned to signify the end of the program.

Similarly, it returns a single C<__POD__> if the line matches C</^=\w/>;
the caller is responsible for skipping appropriate number of lines
until C<=cut>, before calling C<scan_line> again.

=head2 B<scan_chunk>

    $module = scan_chunk($chunk);
    @modules = scan_chunk($chunk);

Apply various heuristics to C<$chunk> to find and return the module
name(s) it contains.  In scalar context, returns only the first module
or C<undef>.

=head2 B<add_deps>

    $rv_ref = add_deps( rv => \%rv, modules => \@modules );
    $rv_ref = add_deps( @modules ); # shorthand, without rv

Resolves a list of module names to its actual on-disk location, by
finding in C<@INC> and C<@Module::ScanDeps::IncludeLibs>;
modules that cannot be found are skipped.

This function populates the C<%rv> hash with module/filename pairs, and
returns a reference to it.

=head2 B<path_to_inc_name>

    $perl_name = path_to_inc_name($path, $warn)

Assumes C<$path> refers to a perl file and does it's best to return the
name as it would appear in %INC. Returns undef if no match was found 
and a prints a warning to STDERR if C<$warn> is true.

E.g. if C<$path> = perl/site/lib/Module/ScanDeps.pm then C<$perl_name>
will be Module/ScanDeps.pm.

=head1 NOTES

=head2 B<@Module::ScanDeps::IncludeLibs>

You can set this global variable to specify additional directories in
which to search modules without modifying C<@INC> itself.

=head2 B<$Module::ScanDeps::ScanFileRE>

You can set this global variable to specify a regular expression to 
identify what files to scan. By default it includes all files of 
the following types: .pm, .pl, .t and .al. Additionally, all files
without a suffix are considered.

For instance, if you want to scan all files then use the following:

C<$Module::ScanDeps::ScanFileRE = qr/./>

=head1 CAVEATS

This module intentially ignores the B<BSDPAN> hack on FreeBSD -- the
additional directory is removed from C<@INC> altogether.

The static-scanning heuristic is not likely to be 100% accurate, especially
on modules that dynamically load other modules.

Chunks that span multiple lines are not handled correctly.  For example,
this one works:

    use base 'Foo::Bar';

But this one does not:

    use base
        'Foo::Bar';

=cut

my $SeenTk;

# Pre-loaded module dependencies {{{
my %Preload;
%Preload = (
    'AnyDBM_File.pm'  => [qw( SDBM_File.pm )],
    'Authen/SASL.pm'  => 'sub',
    'Bio/AlignIO.pm'  => 'sub',
    'Bio/Assembly/IO.pm'  => 'sub',
    'Bio/Biblio/IO.pm'  => 'sub',
    'Bio/ClusterIO.pm'  => 'sub',
    'Bio/CodonUsage/IO.pm'  => 'sub',
    'Bio/DB/Biblio.pm'  => 'sub',
    'Bio/DB/Flat.pm'  => 'sub',
    'Bio/DB/GFF.pm'  => 'sub',
    'Bio/DB/Taxonomy.pm'  => 'sub',
    'Bio/Graphics/Glyph.pm'  => 'sub',
    'Bio/MapIO.pm'  => 'sub',
    'Bio/Matrix/IO.pm'  => 'sub',
    'Bio/Matrix/PSM/IO.pm'  => 'sub',
    'Bio/OntologyIO.pm'  => 'sub',
    'Bio/PopGen/IO.pm'  => 'sub',
    'Bio/Restriction/IO.pm'  => 'sub',
    'Bio/Root/IO.pm'  => 'sub',
    'Bio/SearchIO.pm'  => 'sub',
    'Bio/SeqIO.pm'  => 'sub',
    'Bio/Structure/IO.pm'  => 'sub',
    'Bio/TreeIO.pm'  => 'sub',
    'Bio/LiveSeq/IO.pm'  => 'sub',
    'Bio/Variation/IO.pm'  => 'sub',
    'Catalyst.pm' => sub {
        return ('Catalyst/Runtime.pm',
                'Catalyst/Dispatcher.pm',
                _glob_in_inc('Catalyst/DispatchType', 1));
    },
    'Catalyst/Engine.pm' => 'sub',
    'Class/MakeMethods.pm' => 'sub',
    'Config/Any.pm' =>'sub',
    'Crypt/Random.pm' => sub {
        _glob_in_inc('Crypt/Random/Provider', 1);
    },
    'Crypt/Random/Generator.pm' => sub {
        _glob_in_inc('Crypt/Random/Provider', 1);
    },
    'DBI.pm' => sub {
        grep !/\bProxy\b/, _glob_in_inc('DBD', 1);
    },
    'DBIx/Class.pm' => 'sub',
    'DBIx/SearchBuilder.pm' => 'sub',
    'DBIx/ReportBuilder.pm' => 'sub',
    'Device/ParallelPort.pm' => 'sub',
    'Device/SerialPort.pm' => [ qw(
        termios.ph asm/termios.ph sys/termiox.ph sys/termios.ph sys/ttycom.ph
    ) ],
    'Email/Send.pm' => 'sub',
    'ExtUtils/MakeMaker.pm' => sub {
        grep /\bMM_/, _glob_in_inc('ExtUtils', 1);
    },
    'File/Basename.pm' => [qw( re.pm )],
    'File/Spec.pm'     => sub {
        require File::Spec;
        map { my $name = $_; $name =~ s!::!/!g; "$name.pm" } @File::Spec::ISA;
    },
    'HTTP/Message.pm' => [ qw(
        URI/URL.pm          URI.pm
    ) ],
    'Image/Info.pm' => sub {
        return( _glob_in_inc("Image/Info", 1), qw(
            Image/TIFF.pm
        ));
    },
    'IO.pm' => [ qw(
        IO/Handle.pm        IO/Seekable.pm      IO/File.pm
        IO/Pipe.pm          IO/Socket.pm        IO/Dir.pm
    ) ],
    'IO/Socket.pm'     => [qw( IO/Socket/UNIX.pm )],
    'Log/Log4perl.pm' => 'sub',
    'LWP/UserAgent.pm' => sub {
        return(
            qw(
            URI/URL.pm          URI/http.pm         LWP/Protocol/http.pm
            ),
            _glob_in_inc("LWP/Authen", 1),
            _glob_in_inc("LWP/Protocol", 1),
        );
    },
    'LWP/Parallel.pm' => sub {
        _glob_in_inc( 'LWP/Parallel', 1 ),
        qw(
            LWP/ParallelUA.pm       LWP/UserAgent.pm
            LWP/RobotPUA.pm         LWP/RobotUA.pm
        ),
    },
    'LWP/Parallel/UserAgent.pm' => sub {
        qw( LWP/Parallel.pm ),
        @{ _get_preload('LWP/Parallel.pm') }
    },
    'Locale/Maketext/Lexicon.pm'    => 'sub',
    'Locale/Maketext/GutsLoader.pm' => [qw( Locale/Maketext/Guts.pm )],
    'Mail/Audit.pm'                => 'sub',
    'Math/BigInt.pm'                => 'sub',
    'Math/BigFloat.pm'              => 'sub',
    'Math/Symbolic.pm'              => 'sub',
    'Module/Build.pm'               => 'sub',
    'Module/Pluggable.pm'           => sub {
        _glob_in_inc('$CurrentPackage/Plugin', 1);
    },
    'MIME/Decoder.pm'               => 'sub',
    'Net/DNS/RR.pm'                 => 'sub',
    'Net/FTP.pm'                    => 'sub',
    'Net/SSH/Perl.pm'               => 'sub',
    'PDF/API2/Resource/Font.pm'     => 'sub',
    'PDF/API2/Basic/TTF/Font.pm'    => sub {
        _glob_in_inc('PDF/API2/Basic/TTF', 1);
    },
    'PDF/Writer.pm'                 => 'sub',
    'POE.pm'                           => [ qw(
        POE/Kernel.pm POE/Session.pm
    ) ],
    'POE/Kernel.pm'                    => sub {
	_glob_in_inc('POE/XS/Resource', 1),
	_glob_in_inc('POE/Resource', 1),
	_glob_in_inc('POE/XS/Loop', 1),
	_glob_in_inc('POE/Loop', 1),
    },
    'Parse/AFP.pm'                  => 'sub',
    'Parse/Binary.pm'               => 'sub',
    'PerlIO.pm'                     => [ 'PerlIO/scalar.pm' ],
    'Regexp/Common.pm'              => 'sub',
    'SerialJunk.pm' => [ qw(
        termios.ph asm/termios.ph sys/termiox.ph sys/termios.ph sys/ttycom.ph
    ) ],
    'SOAP/Lite.pm'                  => sub {
        (($] >= 5.008 ? ('utf8.pm') : ()), _glob_in_inc('SOAP/Transport', 1));
    },
    'SQL/Parser.pm' => sub {
        _glob_in_inc('SQL/Dialects', 1);
    },
    'SQL/Translator/Schema.pm' => sub {
        _glob_in_inc('SQL/Translator', 1);
    },
    'SVK/Command.pm' => sub {
        _glob_in_inc('SVK', 1);
    },
    'SVN/Core.pm' => sub {
        _glob_in_inc('SVN', 1),
        map "auto/SVN/$_->{name}", _glob_in_inc('auto/SVN'),
    },
    'Template.pm'      => 'sub',
    'Term/ReadLine.pm' => 'sub',
	'Test/Deep.pm'     => 'sub',
    'Tk.pm'            => sub {
        $SeenTk = 1;
        qw( Tk/FileSelect.pm Encode/Unicode.pm );
    },
    'Tk/Balloon.pm'     => [qw( Tk/balArrow.xbm )],
    'Tk/BrowseEntry.pm' => [qw( Tk/cbxarrow.xbm Tk/arrowdownwin.xbm )],
    'Tk/ColorEditor.pm' => [qw( Tk/ColorEdit.xpm )],
    'Tk/DragDrop/Common.pm' => sub {
        _glob_in_inc('Tk/DragDrop', 1),
    },
    'Tk/FBox.pm'        => [qw( Tk/folder.xpm Tk/file.xpm )],
    'Tk/Getopt.pm'      => [qw( Tk/openfolder.xpm Tk/win.xbm )],
    'Tk/Toplevel.pm'    => [qw( Tk/Wm.pm )],
    'URI.pm'            => sub {
        grep !/.\b[_A-Z]/, _glob_in_inc('URI', 1);
    },
    'Win32/EventLog.pm'    => [qw( Win32/IPC.pm )],
    'Win32/Exe.pm'         => 'sub',
    'Win32/TieRegistry.pm' => [qw( Win32API/Registry.pm )],
    'Win32/SystemInfo.pm'  => [qw( Win32/cpuspd.dll )],
    'XML/Parser.pm'        => sub {
        _glob_in_inc('XML/Parser/Style', 1),
        _glob_in_inc('XML/Parser/Encodings', 1),
    },
    'XML/Parser/Expat.pm' => sub {
        ($] >= 5.008) ? ('utf8.pm') : ();
    },
    'XML/SAX.pm' => [qw( XML/SAX/ParserDetails.ini ) ],
    'XMLRPC/Lite.pm' => sub {
        _glob_in_inc('XMLRPC/Transport', 1),;
    },
    'YAML.pm' => [qw( YAML/Loader.pm YAML/Dumper.pm )],
    'diagnostics.pm' => sub {
        # shamelessly taken and adapted from diagnostics.pm
        use Config;
        my($privlib, $archlib) = @Config{qw(privlibexp archlibexp)};
        if ($^O eq 'VMS') {
            require VMS::Filespec;
            $privlib = VMS::Filespec::unixify($privlib);
            $archlib = VMS::Filespec::unixify($archlib);
        }

        for (
              "pod/perldiag.pod",
              "Pod/perldiag.pod",
              "pod/perldiag-$Config{version}.pod",
              "Pod/perldiag-$Config{version}.pod",
              "pods/perldiag.pod",
              "pods/perldiag-$Config{version}.pod",
        ) {
            return $_ if _find_in_inc($_);
        }
        
        for (
              "$archlib/pods/perldiag.pod",
              "$privlib/pods/perldiag-$Config{version}.pod",
              "$privlib/pods/perldiag.pod",
        ) {
            return $_ if -f $_;
        }

        return 'pod/perldiag.pod';
    },
    'threads/shared.pm' => [qw( attributes.pm )],
    	# anybody using threads::shared is likely to declare variables
	# with attribute :shared
    'utf8.pm' => [
        'utf8_heavy.pl', do {
            my $dir = 'unicore';
            my @subdirs = qw( To );
            my @files = map "$dir/lib/$_->{name}", _glob_in_inc("$dir/lib");

            if (@files) {
                # 5.8.x
                push @files, (map "$dir/$_.pl", qw( Exact Canonical ));
            }
            else {
                # 5.6.x
                $dir = 'unicode';
                @files = map "$dir/Is/$_->{name}", _glob_in_inc("$dir/Is")
                  or return;
                push @subdirs, 'In';
            }

            foreach my $subdir (@subdirs) {
                foreach (_glob_in_inc("$dir/$subdir")) {
                    push @files, "$dir/$subdir/$_->{name}";
                }
            }
            @files;
        }
    ],
    'charnames.pm' => [
        _find_in_inc('unicore/Name.pl') ? 'unicore/Name.pl' : 'unicode/Name.pl'
    ],
);

# }}}

sub path_to_inc_name($$) {
    my $path = shift;
    my $warn = shift;
    my $inc_name;

    if ($path =~ m/\.pm$/io) {
        die "$path doesn't exist" unless (-f $path);
        my $module_info = Module::Build::ModuleInfo->new_from_file($path);
        die "Module::Build::ModuleInfo error: $!" unless defined($module_info);
        $inc_name = $module_info->name();
        if (defined($inc_name)) {
            $inc_name =~ s|\:\:|\/|og;
            $inc_name .= '.pm';
        } else {
            warn "# Couldn't find include name for $path\n" if $warn;
        }
    } else {
        # Bad solution!
        (my $vol, my $dir, $inc_name) = File::Spec->splitpath($path);
    }

    return $inc_name;
}

my $Keys = 'files|keys|recurse|rv|skip|first|execute|compile|warn_missing';
sub scan_deps {
    my %args = (
        rv => {},
        (@_ and $_[0] =~ /^(?:$Keys)$/o) ? @_ : (files => [@_], recurse => 1)
    );

    if (!defined($args{keys})) {
        $args{keys} = [map {path_to_inc_name($_, $args{warn_missing})} @{$args{files}}];
    }

    my ($type, $path);
    foreach my $input_file (@{$args{files}}) {
        if ($input_file !~ $ScanFileRE) {
            warn "Skipping input file $input_file because it matches \$Module::ScanDeps::ScanFileRE\n" if $args{warn_missing};
            next;
        }

        $type = 'module';
        $type = 'data' unless $input_file =~ /\.p[mh]$/io;
        $path = $input_file;
        if ($type eq 'module') {
            # necessary because add_deps does the search for shared libraries and such
            add_deps(
                used_by => undef,
                rv => $args{rv},
                modules => [path_to_inc_name($path, $args{warn_missing})],
                skip => undef,
                warn_missing => $args{warn_missing},
            );
        }
        else {
            _add_info(
                rv      => $args{rv},
                module  => path_to_inc_name($path, $args{warn_missing}),
                file    => $path,
                used_by => undef,
                type    => $type,
            );
        }
    }

    scan_deps_static(\%args);

    if ($args{execute} or $args{compile}) {
        scan_deps_runtime(
            rv      => $args{rv},
            files   => $args{files},
            execute => $args{execute},
            compile => $args{compile},
            skip    => $args{skip}
        );
    }

    # do not include the input files themselves as dependencies!
    delete $args{rv}{$_} foreach @{$args{files}};

    return ($args{rv});
}

sub scan_deps_static {
    my ($args) = @_;
    my ($files, $keys, $recurse, $rv, $skip, $first, $execute, $compile, $_skip) =
        @$args{qw( files keys recurse rv skip first execute compile _skip )};

    $rv   ||= {};
    $_skip ||= { %{$skip || {}} };

    foreach my $file (@{$files}) {
        my $key = shift @{$keys};
        next if $_skip->{$file}++;
        next if is_insensitive_fs()
          and $file ne lc($file) and $_skip->{lc($file)}++;
        next unless $file =~ $ScanFileRE;

        local *FH;
        open FH, $file or die "Cannot open $file: $!";

        $SeenTk = 0;
        # Line-by-line scanning
        LINE:
        while (<FH>) {
            chomp(my $line = $_);
            foreach my $pm (scan_line($line)) {
                last LINE if $pm eq '__END__';

                # Skip Tk hits from Term::ReadLine and Tcl::Tk
                my $pathsep = qr/\/|\\|::/;
                if ($pm =~ /^Tk\b/) {
                  next if $file =~ /(?:^|${pathsep})Term${pathsep}ReadLine\.pm$/;
                  next if $file =~ /(?:^|${pathsep})Tcl${pathsep}Tk\W/;
                }

                if ($pm eq '__POD__') {
                    while (<FH>) { last if (/^=cut/) }
                    next LINE;
                }

                $pm = 'CGI/Apache.pm' if $file =~ /^Apache(?:\.pm)$/;

                add_deps(
                    used_by => $key,
                    rv      => $args->{rv},
                    modules => [$pm],
                    skip    => $args->{skip},
                    warn_missing => $args->{warn_missing},
                );

                my $preload = _get_preload($pm) or next;

                add_deps(
                    used_by => $key,
                    rv      => $args->{rv},
                    modules => $preload,
                    skip    => $args->{skip},
                    warn_missing => $args->{warn_missing},
                );
            }
        }
        close FH;

        # }}}
    }

    # Top-level recursion handling {{{
    while ($recurse) {
        my $count = keys %$rv;
        my @files = sort grep -T $_->{file}, values %$rv;
        scan_deps_static({
            files   => [ map $_->{file}, @files ],
            keys    => [ map $_->{key},  @files ],
            rv      => $rv,
            skip    => $skip,
            recurse => 0,
            _skip   => $_skip,
        }) or ($args->{_deep} and return);
        last if $count == keys %$rv;
    }

    # }}}

    return $rv;
}

sub scan_deps_runtime {
    my %args = (
        perl => $^X,
        rv   => {},
        (@_ and $_[0] =~ /^(?:$Keys)$/o) ? @_ : (files => [@_], recurse => 1)
    );
    my ($files, $rv, $execute, $compile, $skip, $perl) =
      @args{qw( files rv execute compile skip perl )};

    $files = (ref($files)) ? $files : [$files];

    my ($inchash, $incarray, $dl_shared_objects) = ({}, [], []);
    if ($compile) {
        my $file;

        foreach $file (@$files) {
            next unless $file =~ $ScanFileRE;

            ($inchash, $dl_shared_objects, $incarray) = ({}, [], []);
            _compile($perl, $file, $inchash, $dl_shared_objects, $incarray);

            my $rv_sub = _make_rv($inchash, $dl_shared_objects, $incarray);
            _merge_rv($rv_sub, $rv);
        }
    }
    elsif ($execute) {
        my $excarray = (ref($execute)) ? $execute : [@$files];
        my $exc;
        my $first_flag = 1;
        foreach $exc (@$excarray) {
            ($inchash, $dl_shared_objects, $incarray) = ({}, [], []);
            _execute(
                $perl, $exc, $inchash, $dl_shared_objects, $incarray,
                $first_flag
            );
            $first_flag = 0;
        }

        # XXX only retains data from last execute ...  Why? I suspect
        # the above loop was added later.  Needs test cases --Eric
        my $rv_sub = _make_rv($inchash, $dl_shared_objects, $incarray);
        _merge_rv($rv_sub, $rv);
    }

    return ($rv);
}

sub scan_line {
    my $line = shift;
    my %found;

    return '__END__' if $line =~ /^__(?:END|DATA)__$/;
    return '__POD__' if $line =~ /^=\w/;

    $line =~ s/\s*#.*$//;
    $line =~ s/[\\\/]+/\//g;

    foreach (split(/;/, $line)) {
        if (/^\s*package\s+(\w+)/) {
            $CurrentPackage = $1;
            $CurrentPackage =~ s{::}{/}g;
            return;
        }
        # use VERSION:
        if (/^\s*(?:use|require)\s+([\d\._]+)/) {
          # include feaure.pm if we have 5.9.5 or better
          if (version->new($1) >= version->new("5.9.5")) { # seems to catch 5.9, too (but not 5.9.4)
            return "feature.pm";
          }
        }

        if (my ($autouse) = /^\s*use\s+autouse\s+(["'].*?["']|\w+)/)	
        {
            $autouse =~ s/["']//g;
            $autouse =~ s{::}{/}g;
            return ("autouse.pm", "$autouse.pm");
        }

        if (my ($libs) = /\b(?:use\s+lib\s+|(?:unshift|push)\W+\@INC\W+)(.+)/)
        {
            my $archname = defined($Config{archname}) ? $Config{archname} : '';
            my $ver = defined($Config{version}) ? $Config{version} : '';
            foreach (grep(/\w/, split(/["';() ]/, $libs))) {
                unshift(@INC, "$_/$ver")           if -d "$_/$ver";
                unshift(@INC, "$_/$archname")      if -d "$_/$archname";
                unshift(@INC, "$_/$ver/$archname") if -d "$_/$ver/$archname";
            }
            next;
        }

        $found{$_}++ for scan_chunk($_);
    }

    return sort keys %found;
}

sub scan_chunk {
    my $chunk = shift;

    # Module name extraction heuristics {{{
    my $module = eval {
        $_ = $chunk;

        return [ 'base.pm',
            map { s{::}{/}g; "$_.pm" }
              grep { length and !/^q[qw]?$/ } split(/[^\w:]+/, $1) ]
          if /^\s* use \s+ base \s+ (.*)/sx;

        return [ 'prefork.pm',
            map { s{::}{/}g; "$_.pm" }
              grep { length and !/^q[qw]?$/ } split(/[^\w:]+/, $1) ]

          if /^\s* use \s+ base \s+ (.*)/sx;
        return [ 'Class/Autouse.pm',
            map { s{::}{/}g; "$_.pm" }
              grep { length and !/^:|^q[qw]?$/ } split(/[^\w:]+/, $1) ]
          if /^\s* use \s+ Class::Autouse \s+ (.*)/sx
              or /^\s* Class::Autouse \s* -> \s* autouse \s* (.*)/sx;

        return [ 'POE.pm',
            map { s{::}{/}g; "POE/$_.pm" }
              grep { length and !/^q[qw]?$/ } split(/[^\w:]+/, $1) ]
          if /^\s* use \s+ POE \s+ (.*)/sx;

        return [ 'encoding.pm',
            map { _find_encoding($_) }
              grep { length and !/^q[qw]?$/ } split(/[^\w:-]+/, $1) ]
          if /^\s* use \s+ encoding \s+ (.*)/sx;

        return $1 if /(?:^|\s)(?:use|no|require)\s+([\w:\.\-\\\/\"\']+)/;
        return $1
          if /(?:^|\s)(?:use|no|require)\s+\(\s*([\w:\.\-\\\/\"\']+)\s*\)/;

        if (   s/(?:^|\s)eval\s+\"([^\"]+)\"/$1/
            or s/(?:^|\s)eval\s*\(\s*\"([^\"]+)\"\s*\)/$1/)
        {
            return $1 if /(?:^|\s)(?:use|no|require)\s+([\w:\.\-\\\/\"\']*)/;
        }

        if (/(<[^>]*[^\$\w>][^>]*>)/) {
            my $diamond = $1;
            return "File/Glob.pm" if $diamond =~ /[*?\[\]{}~\\]/;
        }
        return "DBD/$1.pm"    if /\b[Dd][Bb][Ii]:(\w+):/;
        if (/(?:(:encoding)|\b(?:en|de)code)\(\s*['"]?([-\w]+)/) {
            my $mod = _find_encoding($2);
            return [ 'PerlIO.pm', $mod ] if $1 and $mod;
            return $mod if $mod;
        }
        return $1 if /(?:^|\s)(?:do|require)\s+[^"]*"(.*?)"/;
        return $1 if /(?:^|\s)(?:do|require)\s+[^']*'(.*?)'/;
        return $1 if /[^\$]\b([\w:]+)->\w/ and $1 ne 'Tk' and $1 ne 'shift';
        return $1 if /\b(\w[\w:]*)::\w+\(/ and $1 ne 'main' and $1 ne 'SUPER';

        if ($SeenTk) {
            my @modules;
            while (/->\s*([A-Z]\w+)/g) {
                push @modules, "Tk/$1.pm";
            }
            while (/->\s*Scrolled\W+([A-Z]\w+)/g) {
                push @modules, "Tk/$1.pm";
                push @modules, "Tk/Scrollbar.pm";
            }
            return \@modules;
        }
        return;
    };

    # }}}

    return unless defined($module);
    return wantarray ? @$module : $module->[0] if ref($module);

    $module =~ s/^['"]//;
    return unless $module =~ /^\w/;

    $module =~ s/\W+$//;
    $module =~ s/::/\//g;
    return if $module =~ /^(?:[\d\._]+|'.*[^']|".*[^"])$/;

    $module .= ".pm" unless $module =~ /\./;
    return $module;
}

sub _find_encoding {
    return unless $] >= 5.008 and eval { require Encode; %Encode::ExtModule };

    my $mod = $Encode::ExtModule{ Encode::find_encoding($_[0])->name }
      or return;
    $mod =~ s{::}{/}g;
    return "$mod.pm";
}

sub _add_info {
    my %args = @_;
    my ($rv, $module, $file, $used_by, $type) = @args{qw/rv module file used_by type/};

    return unless defined($module) and defined($file);

    # Ensure file is always absolute
    $file = File::Spec->rel2abs($file);
    $file =~ s|\\|\/|go;

    # Avoid duplicates that can arise due to case differences that don't actually 
    # matter on a case tolerant system
    if (File::Spec->case_tolerant()) {
        foreach my $key (keys %$rv) {
            if (lc($key) eq lc($module)) {
                $module = $key;
                last;
            }
        }
        if (defined($used_by)) {
            if (lc($used_by) eq lc($module)) {
                $used_by = $module;
            } else {
                foreach my $key (keys %$rv) {
                    if (lc($key) eq lc($used_by)) {
                        $used_by = $key;
                        last;
                    }
                }
            }
        }
    }

    $rv->{$module} ||= {
        file => $file,
        key  => $module,
        type => $type,
    };

    if (defined($used_by) and $used_by ne $module) {
        push @{ $rv->{$module}{used_by} }, $used_by
          if  ( (!File::Spec->case_tolerant() && !grep { $_ eq $used_by } @{ $rv->{$module}{used_by} })
             or ( File::Spec->case_tolerant() && !grep { lc($_) eq lc($used_by) } @{ $rv->{$module}{used_by} }));

        # We assume here that another _add_info will be called to provide the other parts of $rv->{$used_by}    
        push @{ $rv->{$used_by}{uses} }, $module
          if  ( (!File::Spec->case_tolerant() && !grep { $_ eq $module } @{ $rv->{$used_by}{uses} })
             or ( File::Spec->case_tolerant() && !grep { lc($_) eq lc($module) } @{ $rv->{$used_by}{uses} }));
    }
}

# This subroutine relies on not being called for modules that have already been visited
sub add_deps {
    my %args =
      ((@_ and $_[0] =~ /^(?:modules|rv|used_by|warn_missing)$/)
        ? @_
        : (rv => (ref($_[0]) ? shift(@_) : undef), modules => [@_]));

    my $rv = $args{rv}   || {};
    my $skip = $args{skip} || {};
    my $used_by = $args{used_by};

    foreach my $module (@{ $args{modules} }) {
        my $file = _find_in_inc($module)
          or _warn_of_missing_module($module, $args{warn_missing}), next;
        next if $skip->{$file};

        if (exists $rv->{$module}) {
            _add_info( rv     => $rv,      module  => $module,
                       file   => $file,    used_by => $used_by,
                       type   => undef );
            next;
        }

        my $type = 'module';
        $type = 'data' unless $file =~ /\.p[mh]$/i;
        _add_info( rv     => $rv,   module  => $module,
                   file   => $file, used_by => $used_by,
                   type   => $type );

        if ($module =~ /(.*?([^\/]*))\.p[mh]$/i) {
            my ($path, $basename) = ($1, $2);

            foreach (_glob_in_inc("auto/$path")) {
                next if $_->{file} =~ m{\bauto/$path/.*/};  # weed out subdirs
                next if $_->{name} =~ m/(?:^|\/)\.(?:exists|packlist)$/;
                my $ext = lc($1) if $_->{name} =~ /(\.[^.]+)$/;
                next if $ext eq lc(lib_ext());
                my $type = 'shared' if $ext eq lc(dl_ext());
                $type = 'autoload' if $ext eq '.ix' or $ext eq '.al';
                $type ||= 'data';

                _add_info( rv     => $rv,        module  => "auto/$path/$_->{name}",
                           file   => $_->{file}, used_by => $module,
                           type   => $type );
            }
        }
    }

    return $rv;
}

sub _find_in_inc {
    my $file = shift;

    foreach my $dir (grep !/\bBSDPAN\b/, @INC, @IncludeLibs) {
        return "$dir/$file" if -f "$dir/$file";
    }

    # absolute file names
    return $file if -f $file;

    return;
}

sub _glob_in_inc {
    my $subdir  = shift;
    my $pm_only = shift;
    my @files;

    require File::Find;

    $subdir =~ s/\$CurrentPackage/$CurrentPackage/;

    foreach my $dir (map "$_/$subdir", grep !/\bBSDPAN\b/, @INC, @IncludeLibs) {
        next unless -d $dir;
        File::Find::find(
            sub {
                my $name = $File::Find::name;
                $name =~ s!^\Q$dir\E/!!;
                return if $pm_only and lc($name) !~ /\.p[mh]$/i;
                push @files, $pm_only
                  ? "$subdir/$name"
                  : {             file => $File::Find::name,
                    name => $name,
                  }
                  if -f;
            },
            $dir
        );
    }

    return @files;
}

# App::Packer compatibility functions

sub new {
    my ($class, $self) = @_;
    return bless($self ||= {}, $class);
}

sub set_file {
    my $self = shift;
    foreach my $script (@_) {
        my ($vol, $dir, $file) = File::Spec->splitpath($script);
        $self->{main} = {
            key  => $file,
            file => $script,
        };
    }
}

sub set_options {
    my $self = shift;
    my %args = @_;
    foreach my $module (@{ $args{add_modules} }) {
        $module =~ s/::/\//g;
        $module .= '.pm' unless $module =~ /\.p[mh]$/i;
        my $file = _find_in_inc($module)
          or _warn_of_missing_module($module, $args{warn_missing}), next;
        $self->{files}{$module} = $file;
    }
}

sub calculate_info {
    my $self = shift;
    my $rv   = scan_deps(
        'keys' => [ $self->{main}{key}, sort keys %{ $self->{files} }, ],
        files  => [ $self->{main}{file},
            map { $self->{files}{$_} } sort keys %{ $self->{files} },
        ],
        recurse => 1,
    );

    my $info = {
        main => {  file     => $self->{main}{file},
                   store_as => $self->{main}{key},
        },
    };

    my %cache = ($self->{main}{key} => $info->{main});
    foreach my $key (sort keys %{ $self->{files} }) {
        my $file = $self->{files}{$key};

        $cache{$key} = $info->{modules}{$key} = {
            file     => $file,
            store_as => $key,
            used_by  => [ $self->{main}{key} ],
        };
    }

    foreach my $key (sort keys %{$rv}) {
        my $val = $rv->{$key};
        if ($cache{ $val->{key} }) {
            defined($val->{used_by}) or next;
            push @{ $info->{ $val->{type} }->{ $val->{key} }->{used_by} },
              @{ $val->{used_by} };
        }
        else {
            $cache{ $val->{key} } = $info->{ $val->{type} }->{ $val->{key} } =
              {        file     => $val->{file},
                store_as => $val->{key},
                used_by  => $val->{used_by},
              };
        }
    }

    $self->{info} = { main => $info->{main} };

    foreach my $type (sort keys %{$info}) {
        next if $type eq 'main';

        my @val;
        if (UNIVERSAL::isa($info->{$type}, 'HASH')) {
            foreach my $val (sort values %{ $info->{$type} }) {
                @{ $val->{used_by} } = map $cache{$_} || "!!$_!!",
                  @{ $val->{used_by} };
                push @val, $val;
            }
        }

        $type = 'modules' if $type eq 'module';
        $self->{info}{$type} = \@val;
    }
}

sub get_files {
    my $self = shift;
    return $self->{info};
}

# scan_deps_runtime utility functions

sub _compile {
    my ($perl, $file, $inchash, $dl_shared_objects, $incarray) = @_;

    my ($fhout, $fname) = File::Temp::tempfile("XXXXXX");
    my $fhin  = FileHandle->new($file) or die "Couldn't open $file\n";

    my $line = do { local $/; <$fhin> };
    $line =~ s/use Module::ScanDeps::DataFeed.*?\n//sg;
    $line =~ s/^(.*?)((?:[\r\n]+__(?:DATA|END)__[\r\n]+)|$)/
use Module::ScanDeps::DataFeed '$fname.out';
sub {
$1
}
$2/s;
    $fhout->print($line);
    $fhout->close;
    $fhin->close;

    system($perl, $fname);

    _extract_info("$fname.out", $inchash, $dl_shared_objects, $incarray);
    unlink("$fname");
    unlink("$fname.out");
}

sub _execute {
    my ($perl, $file, $inchash, $dl_shared_objects, $incarray, $firstflag) = @_;

    $DB::single = $DB::single = 1;
    my ($fhout, $fname) = File::Temp::tempfile("XXXXXX");
    $fname = _abs_path($fname);
    my $fhin  = FileHandle->new($file) or die "Couldn't open $file";

    my $line = do { local $/; <$fhin> };
    $line =~ s/use Module::ScanDeps::DataFeed.*?\n//sg;
    $line = "use Module::ScanDeps::DataFeed '$fname.out';\n" . $line;
    $fhout->print($line);
    $fhout->close;
    $fhin->close;

    File::Path::rmtree( ['_Inline'], 0, 1); # XXX hack
    system($perl, (map { "-I$_" } @IncludeLibs), $fname) == 0 or die "SYSTEM ERROR in executing $file: $?";

    _extract_info("$fname.out", $inchash, $dl_shared_objects, $incarray);
    unlink("$fname");
    unlink("$fname.out");
}

# create a new hashref, applying fixups
sub _make_rv {
    my ($inchash, $dl_shared_objects, $inc_array) = @_;

    my $rv = {};
    my @newinc = map(quotemeta($_), @$inc_array);
    my $inc = join('|', sort { length($b) <=> length($a) } @newinc);
    # don't pack lib/c:/ or lib/C:/
    $inc = qr/$inc/i if(is_insensitive_fs());

    require File::Spec;

    my $key;
    foreach $key (keys(%$inchash)) {
        my $newkey = $key;
        $newkey =~ s"^(?:(?:$inc)/?)""sg if File::Spec->file_name_is_absolute($newkey);

        $rv->{$newkey} = {
            'used_by' => [],
            'file'    => $inchash->{$key},
            'type'    => _gettype($inchash->{$key}),
            'key'     => $key
        };
    }

    my $dl_file;
    foreach $dl_file (@$dl_shared_objects) {
        my $key = $dl_file;
        $key =~ s"^(?:(?:$inc)/?)""s;

        $rv->{$key} = {
            'used_by' => [],
            'file'    => $dl_file,
            'type'    => 'shared',
            'key'     => $key
        };
    }

    return $rv;
}

sub _extract_info {
    my ($fname, $inchash, $dl_shared_objects, $incarray) = @_;

    use vars qw(%inchash @dl_shared_objects @incarray);
    my $fh = FileHandle->new($fname) or die "Couldn't open $fname";
    my $line = do { local $/; <$fh> };
    $fh->close;

    eval $line;

    $inchash->{$_} = $inchash{$_} for keys %inchash;
    @$dl_shared_objects = @dl_shared_objects;
    @$incarray          = @incarray;
}

sub _gettype {
    my $name = shift;
    my $dlext = quotemeta(dl_ext());

    return 'autoload' if $name =~ /(?:\.ix|\.al|\.bs)$/i;
    return 'module'   if $name =~ /\.p[mh]$/i;
    return 'shared'   if $name =~ /\.$dlext$/i;
    return 'data';
}

# merge all keys from $rv_sub into the $rv mega-ref
sub _merge_rv {
    my ($rv_sub, $rv) = @_;

    my $key;
    foreach $key (keys(%$rv_sub)) {
        my %mark;
        if ($rv->{$key} and _not_dup($key, $rv, $rv_sub)) {
            warn "Different modules for file '$key' were found.\n"
                . " -> Using '" . _abs_path($rv_sub->{$key}{file}) . "'.\n"
                . " -> Ignoring '" . _abs_path($rv->{$key}{file}) . "'.\n";
            $rv->{$key}{used_by} = [
                grep (!$mark{$_}++,
                    @{ $rv->{$key}{used_by} },
                    @{ $rv_sub->{$key}{used_by} })
            ];
            @{ $rv->{$key}{used_by} } = grep length, @{ $rv->{$key}{used_by} };
            $rv->{$key}{file} = $rv_sub->{$key}{file};
        }
        elsif ($rv->{$key}) {
            $rv->{$key}{used_by} = [
                grep (!$mark{$_}++,
                    @{ $rv->{$key}{used_by} },
                    @{ $rv_sub->{$key}{used_by} })
            ];
            @{ $rv->{$key}{used_by} } = grep length, @{ $rv->{$key}{used_by} };
        }
        else {
            $rv->{$key} = {
                used_by => [ @{ $rv_sub->{$key}{used_by} } ],
                file    => $rv_sub->{$key}{file},
                key     => $rv_sub->{$key}{key},
                type    => $rv_sub->{$key}{type}
            };

            @{ $rv->{$key}{used_by} } = grep length, @{ $rv->{$key}{used_by} };
        }
    }
}

sub _not_dup {
    my ($key, $rv1, $rv2) = @_;
    if (File::Spec->case_tolerant()) {
        return lc(_abs_path($rv1->{$key}{file})) ne lc(_abs_path($rv2->{$key}{file}));
    }
    else {
        return _abs_path($rv1->{$key}{file}) ne _abs_path($rv2->{$key}{file});
    }
}

sub _abs_path {
    return join(
        '/',
        Cwd::abs_path(File::Basename::dirname($_[0])),
        File::Basename::basename($_[0]),
    );
}


sub _warn_of_missing_module {
    my $module = shift;
    my $warn = shift;
    return if not $warn;
    return if not $module =~ /\.p[ml]$/;
    warn "# Could not find source file '$module' in \@INC or \@IncludeLibs. Skipping it.\n"
      if not -f $module;
}

sub _get_preload {
    my $pm = shift;
    my $preload = $Preload{$pm} or return();
    if ($preload eq 'sub') {
        $pm =~ s/\.p[mh]$//i;
        $preload = [ _glob_in_inc($pm, 1) ];
    }
    elsif (UNIVERSAL::isa($preload, 'CODE')) {
        $preload = [ $preload->($pm) ];
    }
    return $preload;
}

1;
__END__

=head1 SEE ALSO

L<scandeps.pl> is a bundled utility that writes C<PREREQ_PM> section
for a number of files.

An application of B<Module::ScanDeps> is to generate executables from
scripts that contains prerequisite modules; this module supports two
such projects, L<PAR> and L<App::Packer>.  Please see their respective
documentations on CPAN for further information.

=head1 AUTHORS

Audrey Tang E<lt>cpan@audreyt.orgE<gt>

To a lesser degree: Steffen Mueller E<lt>smueller@cpan.orgE<gt>

Parts of heuristics were deduced from:

=over 4

=item *

B<PerlApp> by ActiveState Tools Corp L<http://www.activestate.com/>

=item *

B<Perl2Exe> by IndigoStar, Inc L<http://www.indigostar.com/>

=back

The B<scan_deps_runtime> function is contributed by Edward S. Peschko.

L<http://par.perl.org/> is the official website for this module.  You
can write to the mailing list at E<lt>par@perl.orgE<gt>, or send an empty
mail to E<lt>par-subscribe@perl.orgE<gt> to participate in the discussion.

Please submit bug reports to E<lt>bug-Module-ScanDeps@rt.cpan.orgE<gt>.

=head1 COPYRIGHT

Copyright 2002-2008 by
Audrey Tang E<lt>cpan@audreyt.orgE<gt>;
2005-2008 by Steffen Mueller E<lt>smueller@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut
