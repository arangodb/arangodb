# -*- Mode: cperl; coding: utf-8; cperl-indent-level: 4 -*-
use strict;
package CPAN;
$CPAN::VERSION = '1.9205';
$CPAN::VERSION = eval $CPAN::VERSION if $CPAN::VERSION =~ /_/;

use CPAN::HandleConfig;
use CPAN::Version;
use CPAN::Debug;
use CPAN::Queue;
use CPAN::Tarzip;
use CPAN::DeferedCode;
use Carp ();
use Config ();
use Cwd ();
use DirHandle ();
use Exporter ();
use ExtUtils::MakeMaker qw(prompt); # for some unknown reason,
                                    # 5.005_04 does not work without
                                    # this
use File::Basename ();
use File::Copy ();
use File::Find;
use File::Path ();
use File::Spec ();
use FileHandle ();
use Fcntl qw(:flock);
use Safe ();
use Sys::Hostname qw(hostname);
use Text::ParseWords ();
use Text::Wrap ();

sub find_perl ();

# we need to run chdir all over and we would get at wrong libraries
# there
BEGIN {
    if (File::Spec->can("rel2abs")) {
        for my $inc (@INC) {
            $inc = File::Spec->rel2abs($inc) unless ref $inc;
        }
    }
}
no lib ".";

require Mac::BuildTools if $^O eq 'MacOS';
$ENV{PERL5_CPAN_IS_RUNNING}=$$;
$ENV{PERL5_CPANPLUS_IS_RUNNING}=$$; # https://rt.cpan.org/Ticket/Display.html?id=23735

END { $CPAN::End++; &cleanup; }

$CPAN::Signal ||= 0;
$CPAN::Frontend ||= "CPAN::Shell";
unless (@CPAN::Defaultsites) {
    @CPAN::Defaultsites = map {
        CPAN::URL->new(TEXT => $_, FROM => "DEF")
    }
        "http://www.perl.org/CPAN/",
            "ftp://ftp.perl.org/pub/CPAN/";
}
# $CPAN::iCwd (i for initial) is going to be initialized during find_perl
$CPAN::Perl ||= CPAN::find_perl();
$CPAN::Defaultdocs ||= "http://search.cpan.org/perldoc?";
$CPAN::Defaultrecent ||= "http://search.cpan.org/uploads.rdf";
$CPAN::Defaultrecent ||= "http://cpan.uwinnipeg.ca/htdocs/cpan.xml";

# our globals are getting a mess
use vars qw(
            $AUTOLOAD
            $Be_Silent
            $CONFIG_DIRTY
            $Defaultdocs
            $Echo_readline
            $Frontend
            $GOTOSHELL
            $HAS_USABLE
            $Have_warned
            $MAX_RECURSION
            $META
            $RUN_DEGRADED
            $Signal
            $SQLite
            $Suppress_readline
            $VERSION
            $autoload_recursion
            $term
            @Defaultsites
            @EXPORT
           );

$MAX_RECURSION = 32;

@CPAN::ISA = qw(CPAN::Debug Exporter);

# note that these functions live in CPAN::Shell and get executed via
# AUTOLOAD when called directly
@EXPORT = qw(
             autobundle
             bundle
             clean
             cvs_import
             expand
             force
             fforce
             get
             install
             install_tested
             is_tested
             make
             mkmyconfig
             notest
             perldoc
             readme
             recent
             recompile
             report
             shell
             smoke
             test
             upgrade
            );

sub soft_chdir_with_alternatives ($);

{
    $autoload_recursion ||= 0;

    #-> sub CPAN::AUTOLOAD ;
    sub AUTOLOAD {
        $autoload_recursion++;
        my($l) = $AUTOLOAD;
        $l =~ s/.*:://;
        if ($CPAN::Signal) {
            warn "Refusing to autoload '$l' while signal pending";
            $autoload_recursion--;
            return;
        }
        if ($autoload_recursion > 1) {
            my $fullcommand = join " ", map { "'$_'" } $l, @_;
            warn "Refusing to autoload $fullcommand in recursion\n";
            $autoload_recursion--;
            return;
        }
        my(%export);
        @export{@EXPORT} = '';
        CPAN::HandleConfig->load unless $CPAN::Config_loaded++;
        if (exists $export{$l}) {
            CPAN::Shell->$l(@_);
        } else {
            die(qq{Unknown CPAN command "$AUTOLOAD". }.
                qq{Type ? for help.\n});
        }
        $autoload_recursion--;
    }
}

#-> sub CPAN::shell ;
sub shell {
    my($self) = @_;
    $Suppress_readline = ! -t STDIN unless defined $Suppress_readline;
    CPAN::HandleConfig->load unless $CPAN::Config_loaded++;

    my $oprompt = shift || CPAN::Prompt->new;
    my $prompt = $oprompt;
    my $commandline = shift || "";
    $CPAN::CurrentCommandId ||= 1;

    local($^W) = 1;
    unless ($Suppress_readline) {
        require Term::ReadLine;
        if (! $term
            or
            $term->ReadLine eq "Term::ReadLine::Stub"
           ) {
            $term = Term::ReadLine->new('CPAN Monitor');
        }
        if ($term->ReadLine eq "Term::ReadLine::Gnu") {
            my $attribs = $term->Attribs;
            $attribs->{attempted_completion_function} = sub {
                &CPAN::Complete::gnu_cpl;
            }
        } else {
            $readline::rl_completion_function =
                $readline::rl_completion_function = 'CPAN::Complete::cpl';
        }
        if (my $histfile = $CPAN::Config->{'histfile'}) {{
            unless ($term->can("AddHistory")) {
                $CPAN::Frontend->mywarn("Terminal does not support AddHistory.\n");
                last;
            }
            $META->readhist($term,$histfile);
        }}
        for ($CPAN::Config->{term_ornaments}) { # alias
            local $Term::ReadLine::termcap_nowarn = 1;
            $term->ornaments($_) if defined;
        }
        # $term->OUT is autoflushed anyway
        my $odef = select STDERR;
        $| = 1;
        select STDOUT;
        $| = 1;
        select $odef;
    }

    $META->checklock();
    my @cwd = grep { defined $_ and length $_ }
        CPAN::anycwd(),
              File::Spec->can("tmpdir") ? File::Spec->tmpdir() : (),
                    File::Spec->rootdir();
    my $try_detect_readline;
    $try_detect_readline = $term->ReadLine eq "Term::ReadLine::Stub" if $term;
    unless ($CPAN::Config->{inhibit_startup_message}) {
        my $rl_avail = $Suppress_readline ? "suppressed" :
            ($term->ReadLine ne "Term::ReadLine::Stub") ? "enabled" :
                "available (maybe install Bundle::CPAN or Bundle::CPANxxl?)";
        $CPAN::Frontend->myprint(
                                 sprintf qq{
cpan shell -- CPAN exploration and modules installation (v%s)
ReadLine support %s

},
                                 $CPAN::VERSION,
                                 $rl_avail
                                )
    }
    my($continuation) = "";
    my $last_term_ornaments;
  SHELLCOMMAND: while () {
        if ($Suppress_readline) {
            if ($Echo_readline) {
                $|=1;
            }
            print $prompt;
            last SHELLCOMMAND unless defined ($_ = <> );
            if ($Echo_readline) {
                # backdoor: I could not find a way to record sessions
                print $_;
            }
            chomp;
        } else {
            last SHELLCOMMAND unless
                defined ($_ = $term->readline($prompt, $commandline));
        }
        $_ = "$continuation$_" if $continuation;
        s/^\s+//;
        next SHELLCOMMAND if /^$/;
        s/^\s*\?\s*/help /;
        if (/^(?:q(?:uit)?|bye|exit)$/i) {
            last SHELLCOMMAND;
        } elsif (s/\\$//s) {
            chomp;
            $continuation = $_;
            $prompt = "    > ";
        } elsif (/^\!/) {
            s/^\!//;
            my($eval) = $_;
            package CPAN::Eval;
            use strict;
            use vars qw($import_done);
            CPAN->import(':DEFAULT') unless $import_done++;
            CPAN->debug("eval[$eval]") if $CPAN::DEBUG;
            eval($eval);
            warn $@ if $@;
            $continuation = "";
            $prompt = $oprompt;
        } elsif (/./) {
            my(@line);
            eval { @line = Text::ParseWords::shellwords($_) };
            warn($@), next SHELLCOMMAND if $@;
            warn("Text::Parsewords could not parse the line [$_]"),
                next SHELLCOMMAND unless @line;
            $CPAN::META->debug("line[".join("|",@line)."]") if $CPAN::DEBUG;
            my $command = shift @line;
            eval { CPAN::Shell->$command(@line) };
            if ($@) {
                my $err = "$@";
                if ($err =~ /\S/) {
                    require Carp;
                    require Dumpvalue;
                    my $dv = Dumpvalue->new();
                    Carp::cluck(sprintf "Catching error: %s", $dv->stringify($err));
                }
            }
            if ($command =~ /^(
                             # classic commands
                             make
                             |test
                             |install
                             |clean

                             # pragmas for classic commands
                             |ff?orce
                             |notest

                             # compounds
                             |report
                             |smoke
                             |upgrade
                            )$/x) {
                # only commands that tell us something about failed distros
                CPAN::Shell->failed($CPAN::CurrentCommandId,1);
            }
            soft_chdir_with_alternatives(\@cwd);
            $CPAN::Frontend->myprint("\n");
            $continuation = "";
            $CPAN::CurrentCommandId++;
            $prompt = $oprompt;
        }
    } continue {
        $commandline = ""; # I do want to be able to pass a default to
                           # shell, but on the second command I see no
                           # use in that
        $Signal=0;
        CPAN::Queue->nullify_queue;
        if ($try_detect_readline) {
            if ($CPAN::META->has_inst("Term::ReadLine::Gnu")
                ||
                $CPAN::META->has_inst("Term::ReadLine::Perl")
            ) {
                delete $INC{"Term/ReadLine.pm"};
                my $redef = 0;
                local($SIG{__WARN__}) = CPAN::Shell::paintdots_onreload(\$redef);
                require Term::ReadLine;
                $CPAN::Frontend->myprint("\n$redef subroutines in ".
                                         "Term::ReadLine redefined\n");
                $GOTOSHELL = 1;
            }
        }
        if ($term and $term->can("ornaments")) {
            for ($CPAN::Config->{term_ornaments}) { # alias
                if (defined $_) {
                    if (not defined $last_term_ornaments
                        or $_ != $last_term_ornaments
                    ) {
                        local $Term::ReadLine::termcap_nowarn = 1;
                        $term->ornaments($_);
                        $last_term_ornaments = $_;
                    }
                } else {
                    undef $last_term_ornaments;
                }
            }
        }
        for my $class (qw(Module Distribution)) {
            # again unsafe meta access?
            for my $dm (keys %{$CPAN::META->{readwrite}{"CPAN::$class"}}) {
                next unless $CPAN::META->{readwrite}{"CPAN::$class"}{$dm}{incommandcolor};
                CPAN->debug("BUG: $class '$dm' was in command state, resetting");
                delete $CPAN::META->{readwrite}{"CPAN::$class"}{$dm}{incommandcolor};
            }
        }
        if ($GOTOSHELL) {
            $GOTOSHELL = 0; # not too often
            $META->savehist if $CPAN::term && $CPAN::term->can("GetHistory");
            @_ = ($oprompt,"");
            goto &shell;
        }
    }
    soft_chdir_with_alternatives(\@cwd);
}

#-> CPAN::soft_chdir_with_alternatives ;
sub soft_chdir_with_alternatives ($) {
    my($cwd) = @_;
    unless (@$cwd) {
        my $root = File::Spec->rootdir();
        $CPAN::Frontend->mywarn(qq{Warning: no good directory to chdir to!
Trying '$root' as temporary haven.
});
        push @$cwd, $root;
    }
    while () {
        if (chdir $cwd->[0]) {
            return;
        } else {
            if (@$cwd>1) {
                $CPAN::Frontend->mywarn(qq{Could not chdir to "$cwd->[0]": $!
Trying to chdir to "$cwd->[1]" instead.
});
                shift @$cwd;
            } else {
                $CPAN::Frontend->mydie(qq{Could not chdir to "$cwd->[0]": $!});
            }
        }
    }
}

sub _flock {
    my($fh,$mode) = @_;
    if ($Config::Config{d_flock}) {
        return flock $fh, $mode;
    } elsif (!$Have_warned->{"d_flock"}++) {
        $CPAN::Frontend->mywarn("Your OS does not support locking; continuing and ignoring all locking issues\n");
        $CPAN::Frontend->mysleep(5);
        return 1;
    } else {
        return 1;
    }
}

sub _yaml_module () {
    my $yaml_module = $CPAN::Config->{yaml_module} || "YAML";
    if (
        $yaml_module ne "YAML"
        &&
        !$CPAN::META->has_inst($yaml_module)
       ) {
        # $CPAN::Frontend->mywarn("'$yaml_module' not installed, falling back to 'YAML'\n");
        $yaml_module = "YAML";
    }
    if ($yaml_module eq "YAML"
        &&
        $CPAN::META->has_inst($yaml_module)
        &&
        $YAML::VERSION < 0.60
        &&
        !$Have_warned->{"YAML"}++
       ) {
        $CPAN::Frontend->mywarn("Warning: YAML version '$YAML::VERSION' is too low, please upgrade!\n".
                                "I'll continue but problems are *very* likely to happen.\n"
                               );
        $CPAN::Frontend->mysleep(5);
    }
    return $yaml_module;
}

# CPAN::_yaml_loadfile
sub _yaml_loadfile {
    my($self,$local_file) = @_;
    return +[] unless -s $local_file;
    my $yaml_module = _yaml_module;
    if ($CPAN::META->has_inst($yaml_module)) {
        # temporarly enable yaml code deserialisation
        no strict 'refs';
        # 5.6.2 could not do the local() with the reference
        local $YAML::LoadCode;
        local $YAML::Syck::LoadCode;
        ${ "$yaml_module\::LoadCode" } = $CPAN::Config->{yaml_load_code} || 0;

        my $code;
        if ($code = UNIVERSAL::can($yaml_module, "LoadFile")) {
            my @yaml;
            eval { @yaml = $code->($local_file); };
            if ($@) {
                # this shall not be done by the frontend
                die CPAN::Exception::yaml_process_error->new($yaml_module,$local_file,"parse",$@);
            }
            return \@yaml;
        } elsif ($code = UNIVERSAL::can($yaml_module, "Load")) {
            local *FH;
            open FH, $local_file or die "Could not open '$local_file': $!";
            local $/;
            my $ystream = <FH>;
            my @yaml;
            eval { @yaml = $code->($ystream); };
            if ($@) {
                # this shall not be done by the frontend
                die CPAN::Exception::yaml_process_error->new($yaml_module,$local_file,"parse",$@);
            }
            return \@yaml;
        }
    } else {
        # this shall not be done by the frontend
        die CPAN::Exception::yaml_not_installed->new($yaml_module, $local_file, "parse");
    }
    return +[];
}

# CPAN::_yaml_dumpfile
sub _yaml_dumpfile {
    my($self,$local_file,@what) = @_;
    my $yaml_module = _yaml_module;
    if ($CPAN::META->has_inst($yaml_module)) {
        my $code;
        if (UNIVERSAL::isa($local_file, "FileHandle")) {
            $code = UNIVERSAL::can($yaml_module, "Dump");
            eval { print $local_file $code->(@what) };
        } elsif ($code = UNIVERSAL::can($yaml_module, "DumpFile")) {
            eval { $code->($local_file,@what); };
        } elsif ($code = UNIVERSAL::can($yaml_module, "Dump")) {
            local *FH;
            open FH, ">$local_file" or die "Could not open '$local_file': $!";
            print FH $code->(@what);
        }
        if ($@) {
            die CPAN::Exception::yaml_process_error->new($yaml_module,$local_file,"dump",$@);
        }
    } else {
        if (UNIVERSAL::isa($local_file, "FileHandle")) {
            # I think this case does not justify a warning at all
        } else {
            die CPAN::Exception::yaml_not_installed->new($yaml_module, $local_file, "dump");
        }
    }
}

sub _init_sqlite () {
    unless ($CPAN::META->has_inst("CPAN::SQLite")) {
        $CPAN::Frontend->mywarn(qq{CPAN::SQLite not installed, trying to work without\n})
            unless $Have_warned->{"CPAN::SQLite"}++;
        return;
    }
    require CPAN::SQLite::META; # not needed since CVS version of 2006-12-17
    $CPAN::SQLite ||= CPAN::SQLite::META->new($CPAN::META);
}

{
    my $negative_cache = {};
    sub _sqlite_running {
        if ($negative_cache->{time} && time < $negative_cache->{time} + 60) {
            # need to cache the result, otherwise too slow
            return $negative_cache->{fact};
        } else {
            $negative_cache = {}; # reset
        }
        my $ret = $CPAN::Config->{use_sqlite} && ($CPAN::SQLite || _init_sqlite());
        return $ret if $ret; # fast anyway
        $negative_cache->{time} = time;
        return $negative_cache->{fact} = $ret;
    }
}

package CPAN::CacheMgr;
use strict;
@CPAN::CacheMgr::ISA = qw(CPAN::InfoObj CPAN);
use File::Find;

package CPAN::FTP;
use strict;
use Fcntl qw(:flock);
use vars qw($connect_to_internet_ok $Ua $Thesite $ThesiteURL $Themethod);
@CPAN::FTP::ISA = qw(CPAN::Debug);

package CPAN::LWP::UserAgent;
use strict;
use vars qw(@ISA $USER $PASSWD $SETUPDONE);
# we delay requiring LWP::UserAgent and setting up inheritance until we need it

package CPAN::Complete;
use strict;
@CPAN::Complete::ISA = qw(CPAN::Debug);
# Q: where is the "How do I add a new command" HOWTO?
# A: svn diff -r 1048:1049 where andk added the report command
@CPAN::Complete::COMMANDS = sort qw(
                                    ? ! a b d h i m o q r u
                                    autobundle
                                    bye
                                    clean
                                    cvs_import
                                    dump
                                    exit
                                    failed
                                    force
                                    fforce
                                    hosts
                                    install
                                    install_tested
                                    is_tested
                                    look
                                    ls
                                    make
                                    mkmyconfig
                                    notest
                                    perldoc
                                    quit
                                    readme
                                    recent
                                    recompile
                                    reload
                                    report
                                    reports
                                    scripts
                                    smoke
                                    test
                                    upgrade
);

package CPAN::Index;
use strict;
use vars qw($LAST_TIME $DATE_OF_02 $DATE_OF_03 $HAVE_REANIMATED);
@CPAN::Index::ISA = qw(CPAN::Debug);
$LAST_TIME ||= 0;
$DATE_OF_03 ||= 0;
# use constant PROTOCOL => "2.0"; # outcommented to avoid warning on upgrade from 1.57
sub PROTOCOL { 2.0 }

package CPAN::InfoObj;
use strict;
@CPAN::InfoObj::ISA = qw(CPAN::Debug);

package CPAN::Author;
use strict;
@CPAN::Author::ISA = qw(CPAN::InfoObj);

package CPAN::Distribution;
use strict;
@CPAN::Distribution::ISA = qw(CPAN::InfoObj);

package CPAN::Bundle;
use strict;
@CPAN::Bundle::ISA = qw(CPAN::Module);

package CPAN::Module;
use strict;
@CPAN::Module::ISA = qw(CPAN::InfoObj);

package CPAN::Exception::RecursiveDependency;
use strict;
use overload '""' => "as_string";

# a module sees its distribution (no version)
# a distribution sees its prereqs (which are module names) (usually with versions)
# a bundle sees its module names and/or its distributions (no version)

sub new {
    my($class) = shift;
    my($deps) = shift;
    my (@deps,%seen,$loop_starts_with);
  DCHAIN: for my $dep (@$deps) {
        push @deps, {name => $dep, display_as => $dep};
        if ($seen{$dep}++) {
            $loop_starts_with = $dep;
            last DCHAIN;
        }
    }
    my $in_loop = 0;
    for my $i (0..$#deps) {
        my $x = $deps[$i]{name};
        $in_loop ||= $x eq $loop_starts_with;
        my $xo = CPAN::Shell->expandany($x) or next;
        if ($xo->isa("CPAN::Module")) {
            my $have = $xo->inst_version || "N/A";
            my($want,$d,$want_type);
            if ($i>0 and $d = $deps[$i-1]{name}) {
                my $do = CPAN::Shell->expandany($d);
                $want = $do->{prereq_pm}{requires}{$x};
                if (defined $want) {
                    $want_type = "requires: ";
                } else {
                    $want = $do->{prereq_pm}{build_requires}{$x};
                    if (defined $want) {
                        $want_type = "build_requires: ";
                    } else {
                        $want_type = "unknown status";
                        $want = "???";
                    }
                }
            } else {
                $want = $xo->cpan_version;
                $want_type = "want: ";
            }
            $deps[$i]{have} = $have;
            $deps[$i]{want_type} = $want_type;
            $deps[$i]{want} = $want;
            $deps[$i]{display_as} = "$x (have: $have; $want_type$want)";
        } elsif ($xo->isa("CPAN::Distribution")) {
            $deps[$i]{display_as} = $xo->pretty_id;
            if ($in_loop) {
                $xo->{make} = CPAN::Distrostatus->new("NO cannot resolve circular dependency");
            } else {
                $xo->{make} = CPAN::Distrostatus->new("NO one dependency ($loop_starts_with) is a circular dependency");
            }
            $xo->store_persistent_state; # otherwise I will not reach
                                         # all involved parties for
                                         # the next session
        }
    }
    bless { deps => \@deps }, $class;
}

sub as_string {
    my($self) = shift;
    my $ret = "\nRecursive dependency detected:\n    ";
    $ret .= join("\n => ", map {$_->{display_as}} @{$self->{deps}});
    $ret .= ".\nCannot resolve.\n";
    $ret;
}

package CPAN::Exception::yaml_not_installed;
use strict;
use overload '""' => "as_string";

sub new {
    my($class,$module,$file,$during) = @_;
    bless { module => $module, file => $file, during => $during }, $class;
}

sub as_string {
    my($self) = shift;
    "'$self->{module}' not installed, cannot $self->{during} '$self->{file}'\n";
}

package CPAN::Exception::yaml_process_error;
use strict;
use overload '""' => "as_string";

sub new {
    my($class,$module,$file,$during,$error) = @_;
    bless { module => $module,
            file => $file,
            during => $during,
            error => $error }, $class;
}

sub as_string {
    my($self) = shift;
    if ($self->{during}) {
        if ($self->{file}) {
            if ($self->{module}) {
                if ($self->{error}) {
                    return "Alert: While trying to '$self->{during}' YAML file\n".
                        " '$self->{file}'\n".
                            "with '$self->{module}' the following error was encountered:\n".
                                "  $self->{error}\n";
                } else {
                    return "Alert: While trying to '$self->{during}' YAML file\n".
                        " '$self->{file}'\n".
                            "with '$self->{module}' some unknown error was encountered\n";
                }
            } else {
                return "Alert: While trying to '$self->{during}' YAML file\n".
                    " '$self->{file}'\n".
                        "some unknown error was encountered\n";
            }
        } else {
            return "Alert: While trying to '$self->{during}' some YAML file\n".
                    "some unknown error was encountered\n";
        }
    } else {
        return "Alert: unknown error encountered\n";
    }
}

package CPAN::Prompt; use overload '""' => "as_string";
use vars qw($prompt);
$prompt = "cpan> ";
$CPAN::CurrentCommandId ||= 0;
sub new {
    bless {}, shift;
}
sub as_string {
    my $word = "cpan";
    unless ($CPAN::META->{LOCK}) {
        $word = "nolock_cpan";
    }
    if ($CPAN::Config->{commandnumber_in_prompt}) {
        sprintf "$word\[%d]> ", $CPAN::CurrentCommandId;
    } else {
        "$word> ";
    }
}

package CPAN::URL; use overload '""' => "as_string", fallback => 1;
# accessors: TEXT(the url string), FROM(DEF=>defaultlist,USER=>urllist),
# planned are things like age or quality
sub new {
    my($class,%args) = @_;
    bless {
           %args
          }, $class;
}
sub as_string {
    my($self) = @_;
    $self->text;
}
sub text {
    my($self,$set) = @_;
    if (defined $set) {
        $self->{TEXT} = $set;
    }
    $self->{TEXT};
}

package CPAN::Distrostatus;
use overload '""' => "as_string",
    fallback => 1;
sub new {
    my($class,$arg) = @_;
    bless {
           TEXT => $arg,
           FAILED => substr($arg,0,2) eq "NO",
           COMMANDID => $CPAN::CurrentCommandId,
           TIME => time,
          }, $class;
}
sub commandid { shift->{COMMANDID} }
sub failed { shift->{FAILED} }
sub text {
    my($self,$set) = @_;
    if (defined $set) {
        $self->{TEXT} = $set;
    }
    $self->{TEXT};
}
sub as_string {
    my($self) = @_;
    $self->text;
}

package CPAN::Shell;
use strict;
use vars qw(
            $ADVANCED_QUERY
            $AUTOLOAD
            $COLOR_REGISTERED
            $Help
            $autoload_recursion
            $reload
            @ISA
           );
@CPAN::Shell::ISA = qw(CPAN::Debug);
$COLOR_REGISTERED ||= 0;
$Help = {
         '?' => \"help",
         '!' => "eval the rest of the line as perl",
         a => "whois author",
         autobundle => "wtite inventory into a bundle file",
         b => "info about bundle",
         bye => \"quit",
         clean => "clean up a distribution's build directory",
         # cvs_import
         d => "info about a distribution",
         # dump
         exit => \"quit",
         failed => "list all failed actions within current session",
         fforce => "redo a command from scratch",
         force => "redo a command",
         h => \"help",
         help => "overview over commands; 'help ...' explains specific commands",
         hosts => "statistics about recently used hosts",
         i => "info about authors/bundles/distributions/modules",
         install => "install a distribution",
         install_tested => "install all distributions tested OK",
         is_tested => "list all distributions tested OK",
         look => "open a subshell in a distribution's directory",
         ls => "list distributions according to a glob",
         m => "info about a module",
         make => "make/build a distribution",
         mkmyconfig => "write current config into a CPAN/MyConfig.pm file",
         notest => "run a (usually install) command but leave out the test phase",
         o => "'o conf ...' for config stuff; 'o debug ...' for debugging",
         perldoc => "try to get a manpage for a module",
         q => \"quit",
         quit => "leave the cpan shell",
         r => "review over upgradeable modules",
         readme => "display the README of a distro woth a pager",
         recent => "show recent uploads to the CPAN",
         # recompile
         reload => "'reload cpan' or 'reload index'",
         report => "test a distribution and send a test report to cpantesters",
         reports => "info about reported tests from cpantesters",
         # scripts
         # smoke
         test => "test a distribution",
         u => "display uninstalled modules",
         upgrade => "combine 'r' command with immediate installation",
        };
{
    $autoload_recursion   ||= 0;

    #-> sub CPAN::Shell::AUTOLOAD ;
    sub AUTOLOAD {
        $autoload_recursion++;
        my($l) = $AUTOLOAD;
        my $class = shift(@_);
        # warn "autoload[$l] class[$class]";
        $l =~ s/.*:://;
        if ($CPAN::Signal) {
            warn "Refusing to autoload '$l' while signal pending";
            $autoload_recursion--;
            return;
        }
        if ($autoload_recursion > 1) {
            my $fullcommand = join " ", map { "'$_'" } $l, @_;
            warn "Refusing to autoload $fullcommand in recursion\n";
            $autoload_recursion--;
            return;
        }
        if ($l =~ /^w/) {
            # XXX needs to be reconsidered
            if ($CPAN::META->has_inst('CPAN::WAIT')) {
                CPAN::WAIT->$l(@_);
            } else {
                $CPAN::Frontend->mywarn(qq{
Commands starting with "w" require CPAN::WAIT to be installed.
Please consider installing CPAN::WAIT to use the fulltext index.
For this you just need to type
    install CPAN::WAIT
});
            }
        } else {
            $CPAN::Frontend->mywarn(qq{Unknown shell command '$l'. }.
                                    qq{Type ? for help.
});
        }
        $autoload_recursion--;
    }
}

package CPAN;
use strict;

$META ||= CPAN->new; # In case we re-eval ourselves we need the ||

# from here on only subs.
################################################################################

sub _perl_fingerprint {
    my($self,$other_fingerprint) = @_;
    my $dll = eval {OS2::DLLname()};
    my $mtime_dll = 0;
    if (defined $dll) {
        $mtime_dll = (-f $dll ? (stat(_))[9] : '-1');
    }
    my $mtime_perl = (-f CPAN::find_perl ? (stat(_))[9] : '-1');
    my $this_fingerprint = {
                            '$^X' => CPAN::find_perl,
                            sitearchexp => $Config::Config{sitearchexp},
                            'mtime_$^X' => $mtime_perl,
                            'mtime_dll' => $mtime_dll,
                           };
    if ($other_fingerprint) {
        if (exists $other_fingerprint->{'stat($^X)'}) { # repair fp from rev. 1.88_57
            $other_fingerprint->{'mtime_$^X'} = $other_fingerprint->{'stat($^X)'}[9];
        }
        # mandatory keys since 1.88_57
        for my $key (qw($^X sitearchexp mtime_dll mtime_$^X)) {
            return unless $other_fingerprint->{$key} eq $this_fingerprint->{$key};
        }
        return 1;
    } else {
        return $this_fingerprint;
    }
}

sub suggest_myconfig () {
  SUGGEST_MYCONFIG: if(!$INC{'CPAN/MyConfig.pm'}) {
        $CPAN::Frontend->myprint("You don't seem to have a user ".
                                 "configuration (MyConfig.pm) yet.\n");
        my $new = CPAN::Shell::colorable_makemaker_prompt("Do you want to create a ".
                                              "user configuration now? (Y/n)",
                                              "yes");
        if($new =~ m{^y}i) {
            CPAN::Shell->mkmyconfig();
            return &checklock;
        } else {
            $CPAN::Frontend->mydie("OK, giving up.");
        }
    }
}

#-> sub CPAN::all_objects ;
sub all_objects {
    my($mgr,$class) = @_;
    CPAN::HandleConfig->load unless $CPAN::Config_loaded++;
    CPAN->debug("mgr[$mgr] class[$class]") if $CPAN::DEBUG;
    CPAN::Index->reload;
    values %{ $META->{readwrite}{$class} }; # unsafe meta access, ok
}

# Called by shell, not in batch mode. In batch mode I see no risk in
# having many processes updating something as installations are
# continually checked at runtime. In shell mode I suspect it is
# unintentional to open more than one shell at a time

#-> sub CPAN::checklock ;
sub checklock {
    my($self) = @_;
    my $lockfile = File::Spec->catfile($CPAN::Config->{cpan_home},".lock");
    if (-f $lockfile && -M _ > 0) {
        my $fh = FileHandle->new($lockfile) or
            $CPAN::Frontend->mydie("Could not open lockfile '$lockfile': $!");
        my $otherpid  = <$fh>;
        my $otherhost = <$fh>;
        $fh->close;
        if (defined $otherpid && $otherpid) {
            chomp $otherpid;
        }
        if (defined $otherhost && $otherhost) {
            chomp $otherhost;
        }
        my $thishost  = hostname();
        if (defined $otherhost && defined $thishost &&
            $otherhost ne '' && $thishost ne '' &&
            $otherhost ne $thishost) {
            $CPAN::Frontend->mydie(sprintf("CPAN.pm panic: Lockfile '$lockfile'\n".
                                           "reports other host $otherhost and other ".
                                           "process $otherpid.\n".
                                           "Cannot proceed.\n"));
        } elsif ($RUN_DEGRADED) {
            $CPAN::Frontend->mywarn("Running in degraded mode (experimental)\n");
        } elsif (defined $otherpid && $otherpid) {
            return if $$ == $otherpid; # should never happen
            $CPAN::Frontend->mywarn(
                                    qq{
There seems to be running another CPAN process (pid $otherpid).  Contacting...
});
            if (kill 0, $otherpid) {
                $CPAN::Frontend->mywarn(qq{Other job is running.\n});
                my($ans) =
                    CPAN::Shell::colorable_makemaker_prompt
                        (qq{Shall I try to run in degraded }.
                        qq{mode? (Y/n)},"y");
                if ($ans =~ /^y/i) {
                    $CPAN::Frontend->mywarn("Running in degraded mode (experimental).
Please report if something unexpected happens\n");
                    $RUN_DEGRADED = 1;
                    for ($CPAN::Config) {
                        # XXX
                        # $_->{build_dir_reuse} = 0; # 2006-11-17 akoenig Why was that?
                        $_->{commandnumber_in_prompt} = 0; # visibility
                        $_->{histfile} = "";               # who should win otherwise?
                        $_->{cache_metadata} = 0;          # better would be a lock?
                        $_->{use_sqlite} = 0;              # better would be a write lock!
                    }
                } else {
                    $CPAN::Frontend->mydie("
You may want to kill the other job and delete the lockfile. On UNIX try:
    kill $otherpid
    rm $lockfile
");
                }
            } elsif (-w $lockfile) {
                my($ans) =
                    CPAN::Shell::colorable_makemaker_prompt
                        (qq{Other job not responding. Shall I overwrite }.
                        qq{the lockfile '$lockfile'? (Y/n)},"y");
            $CPAN::Frontend->myexit("Ok, bye\n")
                unless $ans =~ /^y/i;
            } else {
                Carp::croak(
                    qq{Lockfile '$lockfile' not writeable by you. }.
                    qq{Cannot proceed.\n}.
                    qq{    On UNIX try:\n}.
                    qq{    rm '$lockfile'\n}.
                    qq{  and then rerun us.\n}
                );
            }
        } else {
            $CPAN::Frontend->mydie(sprintf("CPAN.pm panic: Found invalid lockfile ".
                                           "'$lockfile', please remove. Cannot proceed.\n"));
        }
    }
    my $dotcpan = $CPAN::Config->{cpan_home};
    eval { File::Path::mkpath($dotcpan);};
    if ($@) {
        # A special case at least for Jarkko.
        my $firsterror = $@;
        my $seconderror;
        my $symlinkcpan;
        if (-l $dotcpan) {
            $symlinkcpan = readlink $dotcpan;
            die "readlink $dotcpan failed: $!" unless defined $symlinkcpan;
            eval { File::Path::mkpath($symlinkcpan); };
            if ($@) {
                $seconderror = $@;
            } else {
                $CPAN::Frontend->mywarn(qq{
Working directory $symlinkcpan created.
});
            }
        }
        unless (-d $dotcpan) {
            my $mess = qq{
Your configuration suggests "$dotcpan" as your
CPAN.pm working directory. I could not create this directory due
to this error: $firsterror\n};
            $mess .= qq{
As "$dotcpan" is a symlink to "$symlinkcpan",
I tried to create that, but I failed with this error: $seconderror
} if $seconderror;
            $mess .= qq{
Please make sure the directory exists and is writable.
};
            $CPAN::Frontend->mywarn($mess);
            return suggest_myconfig;
        }
    } # $@ after eval mkpath $dotcpan
    if (0) { # to test what happens when a race condition occurs
        for (reverse 1..10) {
            print $_, "\n";
            sleep 1;
        }
    }
    # locking
    if (!$RUN_DEGRADED && !$self->{LOCKFH}) {
        my $fh;
        unless ($fh = FileHandle->new("+>>$lockfile")) {
            if ($! =~ /Permission/) {
                $CPAN::Frontend->mywarn(qq{

Your configuration suggests that CPAN.pm should use a working
directory of
    $CPAN::Config->{cpan_home}
Unfortunately we could not create the lock file
    $lockfile
due to permission problems.

Please make sure that the configuration variable
    \$CPAN::Config->{cpan_home}
points to a directory where you can write a .lock file. You can set
this variable in either a CPAN/MyConfig.pm or a CPAN/Config.pm in your
\@INC path;
});
                return suggest_myconfig;
            }
        }
        my $sleep = 1;
        while (!CPAN::_flock($fh, LOCK_EX|LOCK_NB)) {
            if ($sleep>10) {
                $CPAN::Frontend->mydie("Giving up\n");
            }
            $CPAN::Frontend->mysleep($sleep++);
            $CPAN::Frontend->mywarn("Could not lock lockfile with flock: $!; retrying\n");
        }

        seek $fh, 0, 0;
        truncate $fh, 0;
        $fh->autoflush(1);
        $fh->print($$, "\n");
        $fh->print(hostname(), "\n");
        $self->{LOCK} = $lockfile;
        $self->{LOCKFH} = $fh;
    }
    $SIG{TERM} = sub {
        my $sig = shift;
        &cleanup;
        $CPAN::Frontend->mydie("Got SIG$sig, leaving");
    };
    $SIG{INT} = sub {
      # no blocks!!!
        my $sig = shift;
        &cleanup if $Signal;
        die "Got yet another signal" if $Signal > 1;
        $CPAN::Frontend->mydie("Got another SIG$sig") if $Signal;
        $CPAN::Frontend->mywarn("Caught SIG$sig, trying to continue\n");
        $Signal++;
    };

#       From: Larry Wall <larry@wall.org>
#       Subject: Re: deprecating SIGDIE
#       To: perl5-porters@perl.org
#       Date: Thu, 30 Sep 1999 14:58:40 -0700 (PDT)
#
#       The original intent of __DIE__ was only to allow you to substitute one
#       kind of death for another on an application-wide basis without respect
#       to whether you were in an eval or not.  As a global backstop, it should
#       not be used any more lightly (or any more heavily :-) than class
#       UNIVERSAL.  Any attempt to build a general exception model on it should
#       be politely squashed.  Any bug that causes every eval {} to have to be
#       modified should be not so politely squashed.
#
#       Those are my current opinions.  It is also my optinion that polite
#       arguments degenerate to personal arguments far too frequently, and that
#       when they do, it's because both people wanted it to, or at least didn't
#       sufficiently want it not to.
#
#       Larry

    # global backstop to cleanup if we should really die
    $SIG{__DIE__} = \&cleanup;
    $self->debug("Signal handler set.") if $CPAN::DEBUG;
}

#-> sub CPAN::DESTROY ;
sub DESTROY {
    &cleanup; # need an eval?
}

#-> sub CPAN::anycwd ;
sub anycwd () {
    my $getcwd;
    $getcwd = $CPAN::Config->{'getcwd'} || 'cwd';
    CPAN->$getcwd();
}

#-> sub CPAN::cwd ;
sub cwd {Cwd::cwd();}

#-> sub CPAN::getcwd ;
sub getcwd {Cwd::getcwd();}

#-> sub CPAN::fastcwd ;
sub fastcwd {Cwd::fastcwd();}

#-> sub CPAN::backtickcwd ;
sub backtickcwd {my $cwd = `cwd`; chomp $cwd; $cwd}

#-> sub CPAN::find_perl ;
sub find_perl () {
    my($perl) = File::Spec->file_name_is_absolute($^X) ? $^X : "";
    my $pwd  = $CPAN::iCwd = CPAN::anycwd();
    my $candidate = File::Spec->catfile($pwd,$^X);
    $perl ||= $candidate if MM->maybe_command($candidate);

    unless ($perl) {
        my ($component,$perl_name);
      DIST_PERLNAME: foreach $perl_name ($^X, 'perl', 'perl5', "perl$]") {
          PATH_COMPONENT: foreach $component (File::Spec->path(),
                                                $Config::Config{'binexp'}) {
                next unless defined($component) && $component;
                my($abs) = File::Spec->catfile($component,$perl_name);
                if (MM->maybe_command($abs)) {
                    $perl = $abs;
                    last DIST_PERLNAME;
                }
            }
        }
    }

    return $perl;
}


#-> sub CPAN::exists ;
sub exists {
    my($mgr,$class,$id) = @_;
    CPAN::HandleConfig->load unless $CPAN::Config_loaded++;
    CPAN::Index->reload;
    ### Carp::croak "exists called without class argument" unless $class;
    $id ||= "";
    $id =~ s/:+/::/g if $class eq "CPAN::Module";
    my $exists;
    if (CPAN::_sqlite_running) {
        $exists = (exists $META->{readonly}{$class}{$id} or
                   $CPAN::SQLite->set($class, $id));
    } else {
        $exists =  exists $META->{readonly}{$class}{$id};
    }
    $exists ||= exists $META->{readwrite}{$class}{$id}; # unsafe meta access, ok
}

#-> sub CPAN::delete ;
sub delete {
  my($mgr,$class,$id) = @_;
  delete $META->{readonly}{$class}{$id}; # unsafe meta access, ok
  delete $META->{readwrite}{$class}{$id}; # unsafe meta access, ok
}

#-> sub CPAN::has_usable
# has_inst is sometimes too optimistic, we should replace it with this
# has_usable whenever a case is given
sub has_usable {
    my($self,$mod,$message) = @_;
    return 1 if $HAS_USABLE->{$mod};
    my $has_inst = $self->has_inst($mod,$message);
    return unless $has_inst;
    my $usable;
    $usable = {
               LWP => [ # we frequently had "Can't locate object
                        # method "new" via package "LWP::UserAgent" at
                        # (eval 69) line 2006
                       sub {require LWP},
                       sub {require LWP::UserAgent},
                       sub {require HTTP::Request},
                       sub {require URI::URL},
                      ],
               'Net::FTP' => [
                            sub {require Net::FTP},
                            sub {require Net::Config},
                           ],
               'File::HomeDir' => [
                                   sub {require File::HomeDir;
                                        unless (CPAN::Version->vge(File::HomeDir::->VERSION, 0.52)) {
                                            for ("Will not use File::HomeDir, need 0.52\n") {
                                                $CPAN::Frontend->mywarn($_);
                                                die $_;
                                            }
                                        }
                                    },
                                  ],
               'Archive::Tar' => [
                                  sub {require Archive::Tar;
                                       unless (CPAN::Version->vge(Archive::Tar::->VERSION, 1.00)) {
                                            for ("Will not use Archive::Tar, need 1.00\n") {
                                                $CPAN::Frontend->mywarn($_);
                                                die $_;
                                            }
                                       }
                                  },
                                 ],
               'File::Temp' => [
                                # XXX we should probably delete from
                                # %INC too so we can load after we
                                # installed a new enough version --
                                # I'm not sure.
                                sub {require File::Temp;
                                     unless (CPAN::Version->vge(File::Temp::->VERSION,0.16)) {
                                         for ("Will not use File::Temp, need 0.16\n") {
                                                $CPAN::Frontend->mywarn($_);
                                                die $_;
                                         }
                                     }
                                },
                               ]
              };
    if ($usable->{$mod}) {
        for my $c (0..$#{$usable->{$mod}}) {
            my $code = $usable->{$mod}[$c];
            my $ret = eval { &$code() };
            $ret = "" unless defined $ret;
            if ($@) {
                # warn "DEBUG: c[$c]\$\@[$@]ret[$ret]";
                return;
            }
        }
    }
    return $HAS_USABLE->{$mod} = 1;
}

#-> sub CPAN::has_inst
sub has_inst {
    my($self,$mod,$message) = @_;
    Carp::croak("CPAN->has_inst() called without an argument")
        unless defined $mod;
    my %dont = map { $_ => 1 } keys %{$CPAN::META->{dontload_hash}||{}},
        keys %{$CPAN::Config->{dontload_hash}||{}},
            @{$CPAN::Config->{dontload_list}||[]};
    if (defined $message && $message eq "no"  # afair only used by Nox
        ||
        $dont{$mod}
       ) {
      $CPAN::META->{dontload_hash}{$mod}||=1; # unsafe meta access, ok
      return 0;
    }
    my $file = $mod;
    my $obj;
    $file =~ s|::|/|g;
    $file .= ".pm";
    if ($INC{$file}) {
        # checking %INC is wrong, because $INC{LWP} may be true
        # although $INC{"URI/URL.pm"} may have failed. But as
        # I really want to say "bla loaded OK", I have to somehow
        # cache results.
        ### warn "$file in %INC"; #debug
        return 1;
    } elsif (eval { require $file }) {
        # eval is good: if we haven't yet read the database it's
        # perfect and if we have installed the module in the meantime,
        # it tries again. The second require is only a NOOP returning
        # 1 if we had success, otherwise it's retrying

        my $mtime = (stat $INC{$file})[9];
        # privileged files loaded by has_inst; Note: we use $mtime
        # as a proxy for a checksum.
        $CPAN::Shell::reload->{$file} = $mtime;
        my $v = eval "\$$mod\::VERSION";
        $v = $v ? " (v$v)" : "";
        CPAN::Shell->optprint("load_module","CPAN: $mod loaded ok$v\n");
        if ($mod eq "CPAN::WAIT") {
            push @CPAN::Shell::ISA, 'CPAN::WAIT';
        }
        return 1;
    } elsif ($mod eq "Net::FTP") {
        $CPAN::Frontend->mywarn(qq{
  Please, install Net::FTP as soon as possible. CPAN.pm installs it for you
  if you just type
      install Bundle::libnet

}) unless $Have_warned->{"Net::FTP"}++;
        $CPAN::Frontend->mysleep(3);
    } elsif ($mod eq "Digest::SHA") {
        if ($Have_warned->{"Digest::SHA"}++) {
            $CPAN::Frontend->mywarn(qq{CPAN: checksum security checks disabled }.
                                     qq{because Digest::SHA not installed.\n});
        } else {
            $CPAN::Frontend->mywarn(qq{
  CPAN: checksum security checks disabled because Digest::SHA not installed.
  Please consider installing the Digest::SHA module.

});
            $CPAN::Frontend->mysleep(2);
        }
    } elsif ($mod eq "Module::Signature") {
        # NOT prefs_lookup, we are not a distro
        my $check_sigs = $CPAN::Config->{check_sigs};
        if (not $check_sigs) {
            # they do not want us:-(
        } elsif (not $Have_warned->{"Module::Signature"}++) {
            # No point in complaining unless the user can
            # reasonably install and use it.
            if (eval { require Crypt::OpenPGP; 1 } ||
                (
                 defined $CPAN::Config->{'gpg'}
                 &&
                 $CPAN::Config->{'gpg'} =~ /\S/
                )
               ) {
                $CPAN::Frontend->mywarn(qq{
  CPAN: Module::Signature security checks disabled because Module::Signature
  not installed.  Please consider installing the Module::Signature module.
  You may also need to be able to connect over the Internet to the public
  keyservers like pgp.mit.edu (port 11371).

});
                $CPAN::Frontend->mysleep(2);
            }
        }
    } else {
        delete $INC{$file}; # if it inc'd LWP but failed during, say, URI
    }
    return 0;
}

#-> sub CPAN::instance ;
sub instance {
    my($mgr,$class,$id) = @_;
    CPAN::Index->reload;
    $id ||= "";
    # unsafe meta access, ok?
    return $META->{readwrite}{$class}{$id} if exists $META->{readwrite}{$class}{$id};
    $META->{readwrite}{$class}{$id} ||= $class->new(ID => $id);
}

#-> sub CPAN::new ;
sub new {
    bless {}, shift;
}

#-> sub CPAN::cleanup ;
sub cleanup {
  # warn "cleanup called with arg[@_] End[$CPAN::End] Signal[$Signal]";
  local $SIG{__DIE__} = '';
  my($message) = @_;
  my $i = 0;
  my $ineval = 0;
  my($subroutine);
  while ((undef,undef,undef,$subroutine) = caller(++$i)) {
      $ineval = 1, last if
        $subroutine eq '(eval)';
  }
  return if $ineval && !$CPAN::End;
  return unless defined $META->{LOCK};
  return unless -f $META->{LOCK};
  $META->savehist;
  close $META->{LOCKFH};
  unlink $META->{LOCK};
  # require Carp;
  # Carp::cluck("DEBUGGING");
  if ( $CPAN::CONFIG_DIRTY ) {
      $CPAN::Frontend->mywarn("Warning: Configuration not saved.\n");
  }
  $CPAN::Frontend->myprint("Lockfile removed.\n");
}

#-> sub CPAN::readhist
sub readhist {
    my($self,$term,$histfile) = @_;
    my($fh) = FileHandle->new;
    open $fh, "<$histfile" or last;
    local $/ = "\n";
    while (<$fh>) {
        chomp;
        $term->AddHistory($_);
    }
    close $fh;
}

#-> sub CPAN::savehist
sub savehist {
    my($self) = @_;
    my($histfile,$histsize);
    unless ($histfile = $CPAN::Config->{'histfile'}) {
        $CPAN::Frontend->mywarn("No history written (no histfile specified).\n");
        return;
    }
    $histsize = $CPAN::Config->{'histsize'} || 100;
    if ($CPAN::term) {
        unless ($CPAN::term->can("GetHistory")) {
            $CPAN::Frontend->mywarn("Terminal does not support GetHistory.\n");
            return;
        }
    } else {
        return;
    }
    my @h = $CPAN::term->GetHistory;
    splice @h, 0, @h-$histsize if @h>$histsize;
    my($fh) = FileHandle->new;
    open $fh, ">$histfile" or $CPAN::Frontend->mydie("Couldn't open >$histfile: $!");
    local $\ = local $, = "\n";
    print $fh @h;
    close $fh;
}

#-> sub CPAN::is_tested
sub is_tested {
    my($self,$what,$when) = @_;
    unless ($what) {
        Carp::cluck("DEBUG: empty what");
        return;
    }
    $self->{is_tested}{$what} = $when;
}

#-> sub CPAN::is_installed
# unsets the is_tested flag: as soon as the thing is installed, it is
# not needed in set_perl5lib anymore
sub is_installed {
    my($self,$what) = @_;
    delete $self->{is_tested}{$what};
}

sub _list_sorted_descending_is_tested {
    my($self) = @_;
    sort
        { ($self->{is_tested}{$b}||0) <=> ($self->{is_tested}{$a}||0) }
            keys %{$self->{is_tested}}
}

#-> sub CPAN::set_perl5lib
sub set_perl5lib {
    my($self,$for) = @_;
    unless ($for) {
        (undef,undef,undef,$for) = caller(1);
        $for =~ s/.*://;
    }
    $self->{is_tested} ||= {};
    return unless %{$self->{is_tested}};
    my $env = $ENV{PERL5LIB};
    $env = $ENV{PERLLIB} unless defined $env;
    my @env;
    push @env, $env if defined $env and length $env;
    #my @dirs = map {("$_/blib/arch", "$_/blib/lib")} keys %{$self->{is_tested}};
    #$CPAN::Frontend->myprint("Prepending @dirs to PERL5LIB.\n");

    my @dirs = map {("$_/blib/arch", "$_/blib/lib")} $self->_list_sorted_descending_is_tested;
    if (@dirs < 12) {
        $CPAN::Frontend->myprint("Prepending @dirs to PERL5LIB for '$for'\n");
    } elsif (@dirs < 24) {
        my @d = map {my $cp = $_;
                     $cp =~ s/^\Q$CPAN::Config->{build_dir}\E/%BUILDDIR%/;
                     $cp
                 } @dirs;
        $CPAN::Frontend->myprint("Prepending @d to PERL5LIB; ".
                                 "%BUILDDIR%=$CPAN::Config->{build_dir} ".
                                 "for '$for'\n"
                                );
    } else {
        my $cnt = keys %{$self->{is_tested}};
        $CPAN::Frontend->myprint("Prepending blib/arch and blib/lib of ".
                                 "$cnt build dirs to PERL5LIB; ".
                                 "for '$for'\n"
                                );
    }

    $ENV{PERL5LIB} = join $Config::Config{path_sep}, @dirs, @env;
}

package CPAN::CacheMgr;
use strict;

#-> sub CPAN::CacheMgr::as_string ;
sub as_string {
    eval { require Data::Dumper };
    if ($@) {
        return shift->SUPER::as_string;
    } else {
        return Data::Dumper::Dumper(shift);
    }
}

#-> sub CPAN::CacheMgr::cachesize ;
sub cachesize {
    shift->{DU};
}

#-> sub CPAN::CacheMgr::tidyup ;
sub tidyup {
  my($self) = @_;
  return unless $CPAN::META->{LOCK};
  return unless -d $self->{ID};
  my @toremove = grep { $self->{SIZE}{$_}==0 } @{$self->{FIFO}};
  for my $current (0..$#toremove) {
    my $toremove = $toremove[$current];
    $CPAN::Frontend->myprint(sprintf(
                                     "DEL(%d/%d): %s \n",
                                     $current+1,
                                     scalar @toremove,
                                     $toremove,
                                    )
                            );
    return if $CPAN::Signal;
    $self->_clean_cache($toremove);
    return if $CPAN::Signal;
  }
}

#-> sub CPAN::CacheMgr::dir ;
sub dir {
    shift->{ID};
}

#-> sub CPAN::CacheMgr::entries ;
sub entries {
    my($self,$dir) = @_;
    return unless defined $dir;
    $self->debug("reading dir[$dir]") if $CPAN::DEBUG;
    $dir ||= $self->{ID};
    my($cwd) = CPAN::anycwd();
    chdir $dir or Carp::croak("Can't chdir to $dir: $!");
    my $dh = DirHandle->new(File::Spec->curdir)
        or Carp::croak("Couldn't opendir $dir: $!");
    my(@entries);
    for ($dh->read) {
        next if $_ eq "." || $_ eq "..";
        if (-f $_) {
            push @entries, File::Spec->catfile($dir,$_);
        } elsif (-d _) {
            push @entries, File::Spec->catdir($dir,$_);
        } else {
            $CPAN::Frontend->mywarn("Warning: weird direntry in $dir: $_\n");
        }
    }
    chdir $cwd or Carp::croak("Can't chdir to $cwd: $!");
    sort { -M $a <=> -M $b} @entries;
}

#-> sub CPAN::CacheMgr::disk_usage ;
sub disk_usage {
    my($self,$dir,$fast) = @_;
    return if exists $self->{SIZE}{$dir};
    return if $CPAN::Signal;
    my($Du) = 0;
    if (-e $dir) {
        if (-d $dir) {
            unless (-x $dir) {
                unless (chmod 0755, $dir) {
                    $CPAN::Frontend->mywarn("I have neither the -x permission nor the ".
                                            "permission to change the permission; cannot ".
                                            "estimate disk usage of '$dir'\n");
                    $CPAN::Frontend->mysleep(5);
                    return;
                }
            }
        } elsif (-f $dir) {
            # nothing to say, no matter what the permissions
        }
    } else {
        $CPAN::Frontend->mywarn("File or directory '$dir' has gone, ignoring\n");
        return;
    }
    if ($fast) {
        $Du = 0; # placeholder
    } else {
        find(
             sub {
           $File::Find::prune++ if $CPAN::Signal;
           return if -l $_;
           if ($^O eq 'MacOS') {
             require Mac::Files;
             my $cat  = Mac::Files::FSpGetCatInfo($_);
             $Du += $cat->ioFlLgLen() + $cat->ioFlRLgLen() if $cat;
           } else {
             if (-d _) {
               unless (-x _) {
                 unless (chmod 0755, $_) {
                   $CPAN::Frontend->mywarn("I have neither the -x permission nor ".
                                           "the permission to change the permission; ".
                                           "can only partially estimate disk usage ".
                                           "of '$_'\n");
                   $CPAN::Frontend->mysleep(5);
                   return;
                 }
               }
             } else {
               $Du += (-s _);
             }
           }
         },
         $dir
            );
    }
    return if $CPAN::Signal;
    $self->{SIZE}{$dir} = $Du/1024/1024;
    unshift @{$self->{FIFO}}, $dir;
    $self->debug("measured $dir is $Du") if $CPAN::DEBUG;
    $self->{DU} += $Du/1024/1024;
    $self->{DU};
}

#-> sub CPAN::CacheMgr::_clean_cache ;
sub _clean_cache {
    my($self,$dir) = @_;
    return unless -e $dir;
    unless (File::Spec->canonpath(File::Basename::dirname($dir))
            eq File::Spec->canonpath($CPAN::Config->{build_dir})) {
        $CPAN::Frontend->mywarn("Directory '$dir' not below $CPAN::Config->{build_dir}, ".
                                "will not remove\n");
        $CPAN::Frontend->mysleep(5);
        return;
    }
    $self->debug("have to rmtree $dir, will free $self->{SIZE}{$dir}")
        if $CPAN::DEBUG;
    File::Path::rmtree($dir);
    my $id_deleted = 0;
    if ($dir !~ /\.yml$/ && -f "$dir.yml") {
        my $yaml_module = CPAN::_yaml_module;
        if ($CPAN::META->has_inst($yaml_module)) {
            my($peek_yaml) = eval { CPAN->_yaml_loadfile("$dir.yml"); };
            if ($@) {
                $CPAN::Frontend->mywarn("(parse error on '$dir.yml' removing anyway)");
                unlink "$dir.yml" or
                    $CPAN::Frontend->mywarn("(Could not unlink '$dir.yml': $!)");
                return;
            } elsif (my $id = $peek_yaml->[0]{distribution}{ID}) {
                $CPAN::META->delete("CPAN::Distribution", $id);

                # XXX we should restore the state NOW, otherise this
                # distro does not exist until we read an index. BUG ALERT(?)

                # $CPAN::Frontend->mywarn (" +++\n");
                $id_deleted++;
            }
        }
        unlink "$dir.yml"; # may fail
        unless ($id_deleted) {
            CPAN->debug("no distro found associated with '$dir'");
        }
    }
    $self->{DU} -= $self->{SIZE}{$dir};
    delete $self->{SIZE}{$dir};
}

#-> sub CPAN::CacheMgr::new ;
sub new {
    my $class = shift;
    my $time = time;
    my($debug,$t2);
    $debug = "";
    my $self = {
        ID => $CPAN::Config->{build_dir},
        MAX => $CPAN::Config->{'build_cache'},
        SCAN => $CPAN::Config->{'scan_cache'} || 'atstart',
        DU => 0
    };
    File::Path::mkpath($self->{ID});
    my $dh = DirHandle->new($self->{ID});
    bless $self, $class;
    $self->scan_cache;
    $t2 = time;
    $debug .= "timing of CacheMgr->new: ".($t2 - $time);
    $time = $t2;
    CPAN->debug($debug) if $CPAN::DEBUG;
    $self;
}

#-> sub CPAN::CacheMgr::scan_cache ;
sub scan_cache {
    my $self = shift;
    return if $self->{SCAN} eq 'never';
    $CPAN::Frontend->mydie("Unknown scan_cache argument: $self->{SCAN}")
        unless $self->{SCAN} eq 'atstart';
    return unless $CPAN::META->{LOCK};
    $CPAN::Frontend->myprint(
                             sprintf("Scanning cache %s for sizes\n",
                             $self->{ID}));
    my $e;
    my @entries = $self->entries($self->{ID});
    my $i = 0;
    my $painted = 0;
    for $e (@entries) {
        my $symbol = ".";
        if ($self->{DU} > $self->{MAX}) {
            $symbol = "-";
            $self->disk_usage($e,1);
        } else {
            $self->disk_usage($e);
        }
        $i++;
        while (($painted/76) < ($i/@entries)) {
            $CPAN::Frontend->myprint($symbol);
            $painted++;
        }
        return if $CPAN::Signal;
    }
    $CPAN::Frontend->myprint("DONE\n");
    $self->tidyup;
}

package CPAN::Shell;
use strict;

#-> sub CPAN::Shell::h ;
sub h {
    my($class,$about) = @_;
    if (defined $about) {
        my $help;
        if (exists $Help->{$about}) {
            if (ref $Help->{$about}) { # aliases
                $about = ${$Help->{$about}};
            }
            $help = $Help->{$about};
        } else {
            $help = "No help available";
        }
        $CPAN::Frontend->myprint("$about\: $help\n");
    } else {
        my $filler = " " x (80 - 28 - length($CPAN::VERSION));
        $CPAN::Frontend->myprint(qq{
Display Information $filler (ver $CPAN::VERSION)
 command  argument          description
 a,b,d,m  WORD or /REGEXP/  about authors, bundles, distributions, modules
 i        WORD or /REGEXP/  about any of the above
 ls       AUTHOR or GLOB    about files in the author's directory
    (with WORD being a module, bundle or author name or a distribution
    name of the form AUTHOR/DISTRIBUTION)

Download, Test, Make, Install...
 get      download                     clean    make clean
 make     make (implies get)           look     open subshell in dist directory
 test     make test (implies make)     readme   display these README files
 install  make install (implies test)  perldoc  display POD documentation

Upgrade
 r        WORDs or /REGEXP/ or NONE    report updates for some/matching/all modules
 upgrade  WORDs or /REGEXP/ or NONE    upgrade some/matching/all modules

Pragmas
 force  CMD    try hard to do command  fforce CMD    try harder
 notest CMD    skip testing

Other
 h,?           display this menu       ! perl-code   eval a perl command
 o conf [opt]  set and query options   q             quit the cpan shell
 reload cpan   load CPAN.pm again      reload index  load newer indices
 autobundle    Snapshot                recent        latest CPAN uploads});
}
}

*help = \&h;

#-> sub CPAN::Shell::a ;
sub a {
  my($self,@arg) = @_;
  # authors are always UPPERCASE
  for (@arg) {
    $_ = uc $_ unless /=/;
  }
  $CPAN::Frontend->myprint($self->format_result('Author',@arg));
}

#-> sub CPAN::Shell::globls ;
sub globls {
    my($self,$s,$pragmas) = @_;
    # ls is really very different, but we had it once as an ordinary
    # command in the Shell (upto rev. 321) and we could not handle
    # force well then
    my(@accept,@preexpand);
    if ($s =~ /[\*\?\/]/) {
        if ($CPAN::META->has_inst("Text::Glob")) {
            if (my($au,$pathglob) = $s =~ m|(.*?)/(.*)|) {
                my $rau = Text::Glob::glob_to_regex(uc $au);
                CPAN::Shell->debug("au[$au]pathglob[$pathglob]rau[$rau]")
                      if $CPAN::DEBUG;
                push @preexpand, map { $_->id . "/" . $pathglob }
                    CPAN::Shell->expand_by_method('CPAN::Author',['id'],"/$rau/");
            } else {
                my $rau = Text::Glob::glob_to_regex(uc $s);
                push @preexpand, map { $_->id }
                    CPAN::Shell->expand_by_method('CPAN::Author',
                                                  ['id'],
                                                  "/$rau/");
            }
        } else {
            $CPAN::Frontend->mydie("Text::Glob not installed, cannot proceed");
        }
    } else {
        push @preexpand, uc $s;
    }
    for (@preexpand) {
        unless (/^[A-Z0-9\-]+(\/|$)/i) {
            $CPAN::Frontend->mywarn("ls command rejects argument $_: not an author\n");
            next;
        }
        push @accept, $_;
    }
    my $silent = @accept>1;
    my $last_alpha = "";
    my @results;
    for my $a (@accept) {
        my($author,$pathglob);
        if ($a =~ m|(.*?)/(.*)|) {
            my $a2 = $1;
            $pathglob = $2;
            $author = CPAN::Shell->expand_by_method('CPAN::Author',
                                                    ['id'],
                                                    $a2)
                or $CPAN::Frontend->mydie("No author found for $a2\n");
        } else {
            $author = CPAN::Shell->expand_by_method('CPAN::Author',
                                                    ['id'],
                                                    $a)
                or $CPAN::Frontend->mydie("No author found for $a\n");
        }
        if ($silent) {
            my $alpha = substr $author->id, 0, 1;
            my $ad;
            if ($alpha eq $last_alpha) {
                $ad = "";
            } else {
                $ad = "[$alpha]";
                $last_alpha = $alpha;
            }
            $CPAN::Frontend->myprint($ad);
        }
        for my $pragma (@$pragmas) {
            if ($author->can($pragma)) {
                $author->$pragma();
            }
        }
        push @results, $author->ls($pathglob,$silent); # silent if
                                                       # more than one
                                                       # author
        for my $pragma (@$pragmas) {
            my $unpragma = "un$pragma";
            if ($author->can($unpragma)) {
                $author->$unpragma();
            }
        }
    }
    @results;
}

#-> sub CPAN::Shell::local_bundles ;
sub local_bundles {
    my($self,@which) = @_;
    my($incdir,$bdir,$dh);
    foreach $incdir ($CPAN::Config->{'cpan_home'},@INC) {
        my @bbase = "Bundle";
        while (my $bbase = shift @bbase) {
            $bdir = File::Spec->catdir($incdir,split /::/, $bbase);
            CPAN->debug("bdir[$bdir]\@bbase[@bbase]") if $CPAN::DEBUG;
            if ($dh = DirHandle->new($bdir)) { # may fail
                my($entry);
                for $entry ($dh->read) {
                    next if $entry =~ /^\./;
                    next unless $entry =~ /^\w+(\.pm)?(?!\n)\Z/;
                    if (-d File::Spec->catdir($bdir,$entry)) {
                        push @bbase, "$bbase\::$entry";
                    } else {
                        next unless $entry =~ s/\.pm(?!\n)\Z//;
                        $CPAN::META->instance('CPAN::Bundle',"$bbase\::$entry");
                    }
                }
            }
        }
    }
}

#-> sub CPAN::Shell::b ;
sub b {
    my($self,@which) = @_;
    CPAN->debug("which[@which]") if $CPAN::DEBUG;
    $self->local_bundles;
    $CPAN::Frontend->myprint($self->format_result('Bundle',@which));
}

#-> sub CPAN::Shell::d ;
sub d { $CPAN::Frontend->myprint(shift->format_result('Distribution',@_));}

#-> sub CPAN::Shell::m ;
sub m { # emacs confused here }; sub mimimimimi { # emacs in sync here
    my $self = shift;
    $CPAN::Frontend->myprint($self->format_result('Module',@_));
}

#-> sub CPAN::Shell::i ;
sub i {
    my($self) = shift;
    my(@args) = @_;
    @args = '/./' unless @args;
    my(@result);
    for my $type (qw/Bundle Distribution Module/) {
        push @result, $self->expand($type,@args);
    }
    # Authors are always uppercase.
    push @result, $self->expand("Author", map { uc $_ } @args);

    my $result = @result == 1 ?
        $result[0]->as_string :
            @result == 0 ?
                "No objects found of any type for argument @args\n" :
                    join("",
                         (map {$_->as_glimpse} @result),
                         scalar @result, " items found\n",
                        );
    $CPAN::Frontend->myprint($result);
}

#-> sub CPAN::Shell::o ;

# CPAN::Shell::o and CPAN::HandleConfig::edit are closely related. 'o
# conf' calls through to CPAN::HandleConfig::edit. 'o conf' should
# probably have been called 'set' and 'o debug' maybe 'set debug' or
# 'debug'; 'o conf ARGS' calls ->edit in CPAN/HandleConfig.pm
sub o {
    my($self,$o_type,@o_what) = @_;
    $o_type ||= "";
    CPAN->debug("o_type[$o_type] o_what[".join(" | ",@o_what)."]\n");
    if ($o_type eq 'conf') {
        my($cfilter);
        ($cfilter) = $o_what[0] =~ m|^/(.*)/$| if @o_what;
        if (!@o_what or $cfilter) { # print all things, "o conf"
            $cfilter ||= "";
            my $qrfilter = eval 'qr/$cfilter/';
            my($k,$v);
            $CPAN::Frontend->myprint("\$CPAN::Config options from ");
            my @from;
            if (exists $INC{'CPAN/Config.pm'}) {
                push @from, $INC{'CPAN/Config.pm'};
            }
            if (exists $INC{'CPAN/MyConfig.pm'}) {
                push @from, $INC{'CPAN/MyConfig.pm'};
            }
            $CPAN::Frontend->myprint(join " and ", map {"'$_'"} @from);
            $CPAN::Frontend->myprint(":\n");
            for $k (sort keys %CPAN::HandleConfig::can) {
                next unless $k =~ /$qrfilter/;
                $v = $CPAN::HandleConfig::can{$k};
                $CPAN::Frontend->myprint(sprintf "    %-18s [%s]\n", $k, $v);
            }
            $CPAN::Frontend->myprint("\n");
            for $k (sort keys %CPAN::HandleConfig::keys) {
                next unless $k =~ /$qrfilter/;
                CPAN::HandleConfig->prettyprint($k);
            }
            $CPAN::Frontend->myprint("\n");
        } else {
            if (CPAN::HandleConfig->edit(@o_what)) {
            } else {
                $CPAN::Frontend->myprint(qq{Type 'o conf' to view all configuration }.
                                         qq{items\n\n});
            }
        }
    } elsif ($o_type eq 'debug') {
        my(%valid);
        @o_what = () if defined $o_what[0] && $o_what[0] =~ /help/i;
        if (@o_what) {
            while (@o_what) {
                my($what) = shift @o_what;
                if ($what =~ s/^-// && exists $CPAN::DEBUG{$what}) {
                    $CPAN::DEBUG &= $CPAN::DEBUG ^ $CPAN::DEBUG{$what};
                    next;
                }
                if ( exists $CPAN::DEBUG{$what} ) {
                    $CPAN::DEBUG |= $CPAN::DEBUG{$what};
                } elsif ($what =~ /^\d/) {
                    $CPAN::DEBUG = $what;
                } elsif (lc $what eq 'all') {
                    my($max) = 0;
                    for (values %CPAN::DEBUG) {
                        $max += $_;
                    }
                    $CPAN::DEBUG = $max;
                } else {
                    my($known) = 0;
                    for (keys %CPAN::DEBUG) {
                        next unless lc($_) eq lc($what);
                        $CPAN::DEBUG |= $CPAN::DEBUG{$_};
                        $known = 1;
                    }
                    $CPAN::Frontend->myprint("unknown argument [$what]\n")
                        unless $known;
                }
            }
        } else {
            my $raw = "Valid options for debug are ".
                join(", ",sort(keys %CPAN::DEBUG), 'all').
                     qq{ or a number. Completion works on the options. }.
                     qq{Case is ignored.};
            require Text::Wrap;
            $CPAN::Frontend->myprint(Text::Wrap::fill("","",$raw));
            $CPAN::Frontend->myprint("\n\n");
        }
        if ($CPAN::DEBUG) {
            $CPAN::Frontend->myprint("Options set for debugging ($CPAN::DEBUG):\n");
            my($k,$v);
            for $k (sort {$CPAN::DEBUG{$a} <=> $CPAN::DEBUG{$b}} keys %CPAN::DEBUG) {
                $v = $CPAN::DEBUG{$k};
                $CPAN::Frontend->myprint(sprintf "    %-14s(%s)\n", $k, $v)
                    if $v & $CPAN::DEBUG;
            }
        } else {
            $CPAN::Frontend->myprint("Debugging turned off completely.\n");
        }
    } else {
        $CPAN::Frontend->myprint(qq{
Known options:
  conf    set or get configuration variables
  debug   set or get debugging options
});
    }
}

# CPAN::Shell::paintdots_onreload
sub paintdots_onreload {
    my($ref) = shift;
    sub {
        if ( $_[0] =~ /[Ss]ubroutine ([\w:]+) redefined/ ) {
            my($subr) = $1;
            ++$$ref;
            local($|) = 1;
            # $CPAN::Frontend->myprint(".($subr)");
            $CPAN::Frontend->myprint(".");
            if ($subr =~ /\bshell\b/i) {
                # warn "debug[$_[0]]";

                # It would be nice if we could detect that a
                # subroutine has actually changed, but for now we
                # practically always set the GOTOSHELL global

                $CPAN::GOTOSHELL=1;
            }
            return;
        }
        warn @_;
    };
}

#-> sub CPAN::Shell::hosts ;
sub hosts {
    my($self) = @_;
    my $fullstats = CPAN::FTP->_ftp_statistics();
    my $history = $fullstats->{history} || [];
    my %S; # statistics
    while (my $last = pop @$history) {
        my $attempts = $last->{attempts} or next;
        my $start;
        if (@$attempts) {
            $start = $attempts->[-1]{start};
            if ($#$attempts > 0) {
                for my $i (0..$#$attempts-1) {
                    my $url = $attempts->[$i]{url} or next;
                    $S{no}{$url}++;
                }
            }
        } else {
            $start = $last->{start};
        }
        next unless $last->{thesiteurl}; # C-C? bad filenames?
        $S{start} = $start;
        $S{end} ||= $last->{end};
        my $dltime = $last->{end} - $start;
        my $dlsize = $last->{filesize} || 0;
        my $url = ref $last->{thesiteurl} ? $last->{thesiteurl}->text : $last->{thesiteurl};
        my $s = $S{ok}{$url} ||= {};
        $s->{n}++;
        $s->{dlsize} ||= 0;
        $s->{dlsize} += $dlsize/1024;
        $s->{dltime} ||= 0;
        $s->{dltime} += $dltime;
    }
    my $res;
    for my $url (keys %{$S{ok}}) {
        next if $S{ok}{$url}{dltime} == 0; # div by zero
        push @{$res->{ok}}, [@{$S{ok}{$url}}{qw(n dlsize dltime)},
                             $S{ok}{$url}{dlsize}/$S{ok}{$url}{dltime},
                             $url,
                            ];
    }
    for my $url (keys %{$S{no}}) {
        push @{$res->{no}}, [$S{no}{$url},
                             $url,
                            ];
    }
    my $R = ""; # report
    if ($S{start} && $S{end}) {
        $R .= sprintf "Log starts: %s\n", $S{start} ? scalar(localtime $S{start}) : "unknown";
        $R .= sprintf "Log ends  : %s\n", $S{end}   ? scalar(localtime $S{end})   : "unknown";
    }
    if ($res->{ok} && @{$res->{ok}}) {
        $R .= sprintf "\nSuccessful downloads:
   N       kB  secs      kB/s url\n";
        my $i = 20;
        for (sort { $b->[3] <=> $a->[3] } @{$res->{ok}}) {
            $R .= sprintf "%4d %8d %5d %9.1f %s\n", @$_;
            last if --$i<=0;
        }
    }
    if ($res->{no} && @{$res->{no}}) {
        $R .= sprintf "\nUnsuccessful downloads:\n";
        my $i = 20;
        for (sort { $b->[0] <=> $a->[0] } @{$res->{no}}) {
            $R .= sprintf "%4d %s\n", @$_;
            last if --$i<=0;
        }
    }
    $CPAN::Frontend->myprint($R);
}

#-> sub CPAN::Shell::reload ;
sub reload {
    my($self,$command,@arg) = @_;
    $command ||= "";
    $self->debug("self[$self]command[$command]arg[@arg]") if $CPAN::DEBUG;
    if ($command =~ /^cpan$/i) {
        my $redef = 0;
        chdir $CPAN::iCwd if $CPAN::iCwd; # may fail
        my $failed;
        my @relo = (
                    "CPAN.pm",
                    "CPAN/Debug.pm",
                    "CPAN/FirstTime.pm",
                    "CPAN/HandleConfig.pm",
                    "CPAN/Kwalify.pm",
                    "CPAN/Queue.pm",
                    "CPAN/Reporter/Config.pm",
                    "CPAN/Reporter/History.pm",
                    "CPAN/Reporter.pm",
                    "CPAN/SQLite.pm",
                    "CPAN/Tarzip.pm",
                    "CPAN/Version.pm",
                   );
      MFILE: for my $f (@relo) {
            next unless exists $INC{$f};
            my $p = $f;
            $p =~ s/\.pm$//;
            $p =~ s|/|::|g;
            $CPAN::Frontend->myprint("($p");
            local($SIG{__WARN__}) = paintdots_onreload(\$redef);
            $self->_reload_this($f) or $failed++;
            my $v = eval "$p\::->VERSION";
            $CPAN::Frontend->myprint("v$v)");
        }
        $CPAN::Frontend->myprint("\n$redef subroutines redefined\n");
        if ($failed) {
            my $errors = $failed == 1 ? "error" : "errors";
            $CPAN::Frontend->mywarn("\n$failed $errors during reload. You better quit ".
                                    "this session.\n");
        }
    } elsif ($command =~ /^index$/i) {
      CPAN::Index->force_reload;
    } else {
      $CPAN::Frontend->myprint(qq{cpan     re-evals the CPAN modules
index    re-reads the index files\n});
    }
}

# reload means only load again what we have loaded before
#-> sub CPAN::Shell::_reload_this ;
sub _reload_this {
    my($self,$f,$args) = @_;
    CPAN->debug("f[$f]") if $CPAN::DEBUG;
    return 1 unless $INC{$f}; # we never loaded this, so we do not
                              # reload but say OK
    my $pwd = CPAN::anycwd();
    CPAN->debug("pwd[$pwd]") if $CPAN::DEBUG;
    my($file);
    for my $inc (@INC) {
        $file = File::Spec->catfile($inc,split /\//, $f);
        last if -f $file;
        $file = "";
    }
    CPAN->debug("file[$file]") if $CPAN::DEBUG;
    my @inc = @INC;
    unless ($file && -f $file) {
        # this thingie is not in the INC path, maybe CPAN/MyConfig.pm?
        $file = $INC{$f};
        unless (CPAN->has_inst("File::Basename")) {
            @inc = File::Basename::dirname($file);
        } else {
            # do we ever need this?
            @inc = substr($file,0,-length($f)-1); # bring in back to me!
        }
    }
    CPAN->debug("file[$file]inc[@inc]") if $CPAN::DEBUG;
    unless (-f $file) {
        $CPAN::Frontend->mywarn("Found no file to reload for '$f'\n");
        return;
    }
    my $mtime = (stat $file)[9];
    if ($reload->{$f}) {
    } elsif ($^T < $mtime) {
        # since we started the file has changed, force it to be reloaded
        $reload->{$f} = -1;
    } else {
        $reload->{$f} = $mtime;
    }
    my $must_reload = $mtime != $reload->{$f};
    $args ||= {};
    $must_reload ||= $args->{reloforce}; # o conf defaults needs this
    if ($must_reload) {
        my $fh = FileHandle->new($file) or
            $CPAN::Frontend->mydie("Could not open $file: $!");
        local($/);
        local $^W = 1;
        my $content = <$fh>;
        CPAN->debug(sprintf("reload file[%s] content[%s...]",$file,substr($content,0,128)))
            if $CPAN::DEBUG;
        delete $INC{$f};
        local @INC = @inc;
        eval "require '$f'";
        if ($@) {
            warn $@;
            return;
        }
        $reload->{$f} = $mtime;
    } else {
        $CPAN::Frontend->myprint("__unchanged__");
    }
    return 1;
}

#-> sub CPAN::Shell::mkmyconfig ;
sub mkmyconfig {
    my($self, $cpanpm, %args) = @_;
    require CPAN::FirstTime;
    my $home = CPAN::HandleConfig::home;
    $cpanpm = $INC{'CPAN/MyConfig.pm'} ||
        File::Spec->catfile(split /\//, "$home/.cpan/CPAN/MyConfig.pm");
    File::Path::mkpath(File::Basename::dirname($cpanpm)) unless -e $cpanpm;
    CPAN::HandleConfig::require_myconfig_or_config;
    $CPAN::Config ||= {};
    $CPAN::Config = {
        %$CPAN::Config,
        build_dir           =>  undef,
        cpan_home           =>  undef,
        keep_source_where   =>  undef,
        histfile            =>  undef,
    };
    CPAN::FirstTime::init($cpanpm, %args);
}

#-> sub CPAN::Shell::_binary_extensions ;
sub _binary_extensions {
    my($self) = shift @_;
    my(@result,$module,%seen,%need,$headerdone);
    for $module ($self->expand('Module','/./')) {
        my $file  = $module->cpan_file;
        next if $file eq "N/A";
        next if $file =~ /^Contact Author/;
        my $dist = $CPAN::META->instance('CPAN::Distribution',$file);
        next if $dist->isa_perl;
        next unless $module->xs_file;
        local($|) = 1;
        $CPAN::Frontend->myprint(".");
        push @result, $module;
    }
#    print join " | ", @result;
    $CPAN::Frontend->myprint("\n");
    return @result;
}

#-> sub CPAN::Shell::recompile ;
sub recompile {
    my($self) = shift @_;
    my($module,@module,$cpan_file,%dist);
    @module = $self->_binary_extensions();
    for $module (@module) { # we force now and compile later, so we
                            # don't do it twice
        $cpan_file = $module->cpan_file;
        my $pack = $CPAN::META->instance('CPAN::Distribution',$cpan_file);
        $pack->force;
        $dist{$cpan_file}++;
    }
    for $cpan_file (sort keys %dist) {
        $CPAN::Frontend->myprint("  CPAN: Recompiling $cpan_file\n\n");
        my $pack = $CPAN::META->instance('CPAN::Distribution',$cpan_file);
        $pack->install;
        $CPAN::Signal = 0; # it's tempting to reset Signal, so we can
                           # stop a package from recompiling,
                           # e.g. IO-1.12 when we have perl5.003_10
    }
}

#-> sub CPAN::Shell::scripts ;
sub scripts {
    my($self, $arg) = @_;
    $CPAN::Frontend->mywarn(">>>> experimental command, currently unsupported <<<<\n\n");

    for my $req (qw( HTML::LinkExtor Sort::Versions List::Util )) {
        unless ($CPAN::META->has_inst($req)) {
            $CPAN::Frontend->mywarn("  $req not available\n");
        }
    }
    my $p = HTML::LinkExtor->new();
    my $indexfile = "/home/ftp/pub/PAUSE/scripts/new/index.html";
    unless (-f $indexfile) {
        $CPAN::Frontend->mydie("found no indexfile[$indexfile]\n");
    }
    $p->parse_file($indexfile);
    my @hrefs;
    my $qrarg;
    if ($arg =~ s|^/(.+)/$|$1|) {
        $qrarg = eval 'qr/$arg/'; # hide construct from 5.004
    }
    for my $l ($p->links) {
        my $tag = shift @$l;
        next unless $tag eq "a";
        my %att = @$l;
        my $href = $att{href};
        next unless $href =~ s|^\.\./authors/id/./../||;
        if ($arg) {
            if ($qrarg) {
                if ($href =~ $qrarg) {
                    push @hrefs, $href;
                }
            } else {
                if ($href =~ /\Q$arg\E/) {
                    push @hrefs, $href;
                }
            }
        } else {
            push @hrefs, $href;
        }
    }
    # now filter for the latest version if there is more than one of a name
    my %stems;
    for (sort @hrefs) {
        my $href = $_;
        s/-v?\d.*//;
        my $stem = $_;
        $stems{$stem} ||= [];
        push @{$stems{$stem}}, $href;
    }
    for (sort keys %stems) {
        my $highest;
        if (@{$stems{$_}} > 1) {
            $highest = List::Util::reduce {
                Sort::Versions::versioncmp($a,$b) > 0 ? $a : $b
              } @{$stems{$_}};
        } else {
            $highest = $stems{$_}[0];
        }
        $CPAN::Frontend->myprint("$highest\n");
    }
}

#-> sub CPAN::Shell::report ;
sub report {
    my($self,@args) = @_;
    unless ($CPAN::META->has_inst("CPAN::Reporter")) {
        $CPAN::Frontend->mydie("CPAN::Reporter not installed; cannot continue");
    }
    local $CPAN::Config->{test_report} = 1;
    $self->force("test",@args); # force is there so that the test be
                                # re-run (as documented)
}

# compare with is_tested
#-> sub CPAN::Shell::install_tested
sub install_tested {
    my($self,@some) = @_;
    $CPAN::Frontend->mywarn("install_tested() must not be called with arguments.\n"),
        return if @some;
    CPAN::Index->reload;

    for my $b (reverse $CPAN::META->_list_sorted_descending_is_tested) {
        my $yaml = "$b.yml";
        unless (-f $yaml) {
            $CPAN::Frontend->mywarn("No YAML file for $b available, skipping\n");
            next;
        }
        my $yaml_content = CPAN->_yaml_loadfile($yaml);
        my $id = $yaml_content->[0]{distribution}{ID};
        unless ($id) {
            $CPAN::Frontend->mywarn("No ID found in '$yaml', skipping\n");
            next;
        }
        my $do = CPAN::Shell->expandany($id);
        unless ($do) {
            $CPAN::Frontend->mywarn("Could not expand ID '$id', skipping\n");
            next;
        }
        unless ($do->{build_dir}) {
            $CPAN::Frontend->mywarn("Distro '$id' has no build_dir, skipping\n");
            next;
        }
        unless ($do->{build_dir} eq $b) {
            $CPAN::Frontend->mywarn("Distro '$id' has build_dir '$do->{build_dir}' but expected '$b', skipping\n");
            next;
        }
        push @some, $do;
    }

    $CPAN::Frontend->mywarn("No tested distributions found.\n"),
        return unless @some;

    @some = grep { $_->{make_test} && ! $_->{make_test}->failed } @some;
    $CPAN::Frontend->mywarn("No distributions tested with this build of perl found.\n"),
        return unless @some;

    # @some = grep { not $_->uptodate } @some;
    # $CPAN::Frontend->mywarn("No non-uptodate distributions tested with this build of perl found.\n"),
    #     return unless @some;

    CPAN->debug("some[@some]");
    for my $d (@some) {
        my $id = $d->can("pretty_id") ? $d->pretty_id : $d->id;
        $CPAN::Frontend->myprint("install_tested: Running for $id\n");
        $CPAN::Frontend->mysleep(1);
        $self->install($d);
    }
}

#-> sub CPAN::Shell::upgrade ;
sub upgrade {
    my($self,@args) = @_;
    $self->install($self->r(@args));
}

#-> sub CPAN::Shell::_u_r_common ;
sub _u_r_common {
    my($self) = shift @_;
    my($what) = shift @_;
    CPAN->debug("self[$self] what[$what] args[@_]") if $CPAN::DEBUG;
    Carp::croak "Usage: \$obj->_u_r_common(a|r|u)" unless
          $what && $what =~ /^[aru]$/;
    my(@args) = @_;
    @args = '/./' unless @args;
    my(@result,$module,%seen,%need,$headerdone,
       $version_undefs,$version_zeroes,
       @version_undefs,@version_zeroes);
    $version_undefs = $version_zeroes = 0;
    my $sprintf = "%s%-25s%s %9s %9s  %s\n";
    my @expand = $self->expand('Module',@args);
    my $expand = scalar @expand;
    if (0) { # Looks like noise to me, was very useful for debugging
             # for metadata cache
        $CPAN::Frontend->myprint(sprintf "%d matches in the database\n", $expand);
    }
  MODULE: for $module (@expand) {
        my $file  = $module->cpan_file;
        next MODULE unless defined $file; # ??
        $file =~ s!^./../!!;
        my($latest) = $module->cpan_version;
        my($inst_file) = $module->inst_file;
        my($have);
        return if $CPAN::Signal;
        if ($inst_file) {
            if ($what eq "a") {
                $have = $module->inst_version;
            } elsif ($what eq "r") {
                $have = $module->inst_version;
                local($^W) = 0;
                if ($have eq "undef") {
                    $version_undefs++;
                    push @version_undefs, $module->as_glimpse;
                } elsif (CPAN::Version->vcmp($have,0)==0) {
                    $version_zeroes++;
                    push @version_zeroes, $module->as_glimpse;
                }
                next MODULE unless CPAN::Version->vgt($latest, $have);
# to be pedantic we should probably say:
#    && !($have eq "undef" && $latest ne "undef" && $latest gt "");
# to catch the case where CPAN has a version 0 and we have a version undef
            } elsif ($what eq "u") {
                next MODULE;
            }
        } else {
            if ($what eq "a") {
                next MODULE;
            } elsif ($what eq "r") {
                next MODULE;
            } elsif ($what eq "u") {
                $have = "-";
            }
        }
        return if $CPAN::Signal; # this is sometimes lengthy
        $seen{$file} ||= 0;
        if ($what eq "a") {
            push @result, sprintf "%s %s\n", $module->id, $have;
        } elsif ($what eq "r") {
            push @result, $module->id;
            next MODULE if $seen{$file}++;
        } elsif ($what eq "u") {
            push @result, $module->id;
            next MODULE if $seen{$file}++;
            next MODULE if $file =~ /^Contact/;
        }
        unless ($headerdone++) {
            $CPAN::Frontend->myprint("\n");
            $CPAN::Frontend->myprint(sprintf(
                                             $sprintf,
                                             "",
                                             "Package namespace",
                                             "",
                                             "installed",
                                             "latest",
                                             "in CPAN file"
                                            ));
        }
        my $color_on = "";
        my $color_off = "";
        if (
            $COLOR_REGISTERED
            &&
            $CPAN::META->has_inst("Term::ANSIColor")
            &&
            $module->description
           ) {
            $color_on = Term::ANSIColor::color("green");
            $color_off = Term::ANSIColor::color("reset");
        }
        $CPAN::Frontend->myprint(sprintf $sprintf,
                                 $color_on,
                                 $module->id,
                                 $color_off,
                                 $have,
                                 $latest,
                                 $file);
        $need{$module->id}++;
    }
    unless (%need) {
        if ($what eq "u") {
            $CPAN::Frontend->myprint("No modules found for @args\n");
        } elsif ($what eq "r") {
            $CPAN::Frontend->myprint("All modules are up to date for @args\n");
        }
    }
    if ($what eq "r") {
        if ($version_zeroes) {
            my $s_has = $version_zeroes > 1 ? "s have" : " has";
            $CPAN::Frontend->myprint(qq{$version_zeroes installed module$s_has }.
                                     qq{a version number of 0\n});
            if ($CPAN::Config->{show_zero_versions}) {
                local $" = "\t";
                $CPAN::Frontend->myprint(qq{  they are\n\t@version_zeroes\n});
                $CPAN::Frontend->myprint(qq{(use 'o conf show_zero_versions 0' }.
                                         qq{to hide them)\n});
            } else {
                $CPAN::Frontend->myprint(qq{(use 'o conf show_zero_versions 1' }.
                                         qq{to show them)\n});
            }
        }
        if ($version_undefs) {
            my $s_has = $version_undefs > 1 ? "s have" : " has";
            $CPAN::Frontend->myprint(qq{$version_undefs installed module$s_has no }.
                                     qq{parseable version number\n});
            if ($CPAN::Config->{show_unparsable_versions}) {
                local $" = "\t";
                $CPAN::Frontend->myprint(qq{  they are\n\t@version_undefs\n});
                $CPAN::Frontend->myprint(qq{(use 'o conf show_unparsable_versions 0' }.
                                         qq{to hide them)\n});
            } else {
                $CPAN::Frontend->myprint(qq{(use 'o conf show_unparsable_versions 1' }.
                                         qq{to show them)\n});
            }
        }
    }
    @result;
}

#-> sub CPAN::Shell::r ;
sub r {
    shift->_u_r_common("r",@_);
}

#-> sub CPAN::Shell::u ;
sub u {
    shift->_u_r_common("u",@_);
}

#-> sub CPAN::Shell::failed ;
sub failed {
    my($self,$only_id,$silent) = @_;
    my @failed;
  DIST: for my $d ($CPAN::META->all_objects("CPAN::Distribution")) {
        my $failed = "";
      NAY: for my $nosayer ( # order matters!
                            "unwrapped",
                            "writemakefile",
                            "signature_verify",
                            "make",
                            "make_test",
                            "install",
                            "make_clean",
                           ) {
            next unless exists $d->{$nosayer};
            next unless defined $d->{$nosayer};
            next unless (
                         UNIVERSAL::can($d->{$nosayer},"failed") ?
                         $d->{$nosayer}->failed :
                         $d->{$nosayer} =~ /^NO/
                        );
            next NAY if $only_id && $only_id != (
                                                 UNIVERSAL::can($d->{$nosayer},"commandid")
                                                 ?
                                                 $d->{$nosayer}->commandid
                                                 :
                                                 $CPAN::CurrentCommandId
                                                );
            $failed = $nosayer;
            last;
        }
        next DIST unless $failed;
        my $id = $d->id;
        $id =~ s|^./../||;
        #$print .= sprintf(
        #                  "  %-45s: %s %s\n",
        push @failed,
            (
             UNIVERSAL::can($d->{$failed},"failed") ?
             [
              $d->{$failed}->commandid,
              $id,
              $failed,
              $d->{$failed}->text,
              $d->{$failed}{TIME}||0,
             ] :
             [
              1,
              $id,
              $failed,
              $d->{$failed},
              0,
             ]
            );
    }
    my $scope;
    if ($only_id) {
        $scope = "this command";
    } elsif ($CPAN::Index::HAVE_REANIMATED) {
        $scope = "this or a previous session";
        # it might be nice to have a section for previous session and
        # a second for this
    } else {
        $scope = "this session";
    }
    if (@failed) {
        my $print;
        my $debug = 0;
        if ($debug) {
            $print = join "",
                map { sprintf "%5d %-45s: %s %s\n", @$_ }
                    sort { $a->[0] <=> $b->[0] } @failed;
        } else {
            $print = join "",
                map { sprintf " %-45s: %s %s\n", @$_[1..3] }
                    sort {
                        $a->[0] <=> $b->[0]
                            ||
                                $a->[4] <=> $b->[4]
                       } @failed;
        }
        $CPAN::Frontend->myprint("Failed during $scope:\n$print");
    } elsif (!$only_id || !$silent) {
        $CPAN::Frontend->myprint("Nothing failed in $scope\n");
    }
}

# XXX intentionally undocumented because completely bogus, unportable,
# useless, etc.

#-> sub CPAN::Shell::status ;
sub status {
    my($self) = @_;
    require Devel::Size;
    my $ps = FileHandle->new;
    open $ps, "/proc/$$/status";
    my $vm = 0;
    while (<$ps>) {
        next unless /VmSize:\s+(\d+)/;
        $vm = $1;
        last;
    }
    $CPAN::Frontend->mywarn(sprintf(
                                    "%-27s %6d\n%-27s %6d\n",
                                    "vm",
                                    $vm,
                                    "CPAN::META",
                                    Devel::Size::total_size($CPAN::META)/1024,
                                   ));
    for my $k (sort keys %$CPAN::META) {
        next unless substr($k,0,4) eq "read";
        warn sprintf " %-26s %6d\n", $k, Devel::Size::total_size($CPAN::META->{$k})/1024;
        for my $k2 (sort keys %{$CPAN::META->{$k}}) {
            warn sprintf "  %-25s %6d (keys: %6d)\n",
                $k2,
                    Devel::Size::total_size($CPAN::META->{$k}{$k2})/1024,
                          scalar keys %{$CPAN::META->{$k}{$k2}};
        }
    }
}

# compare with install_tested
#-> sub CPAN::Shell::is_tested
sub is_tested {
    my($self) = @_;
    CPAN::Index->reload;
    for my $b (reverse $CPAN::META->_list_sorted_descending_is_tested) {
        my $time;
        if ($CPAN::META->{is_tested}{$b}) {
            $time = scalar(localtime $CPAN::META->{is_tested}{$b});
        } else {
            $time = scalar localtime;
            $time =~ s/\S/?/g;
        }
        $CPAN::Frontend->myprint(sprintf "%s %s\n", $time, $b);
    }
}

#-> sub CPAN::Shell::autobundle ;
sub autobundle {
    my($self) = shift;
    CPAN::HandleConfig->load unless $CPAN::Config_loaded++;
    my(@bundle) = $self->_u_r_common("a",@_);
    my($todir) = File::Spec->catdir($CPAN::Config->{'cpan_home'},"Bundle");
    File::Path::mkpath($todir);
    unless (-d $todir) {
        $CPAN::Frontend->myprint("Couldn't mkdir $todir for some reason\n");
        return;
    }
    my($y,$m,$d) =  (localtime)[5,4,3];
    $y+=1900;
    $m++;
    my($c) = 0;
    my($me) = sprintf "Snapshot_%04d_%02d_%02d_%02d", $y, $m, $d, $c;
    my($to) = File::Spec->catfile($todir,"$me.pm");
    while (-f $to) {
        $me = sprintf "Snapshot_%04d_%02d_%02d_%02d", $y, $m, $d, ++$c;
        $to = File::Spec->catfile($todir,"$me.pm");
    }
    my($fh) = FileHandle->new(">$to") or Carp::croak "Can't open >$to: $!";
    $fh->print(
               "package Bundle::$me;\n\n",
               "\$VERSION = '0.01';\n\n",
               "1;\n\n",
               "__END__\n\n",
               "=head1 NAME\n\n",
               "Bundle::$me - Snapshot of installation on ",
               $Config::Config{'myhostname'},
               " on ",
               scalar(localtime),
               "\n\n=head1 SYNOPSIS\n\n",
               "perl -MCPAN -e 'install Bundle::$me'\n\n",
               "=head1 CONTENTS\n\n",
               join("\n", @bundle),
               "\n\n=head1 CONFIGURATION\n\n",
               Config->myconfig,
               "\n\n=head1 AUTHOR\n\n",
               "This Bundle has been generated automatically ",
               "by the autobundle routine in CPAN.pm.\n",
              );
    $fh->close;
    $CPAN::Frontend->myprint("\nWrote bundle file
    $to\n\n");
}

#-> sub CPAN::Shell::expandany ;
sub expandany {
    my($self,$s) = @_;
    CPAN->debug("s[$s]") if $CPAN::DEBUG;
    if ($s =~ m|/| or substr($s,-1,1) eq ".") { # looks like a file or a directory
        $s = CPAN::Distribution->normalize($s);
        return $CPAN::META->instance('CPAN::Distribution',$s);
        # Distributions spring into existence, not expand
    } elsif ($s =~ m|^Bundle::|) {
        $self->local_bundles; # scanning so late for bundles seems
                              # both attractive and crumpy: always
                              # current state but easy to forget
                              # somewhere
        return $self->expand('Bundle',$s);
    } else {
        return $self->expand('Module',$s)
            if $CPAN::META->exists('CPAN::Module',$s);
    }
    return;
}

#-> sub CPAN::Shell::expand ;
sub expand {
    my $self = shift;
    my($type,@args) = @_;
    CPAN->debug("type[$type]args[@args]") if $CPAN::DEBUG;
    my $class = "CPAN::$type";
    my $methods = ['id'];
    for my $meth (qw(name)) {
        next unless $class->can($meth);
        push @$methods, $meth;
    }
    $self->expand_by_method($class,$methods,@args);
}

#-> sub CPAN::Shell::expand_by_method ;
sub expand_by_method {
    my $self = shift;
    my($class,$methods,@args) = @_;
    my($arg,@m);
    for $arg (@args) {
        my($regex,$command);
        if ($arg =~ m|^/(.*)/$|) {
            $regex = $1;
# FIXME:  there seem to be some ='s in the author data, which trigger
#         a failure here.  This needs to be contemplated.
#            } elsif ($arg =~ m/=/) {
#                $command = 1;
        }
        my $obj;
        CPAN->debug(sprintf "class[%s]regex[%s]command[%s]",
                    $class,
                    defined $regex ? $regex : "UNDEFINED",
                    defined $command ? $command : "UNDEFINED",
                   ) if $CPAN::DEBUG;
        if (defined $regex) {
            if (CPAN::_sqlite_running) {
                $CPAN::SQLite->search($class, $regex);
            }
            for $obj (
                      $CPAN::META->all_objects($class)
                     ) {
                unless ($obj && UNIVERSAL::can($obj,"id") && $obj->id) {
                    # BUG, we got an empty object somewhere
                    require Data::Dumper;
                    CPAN->debug(sprintf(
                                        "Bug in CPAN: Empty id on obj[%s][%s]",
                                        $obj,
                                        Data::Dumper::Dumper($obj)
                                       )) if $CPAN::DEBUG;
                    next;
                }
                for my $method (@$methods) {
                    my $match = eval {$obj->$method() =~ /$regex/i};
                    if ($@) {
                        my($err) = $@ =~ /^(.+) at .+? line \d+\.$/;
                        $err ||= $@; # if we were too restrictive above
                        $CPAN::Frontend->mydie("$err\n");
                    } elsif ($match) {
                        push @m, $obj;
                        last;
                    }
                }
            }
        } elsif ($command) {
            die "equal sign in command disabled (immature interface), ".
                "you can set
 ! \$CPAN::Shell::ADVANCED_QUERY=1
to enable it. But please note, this is HIGHLY EXPERIMENTAL code
that may go away anytime.\n"
                    unless $ADVANCED_QUERY;
            my($method,$criterion) = $arg =~ /(.+?)=(.+)/;
            my($matchcrit) = $criterion =~ m/^~(.+)/;
            for my $self (
                          sort
                          {$a->id cmp $b->id}
                          $CPAN::META->all_objects($class)
                         ) {
                my $lhs = $self->$method() or next; # () for 5.00503
                if ($matchcrit) {
                    push @m, $self if $lhs =~ m/$matchcrit/;
                } else {
                    push @m, $self if $lhs eq $criterion;
                }
            }
        } else {
            my($xarg) = $arg;
            if ( $class eq 'CPAN::Bundle' ) {
                $xarg =~ s/^(Bundle::)?(.*)/Bundle::$2/;
            } elsif ($class eq "CPAN::Distribution") {
                $xarg = CPAN::Distribution->normalize($arg);
            } else {
                $xarg =~ s/:+/::/g;
            }
            if ($CPAN::META->exists($class,$xarg)) {
                $obj = $CPAN::META->instance($class,$xarg);
            } elsif ($CPAN::META->exists($class,$arg)) {
                $obj = $CPAN::META->instance($class,$arg);
            } else {
                next;
            }
            push @m, $obj;
        }
    }
    @m = sort {$a->id cmp $b->id} @m;
    if ( $CPAN::DEBUG ) {
        my $wantarray = wantarray;
        my $join_m = join ",", map {$_->id} @m;
        $self->debug("wantarray[$wantarray]join_m[$join_m]");
    }
    return wantarray ? @m : $m[0];
}

#-> sub CPAN::Shell::format_result ;
sub format_result {
    my($self) = shift;
    my($type,@args) = @_;
    @args = '/./' unless @args;
    my(@result) = $self->expand($type,@args);
    my $result = @result == 1 ?
        $result[0]->as_string :
            @result == 0 ?
                "No objects of type $type found for argument @args\n" :
                    join("",
                         (map {$_->as_glimpse} @result),
                         scalar @result, " items found\n",
                        );
    $result;
}

#-> sub CPAN::Shell::report_fh ;
{
    my $installation_report_fh;
    my $previously_noticed = 0;

    sub report_fh {
        return $installation_report_fh if $installation_report_fh;
        if ($CPAN::META->has_usable("File::Temp")) {
            $installation_report_fh
                = File::Temp->new(
                                  dir      => File::Spec->tmpdir,
                                  template => 'cpan_install_XXXX',
                                  suffix   => '.txt',
                                  unlink   => 0,
                                 );
        }
        unless ( $installation_report_fh ) {
            warn("Couldn't open installation report file; " .
                 "no report file will be generated."
                ) unless $previously_noticed++;
        }
    }
}


# The only reason for this method is currently to have a reliable
# debugging utility that reveals which output is going through which
# channel. No, I don't like the colors ;-)

# to turn colordebugging on, write
# cpan> o conf colorize_output 1

#-> sub CPAN::Shell::print_ornamented ;
{
    my $print_ornamented_have_warned = 0;
    sub colorize_output {
        my $colorize_output = $CPAN::Config->{colorize_output};
        if ($colorize_output && !$CPAN::META->has_inst("Term::ANSIColor")) {
            unless ($print_ornamented_have_warned++) {
                # no myprint/mywarn within myprint/mywarn!
                warn "Colorize_output is set to true but Term::ANSIColor is not
installed. To activate colorized output, please install Term::ANSIColor.\n\n";
            }
            $colorize_output = 0;
        }
        return $colorize_output;
    }
}


#-> sub CPAN::Shell::print_ornamented ;
sub print_ornamented {
    my($self,$what,$ornament) = @_;
    return unless defined $what;

    local $| = 1; # Flush immediately
    if ( $CPAN::Be_Silent ) {
        print {report_fh()} $what;
        return;
    }
    my $swhat = "$what"; # stringify if it is an object
    if ($CPAN::Config->{term_is_latin}) {
        # note: deprecated, need to switch to $LANG and $LC_*
        # courtesy jhi:
        $swhat
            =~ s{([\xC0-\xDF])([\x80-\xBF])}{chr(ord($1)<<6&0xC0|ord($2)&0x3F)}eg; #};
    }
    if ($self->colorize_output) {
        if ( $CPAN::DEBUG && $swhat =~ /^Debug\(/ ) {
            # if you want to have this configurable, please file a bugreport
            $ornament = $CPAN::Config->{colorize_debug} || "black on_cyan";
        }
        my $color_on = eval { Term::ANSIColor::color($ornament) } || "";
        if ($@) {
            print "Term::ANSIColor rejects color[$ornament]: $@\n
Please choose a different color (Hint: try 'o conf init /color/')\n";
        }
        # GGOLDBACH/Test-GreaterVersion-0.008 broke wthout this
        # $trailer construct. We want the newline be the last thing if
        # there is a newline at the end ensuring that the next line is
        # empty for other players
        my $trailer = "";
        $trailer = $1 if $swhat =~ s/([\r\n]+)\z//;
        print $color_on,
            $swhat,
                Term::ANSIColor::color("reset"),
                      $trailer;
    } else {
        print $swhat;
    }
}

#-> sub CPAN::Shell::myprint ;

# where is myprint/mywarn/Frontend/etc. documented? Where to use what?
# I think, we send everything to STDOUT and use print for normal/good
# news and warn for news that need more attention. Yes, this is our
# working contract for now.
sub myprint {
    my($self,$what) = @_;
    $self->print_ornamented($what,
                            $CPAN::Config->{colorize_print}||'bold blue on_white',
                           );
}

sub optprint {
    my($self,$category,$what) = @_;
    my $vname = $category . "_verbosity";
    CPAN::HandleConfig->load unless $CPAN::Config_loaded++;
    if (!$CPAN::Config->{$vname}
        || $CPAN::Config->{$vname} =~ /^v/
       ) {
        $CPAN::Frontend->myprint($what);
    }
}

#-> sub CPAN::Shell::myexit ;
sub myexit {
    my($self,$what) = @_;
    $self->myprint($what);
    exit;
}

#-> sub CPAN::Shell::mywarn ;
sub mywarn {
    my($self,$what) = @_;
    $self->print_ornamented($what, $CPAN::Config->{colorize_warn}||'bold red on_white');
}

# only to be used for shell commands
#-> sub CPAN::Shell::mydie ;
sub mydie {
    my($self,$what) = @_;
    $self->mywarn($what);

    # If it is the shell, we want the following die to be silent,
    # but if it is not the shell, we would need a 'die $what'. We need
    # to take care that only shell commands use mydie. Is this
    # possible?

    die "\n";
}

# sub CPAN::Shell::colorable_makemaker_prompt ;
sub colorable_makemaker_prompt {
    my($foo,$bar) = @_;
    if (CPAN::Shell->colorize_output) {
        my $ornament = $CPAN::Config->{colorize_print}||'bold blue on_white';
        my $color_on = eval { Term::ANSIColor::color($ornament); } || "";
        print $color_on;
    }
    my $ans = ExtUtils::MakeMaker::prompt($foo,$bar);
    if (CPAN::Shell->colorize_output) {
        print Term::ANSIColor::color('reset');
    }
    return $ans;
}

# use this only for unrecoverable errors!
#-> sub CPAN::Shell::unrecoverable_error ;
sub unrecoverable_error {
    my($self,$what) = @_;
    my @lines = split /\n/, $what;
    my $longest = 0;
    for my $l (@lines) {
        $longest = length $l if length $l > $longest;
    }
    $longest = 62 if $longest > 62;
    for my $l (@lines) {
        if ($l =~ /^\s*$/) {
            $l = "\n";
            next;
        }
        $l = "==> $l";
        if (length $l < 66) {
            $l = pack "A66 A*", $l, "<==";
        }
        $l .= "\n";
    }
    unshift @lines, "\n";
    $self->mydie(join "", @lines);
}

#-> sub CPAN::Shell::mysleep ;
sub mysleep {
    my($self, $sleep) = @_;
    if (CPAN->has_inst("Time::HiRes")) {
        Time::HiRes::sleep($sleep);
    } else {
        sleep($sleep < 1 ? 1 : int($sleep + 0.5));
    }
}

#-> sub CPAN::Shell::setup_output ;
sub setup_output {
    return if -t STDOUT;
    my $odef = select STDERR;
    $| = 1;
    select STDOUT;
    $| = 1;
    select $odef;
}

#-> sub CPAN::Shell::rematein ;
# RE-adme||MA-ke||TE-st||IN-stall : nearly everything runs through here
sub rematein {
    my $self = shift;
    my($meth,@some) = @_;
    my @pragma;
    while($meth =~ /^(ff?orce|notest)$/) {
        push @pragma, $meth;
        $meth = shift @some or
            $CPAN::Frontend->mydie("Pragma $pragma[-1] used without method: ".
                                   "cannot continue");
    }
    setup_output();
    CPAN->debug("pragma[@pragma]meth[$meth]some[@some]") if $CPAN::DEBUG;

    # Here is the place to set "test_count" on all involved parties to
    # 0. We then can pass this counter on to the involved
    # distributions and those can refuse to test if test_count > X. In
    # the first stab at it we could use a 1 for "X".

    # But when do I reset the distributions to start with 0 again?
    # Jost suggested to have a random or cycling interaction ID that
    # we pass through. But the ID is something that is just left lying
    # around in addition to the counter, so I'd prefer to set the
    # counter to 0 now, and repeat at the end of the loop. But what
    # about dependencies? They appear later and are not reset, they
    # enter the queue but not its copy. How do they get a sensible
    # test_count?

    # With configure_requires, "get" is vulnerable in recursion.

    my $needs_recursion_protection = "get|make|test|install";

    # construct the queue
    my($s,@s,@qcopy);
  STHING: foreach $s (@some) {
        my $obj;
        if (ref $s) {
            CPAN->debug("s is an object[$s]") if $CPAN::DEBUG;
            $obj = $s;
        } elsif ($s =~ m|[\$\@\%]|) { # looks like a perl variable
        } elsif ($s =~ m|^/|) { # looks like a regexp
            if (substr($s,-1,1) eq ".") {
                $obj = CPAN::Shell->expandany($s);
            } else {
                $CPAN::Frontend->mywarn("Sorry, $meth with a regular expression is ".
                                        "not supported.\nRejecting argument '$s'\n");
                $CPAN::Frontend->mysleep(2);
                next;
            }
        } elsif ($meth eq "ls") {
            $self->globls($s,\@pragma);
            next STHING;
        } else {
            CPAN->debug("calling expandany [$s]") if $CPAN::DEBUG;
            $obj = CPAN::Shell->expandany($s);
        }
        if (0) {
        } elsif (ref $obj) {
            if ($meth =~ /^($needs_recursion_protection)$/) {
                # it would be silly to check for recursion for look or dump
                # (we are in CPAN::Shell::rematein)
                CPAN->debug("Going to test against recursion") if $CPAN::DEBUG;
                eval {  $obj->color_cmd_tmps(0,1); };
                if ($@) {
                    if (ref $@
                        and $@->isa("CPAN::Exception::RecursiveDependency")) {
                        $CPAN::Frontend->mywarn($@);
                    } else {
                        if (0) {
                            require Carp;
                            Carp::confess(sprintf "DEBUG: \$\@[%s]ref[%s]", $@, ref $@);
                        }
                        die;
                    }
                }
            }
            CPAN::Queue->queue_item(qmod => $obj->id, reqtype => "c");
            push @qcopy, $obj;
        } elsif ($CPAN::META->exists('CPAN::Author',uc($s))) {
            $obj = $CPAN::META->instance('CPAN::Author',uc($s));
            if ($meth =~ /^(dump|ls|reports)$/) {
                $obj->$meth();
            } else {
                $CPAN::Frontend->mywarn(
                                        join "",
                                        "Don't be silly, you can't $meth ",
                                        $obj->fullname,
                                        " ;-)\n"
                                       );
                $CPAN::Frontend->mysleep(2);
            }
        } elsif ($s =~ m|[\$\@\%]| && $meth eq "dump") {
            CPAN::InfoObj->dump($s);
        } else {
            $CPAN::Frontend
                ->mywarn(qq{Warning: Cannot $meth $s, }.
                         qq{don't know what it is.
Try the command

    i /$s/

to find objects with matching identifiers.
});
            $CPAN::Frontend->mysleep(2);
        }
    }

    # queuerunner (please be warned: when I started to change the
    # queue to hold objects instead of names, I made one or two
    # mistakes and never found which. I reverted back instead)
    while (my $q = CPAN::Queue->first) {
        my $obj;
        my $s = $q->as_string;
        my $reqtype = $q->reqtype || "";
        $obj = CPAN::Shell->expandany($s);
        unless ($obj) {
            # don't know how this can happen, maybe we should panic,
            # but maybe we get a solution from the first user who hits
            # this unfortunate exception?
            $CPAN::Frontend->mywarn("Warning: Could not expand string '$s' ".
                                    "to an object. Skipping.\n");
            $CPAN::Frontend->mysleep(5);
            CPAN::Queue->delete_first($s);
            next;
        }
        $obj->{reqtype} ||= "";
        {
            # force debugging because CPAN::SQLite somehow delivers us
            # an empty object;

            # local $CPAN::DEBUG = 1024; # Shell; probably fixed now

            CPAN->debug("s[$s]obj-reqtype[$obj->{reqtype}]".
                        "q-reqtype[$reqtype]") if $CPAN::DEBUG;
        }
        if ($obj->{reqtype}) {
            if ($obj->{reqtype} eq "b" && $reqtype =~ /^[rc]$/) {
                $obj->{reqtype} = $reqtype;
                if (
                    exists $obj->{install}
                    &&
                    (
                     UNIVERSAL::can($obj->{install},"failed") ?
                     $obj->{install}->failed :
                     $obj->{install} =~ /^NO/
                    )
                   ) {
                    delete $obj->{install};
                    $CPAN::Frontend->mywarn
                        ("Promoting $obj->{ID} from 'build_requires' to 'requires'");
                }
            }
        } else {
            $obj->{reqtype} = $reqtype;
        }

        for my $pragma (@pragma) {
            if ($pragma
                &&
                $obj->can($pragma)) {
                $obj->$pragma($meth);
            }
        }
        if (UNIVERSAL::can($obj, 'called_for')) {
            $obj->called_for($s);
        }
        CPAN->debug(qq{pragma[@pragma]meth[$meth]}.
                    qq{ID[$obj->{ID}]}) if $CPAN::DEBUG;

        push @qcopy, $obj;
        if ($meth =~ /^(report)$/) { # they came here with a pragma?
            $self->$meth($obj);
        } elsif (! UNIVERSAL::can($obj,$meth)) {
            # Must never happen
            my $serialized = "";
            if (0) {
            } elsif ($CPAN::META->has_inst("YAML::Syck")) {
                $serialized = YAML::Syck::Dump($obj);
            } elsif ($CPAN::META->has_inst("YAML")) {
                $serialized = YAML::Dump($obj);
            } elsif ($CPAN::META->has_inst("Data::Dumper")) {
                $serialized = Data::Dumper::Dumper($obj);
            } else {
                require overload;
                $serialized = overload::StrVal($obj);
            }
            CPAN->debug("Going to panic. meth[$meth]s[$s]") if $CPAN::DEBUG;
            $CPAN::Frontend->mydie("Panic: obj[$serialized] cannot meth[$meth]");
        } elsif ($obj->$meth()) {
            CPAN::Queue->delete($s);
            CPAN->debug("From queue deleted. meth[$meth]s[$s]") if $CPAN::DEBUG;
        } else {
            CPAN->debug("Failed. pragma[@pragma]meth[$meth]") if $CPAN::DEBUG;
        }

        $obj->undelay;
        for my $pragma (@pragma) {
            my $unpragma = "un$pragma";
            if ($obj->can($unpragma)) {
                $obj->$unpragma();
            }
        }
        CPAN::Queue->delete_first($s);
    }
    if ($meth =~ /^($needs_recursion_protection)$/) {
        for my $obj (@qcopy) {
            $obj->color_cmd_tmps(0,0);
        }
    }
}

#-> sub CPAN::Shell::recent ;
sub recent {
  my($self) = @_;
  if ($CPAN::META->has_inst("XML::LibXML")) {
      my $url = $CPAN::Defaultrecent;
      $CPAN::Frontend->myprint("Going to fetch '$url'\n");
      unless ($CPAN::META->has_usable("LWP")) {
          $CPAN::Frontend->mydie("LWP not installed; cannot continue");
      }
      CPAN::LWP::UserAgent->config;
      my $Ua;
      eval { $Ua = CPAN::LWP::UserAgent->new; };
      if ($@) {
          $CPAN::Frontend->mydie("CPAN::LWP::UserAgent->new dies with $@\n");
      }
      my $resp = $Ua->get($url);
      unless ($resp->is_success) {
          $CPAN::Frontend->mydie(sprintf "Could not download '%s': %s\n", $url, $resp->code);
      }
      $CPAN::Frontend->myprint("DONE\n\n");
      my $xml = XML::LibXML->new->parse_string($resp->content);
      if (0) {
          my $s = $xml->serialize(2);
          $s =~ s/\n\s*\n/\n/g;
          $CPAN::Frontend->myprint($s);
          return;
      }
      my @distros;
      if ($url =~ /winnipeg/) {
          my $pubdate = $xml->findvalue("/rss/channel/pubDate");
          $CPAN::Frontend->myprint("    pubDate: $pubdate\n\n");
          for my $eitem ($xml->findnodes("/rss/channel/item")) {
              my $distro = $eitem->findvalue("enclosure/\@url");
              $distro =~ s|.*?/authors/id/./../||;
              my $size   = $eitem->findvalue("enclosure/\@length");
              my $desc   = $eitem->findvalue("description");
               $desc =~ s/.+? - //;
              $CPAN::Frontend->myprint("$distro [$size b]\n    $desc\n");
              push @distros, $distro;
          }
      } elsif ($url =~ /search.*uploads.rdf/) {
          # xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
          # xmlns="http://purl.org/rss/1.0/"
          # xmlns:taxo="http://purl.org/rss/1.0/modules/taxonomy/"
          # xmlns:dc="http://purl.org/dc/elements/1.1/"
          # xmlns:syn="http://purl.org/rss/1.0/modules/syndication/"
          # xmlns:admin="http://webns.net/mvcb/"


          my $dc_date = $xml->findvalue("//*[local-name(.) = 'RDF']/*[local-name(.) = 'channel']/*[local-name(.) = 'date']");
          $CPAN::Frontend->myprint("    dc:date: $dc_date\n\n");
          my $finish_eitem = 0;
          local $SIG{INT} = sub { $finish_eitem = 1 };
        EITEM: for my $eitem ($xml->findnodes("//*[local-name(.) = 'RDF']/*[local-name(.) = 'item']")) {
              my $distro = $eitem->findvalue("\@rdf:about");
              $distro =~ s|.*~||; # remove up to the tilde before the name
              $distro =~ s|/$||; # remove trailing slash
              $distro =~ s|([^/]+)|\U$1\E|; # upcase the name
              my $author = uc $1 or die "distro[$distro] without author, cannot continue";
              my $desc   = $eitem->findvalue("*[local-name(.) = 'description']");
              my $i = 0;
            SUBDIRTEST: while () {
                  last SUBDIRTEST if ++$i >= 6; # half a dozen must do!
                  if (my @ret = $self->globls("$distro*")) {
                      @ret = grep {$_->[2] !~ /meta/} @ret;
                      @ret = grep {length $_->[2]} @ret;
                      if (@ret) {
                          $distro = "$author/$ret[0][2]";
                          last SUBDIRTEST;
                      }
                  }
                  $distro =~ s|/|/*/|; # allow it to reside in a subdirectory
              }

              next EITEM if $distro =~ m|\*|; # did not find the thing
              $CPAN::Frontend->myprint("____$desc\n");
              push @distros, $distro;
              last EITEM if $finish_eitem;
          }
      }
      return \@distros;
  } else {
      # deprecated old version
      $CPAN::Frontend->mydie("no XML::LibXML installed, cannot continue\n");
  }
}

#-> sub CPAN::Shell::smoke ;
sub smoke {
    my($self) = @_;
    my $distros = $self->recent;
  DISTRO: for my $distro (@$distros) {
        $CPAN::Frontend->myprint(sprintf "Going to download and test '$distro'\n");
        {
            my $skip = 0;
            local $SIG{INT} = sub { $skip = 1 };
            for (0..9) {
                $CPAN::Frontend->myprint(sprintf "\r%2d (Hit ^C to skip)", 10-$_);
                sleep 1;
                if ($skip) {
                    $CPAN::Frontend->myprint(" skipped\n");
                    next DISTRO;
                }
            }
        }
        $CPAN::Frontend->myprint("\r  \n"); # leave the dirty line with a newline
        $self->test($distro);
    }
}

{
    # set up the dispatching methods
    no strict "refs";
    for my $command (qw(
                        clean
                        cvs_import
                        dump
                        force
                        fforce
                        get
                        install
                        look
                        ls
                        make
                        notest
                        perldoc
                        readme
                        reports
                        test
                       )) {
        *$command = sub { shift->rematein($command, @_); };
    }
}

package CPAN::LWP::UserAgent;
use strict;

sub config {
    return if $SETUPDONE;
    if ($CPAN::META->has_usable('LWP::UserAgent')) {
        require LWP::UserAgent;
        @ISA = qw(Exporter LWP::UserAgent);
        $SETUPDONE++;
    } else {
        $CPAN::Frontend->mywarn("  LWP::UserAgent not available\n");
    }
}

sub get_basic_credentials {
    my($self, $realm, $uri, $proxy) = @_;
    if ($USER && $PASSWD) {
        return ($USER, $PASSWD);
    }
    if ( $proxy ) {
        ($USER,$PASSWD) = $self->get_proxy_credentials();
    } else {
        ($USER,$PASSWD) = $self->get_non_proxy_credentials();
    }
    return($USER,$PASSWD);
}

sub get_proxy_credentials {
    my $self = shift;
    my ($user, $password);
    if ( defined $CPAN::Config->{proxy_user} &&
         defined $CPAN::Config->{proxy_pass}) {
        $user = $CPAN::Config->{proxy_user};
        $password = $CPAN::Config->{proxy_pass};
        return ($user, $password);
    }
    my $username_prompt = "\nProxy authentication needed!
 (Note: to permanently configure username and password run
   o conf proxy_user your_username
   o conf proxy_pass your_password
     )\nUsername:";
    ($user, $password) =
        _get_username_and_password_from_user($username_prompt);
    return ($user,$password);
}

sub get_non_proxy_credentials {
    my $self = shift;
    my ($user,$password);
    if ( defined $CPAN::Config->{username} &&
         defined $CPAN::Config->{password}) {
        $user = $CPAN::Config->{username};
        $password = $CPAN::Config->{password};
        return ($user, $password);
    }
    my $username_prompt = "\nAuthentication needed!
     (Note: to permanently configure username and password run
       o conf username your_username
       o conf password your_password
     )\nUsername:";

    ($user, $password) =
        _get_username_and_password_from_user($username_prompt);
    return ($user,$password);
}

sub _get_username_and_password_from_user {
    my $username_message = shift;
    my ($username,$password);

    ExtUtils::MakeMaker->import(qw(prompt));
    $username = prompt($username_message);
        if ($CPAN::META->has_inst("Term::ReadKey")) {
            Term::ReadKey::ReadMode("noecho");
        }
    else {
        $CPAN::Frontend->mywarn(
            "Warning: Term::ReadKey seems not to be available, your password will be echoed to the terminal!\n"
        );
    }
    $password = prompt("Password:");

        if ($CPAN::META->has_inst("Term::ReadKey")) {
            Term::ReadKey::ReadMode("restore");
        }
        $CPAN::Frontend->myprint("\n\n");
    return ($username,$password);
}

# mirror(): Its purpose is to deal with proxy authentication. When we
# call SUPER::mirror, we relly call the mirror method in
# LWP::UserAgent. LWP::UserAgent will then call
# $self->get_basic_credentials or some equivalent and this will be
# $self->dispatched to our own get_basic_credentials method.

# Our own get_basic_credentials sets $USER and $PASSWD, two globals.

# 407 stands for HTTP_PROXY_AUTHENTICATION_REQUIRED. Which means
# although we have gone through our get_basic_credentials, the proxy
# server refuses to connect. This could be a case where the username or
# password has changed in the meantime, so I'm trying once again without
# $USER and $PASSWD to give the get_basic_credentials routine another
# chance to set $USER and $PASSWD.

# mirror(): Its purpose is to deal with proxy authentication. When we
# call SUPER::mirror, we relly call the mirror method in
# LWP::UserAgent. LWP::UserAgent will then call
# $self->get_basic_credentials or some equivalent and this will be
# $self->dispatched to our own get_basic_credentials method.

# Our own get_basic_credentials sets $USER and $PASSWD, two globals.

# 407 stands for HTTP_PROXY_AUTHENTICATION_REQUIRED. Which means
# although we have gone through our get_basic_credentials, the proxy
# server refuses to connect. This could be a case where the username or
# password has changed in the meantime, so I'm trying once again without
# $USER and $PASSWD to give the get_basic_credentials routine another
# chance to set $USER and $PASSWD.

sub mirror {
    my($self,$url,$aslocal) = @_;
    my $result = $self->SUPER::mirror($url,$aslocal);
    if ($result->code == 407) {
        undef $USER;
        undef $PASSWD;
        $result = $self->SUPER::mirror($url,$aslocal);
    }
    $result;
}

package CPAN::FTP;
use strict;

#-> sub CPAN::FTP::ftp_statistics
# if they want to rewrite, they need to pass in a filehandle
sub _ftp_statistics {
    my($self,$fh) = @_;
    my $locktype = $fh ? LOCK_EX : LOCK_SH;
    $fh ||= FileHandle->new;
    my $file = File::Spec->catfile($CPAN::Config->{cpan_home},"FTPstats.yml");
    open $fh, "+>>$file" or $CPAN::Frontend->mydie("Could not open '$file': $!");
    my $sleep = 1;
    my $waitstart;
    while (!CPAN::_flock($fh, $locktype|LOCK_NB)) {
        $waitstart ||= localtime();
        if ($sleep>3) {
            $CPAN::Frontend->mywarn("Waiting for a read lock on '$file' (since $waitstart)\n");
        }
        $CPAN::Frontend->mysleep($sleep);
        if ($sleep <= 3) {
            $sleep+=0.33;
        } elsif ($sleep <=6) {
            $sleep+=0.11;
        }
    }
    my $stats = eval { CPAN->_yaml_loadfile($file); };
    if ($@) {
        if (ref $@) {
            if (ref $@ eq "CPAN::Exception::yaml_not_installed") {
                $CPAN::Frontend->myprint("Warning (usually harmless): $@");
                return;
            } elsif (ref $@ eq "CPAN::Exception::yaml_process_error") {
                $CPAN::Frontend->mydie($@);
            }
        } else {
            $CPAN::Frontend->mydie($@);
        }
    }
    return $stats->[0];
}

#-> sub CPAN::FTP::_mytime
sub _mytime () {
    if (CPAN->has_inst("Time::HiRes")) {
        return Time::HiRes::time();
    } else {
        return time;
    }
}

#-> sub CPAN::FTP::_new_stats
sub _new_stats {
    my($self,$file) = @_;
    my $ret = {
               file => $file,
               attempts => [],
               start => _mytime,
              };
    $ret;
}

#-> sub CPAN::FTP::_add_to_statistics
sub _add_to_statistics {
    my($self,$stats) = @_;
    my $yaml_module = CPAN::_yaml_module;
    $self->debug("yaml_module[$yaml_module]") if $CPAN::DEBUG;
    if ($CPAN::META->has_inst($yaml_module)) {
        $stats->{thesiteurl} = $ThesiteURL;
        if (CPAN->has_inst("Time::HiRes")) {
            $stats->{end} = Time::HiRes::time();
        } else {
            $stats->{end} = time;
        }
        my $fh = FileHandle->new;
        my $time = time;
        my $sdebug = 0;
        my @debug;
        @debug = $time if $sdebug;
        my $fullstats = $self->_ftp_statistics($fh);
        close $fh;
        $fullstats->{history} ||= [];
        push @debug, scalar @{$fullstats->{history}} if $sdebug;
        push @debug, time if $sdebug;
        push @{$fullstats->{history}}, $stats;
        # arbitrary hardcoded constants until somebody demands to have
        # them settable; YAML.pm 0.62 is unacceptably slow with 999;
        # YAML::Syck 0.82 has no noticable performance problem with 999;
        while (
               @{$fullstats->{history}} > 99
               || $time - $fullstats->{history}[0]{start} > 14*86400
              ) {
            shift @{$fullstats->{history}}
        }
        push @debug, scalar @{$fullstats->{history}} if $sdebug;
        push @debug, time if $sdebug;
        push @debug, scalar localtime($fullstats->{history}[0]{start}) if $sdebug;
        # need no eval because if this fails, it is serious
        my $sfile = File::Spec->catfile($CPAN::Config->{cpan_home},"FTPstats.yml");
        CPAN->_yaml_dumpfile("$sfile.$$",$fullstats);
        if ( $sdebug ) {
            local $CPAN::DEBUG = 512; # FTP
            push @debug, time;
            CPAN->debug(sprintf("DEBUG history: before_read[%d]before[%d]at[%d]".
                                "after[%d]at[%d]oldest[%s]dumped backat[%d]",
                                @debug,
                               ));
        }
        # Win32 cannot rename a file to an existing filename
        unlink($sfile) if ($^O eq 'MSWin32');
        rename "$sfile.$$", $sfile
            or $CPAN::Frontend->mydie("Could not rename '$sfile.$$' to '$sfile': $!\n");
    }
}

# if file is CHECKSUMS, suggest the place where we got the file to be
# checked from, maybe only for young files?
#-> sub CPAN::FTP::_recommend_url_for
sub _recommend_url_for {
    my($self, $file) = @_;
    my $urllist = $self->_get_urllist;
    if ($file =~ s|/CHECKSUMS(.gz)?$||) {
        my $fullstats = $self->_ftp_statistics();
        my $history = $fullstats->{history} || [];
        while (my $last = pop @$history) {
            last if $last->{end} - time > 3600; # only young results are interesting
            next unless $last->{file}; # dirname of nothing dies!
            next unless $file eq File::Basename::dirname($last->{file});
            return $last->{thesiteurl};
        }
    }
    if ($CPAN::Config->{randomize_urllist}
        &&
        rand(1) < $CPAN::Config->{randomize_urllist}
       ) {
        $urllist->[int rand scalar @$urllist];
    } else {
        return ();
    }
}

#-> sub CPAN::FTP::_get_urllist
sub _get_urllist {
    my($self) = @_;
    $CPAN::Config->{urllist} ||= [];
    unless (ref $CPAN::Config->{urllist} eq 'ARRAY') {
        $CPAN::Frontend->mywarn("Malformed urllist; ignoring.  Configuration file corrupt?\n");
        $CPAN::Config->{urllist} = [];
    }
    my @urllist = grep { defined $_ and length $_ } @{$CPAN::Config->{urllist}};
    for my $u (@urllist) {
        CPAN->debug("u[$u]") if $CPAN::DEBUG;
        if (UNIVERSAL::can($u,"text")) {
            $u->{TEXT} .= "/" unless substr($u->{TEXT},-1) eq "/";
        } else {
            $u .= "/" unless substr($u,-1) eq "/";
            $u = CPAN::URL->new(TEXT => $u, FROM => "USER");
        }
    }
    \@urllist;
}

#-> sub CPAN::FTP::ftp_get ;
sub ftp_get {
    my($class,$host,$dir,$file,$target) = @_;
    $class->debug(
                  qq[Going to fetch file [$file] from dir [$dir]
	on host [$host] as local [$target]\n]
                 ) if $CPAN::DEBUG;
    my $ftp = Net::FTP->new($host);
    unless ($ftp) {
        $CPAN::Frontend->mywarn("  Could not connect to host '$host' with Net::FTP\n");
        return;
    }
    return 0 unless defined $ftp;
    $ftp->debug(1) if $CPAN::DEBUG{'FTP'} & $CPAN::DEBUG;
    $class->debug(qq[Going to login("anonymous","$Config::Config{cf_email}")]);
    unless ( $ftp->login("anonymous",$Config::Config{'cf_email'}) ) {
        my $msg = $ftp->message;
        $CPAN::Frontend->mywarn("  Couldn't login on $host: $msg");
        return;
    }
    unless ( $ftp->cwd($dir) ) {
        my $msg = $ftp->message;
        $CPAN::Frontend->mywarn("  Couldn't cwd $dir: $msg");
        return;
    }
    $ftp->binary;
    $class->debug(qq[Going to ->get("$file","$target")\n]) if $CPAN::DEBUG;
    unless ( $ftp->get($file,$target) ) {
        my $msg = $ftp->message;
        $CPAN::Frontend->mywarn("  Couldn't fetch $file from $host: $msg");
        return;
    }
    $ftp->quit; # it's ok if this fails
    return 1;
}

# If more accuracy is wanted/needed, Chris Leach sent me this patch...

 # > *** /install/perl/live/lib/CPAN.pm-	Wed Sep 24 13:08:48 1997
 # > --- /tmp/cp	Wed Sep 24 13:26:40 1997
 # > ***************
 # > *** 1562,1567 ****
 # > --- 1562,1580 ----
 # >       return 1 if substr($url,0,4) eq "file";
 # >       return 1 unless $url =~ m|://([^/]+)|;
 # >       my $host = $1;
 # > +     my $proxy = $CPAN::Config->{'http_proxy'} || $ENV{'http_proxy'};
 # > +     if ($proxy) {
 # > +         $proxy =~ m|://([^/:]+)|;
 # > +         $proxy = $1;
 # > +         my $noproxy = $CPAN::Config->{'no_proxy'} || $ENV{'no_proxy'};
 # > +         if ($noproxy) {
 # > +             if ($host !~ /$noproxy$/) {
 # > +                 $host = $proxy;
 # > +             }
 # > +         } else {
 # > +             $host = $proxy;
 # > +         }
 # > +     }
 # >       require Net::Ping;
 # >       return 1 unless $Net::Ping::VERSION >= 2;
 # >       my $p;


#-> sub CPAN::FTP::localize ;
sub localize {
    my($self,$file,$aslocal,$force) = @_;
    $force ||= 0;
    Carp::croak "Usage: ->localize(cpan_file,as_local_file[,$force])"
        unless defined $aslocal;
    $self->debug("file[$file] aslocal[$aslocal] force[$force]")
        if $CPAN::DEBUG;

    if ($^O eq 'MacOS') {
        # Comment by AK on 2000-09-03: Uniq short filenames would be
        # available in CHECKSUMS file
        my($name, $path) = File::Basename::fileparse($aslocal, '');
        if (length($name) > 31) {
            $name =~ s/(
                        \.(
                           readme(\.(gz|Z))? |
                           (tar\.)?(gz|Z) |
                           tgz |
                           zip |
                           pm\.(gz|Z)
                          )
                       )$//x;
            my $suf = $1;
            my $size = 31 - length($suf);
            while (length($name) > $size) {
                chop $name;
            }
            $name .= $suf;
            $aslocal = File::Spec->catfile($path, $name);
        }
    }

    if (-f $aslocal && -r _ && !($force & 1)) {
        my $size;
        if ($size = -s $aslocal) {
            $self->debug("aslocal[$aslocal]size[$size]") if $CPAN::DEBUG;
            return $aslocal;
        } else {
            # empty file from a previous unsuccessful attempt to download it
            unlink $aslocal or
                $CPAN::Frontend->mydie("Found a zero-length '$aslocal' that I ".
                                       "could not remove.");
        }
    }
    my($maybe_restore) = 0;
    if (-f $aslocal) {
        rename $aslocal, "$aslocal.bak$$";
        $maybe_restore++;
    }

    my($aslocal_dir) = File::Basename::dirname($aslocal);
    $self->mymkpath($aslocal_dir); # too early for file URLs / RT #28438
    # Inheritance is not easier to manage than a few if/else branches
    if ($CPAN::META->has_usable('LWP::UserAgent')) {
        unless ($Ua) {
            CPAN::LWP::UserAgent->config;
            eval {$Ua = CPAN::LWP::UserAgent->new;}; # Why is has_usable still not fit enough?
            if ($@) {
                $CPAN::Frontend->mywarn("CPAN::LWP::UserAgent->new dies with $@\n")
                    if $CPAN::DEBUG;
            } else {
                my($var);
                $Ua->proxy('ftp',  $var)
                    if $var = $CPAN::Config->{ftp_proxy} || $ENV{ftp_proxy};
                $Ua->proxy('http', $var)
                    if $var = $CPAN::Config->{http_proxy} || $ENV{http_proxy};
                $Ua->no_proxy($var)
                    if $var = $CPAN::Config->{no_proxy} || $ENV{no_proxy};
            }
        }
    }
    for my $prx (qw(ftp_proxy http_proxy no_proxy)) {
        $ENV{$prx} = $CPAN::Config->{$prx} if $CPAN::Config->{$prx};
    }

    # Try the list of urls for each single object. We keep a record
    # where we did get a file from
    my(@reordered,$last);
    my $ccurllist = $self->_get_urllist;
    $last = $#$ccurllist;
    if ($force & 2) { # local cpans probably out of date, don't reorder
        @reordered = (0..$last);
    } else {
        @reordered =
            sort {
                (substr($ccurllist->[$b],0,4) eq "file")
                    <=>
                (substr($ccurllist->[$a],0,4) eq "file")
                    or
                defined($ThesiteURL)
                    and
                ($ccurllist->[$b] eq $ThesiteURL)
                    <=>
                ($ccurllist->[$a] eq $ThesiteURL)
            } 0..$last;
    }
    my(@levels);
    $Themethod ||= "";
    $self->debug("Themethod[$Themethod]reordered[@reordered]") if $CPAN::DEBUG;
    my @all_levels = (
                      ["dleasy",   "file"],
                      ["dleasy"],
                      ["dlhard"],
                      ["dlhardest"],
                      ["dleasy",   "http","defaultsites"],
                      ["dlhard",   "http","defaultsites"],
                      ["dleasy",   "ftp", "defaultsites"],
                      ["dlhard",   "ftp", "defaultsites"],
                      ["dlhardest","",    "defaultsites"],
                     );
    if ($Themethod) {
        @levels = grep {$_->[0] eq $Themethod} @all_levels;
        push @levels, grep {$_->[0] ne $Themethod} @all_levels;
    } else {
        @levels = @all_levels;
    }
    @levels = qw/dleasy/ if $^O eq 'MacOS';
    my($levelno);
    local $ENV{FTP_PASSIVE} =
        exists $CPAN::Config->{ftp_passive} ?
        $CPAN::Config->{ftp_passive} : 1;
    my $ret;
    my $stats = $self->_new_stats($file);
  LEVEL: for $levelno (0..$#levels) {
        my $level_tuple = $levels[$levelno];
        my($level,$scheme,$sitetag) = @$level_tuple;
        my $defaultsites = $sitetag && $sitetag eq "defaultsites";
        my @urllist;
        if ($defaultsites) {
            unless (defined $connect_to_internet_ok) {
                $CPAN::Frontend->myprint(sprintf qq{
I would like to connect to one of the following sites to get '%s':

%s
},
                                         $file,
                                         join("",map { " ".$_->text."\n" } @CPAN::Defaultsites),
                                        );
                my $answer = CPAN::Shell::colorable_makemaker_prompt("Is it OK to try to connect to the Internet?", "yes");
                if ($answer =~ /^y/i) {
                    $connect_to_internet_ok = 1;
                } else {
                    $connect_to_internet_ok = 0;
                }
            }
            if ($connect_to_internet_ok) {
                @urllist = @CPAN::Defaultsites;
            } else {
                @urllist = ();
            }
        } else {
            my @host_seq = $level =~ /dleasy/ ?
                @reordered : 0..$last;  # reordered has file and $Thesiteurl first
            @urllist = map { $ccurllist->[$_] } @host_seq;
        }
        $self->debug("synth. urllist[@urllist]") if $CPAN::DEBUG;
        my $aslocal_tempfile = $aslocal . ".tmp" . $$;
        if (my $recommend = $self->_recommend_url_for($file)) {
            @urllist = grep { $_ ne $recommend } @urllist;
            unshift @urllist, $recommend;
        }
        $self->debug("synth. urllist[@urllist]") if $CPAN::DEBUG;
        $ret = $self->hostdlxxx($level,$scheme,\@urllist,$file,$aslocal_tempfile,$stats);
        if ($ret) {
            CPAN->debug("ret[$ret]aslocal[$aslocal]") if $CPAN::DEBUG;
            if ($ret eq $aslocal_tempfile) {
                # if we got it exactly as we asked for, only then we
                # want to rename
                rename $aslocal_tempfile, $aslocal
                    or $CPAN::Frontend->mydie("Error while trying to rename ".
                                              "'$ret' to '$aslocal': $!");
                $ret = $aslocal;
            }
            $Themethod = $level;
            my $now = time;
            # utime $now, $now, $aslocal; # too bad, if we do that, we
                                          # might alter a local mirror
            $self->debug("level[$level]") if $CPAN::DEBUG;
            last LEVEL;
        } else {
            unlink $aslocal_tempfile;
            last if $CPAN::Signal; # need to cleanup
        }
    }
    if ($ret) {
        $stats->{filesize} = -s $ret;
    }
    $self->debug("before _add_to_statistics") if $CPAN::DEBUG;
    $self->_add_to_statistics($stats);
    $self->debug("after _add_to_statistics") if $CPAN::DEBUG;
    if ($ret) {
        unlink "$aslocal.bak$$";
        return $ret;
    }
    unless ($CPAN::Signal) {
        my(@mess);
        local $" = " ";
        if (@{$CPAN::Config->{urllist}}) {
            push @mess,
                qq{Please check, if the URLs I found in your configuration file \(}.
                    join(", ", @{$CPAN::Config->{urllist}}).
                        qq{\) are valid.};
        } else {
            push @mess, qq{Your urllist is empty!};
        }
        push @mess, qq{The urllist can be edited.},
            qq{E.g. with 'o conf urllist push ftp://myurl/'};
        $CPAN::Frontend->mywarn(Text::Wrap::wrap("","","@mess"). "\n\n");
        $CPAN::Frontend->mywarn("Could not fetch $file\n");
        $CPAN::Frontend->mysleep(2);
    }
    if ($maybe_restore) {
        rename "$aslocal.bak$$", $aslocal;
        $CPAN::Frontend->myprint("Trying to get away with old file:\n" .
                                 $self->ls($aslocal));
        return $aslocal;
    }
    return;
}

sub mymkpath {
    my($self, $aslocal_dir) = @_;
    File::Path::mkpath($aslocal_dir);
    $CPAN::Frontend->mywarn(qq{Warning: You are not allowed to write into }.
                            qq{directory "$aslocal_dir".
    I\'ll continue, but if you encounter problems, they may be due
    to insufficient permissions.\n}) unless -w $aslocal_dir;
}

sub hostdlxxx {
    my $self = shift;
    my $level = shift;
    my $scheme = shift;
    my $h = shift;
    $h = [ grep /^\Q$scheme\E:/, @$h ] if $scheme;
    my $method = "host$level";
    $self->$method($h, @_);
}

sub _set_attempt {
    my($self,$stats,$method,$url) = @_;
    push @{$stats->{attempts}}, {
                                 method => $method,
                                 start => _mytime,
                                 url => $url,
                                };
}

# package CPAN::FTP;
sub hostdleasy {
    my($self,$host_seq,$file,$aslocal,$stats) = @_;
    my($ro_url);
  HOSTEASY: for $ro_url (@$host_seq) {
        $self->_set_attempt($stats,"dleasy",$ro_url);
        my $url .= "$ro_url$file";
        $self->debug("localizing perlish[$url]") if $CPAN::DEBUG;
        if ($url =~ /^file:/) {
            my $l;
            if ($CPAN::META->has_inst('URI::URL')) {
                my $u =  URI::URL->new($url);
                $l = $u->path;
            } else { # works only on Unix, is poorly constructed, but
                # hopefully better than nothing.
                # RFC 1738 says fileurl BNF is
                # fileurl = "file://" [ host | "localhost" ] "/" fpath
                # Thanks to "Mark D. Baushke" <mdb@cisco.com> for
                # the code
                ($l = $url) =~ s|^file://[^/]*/|/|; # discard the host part
                $l =~ s|^file:||;                   # assume they
                                                    # meant
                                                    # file://localhost
                $l =~ s|^/||s
                    if ! -f $l && $l =~ m|^/\w:|;   # e.g. /P:
            }
            $self->debug("local file[$l]") if $CPAN::DEBUG;
            if ( -f $l && -r _) {
                $ThesiteURL = $ro_url;
                return $l;
            }
            if ($l =~ /(.+)\.gz$/) {
                my $ungz = $1;
                if ( -f $ungz && -r _) {
                    $ThesiteURL = $ro_url;
                    return $ungz;
                }
            }
            # Maybe mirror has compressed it?
            if (-f "$l.gz") {
                $self->debug("found compressed $l.gz") if $CPAN::DEBUG;
                eval { CPAN::Tarzip->new("$l.gz")->gunzip($aslocal) };
                if ( -f $aslocal) {
                    $ThesiteURL = $ro_url;
                    return $aslocal;
                }
            }
            $CPAN::Frontend->mywarn("Could not find '$l'\n");
        }
        $self->debug("it was not a file URL") if $CPAN::DEBUG;
        if ($CPAN::META->has_usable('LWP')) {
            $CPAN::Frontend->myprint("Fetching with LWP:
  $url
");
            unless ($Ua) {
                CPAN::LWP::UserAgent->config;
                eval { $Ua = CPAN::LWP::UserAgent->new; };
                if ($@) {
                    $CPAN::Frontend->mywarn("CPAN::LWP::UserAgent->new dies with $@\n");
                }
            }
            my $res = $Ua->mirror($url, $aslocal);
            if ($res->is_success) {
                $ThesiteURL = $ro_url;
                my $now = time;
                utime $now, $now, $aslocal; # download time is more
                                            # important than upload
                                            # time
                return $aslocal;
            } elsif ($url !~ /\.gz(?!\n)\Z/) {
                my $gzurl = "$url.gz";
                $CPAN::Frontend->myprint("Fetching with LWP:
  $gzurl
");
                $res = $Ua->mirror($gzurl, "$aslocal.gz");
                if ($res->is_success) {
                    if (eval {CPAN::Tarzip->new("$aslocal.gz")->gunzip($aslocal)}) {
                        $ThesiteURL = $ro_url;
                        return $aslocal;
                    }
                }
            } else {
                $CPAN::Frontend->myprint(sprintf(
                                                 "LWP failed with code[%s] message[%s]\n",
                                                 $res->code,
                                                 $res->message,
                                                ));
                # Alan Burlison informed me that in firewall environments
                # Net::FTP can still succeed where LWP fails. So we do not
                # skip Net::FTP anymore when LWP is available.
            }
        } else {
            $CPAN::Frontend->mywarn("  LWP not available\n");
        }
        return if $CPAN::Signal;
        if ($url =~ m|^ftp://(.*?)/(.*)/(.*)|) {
            # that's the nice and easy way thanks to Graham
            $self->debug("recognized ftp") if $CPAN::DEBUG;
            my($host,$dir,$getfile) = ($1,$2,$3);
            if ($CPAN::META->has_usable('Net::FTP')) {
                $dir =~ s|/+|/|g;
                $CPAN::Frontend->myprint("Fetching with Net::FTP:
  $url
");
                $self->debug("getfile[$getfile]dir[$dir]host[$host]" .
                             "aslocal[$aslocal]") if $CPAN::DEBUG;
                if (CPAN::FTP->ftp_get($host,$dir,$getfile,$aslocal)) {
                    $ThesiteURL = $ro_url;
                    return $aslocal;
                }
                if ($aslocal !~ /\.gz(?!\n)\Z/) {
                    my $gz = "$aslocal.gz";
                    $CPAN::Frontend->myprint("Fetching with Net::FTP
  $url.gz
");
                    if (CPAN::FTP->ftp_get($host,
                                           $dir,
                                           "$getfile.gz",
                                           $gz) &&
                        eval{CPAN::Tarzip->new($gz)->gunzip($aslocal)}
                    ) {
                        $ThesiteURL = $ro_url;
                        return $aslocal;
                    }
                }
                # next HOSTEASY;
            } else {
                CPAN->debug("Net::FTP does not count as usable atm") if $CPAN::DEBUG;
            }
        }
        if (
            UNIVERSAL::can($ro_url,"text")
            and
            $ro_url->{FROM} eq "USER"
           ) {
            ##address #17973: default URLs should not try to override
            ##user-defined URLs just because LWP is not available
            my $ret = $self->hostdlhard([$ro_url],$file,$aslocal,$stats);
            return $ret if $ret;
        }
        return if $CPAN::Signal;
    }
}

# package CPAN::FTP;
sub hostdlhard {
    my($self,$host_seq,$file,$aslocal,$stats) = @_;

    # Came back if Net::FTP couldn't establish connection (or
    # failed otherwise) Maybe they are behind a firewall, but they
    # gave us a socksified (or other) ftp program...

    my($ro_url);
    my($devnull) = $CPAN::Config->{devnull} || "";
    # < /dev/null ";
    my($aslocal_dir) = File::Basename::dirname($aslocal);
    File::Path::mkpath($aslocal_dir);
  HOSTHARD: for $ro_url (@$host_seq) {
        $self->_set_attempt($stats,"dlhard",$ro_url);
        my $url = "$ro_url$file";
        my($proto,$host,$dir,$getfile);

        # Courtesy Mark Conty mark_conty@cargill.com change from
        # if ($url =~ m|^ftp://(.*?)/(.*)/(.*)|) {
        # to
        if ($url =~ m|^([^:]+)://(.*?)/(.*)/(.*)|) {
            # proto not yet used
            ($proto,$host,$dir,$getfile) = ($1,$2,$3,$4);
        } else {
            next HOSTHARD; # who said, we could ftp anything except ftp?
        }
        next HOSTHARD if $proto eq "file"; # file URLs would have had
                                           # success above. Likely a bogus URL

        $self->debug("localizing funkyftpwise[$url]") if $CPAN::DEBUG;

        # Try the most capable first and leave ncftp* for last as it only
        # does FTP.
      DLPRG: for my $f (qw(curl wget lynx ncftpget ncftp)) {
            my $funkyftp = CPAN::HandleConfig->safe_quote($CPAN::Config->{$f});
            next unless defined $funkyftp;
            next if $funkyftp =~ /^\s*$/;

            my($asl_ungz, $asl_gz);
            ($asl_ungz = $aslocal) =~ s/\.gz//;
                $asl_gz = "$asl_ungz.gz";

            my($src_switch) = "";
            my($chdir) = "";
            my($stdout_redir) = " > $asl_ungz";
            if ($f eq "lynx") {
                $src_switch = " -source";
            } elsif ($f eq "ncftp") {
                $src_switch = " -c";
            } elsif ($f eq "wget") {
                $src_switch = " -O $asl_ungz";
                $stdout_redir = "";
            } elsif ($f eq 'curl') {
                $src_switch = ' -L -f -s -S --netrc-optional';
            }

            if ($f eq "ncftpget") {
                $chdir = "cd $aslocal_dir && ";
                $stdout_redir = "";
            }
            $CPAN::Frontend->myprint(
                                     qq[
Trying with "$funkyftp$src_switch" to get
    $url
]);
            my($system) =
                "$chdir$funkyftp$src_switch \"$url\" $devnull$stdout_redir";
            $self->debug("system[$system]") if $CPAN::DEBUG;
            my($wstatus) = system($system);
            if ($f eq "lynx") {
                # lynx returns 0 when it fails somewhere
                if (-s $asl_ungz) {
                    my $content = do { local *FH;
                                       open FH, $asl_ungz or die;
                                       local $/;
                                       <FH> };
                    if ($content =~ /^<.*(<title>[45]|Error [45])/si) {
                        $CPAN::Frontend->mywarn(qq{
No success, the file that lynx has downloaded looks like an error message:
$content
});
                        $CPAN::Frontend->mysleep(1);
                        next DLPRG;
                    }
                } else {
                    $CPAN::Frontend->myprint(qq{
No success, the file that lynx has downloaded is an empty file.
});
                    next DLPRG;
                }
            }
            if ($wstatus == 0) {
                if (-s $aslocal) {
                    # Looks good
                } elsif ($asl_ungz ne $aslocal) {
                    # test gzip integrity
                    if (eval{CPAN::Tarzip->new($asl_ungz)->gtest}) {
                        # e.g. foo.tar is gzipped --> foo.tar.gz
                        rename $asl_ungz, $aslocal;
                    } else {
                        eval{CPAN::Tarzip->new($asl_gz)->gzip($asl_ungz)};
                    }
                }
                $ThesiteURL = $ro_url;
                return $aslocal;
            } elsif ($url !~ /\.gz(?!\n)\Z/) {
                unlink $asl_ungz if
                    -f $asl_ungz && -s _ == 0;
                my $gz = "$aslocal.gz";
                my $gzurl = "$url.gz";
                $CPAN::Frontend->myprint(
                                        qq[
    Trying with "$funkyftp$src_switch" to get
    $url.gz
    ]);
                my($system) = "$funkyftp$src_switch \"$url.gz\" $devnull > $asl_gz";
                $self->debug("system[$system]") if $CPAN::DEBUG;
                my($wstatus);
                if (($wstatus = system($system)) == 0
                    &&
                    -s $asl_gz
                ) {
                    # test gzip integrity
                    my $ct = eval{CPAN::Tarzip->new($asl_gz)};
                    if ($ct && $ct->gtest) {
                        $ct->gunzip($aslocal);
                    } else {
                        # somebody uncompressed file for us?
                        rename $asl_ungz, $aslocal;
                    }
                    $ThesiteURL = $ro_url;
                    return $aslocal;
                } else {
                    unlink $asl_gz if -f $asl_gz;
                }
            } else {
                my $estatus = $wstatus >> 8;
                my $size = -f $aslocal ?
                    ", left\n$aslocal with size ".-s _ :
                    "\nWarning: expected file [$aslocal] doesn't exist";
                $CPAN::Frontend->myprint(qq{
    System call "$system"
    returned status $estatus (wstat $wstatus)$size
    });
            }
            return if $CPAN::Signal;
        } # transfer programs
    } # host
}

# package CPAN::FTP;
sub hostdlhardest {
    my($self,$host_seq,$file,$aslocal,$stats) = @_;

    return unless @$host_seq;
    my($ro_url);
    my($aslocal_dir) = File::Basename::dirname($aslocal);
    File::Path::mkpath($aslocal_dir);
    my $ftpbin = $CPAN::Config->{ftp};
    unless ($ftpbin && length $ftpbin && MM->maybe_command($ftpbin)) {
        $CPAN::Frontend->myprint("No external ftp command available\n\n");
        return;
    }
    $CPAN::Frontend->mywarn(qq{
As a last ressort we now switch to the external ftp command '$ftpbin'
to get '$aslocal'.

Doing so often leads to problems that are hard to diagnose.

If you're victim of such problems, please consider unsetting the ftp
config variable with

    o conf ftp ""
    o conf commit

});
    $CPAN::Frontend->mysleep(2);
  HOSTHARDEST: for $ro_url (@$host_seq) {
        $self->_set_attempt($stats,"dlhardest",$ro_url);
        my $url = "$ro_url$file";
        $self->debug("localizing ftpwise[$url]") if $CPAN::DEBUG;
        unless ($url =~ m|^ftp://(.*?)/(.*)/(.*)|) {
            next;
        }
        my($host,$dir,$getfile) = ($1,$2,$3);
        my $timestamp = 0;
        my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,
            $ctime,$blksize,$blocks) = stat($aslocal);
        $timestamp = $mtime ||= 0;
        my($netrc) = CPAN::FTP::netrc->new;
        my($netrcfile) = $netrc->netrc;
        my($verbose) = $CPAN::DEBUG{'FTP'} & $CPAN::DEBUG ? " -v" : "";
        my $targetfile = File::Basename::basename($aslocal);
        my(@dialog);
        push(
             @dialog,
             "lcd $aslocal_dir",
             "cd /",
             map("cd $_", split /\//, $dir), # RFC 1738
             "bin",
             "get $getfile $targetfile",
             "quit"
        );
        if (! $netrcfile) {
            CPAN->debug("No ~/.netrc file found") if $CPAN::DEBUG;
        } elsif ($netrc->hasdefault || $netrc->contains($host)) {
            CPAN->debug(sprintf("hasdef[%d]cont($host)[%d]",
                                $netrc->hasdefault,
                                $netrc->contains($host))) if $CPAN::DEBUG;
            if ($netrc->protected) {
                my $dialog = join "", map { "    $_\n" } @dialog;
                my $netrc_explain;
                if ($netrc->contains($host)) {
                    $netrc_explain = "Relying that your .netrc entry for '$host' ".
                        "manages the login";
                } else {
                    $netrc_explain = "Relying that your default .netrc entry ".
                        "manages the login";
                }
                $CPAN::Frontend->myprint(qq{
  Trying with external ftp to get
    $url
  $netrc_explain
  Going to send the dialog
$dialog
}
                );
                $self->talk_ftp("$ftpbin$verbose $host",
                                @dialog);
                ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
                    $atime,$mtime,$ctime,$blksize,$blocks) = stat($aslocal);
                $mtime ||= 0;
                if ($mtime > $timestamp) {
                    $CPAN::Frontend->myprint("GOT $aslocal\n");
                    $ThesiteURL = $ro_url;
                    return $aslocal;
                } else {
                    $CPAN::Frontend->myprint("Hmm... Still failed!\n");
                }
                    return if $CPAN::Signal;
            } else {
                $CPAN::Frontend->mywarn(qq{Your $netrcfile is not }.
                                        qq{correctly protected.\n});
            }
        } else {
            $CPAN::Frontend->mywarn("Your ~/.netrc neither contains $host
  nor does it have a default entry\n");
        }

        # OK, they don't have a valid ~/.netrc. Use 'ftp -n'
        # then and login manually to host, using e-mail as
        # password.
        $CPAN::Frontend->myprint(qq{Issuing "$ftpbin$verbose -n"\n});
        unshift(
                @dialog,
                "open $host",
                "user anonymous $Config::Config{'cf_email'}"
        );
        my $dialog = join "", map { "    $_\n" } @dialog;
        $CPAN::Frontend->myprint(qq{
  Trying with external ftp to get
    $url
  Going to send the dialog
$dialog
}
        );
        $self->talk_ftp("$ftpbin$verbose -n", @dialog);
        ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
            $atime,$mtime,$ctime,$blksize,$blocks) = stat($aslocal);
        $mtime ||= 0;
        if ($mtime > $timestamp) {
            $CPAN::Frontend->myprint("GOT $aslocal\n");
            $ThesiteURL = $ro_url;
            return $aslocal;
        } else {
            $CPAN::Frontend->myprint("Bad luck... Still failed!\n");
        }
        return if $CPAN::Signal;
        $CPAN::Frontend->mywarn("Can't access URL $url.\n\n");
        $CPAN::Frontend->mysleep(2);
    } # host
}

# package CPAN::FTP;
sub talk_ftp {
    my($self,$command,@dialog) = @_;
    my $fh = FileHandle->new;
    $fh->open("|$command") or die "Couldn't open ftp: $!";
    foreach (@dialog) { $fh->print("$_\n") }
    $fh->close; # Wait for process to complete
    my $wstatus = $?;
    my $estatus = $wstatus >> 8;
    $CPAN::Frontend->myprint(qq{
Subprocess "|$command"
  returned status $estatus (wstat $wstatus)
}) if $wstatus;
}

# find2perl needs modularization, too, all the following is stolen
# from there
# CPAN::FTP::ls
sub ls {
    my($self,$name) = @_;
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$sizemm,
     $atime,$mtime,$ctime,$blksize,$blocks) = lstat($name);

    my($perms,%user,%group);
    my $pname = $name;

    if ($blocks) {
        $blocks = int(($blocks + 1) / 2);
    }
    else {
        $blocks = int(($sizemm + 1023) / 1024);
    }

    if    (-f _) { $perms = '-'; }
    elsif (-d _) { $perms = 'd'; }
    elsif (-c _) { $perms = 'c'; $sizemm = &sizemm; }
    elsif (-b _) { $perms = 'b'; $sizemm = &sizemm; }
    elsif (-p _) { $perms = 'p'; }
    elsif (-S _) { $perms = 's'; }
    else         { $perms = 'l'; $pname .= ' -> ' . readlink($_); }

    my(@rwx) = ('---','--x','-w-','-wx','r--','r-x','rw-','rwx');
    my(@moname) = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
    my $tmpmode = $mode;
    my $tmp = $rwx[$tmpmode & 7];
    $tmpmode >>= 3;
    $tmp = $rwx[$tmpmode & 7] . $tmp;
    $tmpmode >>= 3;
    $tmp = $rwx[$tmpmode & 7] . $tmp;
    substr($tmp,2,1) =~ tr/-x/Ss/ if -u _;
    substr($tmp,5,1) =~ tr/-x/Ss/ if -g _;
    substr($tmp,8,1) =~ tr/-x/Tt/ if -k _;
    $perms .= $tmp;

    my $user = $user{$uid} || $uid;   # too lazy to implement lookup
    my $group = $group{$gid} || $gid;

    my($sec,$min,$hour,$mday,$mon,$year) = localtime($mtime);
    my($timeyear);
    my($moname) = $moname[$mon];
    if (-M _ > 365.25 / 2) {
        $timeyear = $year + 1900;
    }
    else {
        $timeyear = sprintf("%02d:%02d", $hour, $min);
    }

    sprintf "%5lu %4ld %-10s %2d %-8s %-8s %8s %s %2d %5s %s\n",
             $ino,
                  $blocks,
                       $perms,
                             $nlink,
                                 $user,
                                      $group,
                                           $sizemm,
                                               $moname,
                                                  $mday,
                                                      $timeyear,
                                                          $pname;
}

package CPAN::FTP::netrc;
use strict;

# package CPAN::FTP::netrc;
sub new {
    my($class) = @_;
    my $home = CPAN::HandleConfig::home;
    my $file = File::Spec->catfile($home,".netrc");

    my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
       $atime,$mtime,$ctime,$blksize,$blocks)
        = stat($file);
    $mode ||= 0;
    my $protected = 0;

    my($fh,@machines,$hasdefault);
    $hasdefault = 0;
    $fh = FileHandle->new or die "Could not create a filehandle";

    if($fh->open($file)) {
        $protected = ($mode & 077) == 0;
        local($/) = "";
      NETRC: while (<$fh>) {
            my(@tokens) = split " ", $_;
          TOKEN: while (@tokens) {
                my($t) = shift @tokens;
                if ($t eq "default") {
                    $hasdefault++;
                    last NETRC;
                }
                last TOKEN if $t eq "macdef";
                if ($t eq "machine") {
                    push @machines, shift @tokens;
                }
            }
        }
    } else {
        $file = $hasdefault = $protected = "";
    }

    bless {
        'mach' => [@machines],
        'netrc' => $file,
        'hasdefault' => $hasdefault,
        'protected' => $protected,
    }, $class;
}

# CPAN::FTP::netrc::hasdefault;
sub hasdefault { shift->{'hasdefault'} }
sub netrc      { shift->{'netrc'}      }
sub protected  { shift->{'protected'}  }
sub contains {
    my($self,$mach) = @_;
    for ( @{$self->{'mach'}} ) {
        return 1 if $_ eq $mach;
    }
    return 0;
}

package CPAN::Complete;
use strict;

sub gnu_cpl {
    my($text, $line, $start, $end) = @_;
    my(@perlret) = cpl($text, $line, $start);
    # find longest common match. Can anybody show me how to peruse
    # T::R::Gnu to have this done automatically? Seems expensive.
    return () unless @perlret;
    my($newtext) = $text;
    for (my $i = length($text)+1;;$i++) {
        last unless length($perlret[0]) && length($perlret[0]) >= $i;
        my $try = substr($perlret[0],0,$i);
        my @tries = grep {substr($_,0,$i) eq $try} @perlret;
        # warn "try[$try]tries[@tries]";
        if (@tries == @perlret) {
            $newtext = $try;
        } else {
            last;
        }
    }
    ($newtext,@perlret);
}

#-> sub CPAN::Complete::cpl ;
sub cpl {
    my($word,$line,$pos) = @_;
    $word ||= "";
    $line ||= "";
    $pos ||= 0;
    CPAN->debug("word [$word] line[$line] pos[$pos]") if $CPAN::DEBUG;
    $line =~ s/^\s*//;
    if ($line =~ s/^((?:notest|f?force)\s*)//) {
        $pos -= length($1);
    }
    my @return;
    if ($pos == 0 || $line =~ /^(?:h(?:elp)?|\?)\s/) {
        @return = grep /^\Q$word\E/, @CPAN::Complete::COMMANDS;
    } elsif ( $line !~ /^[\!abcdghimorutl]/ ) {
        @return = ();
    } elsif ($line =~ /^(a|ls)\s/) {
        @return = cplx('CPAN::Author',uc($word));
    } elsif ($line =~ /^b\s/) {
        CPAN::Shell->local_bundles;
        @return = cplx('CPAN::Bundle',$word);
    } elsif ($line =~ /^d\s/) {
        @return = cplx('CPAN::Distribution',$word);
    } elsif ($line =~ m/^(
                          [mru]|make|clean|dump|get|test|install|readme|look|cvs_import|perldoc|recent
                         )\s/x ) {
        if ($word =~ /^Bundle::/) {
            CPAN::Shell->local_bundles;
        }
        @return = (cplx('CPAN::Module',$word),cplx('CPAN::Bundle',$word));
    } elsif ($line =~ /^i\s/) {
        @return = cpl_any($word);
    } elsif ($line =~ /^reload\s/) {
        @return = cpl_reload($word,$line,$pos);
    } elsif ($line =~ /^o\s/) {
        @return = cpl_option($word,$line,$pos);
    } elsif ($line =~ m/^\S+\s/ ) {
        # fallback for future commands and what we have forgotten above
        @return = (cplx('CPAN::Module',$word),cplx('CPAN::Bundle',$word));
    } else {
        @return = ();
    }
    return @return;
}

#-> sub CPAN::Complete::cplx ;
sub cplx {
    my($class, $word) = @_;
    if (CPAN::_sqlite_running) {
        $CPAN::SQLite->search($class, "^\Q$word\E");
    }
    sort grep /^\Q$word\E/, map { $_->id } $CPAN::META->all_objects($class);
}

#-> sub CPAN::Complete::cpl_any ;
sub cpl_any {
    my($word) = shift;
    return (
            cplx('CPAN::Author',$word),
            cplx('CPAN::Bundle',$word),
            cplx('CPAN::Distribution',$word),
            cplx('CPAN::Module',$word),
           );
}

#-> sub CPAN::Complete::cpl_reload ;
sub cpl_reload {
    my($word,$line,$pos) = @_;
    $word ||= "";
    my(@words) = split " ", $line;
    CPAN->debug("word[$word] line[$line] pos[$pos]") if $CPAN::DEBUG;
    my(@ok) = qw(cpan index);
    return @ok if @words == 1;
    return grep /^\Q$word\E/, @ok if @words == 2 && $word;
}

#-> sub CPAN::Complete::cpl_option ;
sub cpl_option {
    my($word,$line,$pos) = @_;
    $word ||= "";
    my(@words) = split " ", $line;
    CPAN->debug("word[$word] line[$line] pos[$pos]") if $CPAN::DEBUG;
    my(@ok) = qw(conf debug);
    return @ok if @words == 1;
    return grep /^\Q$word\E/, @ok if @words == 2 && length($word);
    if (0) {
    } elsif ($words[1] eq 'index') {
        return ();
    } elsif ($words[1] eq 'conf') {
        return CPAN::HandleConfig::cpl(@_);
    } elsif ($words[1] eq 'debug') {
        return sort grep /^\Q$word\E/i,
            sort keys %CPAN::DEBUG, 'all';
    }
}

package CPAN::Index;
use strict;

#-> sub CPAN::Index::force_reload ;
sub force_reload {
    my($class) = @_;
    $CPAN::Index::LAST_TIME = 0;
    $class->reload(1);
}

#-> sub CPAN::Index::reload ;
sub reload {
    my($self,$force) = @_;
    my $time = time;

    # XXX check if a newer one is available. (We currently read it
    # from time to time)
    for ($CPAN::Config->{index_expire}) {
        $_ = 0.001 unless $_ && $_ > 0.001;
    }
    unless (1 || $CPAN::Have_warned->{readmetadatacache}++) {
        # debug here when CPAN doesn't seem to read the Metadata
        require Carp;
        Carp::cluck("META-PROTOCOL[$CPAN::META->{PROTOCOL}]");
    }
    unless ($CPAN::META->{PROTOCOL}) {
        $self->read_metadata_cache;
        $CPAN::META->{PROTOCOL} ||= "1.0";
    }
    if ( $CPAN::META->{PROTOCOL} < PROTOCOL  ) {
        # warn "Setting last_time to 0";
        $LAST_TIME = 0; # No warning necessary
    }
    if ($LAST_TIME + $CPAN::Config->{index_expire}*86400 > $time
        and ! $force) {
        # called too often
        # CPAN->debug("LAST_TIME[$LAST_TIME]index_expire[$CPAN::Config->{index_expire}]time[$time]");
    } elsif (0) {
        # IFF we are developing, it helps to wipe out the memory
        # between reloads, otherwise it is not what a user expects.
        undef $CPAN::META; # Neue Gruendlichkeit since v1.52(r1.274)
        $CPAN::META = CPAN->new;
    } else {
        my($debug,$t2);
        local $LAST_TIME = $time;
        local $CPAN::META->{PROTOCOL} = PROTOCOL;

        my $needshort = $^O eq "dos";

        $self->rd_authindex($self
                          ->reload_x(
                                     "authors/01mailrc.txt.gz",
                                     $needshort ?
                                     File::Spec->catfile('authors', '01mailrc.gz') :
                                     File::Spec->catfile('authors', '01mailrc.txt.gz'),
                                     $force));
        $t2 = time;
        $debug = "timing reading 01[".($t2 - $time)."]";
        $time = $t2;
        return if $CPAN::Signal; # this is sometimes lengthy
        $self->rd_modpacks($self
                         ->reload_x(
                                    "modules/02packages.details.txt.gz",
                                    $needshort ?
                                    File::Spec->catfile('modules', '02packag.gz') :
                                    File::Spec->catfile('modules', '02packages.details.txt.gz'),
                                    $force));
        $t2 = time;
        $debug .= "02[".($t2 - $time)."]";
        $time = $t2;
        return if $CPAN::Signal; # this is sometimes lengthy
        $self->rd_modlist($self
                        ->reload_x(
                                   "modules/03modlist.data.gz",
                                   $needshort ?
                                   File::Spec->catfile('modules', '03mlist.gz') :
                                   File::Spec->catfile('modules', '03modlist.data.gz'),
                                   $force));
        $self->write_metadata_cache;
        $t2 = time;
        $debug .= "03[".($t2 - $time)."]";
        $time = $t2;
        CPAN->debug($debug) if $CPAN::DEBUG;
    }
    if ($CPAN::Config->{build_dir_reuse}) {
        $self->reanimate_build_dir;
    }
    if (CPAN::_sqlite_running) {
        $CPAN::SQLite->reload(time => $time, force => $force)
            if not $LAST_TIME;
    }
    $LAST_TIME = $time;
    $CPAN::META->{PROTOCOL} = PROTOCOL;
}

#-> sub CPAN::Index::reanimate_build_dir ;
sub reanimate_build_dir {
    my($self) = @_;
    unless ($CPAN::META->has_inst($CPAN::Config->{yaml_module}||"YAML")) {
        return;
    }
    return if $HAVE_REANIMATED++;
    my $d = $CPAN::Config->{build_dir};
    my $dh = DirHandle->new;
    opendir $dh, $d or return; # does not exist
    my $dirent;
    my $i = 0;
    my $painted = 0;
    my $restored = 0;
    $CPAN::Frontend->myprint("Going to read $CPAN::Config->{build_dir}/\n");
    my @candidates = map { $_->[0] }
        sort { $b->[1] <=> $a->[1] }
            map { [ $_, -M File::Spec->catfile($d,$_) ] }
                grep {/\.yml$/} readdir $dh;
  DISTRO: for $i (0..$#candidates) {
        my $dirent = $candidates[$i];
        my $y = eval {CPAN->_yaml_loadfile(File::Spec->catfile($d,$dirent))};
        if ($@) {
            warn "Error while parsing file '$dirent'; error: '$@'";
            next DISTRO;
        }
        my $c = $y->[0];
        if ($c && CPAN->_perl_fingerprint($c->{perl})) {
            my $key = $c->{distribution}{ID};
            for my $k (keys %{$c->{distribution}}) {
                if ($c->{distribution}{$k}
                    && ref $c->{distribution}{$k}
                    && UNIVERSAL::isa($c->{distribution}{$k},"CPAN::Distrostatus")) {
                    $c->{distribution}{$k}{COMMANDID} = $i - @candidates;
                }
            }

            #we tried to restore only if element already
            #exists; but then we do not work with metadata
            #turned off.
            my $do
                = $CPAN::META->{readwrite}{'CPAN::Distribution'}{$key}
                    = $c->{distribution};
            for my $skipper (qw(
                                badtestcnt
                                configure_requires_later
                                configure_requires_later_for
                                force_update
                                later
                                later_for
                                notest
                                should_report
                                sponsored_mods
                               )) {
                delete $do->{$skipper};
            }
            # $DB::single = 1;
            if ($do->{make_test}
                && $do->{build_dir}
                && !(UNIVERSAL::can($do->{make_test},"failed") ?
                     $do->{make_test}->failed :
                     $do->{make_test} =~ /^YES/
                    )
                && (
                    !$do->{install}
                    ||
                    $do->{install}->failed
                   )
               ) {
                $CPAN::META->is_tested($do->{build_dir},$do->{make_test}{TIME});
            }
            $restored++;
        }
        $i++;
        while (($painted/76) < ($i/@candidates)) {
            $CPAN::Frontend->myprint(".");
            $painted++;
        }
    }
    $CPAN::Frontend->myprint(sprintf(
                                     "DONE\nFound %s old build%s, restored the state of %s\n",
                                     @candidates ? sprintf("%d",scalar @candidates) : "no",
                                     @candidates==1 ? "" : "s",
                                     $restored || "none",
                                    ));
}


#-> sub CPAN::Index::reload_x ;
sub reload_x {
    my($cl,$wanted,$localname,$force) = @_;
    $force |= 2; # means we're dealing with an index here
    CPAN::HandleConfig->load; # we should guarantee loading wherever
                              # we rely on Config XXX
    $localname ||= $wanted;
    my $abs_wanted = File::Spec->catfile($CPAN::Config->{'keep_source_where'},
                                         $localname);
    if (
        -f $abs_wanted &&
        -M $abs_wanted < $CPAN::Config->{'index_expire'} &&
        !($force & 1)
       ) {
        my $s = $CPAN::Config->{'index_expire'} == 1 ? "" : "s";
        $cl->debug(qq{$abs_wanted younger than $CPAN::Config->{'index_expire'} }.
                   qq{day$s. I\'ll use that.});
        return $abs_wanted;
    } else {
        $force |= 1; # means we're quite serious about it.
    }
    return CPAN::FTP->localize($wanted,$abs_wanted,$force);
}

#-> sub CPAN::Index::rd_authindex ;
sub rd_authindex {
    my($cl, $index_target) = @_;
    return unless defined $index_target;
    return if CPAN::_sqlite_running;
    my @lines;
    $CPAN::Frontend->myprint("Going to read $index_target\n");
    local(*FH);
    tie *FH, 'CPAN::Tarzip', $index_target;
    local($/) = "\n";
    local($_);
    push @lines, split /\012/ while <FH>;
    my $i = 0;
    my $painted = 0;
    foreach (@lines) {
        my($userid,$fullname,$email) =
            m/alias\s+(\S+)\s+\"([^\"\<]*)\s+\<(.*)\>\"/;
        $fullname ||= $email;
        if ($userid && $fullname && $email) {
            my $userobj = $CPAN::META->instance('CPAN::Author',$userid);
            $userobj->set('FULLNAME' => $fullname, 'EMAIL' => $email);
        } else {
            CPAN->debug(sprintf "line[%s]", $_) if $CPAN::DEBUG;
        }
        $i++;
        while (($painted/76) < ($i/@lines)) {
            $CPAN::Frontend->myprint(".");
            $painted++;
        }
        return if $CPAN::Signal;
    }
    $CPAN::Frontend->myprint("DONE\n");
}

sub userid {
  my($self,$dist) = @_;
  $dist = $self->{'id'} unless defined $dist;
  my($ret) = $dist =~ m|(?:\w/\w\w/)?([^/]+)/|;
  $ret;
}

#-> sub CPAN::Index::rd_modpacks ;
sub rd_modpacks {
    my($self, $index_target) = @_;
    return unless defined $index_target;
    return if CPAN::_sqlite_running;
    $CPAN::Frontend->myprint("Going to read $index_target\n");
    my $fh = CPAN::Tarzip->TIEHANDLE($index_target);
    local $_;
    CPAN->debug(sprintf "start[%d]", time) if $CPAN::DEBUG;
    my $slurp = "";
    my $chunk;
    while (my $bytes = $fh->READ(\$chunk,8192)) {
        $slurp.=$chunk;
    }
    my @lines = split /\012/, $slurp;
    CPAN->debug(sprintf "end[%d]", time) if $CPAN::DEBUG;
    undef $fh;
    # read header
    my($line_count,$last_updated);
    while (@lines) {
        my $shift = shift(@lines);
        last if $shift =~ /^\s*$/;
        $shift =~ /^Line-Count:\s+(\d+)/ and $line_count = $1;
        $shift =~ /^Last-Updated:\s+(.+)/ and $last_updated = $1;
    }
    CPAN->debug("line_count[$line_count]last_updated[$last_updated]") if $CPAN::DEBUG;
    if (not defined $line_count) {

        $CPAN::Frontend->mywarn(qq{Warning: Your $index_target does not contain a Line-Count header.
Please check the validity of the index file by comparing it to more
than one CPAN mirror. I'll continue but problems seem likely to
happen.\a
});

        $CPAN::Frontend->mysleep(5);
    } elsif ($line_count != scalar @lines) {

        $CPAN::Frontend->mywarn(sprintf qq{Warning: Your %s
contains a Line-Count header of %d but I see %d lines there. Please
check the validity of the index file by comparing it to more than one
CPAN mirror. I'll continue but problems seem likely to happen.\a\n},
$index_target, $line_count, scalar(@lines));

    }
    if (not defined $last_updated) {

        $CPAN::Frontend->mywarn(qq{Warning: Your $index_target does not contain a Last-Updated header.
Please check the validity of the index file by comparing it to more
than one CPAN mirror. I'll continue but problems seem likely to
happen.\a
});

        $CPAN::Frontend->mysleep(5);
    } else {

        $CPAN::Frontend
            ->myprint(sprintf qq{  Database was generated on %s\n},
                      $last_updated);
        $DATE_OF_02 = $last_updated;

        my $age = time;
        if ($CPAN::META->has_inst('HTTP::Date')) {
            require HTTP::Date;
            $age -= HTTP::Date::str2time($last_updated);
        } else {
            $CPAN::Frontend->mywarn("  HTTP::Date not available\n");
            require Time::Local;
            my(@d) = $last_updated =~ / (\d+) (\w+) (\d+) (\d+):(\d+):(\d+) /;
            $d[1] = index("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec", $d[1])/4;
            $age -= $d[1]>=0 ? Time::Local::timegm(@d[5,4,3,0,1,2]) : 0;
        }
        $age /= 3600*24;
        if ($age > 30) {

            $CPAN::Frontend
                ->mywarn(sprintf
                         qq{Warning: This index file is %d days old.
  Please check the host you chose as your CPAN mirror for staleness.
  I'll continue but problems seem likely to happen.\a\n},
                         $age);

        } elsif ($age < -1) {

            $CPAN::Frontend
                ->mywarn(sprintf
                         qq{Warning: Your system date is %d days behind this index file!
  System time:          %s
  Timestamp index file: %s
  Please fix your system time, problems with the make command expected.\n},
                         -$age,
                         scalar gmtime,
                         $DATE_OF_02,
                        );

        }
    }


    # A necessity since we have metadata_cache: delete what isn't
    # there anymore
    my $secondtime = $CPAN::META->exists("CPAN::Module","CPAN");
    CPAN->debug("secondtime[$secondtime]") if $CPAN::DEBUG;
    my(%exists);
    my $i = 0;
    my $painted = 0;
    foreach (@lines) {
        # before 1.56 we split into 3 and discarded the rest. From
        # 1.57 we assign remaining text to $comment thus allowing to
        # influence isa_perl
        my($mod,$version,$dist,$comment) = split " ", $_, 4;
        my($bundle,$id,$userid);

        if ($mod eq 'CPAN' &&
            ! (
            CPAN::Queue->exists('Bundle::CPAN') ||
            CPAN::Queue->exists('CPAN')
            )
        ) {
            local($^W)= 0;
            if ($version > $CPAN::VERSION) {
                $CPAN::Frontend->mywarn(qq{
  New CPAN.pm version (v$version) available.
  [Currently running version is v$CPAN::VERSION]
  You might want to try
    install CPAN
    reload cpan
  to both upgrade CPAN.pm and run the new version without leaving
  the current session.

}); #});
                $CPAN::Frontend->mysleep(2);
                $CPAN::Frontend->myprint(qq{\n});
            }
            last if $CPAN::Signal;
        } elsif ($mod =~ /^Bundle::(.*)/) {
            $bundle = $1;
        }

        if ($bundle) {
            $id =  $CPAN::META->instance('CPAN::Bundle',$mod);
            # Let's make it a module too, because bundles have so much
            # in common with modules.

            # Changed in 1.57_63: seems like memory bloat now without
            # any value, so commented out

            # $CPAN::META->instance('CPAN::Module',$mod);

        } else {

            # instantiate a module object
            $id = $CPAN::META->instance('CPAN::Module',$mod);

        }

        # Although CPAN prohibits same name with different version the
        # indexer may have changed the version for the same distro
        # since the last time ("Force Reindexing" feature)
        if ($id->cpan_file ne $dist
            ||
            $id->cpan_version ne $version
           ) {
            $userid = $id->userid || $self->userid($dist);
            $id->set(
                     'CPAN_USERID' => $userid,
                     'CPAN_VERSION' => $version,
                     'CPAN_FILE' => $dist,
                    );
        }

        # instantiate a distribution object
        if ($CPAN::META->exists('CPAN::Distribution',$dist)) {
        # we do not need CONTAINSMODS unless we do something with
        # this dist, so we better produce it on demand.

        ## my $obj = $CPAN::META->instance(
        ##                                 'CPAN::Distribution' => $dist
        ##                                );
        ## $obj->{CONTAINSMODS}{$mod} = undef; # experimental
        } else {
            $CPAN::META->instance(
                                  'CPAN::Distribution' => $dist
                                 )->set(
                                        'CPAN_USERID' => $userid,
                                        'CPAN_COMMENT' => $comment,
                                       );
        }
        if ($secondtime) {
            for my $name ($mod,$dist) {
                # $self->debug("exists name[$name]") if $CPAN::DEBUG;
                $exists{$name} = undef;
            }
        }
        $i++;
        while (($painted/76) < ($i/@lines)) {
            $CPAN::Frontend->myprint(".");
            $painted++;
        }
        return if $CPAN::Signal;
    }
    $CPAN::Frontend->myprint("DONE\n");
    if ($secondtime) {
        for my $class (qw(CPAN::Module CPAN::Bundle CPAN::Distribution)) {
            for my $o ($CPAN::META->all_objects($class)) {
                next if exists $exists{$o->{ID}};
                $CPAN::META->delete($class,$o->{ID});
                # CPAN->debug("deleting ID[$o->{ID}] in class[$class]")
                #     if $CPAN::DEBUG;
            }
        }
    }
}

#-> sub CPAN::Index::rd_modlist ;
sub rd_modlist {
    my($cl,$index_target) = @_;
    return unless defined $index_target;
    return if CPAN::_sqlite_running;
    $CPAN::Frontend->myprint("Going to read $index_target\n");
    my $fh = CPAN::Tarzip->TIEHANDLE($index_target);
    local $_;
    my $slurp = "";
    my $chunk;
    while (my $bytes = $fh->READ(\$chunk,8192)) {
        $slurp.=$chunk;
    }
    my @eval2 = split /\012/, $slurp;

    while (@eval2) {
        my $shift = shift(@eval2);
        if ($shift =~ /^Date:\s+(.*)/) {
            if ($DATE_OF_03 eq $1) {
                $CPAN::Frontend->myprint("Unchanged.\n");
                return;
            }
            ($DATE_OF_03) = $1;
        }
        last if $shift =~ /^\s*$/;
    }
    push @eval2, q{CPAN::Modulelist->data;};
    local($^W) = 0;
    my($comp) = Safe->new("CPAN::Safe1");
    my($eval2) = join("\n", @eval2);
    CPAN->debug(sprintf "length of eval2[%d]", length $eval2) if $CPAN::DEBUG;
    my $ret = $comp->reval($eval2);
    Carp::confess($@) if $@;
    return if $CPAN::Signal;
    my $i = 0;
    my $until = keys(%$ret);
    my $painted = 0;
    CPAN->debug(sprintf "until[%d]", $until) if $CPAN::DEBUG;
    for (keys %$ret) {
        my $obj = $CPAN::META->instance("CPAN::Module",$_);
        delete $ret->{$_}{modid}; # not needed here, maybe elsewhere
        $obj->set(%{$ret->{$_}});
        $i++;
        while (($painted/76) < ($i/$until)) {
            $CPAN::Frontend->myprint(".");
            $painted++;
        }
        return if $CPAN::Signal;
    }
    $CPAN::Frontend->myprint("DONE\n");
}

#-> sub CPAN::Index::write_metadata_cache ;
sub write_metadata_cache {
    my($self) = @_;
    return unless $CPAN::Config->{'cache_metadata'};
    return if CPAN::_sqlite_running;
    return unless $CPAN::META->has_usable("Storable");
    my $cache;
    foreach my $k (qw(CPAN::Bundle CPAN::Author CPAN::Module
                      CPAN::Distribution)) {
        $cache->{$k} = $CPAN::META->{readonly}{$k}; # unsafe meta access, ok
    }
    my $metadata_file = File::Spec->catfile($CPAN::Config->{cpan_home},"Metadata");
    $cache->{last_time} = $LAST_TIME;
    $cache->{DATE_OF_02} = $DATE_OF_02;
    $cache->{PROTOCOL} = PROTOCOL;
    $CPAN::Frontend->myprint("Going to write $metadata_file\n");
    eval { Storable::nstore($cache, $metadata_file) };
    $CPAN::Frontend->mywarn($@) if $@; # ?? missing "\n" after $@ in mywarn ??
}

#-> sub CPAN::Index::read_metadata_cache ;
sub read_metadata_cache {
    my($self) = @_;
    return unless $CPAN::Config->{'cache_metadata'};
    return if CPAN::_sqlite_running;
    return unless $CPAN::META->has_usable("Storable");
    my $metadata_file = File::Spec->catfile($CPAN::Config->{cpan_home},"Metadata");
    return unless -r $metadata_file and -f $metadata_file;
    $CPAN::Frontend->myprint("Going to read $metadata_file\n");
    my $cache;
    eval { $cache = Storable::retrieve($metadata_file) };
    $CPAN::Frontend->mywarn($@) if $@; # ?? missing "\n" after $@ in mywarn ??
    if (!$cache || !UNIVERSAL::isa($cache, 'HASH')) {
        $LAST_TIME = 0;
        return;
    }
    if (exists $cache->{PROTOCOL}) {
        if (PROTOCOL > $cache->{PROTOCOL}) {
            $CPAN::Frontend->mywarn(sprintf("Ignoring Metadata cache written ".
                                            "with protocol v%s, requiring v%s\n",
                                            $cache->{PROTOCOL},
                                            PROTOCOL)
                                   );
            return;
        }
    } else {
        $CPAN::Frontend->mywarn("Ignoring Metadata cache written ".
                                "with protocol v1.0\n");
        return;
    }
    my $clcnt = 0;
    my $idcnt = 0;
    while(my($class,$v) = each %$cache) {
        next unless $class =~ /^CPAN::/;
        $CPAN::META->{readonly}{$class} = $v; # unsafe meta access, ok
        while (my($id,$ro) = each %$v) {
            $CPAN::META->{readwrite}{$class}{$id} ||=
                $class->new(ID=>$id, RO=>$ro);
            $idcnt++;
        }
        $clcnt++;
    }
    unless ($clcnt) { # sanity check
        $CPAN::Frontend->myprint("Warning: Found no data in $metadata_file\n");
        return;
    }
    if ($idcnt < 1000) {
        $CPAN::Frontend->myprint("Warning: Found only $idcnt objects ".
                                 "in $metadata_file\n");
        return;
    }
    $CPAN::META->{PROTOCOL} ||=
        $cache->{PROTOCOL}; # reading does not up or downgrade, but it
                            # does initialize to some protocol
    $LAST_TIME = $cache->{last_time};
    $DATE_OF_02 = $cache->{DATE_OF_02};
    $CPAN::Frontend->myprint("  Database was generated on $DATE_OF_02\n")
        if defined $DATE_OF_02; # An old cache may not contain DATE_OF_02
    return;
}

package CPAN::InfoObj;
use strict;

sub ro {
    my $self = shift;
    exists $self->{RO} and return $self->{RO};
}

#-> sub CPAN::InfoObj::cpan_userid
sub cpan_userid {
    my $self = shift;
    my $ro = $self->ro;
    if ($ro) {
        return $ro->{CPAN_USERID} || "N/A";
    } else {
        $self->debug("ID[$self->{ID}]");
        # N/A for bundles found locally
        return "N/A";
    }
}

sub id { shift->{ID}; }

#-> sub CPAN::InfoObj::new ;
sub new {
    my $this = bless {}, shift;
    %$this = @_;
    $this
}

# The set method may only be used by code that reads index data or
# otherwise "objective" data from the outside world. All session
# related material may do anything else with instance variables but
# must not touch the hash under the RO attribute. The reason is that
# the RO hash gets written to Metadata file and is thus persistent.

#-> sub CPAN::InfoObj::safe_chdir ;
sub safe_chdir {
  my($self,$todir) = @_;
  # we die if we cannot chdir and we are debuggable
  Carp::confess("safe_chdir called without todir argument")
        unless defined $todir and length $todir;
  if (chdir $todir) {
    $self->debug(sprintf "changed directory to %s", CPAN::anycwd())
        if $CPAN::DEBUG;
  } else {
    if (-e $todir) {
        unless (-x $todir) {
            unless (chmod 0755, $todir) {
                my $cwd = CPAN::anycwd();
                $CPAN::Frontend->mywarn("I have neither the -x permission nor the ".
                                        "permission to change the permission; cannot ".
                                        "chdir to '$todir'\n");
                $CPAN::Frontend->mysleep(5);
                $CPAN::Frontend->mydie(qq{Could not chdir from cwd[$cwd] }.
                                       qq{to todir[$todir]: $!});
            }
        }
    } else {
        $CPAN::Frontend->mydie("Directory '$todir' has gone. Cannot continue.\n");
    }
    if (chdir $todir) {
      $self->debug(sprintf "changed directory to %s", CPAN::anycwd())
          if $CPAN::DEBUG;
    } else {
      my $cwd = CPAN::anycwd();
      $CPAN::Frontend->mydie(qq{Could not chdir from cwd[$cwd] }.
                             qq{to todir[$todir] (a chmod has been issued): $!});
    }
  }
}

#-> sub CPAN::InfoObj::set ;
sub set {
    my($self,%att) = @_;
    my $class = ref $self;

    # This must be ||=, not ||, because only if we write an empty
    # reference, only then the set method will write into the readonly
    # area. But for Distributions that spring into existence, maybe
    # because of a typo, we do not like it that they are written into
    # the readonly area and made permanent (at least for a while) and
    # that is why we do not "allow" other places to call ->set.
    unless ($self->id) {
        CPAN->debug("Bug? Empty ID, rejecting");
        return;
    }
    my $ro = $self->{RO} =
        $CPAN::META->{readonly}{$class}{$self->id} ||= {};

    while (my($k,$v) = each %att) {
        $ro->{$k} = $v;
    }
}

#-> sub CPAN::InfoObj::as_glimpse ;
sub as_glimpse {
    my($self) = @_;
    my(@m);
    my $class = ref($self);
    $class =~ s/^CPAN:://;
    my $id = $self->can("pretty_id") ? $self->pretty_id : $self->{ID};
    push @m, sprintf "%-15s %s\n", $class, $id;
    join "", @m;
}

#-> sub CPAN::InfoObj::as_string ;
sub as_string {
    my($self) = @_;
    my(@m);
    my $class = ref($self);
    $class =~ s/^CPAN:://;
    push @m, $class, " id = $self->{ID}\n";
    my $ro;
    unless ($ro = $self->ro) {
        if (substr($self->{ID},-1,1) eq ".") { # directory
            $ro = +{};
        } else {
            $CPAN::Frontend->mywarn("Unknown object $self->{ID}\n");
            $CPAN::Frontend->mysleep(5);
            return;
        }
    }
    for (sort keys %$ro) {
        # next if m/^(ID|RO)$/;
        my $extra = "";
        if ($_ eq "CPAN_USERID") {
            $extra .= " (";
            $extra .= $self->fullname;
            my $email; # old perls!
            if ($email = $CPAN::META->instance("CPAN::Author",
                                               $self->cpan_userid
                                              )->email) {
                $extra .= " <$email>";
            } else {
                $extra .= " <no email>";
            }
            $extra .= ")";
        } elsif ($_ eq "FULLNAME") { # potential UTF-8 conversion
            push @m, sprintf "    %-12s %s\n", $_, $self->fullname;
            next;
        }
        next unless defined $ro->{$_};
        push @m, sprintf "    %-12s %s%s\n", $_, $ro->{$_}, $extra;
    }
  KEY: for (sort keys %$self) {
        next if m/^(ID|RO)$/;
        unless (defined $self->{$_}) {
            delete $self->{$_};
            next KEY;
        }
        if (ref($self->{$_}) eq "ARRAY") {
            push @m, sprintf "    %-12s %s\n", $_, "@{$self->{$_}}";
        } elsif (ref($self->{$_}) eq "HASH") {
            my $value;
            if (/^CONTAINSMODS$/) {
                $value = join(" ",sort keys %{$self->{$_}});
            } elsif (/^prereq_pm$/) {
                my @value;
                my $v = $self->{$_};
                for my $x (sort keys %$v) {
                    my @svalue;
                    for my $y (sort keys %{$v->{$x}}) {
                        push @svalue, "$y=>$v->{$x}{$y}";
                    }
                    push @value, "$x\:" . join ",", @svalue if @svalue;
                }
                $value = join ";", @value;
            } else {
                $value = $self->{$_};
            }
            push @m, sprintf(
                             "    %-12s %s\n",
                             $_,
                             $value,
                            );
        } else {
            push @m, sprintf "    %-12s %s\n", $_, $self->{$_};
        }
    }
    join "", @m, "\n";
}

#-> sub CPAN::InfoObj::fullname ;
sub fullname {
    my($self) = @_;
    $CPAN::META->instance("CPAN::Author",$self->cpan_userid)->fullname;
}

#-> sub CPAN::InfoObj::dump ;
sub dump {
    my($self, $what) = @_;
    unless ($CPAN::META->has_inst("Data::Dumper")) {
        $CPAN::Frontend->mydie("dump command requires Data::Dumper installed");
    }
    local $Data::Dumper::Sortkeys;
    $Data::Dumper::Sortkeys = 1;
    my $out = Data::Dumper::Dumper($what ? eval $what : $self);
    if (length $out > 100000) {
        my $fh_pager = FileHandle->new;
        local($SIG{PIPE}) = "IGNORE";
        my $pager = $CPAN::Config->{'pager'} || "cat";
        $fh_pager->open("|$pager")
            or die "Could not open pager $pager\: $!";
        $fh_pager->print($out);
        close $fh_pager;
    } else {
        $CPAN::Frontend->myprint($out);
    }
}

package CPAN::Author;
use strict;

#-> sub CPAN::Author::force
sub force {
    my $self = shift;
    $self->{force}++;
}

#-> sub CPAN::Author::force
sub unforce {
    my $self = shift;
    delete $self->{force};
}

#-> sub CPAN::Author::id
sub id {
    my $self = shift;
    my $id = $self->{ID};
    $CPAN::Frontend->mydie("Illegal author id[$id]") unless $id =~ /^[A-Z]/;
    $id;
}

#-> sub CPAN::Author::as_glimpse ;
sub as_glimpse {
    my($self) = @_;
    my(@m);
    my $class = ref($self);
    $class =~ s/^CPAN:://;
    push @m, sprintf(qq{%-15s %s ("%s" <%s>)\n},
                     $class,
                     $self->{ID},
                     $self->fullname,
                     $self->email);
    join "", @m;
}

#-> sub CPAN::Author::fullname ;
sub fullname {
    shift->ro->{FULLNAME};
}
*name = \&fullname;

#-> sub CPAN::Author::email ;
sub email    { shift->ro->{EMAIL}; }

#-> sub CPAN::Author::ls ;
sub ls {
    my $self = shift;
    my $glob = shift || "";
    my $silent = shift || 0;
    my $id = $self->id;

    # adapted from CPAN::Distribution::verifyCHECKSUM ;
    my(@csf); # chksumfile
    @csf = $self->id =~ /(.)(.)(.*)/;
    $csf[1] = join "", @csf[0,1];
    $csf[2] = join "", @csf[1,2]; # ("A","AN","ANDK")
    my(@dl);
    @dl = $self->dir_listing([$csf[0],"CHECKSUMS"], 0, 1);
    unless (grep {$_->[2] eq $csf[1]} @dl) {
        $CPAN::Frontend->myprint("Directory $csf[1]/ does not exist\n") unless $silent ;
        return;
    }
    @dl = $self->dir_listing([@csf[0,1],"CHECKSUMS"], 0, 1);
    unless (grep {$_->[2] eq $csf[2]} @dl) {
        $CPAN::Frontend->myprint("Directory $id/ does not exist\n") unless $silent;
        return;
    }
    @dl = $self->dir_listing([@csf,"CHECKSUMS"], 1, 1);
    if ($glob) {
        if ($CPAN::META->has_inst("Text::Glob")) {
            my $rglob = Text::Glob::glob_to_regex($glob);
            @dl = grep { $_->[2] =~ /$rglob/ } @dl;
        } else {
            $CPAN::Frontend->mydie("Text::Glob not installed, cannot proceed");
        }
    }
    unless ($silent >= 2) {
        $CPAN::Frontend->myprint(join "", map {
            sprintf("%8d %10s %s/%s\n", $_->[0], $_->[1], $id, $_->[2])
        } sort { $a->[2] cmp $b->[2] } @dl);
    }
    @dl;
}

# returns an array of arrays, the latter contain (size,mtime,filename)
#-> sub CPAN::Author::dir_listing ;
sub dir_listing {
    my $self = shift;
    my $chksumfile = shift;
    my $recursive = shift;
    my $may_ftp = shift;

    my $lc_want =
        File::Spec->catfile($CPAN::Config->{keep_source_where},
                            "authors", "id", @$chksumfile);

    my $fh;

    # Purge and refetch old (pre-PGP) CHECKSUMS; they are a security
    # hazard.  (Without GPG installed they are not that much better,
    # though.)
    $fh = FileHandle->new;
    if (open($fh, $lc_want)) {
        my $line = <$fh>; close $fh;
        unlink($lc_want) unless $line =~ /PGP/;
    }

    local($") = "/";
    # connect "force" argument with "index_expire".
    my $force = $self->{force};
    if (my @stat = stat $lc_want) {
        $force ||= $stat[9] + $CPAN::Config->{index_expire}*86400 <= time;
    }
    my $lc_file;
    if ($may_ftp) {
        $lc_file = CPAN::FTP->localize(
                                       "authors/id/@$chksumfile",
                                       $lc_want,
                                       $force,
                                      );
        unless ($lc_file) {
            $CPAN::Frontend->myprint("Trying $lc_want.gz\n");
            $chksumfile->[-1] .= ".gz";
            $lc_file = CPAN::FTP->localize("authors/id/@$chksumfile",
                                           "$lc_want.gz",1);
            if ($lc_file) {
                $lc_file =~ s{\.gz(?!\n)\Z}{}; #};
                eval{CPAN::Tarzip->new("$lc_file.gz")->gunzip($lc_file)};
            } else {
                return;
            }
        }
    } else {
        $lc_file = $lc_want;
        # we *could* second-guess and if the user has a file: URL,
        # then we could look there. But on the other hand, if they do
        # have a file: URL, wy did they choose to set
        # $CPAN::Config->{show_upload_date} to false?
    }

    # adapted from CPAN::Distribution::CHECKSUM_check_file ;
    $fh = FileHandle->new;
    my($cksum);
    if (open $fh, $lc_file) {
        local($/);
        my $eval = <$fh>;
        $eval =~ s/\015?\012/\n/g;
        close $fh;
        my($comp) = Safe->new();
        $cksum = $comp->reval($eval);
        if ($@) {
            rename $lc_file, "$lc_file.bad";
            Carp::confess($@) if $@;
        }
    } elsif ($may_ftp) {
        Carp::carp "Could not open '$lc_file' for reading.";
    } else {
        # Maybe should warn: "You may want to set show_upload_date to a true value"
        return;
    }
    my(@result,$f);
    for $f (sort keys %$cksum) {
        if (exists $cksum->{$f}{isdir}) {
            if ($recursive) {
                my(@dir) = @$chksumfile;
                pop @dir;
                push @dir, $f, "CHECKSUMS";
                push @result, map {
                    [$_->[0], $_->[1], "$f/$_->[2]"]
                } $self->dir_listing(\@dir,1,$may_ftp);
            } else {
                push @result, [ 0, "-", $f ];
            }
        } else {
            push @result, [
                           ($cksum->{$f}{"size"}||0),
                           $cksum->{$f}{"mtime"}||"---",
                           $f
                          ];
        }
    }
    @result;
}

#-> sub CPAN::Author::reports
sub reports {
    $CPAN::Frontend->mywarn("reports on authors not implemented.
Please file a bugreport if you need this.\n");
}

package CPAN::Distribution;
use strict;

# Accessors
sub cpan_comment {
    my $self = shift;
    my $ro = $self->ro or return;
    $ro->{CPAN_COMMENT}
}

#-> CPAN::Distribution::undelay
sub undelay {
    my $self = shift;
    for my $delayer (
                     "configure_requires_later",
                     "configure_requires_later_for",
                     "later",
                     "later_for",
                    ) {
        delete $self->{$delayer};
    }
}

#-> CPAN::Distribution::is_dot_dist
sub is_dot_dist {
    my($self) = @_;
    return substr($self->id,-1,1) eq ".";
}

# add the A/AN/ stuff
#-> CPAN::Distribution::normalize
sub normalize {
    my($self,$s) = @_;
    $s = $self->id unless defined $s;
    if (substr($s,-1,1) eq ".") {
        # using a global because we are sometimes called as static method
        if (!$CPAN::META->{LOCK}
            && !$CPAN::Have_warned->{"$s is unlocked"}++
           ) {
            $CPAN::Frontend->mywarn("You are visiting the local directory
  '$s'
  without lock, take care that concurrent processes do not do likewise.\n");
            $CPAN::Frontend->mysleep(1);
        }
        if ($s eq ".") {
            $s = "$CPAN::iCwd/.";
        } elsif (File::Spec->file_name_is_absolute($s)) {
        } elsif (File::Spec->can("rel2abs")) {
            $s = File::Spec->rel2abs($s);
        } else {
            $CPAN::Frontend->mydie("Your File::Spec is too old, please upgrade File::Spec");
        }
        CPAN->debug("s[$s]") if $CPAN::DEBUG;
        unless ($CPAN::META->exists("CPAN::Distribution", $s)) {
            for ($CPAN::META->instance("CPAN::Distribution", $s)) {
                $_->{build_dir} = $s;
                $_->{archived} = "local_directory";
                $_->{unwrapped} = CPAN::Distrostatus->new("YES -- local_directory");
            }
        }
    } elsif (
        $s =~ tr|/|| == 1
        or
        $s !~ m|[A-Z]/[A-Z-]{2}/[A-Z-]{2,}/|
       ) {
        return $s if $s =~ m:^N/A|^Contact Author: ;
        $s =~ s|^(.)(.)([^/]*/)(.+)$|$1/$1$2/$1$2$3$4| or
            $CPAN::Frontend->mywarn("Strange distribution name [$s]\n");
        CPAN->debug("s[$s]") if $CPAN::DEBUG;
    }
    $s;
}

#-> sub CPAN::Distribution::author ;
sub author {
    my($self) = @_;
    my($authorid);
    if (substr($self->id,-1,1) eq ".") {
        $authorid = "LOCAL";
    } else {
        ($authorid) = $self->pretty_id =~ /^([\w\-]+)/;
    }
    CPAN::Shell->expand("Author",$authorid);
}

# tries to get the yaml from CPAN instead of the distro itself:
# EXPERIMENTAL, UNDOCUMENTED AND UNTESTED, for Tels
sub fast_yaml {
    my($self) = @_;
    my $meta = $self->pretty_id;
    $meta =~ s/\.(tar.gz|tgz|zip|tar.bz2)/.meta/;
    my(@ls) = CPAN::Shell->globls($meta);
    my $norm = $self->normalize($meta);

    my($local_file);
    my($local_wanted) =
        File::Spec->catfile(
                            $CPAN::Config->{keep_source_where},
                            "authors",
                            "id",
                            split(/\//,$norm)
                           );
    $self->debug("Doing localize") if $CPAN::DEBUG;
    unless ($local_file =
            CPAN::FTP->localize("authors/id/$norm",
                                $local_wanted)) {
        $CPAN::Frontend->mydie("Giving up on downloading yaml file '$local_wanted'\n");
    }
    my $yaml = CPAN->_yaml_loadfile($local_file)->[0];
}

#-> sub CPAN::Distribution::cpan_userid
sub cpan_userid {
    my $self = shift;
    if ($self->{ID} =~ m{[A-Z]/[A-Z\-]{2}/([A-Z\-]+)/}) {
        return $1;
    }
    return $self->SUPER::cpan_userid;
}

#-> sub CPAN::Distribution::pretty_id
sub pretty_id {
    my $self = shift;
    my $id = $self->id;
    return $id unless $id =~ m|^./../|;
    substr($id,5);
}

#-> sub CPAN::Distribution::base_id
sub base_id {
    my $self = shift;
    my $id = $self->pretty_id();
    my $base_id = File::Basename::basename($id);
    $base_id =~ s{\.(?:tar\.(bz2|gz|Z)|t(?:gz|bz)|zip)$}{}i;
    return $base_id;
}

# mark as dirty/clean for the sake of recursion detection. $color=1
# means "in use", $color=0 means "not in use anymore". $color=2 means
# we have determined prereqs now and thus insist on passing this
# through (at least) once again.

#-> sub CPAN::Distribution::color_cmd_tmps ;
sub color_cmd_tmps {
    my($self) = shift;
    my($depth) = shift || 0;
    my($color) = shift || 0;
    my($ancestors) = shift || [];
    # a distribution needs to recurse into its prereq_pms

    return if exists $self->{incommandcolor}
        && $color==1
        && $self->{incommandcolor}==$color;
    if ($depth>=$CPAN::MAX_RECURSION) {
        die(CPAN::Exception::RecursiveDependency->new($ancestors));
    }
    # warn "color_cmd_tmps $depth $color " . $self->id; # sleep 1;
    my $prereq_pm = $self->prereq_pm;
    if (defined $prereq_pm) {
      PREREQ: for my $pre (keys %{$prereq_pm->{requires}||{}},
                           keys %{$prereq_pm->{build_requires}||{}}) {
            next PREREQ if $pre eq "perl";
            my $premo;
            unless ($premo = CPAN::Shell->expand("Module",$pre)) {
                $CPAN::Frontend->mywarn("prerequisite module[$pre] not known\n");
                $CPAN::Frontend->mysleep(2);
                next PREREQ;
            }
            $premo->color_cmd_tmps($depth+1,$color,[@$ancestors, $self->id]);
        }
    }
    if ($color==0) {
        delete $self->{sponsored_mods};

        # as we are at the end of a command, we'll give up this
        # reminder of a broken test. Other commands may test this guy
        # again. Maybe 'badtestcnt' should be renamed to
        # 'make_test_failed_within_command'?
        delete $self->{badtestcnt};
    }
    $self->{incommandcolor} = $color;
}

#-> sub CPAN::Distribution::as_string ;
sub as_string {
    my $self = shift;
    $self->containsmods;
    $self->upload_date;
    $self->SUPER::as_string(@_);
}

#-> sub CPAN::Distribution::containsmods ;
sub containsmods {
    my $self = shift;
    return keys %{$self->{CONTAINSMODS}} if exists $self->{CONTAINSMODS};
    my $dist_id = $self->{ID};
    for my $mod ($CPAN::META->all_objects("CPAN::Module")) {
        my $mod_file = $mod->cpan_file or next;
        my $mod_id = $mod->{ID} or next;
        # warn "mod_file[$mod_file] dist_id[$dist_id] mod_id[$mod_id]";
        # sleep 1;
        if ($CPAN::Signal) {
            delete $self->{CONTAINSMODS};
            return;
        }
        $self->{CONTAINSMODS}{$mod_id} = undef if $mod_file eq $dist_id;
    }
    keys %{$self->{CONTAINSMODS}||={}};
}

#-> sub CPAN::Distribution::upload_date ;
sub upload_date {
    my $self = shift;
    return $self->{UPLOAD_DATE} if exists $self->{UPLOAD_DATE};
    my(@local_wanted) = split(/\//,$self->id);
    my $filename = pop @local_wanted;
    push @local_wanted, "CHECKSUMS";
    my $author = CPAN::Shell->expand("Author",$self->cpan_userid);
    return unless $author;
    my @dl = $author->dir_listing(\@local_wanted,0,$CPAN::Config->{show_upload_date});
    return unless @dl;
    my($dirent) = grep { $_->[2] eq $filename } @dl;
    # warn sprintf "dirent[%s]id[%s]", $dirent, $self->id;
    return unless $dirent->[1];
    return $self->{UPLOAD_DATE} = $dirent->[1];
}

#-> sub CPAN::Distribution::uptodate ;
sub uptodate {
    my($self) = @_;
    my $c;
    foreach $c ($self->containsmods) {
        my $obj = CPAN::Shell->expandany($c);
        unless ($obj->uptodate) {
            my $id = $self->pretty_id;
            $self->debug("$id not uptodate due to $c") if $CPAN::DEBUG;
            return 0;
        }
    }
    return 1;
}

#-> sub CPAN::Distribution::called_for ;
sub called_for {
    my($self,$id) = @_;
    $self->{CALLED_FOR} = $id if defined $id;
    return $self->{CALLED_FOR};
}

#-> sub CPAN::Distribution::get ;
sub get {
    my($self) = @_;
    $self->debug("checking goto id[$self->{ID}]") if $CPAN::DEBUG;
    if (my $goto = $self->prefs->{goto}) {
        $CPAN::Frontend->mywarn
            (sprintf(
                     "delegating to '%s' as specified in prefs file '%s' doc %d\n",
                     $goto,
                     $self->{prefs_file},
                     $self->{prefs_file_doc},
                    ));
        return $self->goto($goto);
    }
    local $ENV{PERL5LIB} = defined($ENV{PERL5LIB})
                           ? $ENV{PERL5LIB}
                           : ($ENV{PERLLIB} || "");

    $CPAN::META->set_perl5lib;
    local $ENV{MAKEFLAGS}; # protect us from outer make calls

  EXCUSE: {
        my @e;
        my $goodbye_message;
        $self->debug("checking disabled id[$self->{ID}]") if $CPAN::DEBUG;
        if ($self->prefs->{disabled}) {
            my $why = sprintf(
                              "Disabled via prefs file '%s' doc %d",
                              $self->{prefs_file},
                              $self->{prefs_file_doc},
                             );
            push @e, $why;
            $self->{unwrapped} = CPAN::Distrostatus->new("NO $why");
            $goodbye_message = "[disabled] -- NA $why";
            # note: not intended to be persistent but at least visible
            # during this session
        } else {
            if (exists $self->{build_dir} && -d $self->{build_dir}
                && ($self->{modulebuild}||$self->{writemakefile})
               ) {
                # this deserves print, not warn:
                $CPAN::Frontend->myprint("  Has already been unwrapped into directory ".
                                         "$self->{build_dir}\n"
                                        );
                return 1;
            }

            # although we talk about 'force' we shall not test on
            # force directly. New model of force tries to refrain from
            # direct checking of force.
            exists $self->{unwrapped} and (
                                           UNIVERSAL::can($self->{unwrapped},"failed") ?
                                           $self->{unwrapped}->failed :
                                           $self->{unwrapped} =~ /^NO/
                                          )
                and push @e, "Unwrapping had some problem, won't try again without force";
        }
        if (@e) {
            $CPAN::Frontend->mywarn(join "", map {"$_\n"} @e);
            if ($goodbye_message) {
                 $self->goodbye($goodbye_message);
            }
            return;
        }
    }
    my $sub_wd = CPAN::anycwd(); # for cleaning up as good as possible

    my($local_file);
    unless ($self->{build_dir} && -d $self->{build_dir}) {
        $self->get_file_onto_local_disk;
        return if $CPAN::Signal;
        $self->check_integrity;
        return if $CPAN::Signal;
        (my $packagedir,$local_file) = $self->run_preps_on_packagedir;
        $packagedir ||= $self->{build_dir};
        $self->{build_dir} = $packagedir;
    }

    if ($CPAN::Signal) {
        $self->safe_chdir($sub_wd);
        return;
    }
    return $self->run_MM_or_MB($local_file);
}

#-> CPAN::Distribution::get_file_onto_local_disk
sub get_file_onto_local_disk {
    my($self) = @_;

    return if $self->is_dot_dist;
    my($local_file);
    my($local_wanted) =
        File::Spec->catfile(
                            $CPAN::Config->{keep_source_where},
                            "authors",
                            "id",
                            split(/\//,$self->id)
                           );

    $self->debug("Doing localize") if $CPAN::DEBUG;
    unless ($local_file =
            CPAN::FTP->localize("authors/id/$self->{ID}",
                                $local_wanted)) {
        my $note = "";
        if ($CPAN::Index::DATE_OF_02) {
            $note = "Note: Current database in memory was generated ".
                "on $CPAN::Index::DATE_OF_02\n";
        }
        $CPAN::Frontend->mydie("Giving up on '$local_wanted'\n$note");
    }

    $self->debug("local_wanted[$local_wanted]local_file[$local_file]") if $CPAN::DEBUG;
    $self->{localfile} = $local_file;
}


#-> CPAN::Distribution::check_integrity
sub check_integrity {
    my($self) = @_;

    return if $self->is_dot_dist;
    if ($CPAN::META->has_inst("Digest::SHA")) {
        $self->debug("Digest::SHA is installed, verifying");
        $self->verifyCHECKSUM;
    } else {
        $self->debug("Digest::SHA is NOT installed");
    }
}

#-> CPAN::Distribution::run_preps_on_packagedir
sub run_preps_on_packagedir {
    my($self) = @_;
    return if $self->is_dot_dist;

    $CPAN::META->{cachemgr} ||= CPAN::CacheMgr->new(); # unsafe meta access, ok
    my $builddir = $CPAN::META->{cachemgr}->dir; # unsafe meta access, ok
    $self->safe_chdir($builddir);
    $self->debug("Removing tmp-$$") if $CPAN::DEBUG;
    File::Path::rmtree("tmp-$$");
    unless (mkdir "tmp-$$", 0755) {
        $CPAN::Frontend->unrecoverable_error(<<EOF);
Couldn't mkdir '$builddir/tmp-$$': $!

Cannot continue: Please find the reason why I cannot make the
directory
$builddir/tmp-$$
and fix the problem, then retry.

EOF
    }
    if ($CPAN::Signal) {
        return;
    }
    $self->safe_chdir("tmp-$$");

    #
    # Unpack the goods
    #
    my $local_file = $self->{localfile};
    my $ct = eval{CPAN::Tarzip->new($local_file)};
    unless ($ct) {
        $self->{unwrapped} = CPAN::Distrostatus->new("NO");
        delete $self->{build_dir};
        return;
    }
    if ($local_file =~ /(\.tar\.(bz2|gz|Z)|\.tgz)(?!\n)\Z/i) {
        $self->{was_uncompressed}++ unless eval{$ct->gtest()};
        $self->untar_me($ct);
    } elsif ( $local_file =~ /\.zip(?!\n)\Z/i ) {
        $self->unzip_me($ct);
    } else {
        $self->{was_uncompressed}++ unless $ct->gtest();
        $local_file = $self->handle_singlefile($local_file);
    }

    # we are still in the tmp directory!
    # Let's check if the package has its own directory.
    my $dh = DirHandle->new(File::Spec->curdir)
        or Carp::croak("Couldn't opendir .: $!");
    my @readdir = grep $_ !~ /^\.\.?(?!\n)\Z/s, $dh->read; ### MAC??
    $dh->close;
    my ($packagedir);
    # XXX here we want in each branch File::Temp to protect all build_dir directories
    if (CPAN->has_usable("File::Temp")) {
        my $tdir_base;
        my $from_dir;
        my @dirents;
        if (@readdir == 1 && -d $readdir[0]) {
            $tdir_base = $readdir[0];
            $from_dir = File::Spec->catdir(File::Spec->curdir,$readdir[0]);
            my $dh2 = DirHandle->new($from_dir)
                or Carp::croak("Couldn't opendir $from_dir: $!");
            @dirents = grep $_ !~ /^\.\.?(?!\n)\Z/s, $dh2->read; ### MAC??
        } else {
            my $userid = $self->cpan_userid;
            CPAN->debug("userid[$userid]");
            if (!$userid or $userid eq "N/A") {
                $userid = "anon";
            }
            $tdir_base = $userid;
            $from_dir = File::Spec->curdir;
            @dirents = @readdir;
        }
        $packagedir = File::Temp::tempdir(
                                          "$tdir_base-XXXXXX",
                                          DIR => $builddir,
                                          CLEANUP => 0,
                                         );
        my $f;
        for $f (@dirents) { # is already without "." and ".."
            my $from = File::Spec->catdir($from_dir,$f);
            my $to = File::Spec->catdir($packagedir,$f);
            unless (File::Copy::move($from,$to)) {
                my $err = $!;
                $from = File::Spec->rel2abs($from);
                Carp::confess("Couldn't move $from to $to: $err");
            }
        }
    } else { # older code below, still better than nothing when there is no File::Temp
        my($distdir);
        if (@readdir == 1 && -d $readdir[0]) {
            $distdir = $readdir[0];
            $packagedir = File::Spec->catdir($builddir,$distdir);
            $self->debug("packagedir[$packagedir]builddir[$builddir]distdir[$distdir]")
                if $CPAN::DEBUG;
            -d $packagedir and $CPAN::Frontend->myprint("Removing previously used ".
                                                        "$packagedir\n");
            File::Path::rmtree($packagedir);
            unless (File::Copy::move($distdir,$packagedir)) {
                $CPAN::Frontend->unrecoverable_error(<<EOF);
Couldn't move '$distdir' to '$packagedir': $!

Cannot continue: Please find the reason why I cannot move
$builddir/tmp-$$/$distdir
to
$packagedir
and fix the problem, then retry

EOF
            }
            $self->debug(sprintf("moved distdir[%s] to packagedir[%s] -e[%s]-d[%s]",
                                 $distdir,
                                 $packagedir,
                                 -e $packagedir,
                                 -d $packagedir,
                                )) if $CPAN::DEBUG;
        } else {
            my $userid = $self->cpan_userid;
            CPAN->debug("userid[$userid]") if $CPAN::DEBUG;
            if (!$userid or $userid eq "N/A") {
                $userid = "anon";
            }
            my $pragmatic_dir = $userid . '000';
            $pragmatic_dir =~ s/\W_//g;
            $pragmatic_dir++ while -d "../$pragmatic_dir";
            $packagedir = File::Spec->catdir($builddir,$pragmatic_dir);
            $self->debug("packagedir[$packagedir]") if $CPAN::DEBUG;
            File::Path::mkpath($packagedir);
            my($f);
            for $f (@readdir) { # is already without "." and ".."
                my $to = File::Spec->catdir($packagedir,$f);
                File::Copy::move($f,$to) or Carp::confess("Couldn't move $f to $to: $!");
            }
        }
    }
    $self->{build_dir} = $packagedir;
    $self->safe_chdir($builddir);
    File::Path::rmtree("tmp-$$");

    $self->safe_chdir($packagedir);
    $self->_signature_business();
    $self->safe_chdir($builddir);

    return($packagedir,$local_file);
}

#-> sub CPAN::Distribution::parse_meta_yml ;
sub parse_meta_yml {
    my($self) = @_;
    my $build_dir = $self->{build_dir} or die "PANIC: cannot parse yaml without a build_dir";
    my $yaml = File::Spec->catfile($build_dir,"META.yml");
    $self->debug("yaml[$yaml]") if $CPAN::DEBUG;
    return unless -f $yaml;
    my $early_yaml;
    eval {
        require Parse::Metayaml; # hypothetical
        $early_yaml = Parse::Metayaml::LoadFile($yaml)->[0];
    };
    unless ($early_yaml) {
        eval { $early_yaml = CPAN->_yaml_loadfile($yaml)->[0]; };
    }
    unless ($early_yaml) {
        return;
    }
    return $early_yaml;
}

#-> sub CPAN::Distribution::satisfy_configure_requires ;
sub satisfy_configure_requires {
    my($self) = @_;
    my $enable_configure_requires = 1;
    if (!$enable_configure_requires) {
        return 1;
        # if we return 1 here, everything is as before we introduced
        # configure_requires that means, things with
        # configure_requires simply fail, all others succeed
    }
    my @prereq = $self->unsat_prereq("configure_requires_later") or return 1;
    if ($self->{configure_requires_later}) {
        for my $k (keys %{$self->{configure_requires_later_for}||{}}) {
            if ($self->{configure_requires_later_for}{$k}>1) {
                # we must not come here a second time
                $CPAN::Frontend->mywarn("Panic: Some prerequisites is not available, please investigate...");
                require YAML::Syck;
                $CPAN::Frontend->mydie
                    (
                     YAML::Syck::Dump
                     ({self=>$self, prereq=>\@prereq})
                    );
            }
        }
    }
    if ($prereq[0][0] eq "perl") {
        my $need = "requires perl '$prereq[0][1]'";
        my $id = $self->pretty_id;
        $CPAN::Frontend->mywarn("$id $need; you have only $]; giving up\n");
        $self->{make} = CPAN::Distrostatus->new("NO $need");
        $self->store_persistent_state;
        return $self->goodbye("[prereq] -- NOT OK");
    } else {
        my $follow = eval {
            $self->follow_prereqs("configure_requires_later", @prereq);
        };
        if (0) {
        } elsif ($follow) {
            return;
        } elsif ($@ && ref $@ && $@->isa("CPAN::Exception::RecursiveDependency")) {
            $CPAN::Frontend->mywarn($@);
            return $self->goodbye("[depend] -- NOT OK");
        }
    }
    die "never reached";
}

#-> sub CPAN::Distribution::run_MM_or_MB ;
sub run_MM_or_MB {
    my($self,$local_file) = @_;
    $self->satisfy_configure_requires() or return;
    my($mpl) = File::Spec->catfile($self->{build_dir},"Makefile.PL");
    my($mpl_exists) = -f $mpl;
    unless ($mpl_exists) {
        # NFS has been reported to have racing problems after the
        # renaming of a directory in some environments.
        # This trick helps.
        $CPAN::Frontend->mysleep(1);
        my $mpldh = DirHandle->new($self->{build_dir})
            or Carp::croak("Couldn't opendir $self->{build_dir}: $!");
        $mpl_exists = grep /^Makefile\.PL$/, $mpldh->read;
        $mpldh->close;
    }
    my $prefer_installer = "eumm"; # eumm|mb
    if (-f File::Spec->catfile($self->{build_dir},"Build.PL")) {
        if ($mpl_exists) { # they *can* choose
            if ($CPAN::META->has_inst("Module::Build")) {
                $prefer_installer = CPAN::HandleConfig->prefs_lookup($self,
                                                                     q{prefer_installer});
            }
        } else {
            $prefer_installer = "mb";
        }
    }
    return unless $self->patch;
    if (lc($prefer_installer) eq "rand") {
        $prefer_installer = rand()<.5 ? "eumm" : "mb";
    }
    if (lc($prefer_installer) eq "mb") {
        $self->{modulebuild} = 1;
    } elsif ($self->{archived} eq "patch") {
        # not an edge case, nothing to install for sure
        my $why = "A patch file cannot be installed";
        $CPAN::Frontend->mywarn("Refusing to handle this file: $why\n");
        $self->{writemakefile} = CPAN::Distrostatus->new("NO $why");
    } elsif (! $mpl_exists) {
        $self->_edge_cases($mpl,$local_file);
    }
    if ($self->{build_dir}
        &&
        $CPAN::Config->{build_dir_reuse}
       ) {
        $self->store_persistent_state;
    }
    return $self;
}

#-> CPAN::Distribution::store_persistent_state
sub store_persistent_state {
    my($self) = @_;
    my $dir = $self->{build_dir};
    unless (File::Spec->canonpath(File::Basename::dirname($dir))
            eq File::Spec->canonpath($CPAN::Config->{build_dir})) {
        $CPAN::Frontend->mywarn("Directory '$dir' not below $CPAN::Config->{build_dir}, ".
                                "will not store persistent state\n");
        return;
    }
    my $file = sprintf "%s.yml", $dir;
    my $yaml_module = CPAN::_yaml_module;
    if ($CPAN::META->has_inst($yaml_module)) {
        CPAN->_yaml_dumpfile(
                             $file,
                             {
                              time => time,
                              perl => CPAN::_perl_fingerprint,
                              distribution => $self,
                             }
                            );
    } else {
        $CPAN::Frontend->myprint("Warning (usually harmless): '$yaml_module' not installed, ".
                                "will not store persistent state\n");
    }
}

#-> CPAN::Distribution::try_download
sub try_download {
    my($self,$patch) = @_;
    my $norm = $self->normalize($patch);
    my($local_wanted) =
        File::Spec->catfile(
                            $CPAN::Config->{keep_source_where},
                            "authors",
                            "id",
                            split(/\//,$norm),
                           );
    $self->debug("Doing localize") if $CPAN::DEBUG;
    return CPAN::FTP->localize("authors/id/$norm",
                               $local_wanted);
}

{
    my $stdpatchargs = "";
    #-> CPAN::Distribution::patch
    sub patch {
        my($self) = @_;
        $self->debug("checking patches id[$self->{ID}]") if $CPAN::DEBUG;
        my $patches = $self->prefs->{patches};
        $patches ||= "";
        $self->debug("patches[$patches]") if $CPAN::DEBUG;
        if ($patches) {
            return unless @$patches;
            $self->safe_chdir($self->{build_dir});
            CPAN->debug("patches[$patches]") if $CPAN::DEBUG;
            my $patchbin = $CPAN::Config->{patch};
            unless ($patchbin && length $patchbin) {
                $CPAN::Frontend->mydie("No external patch command configured\n\n".
                                       "Please run 'o conf init /patch/'\n\n");
            }
            unless (MM->maybe_command($patchbin)) {
                $CPAN::Frontend->mydie("No external patch command available\n\n".
                                       "Please run 'o conf init /patch/'\n\n");
            }
            $patchbin = CPAN::HandleConfig->safe_quote($patchbin);
            local $ENV{PATCH_GET} = 0; # formerly known as -g0
            unless ($stdpatchargs) {
                my $system = "$patchbin --version |";
                local *FH;
                open FH, $system or die "Could not fork '$system': $!";
                local $/ = "\n";
                my $pversion;
              PARSEVERSION: while (<FH>) {
                    if (/^patch\s+([\d\.]+)/) {
                        $pversion = $1;
                        last PARSEVERSION;
                    }
                }
                if ($pversion) {
                    $stdpatchargs = "-N --fuzz=3";
                } else {
                    $stdpatchargs = "-N";
                }
            }
            my $countedpatches = @$patches == 1 ? "1 patch" : (scalar @$patches . " patches");
            $CPAN::Frontend->myprint("Going to apply $countedpatches:\n");
            for my $patch (@$patches) {
                unless (-f $patch) {
                    if (my $trydl = $self->try_download($patch)) {
                        $patch = $trydl;
                    } else {
                        my $fail = "Could not find patch '$patch'";
                        $CPAN::Frontend->mywarn("$fail; cannot continue\n");
                        $self->{unwrapped} = CPAN::Distrostatus->new("NO -- $fail");
                        delete $self->{build_dir};
                        return;
                    }
                }
                $CPAN::Frontend->myprint("  $patch\n");
                my $readfh = CPAN::Tarzip->TIEHANDLE($patch);

                my $pcommand;
                my $ppp = $self->_patch_p_parameter($readfh);
                if ($ppp eq "applypatch") {
                    $pcommand = "$CPAN::Config->{applypatch} -verbose";
                } else {
                    my $thispatchargs = join " ", $stdpatchargs, $ppp;
                    $pcommand = "$patchbin $thispatchargs";
                }

                $readfh = CPAN::Tarzip->TIEHANDLE($patch); # open again
                my $writefh = FileHandle->new;
                $CPAN::Frontend->myprint("  $pcommand\n");
                unless (open $writefh, "|$pcommand") {
                    my $fail = "Could not fork '$pcommand'";
                    $CPAN::Frontend->mywarn("$fail; cannot continue\n");
                    $self->{unwrapped} = CPAN::Distrostatus->new("NO -- $fail");
                    delete $self->{build_dir};
                    return;
                }
                while (my $x = $readfh->READLINE) {
                    print $writefh $x;
                }
                unless (close $writefh) {
                    my $fail = "Could not apply patch '$patch'";
                    $CPAN::Frontend->mywarn("$fail; cannot continue\n");
                    $self->{unwrapped} = CPAN::Distrostatus->new("NO -- $fail");
                    delete $self->{build_dir};
                    return;
                }
            }
            $self->{patched}++;
        }
        return 1;
    }
}

sub _patch_p_parameter {
    my($self,$fh) = @_;
    my $cnt_files   = 0;
    my $cnt_p0files = 0;
    local($_);
    while ($_ = $fh->READLINE) {
        if (
            $CPAN::Config->{applypatch}
            &&
            /\#\#\#\# ApplyPatch data follows \#\#\#\#/
           ) {
            return "applypatch"
        }
        next unless /^[\*\+]{3}\s(\S+)/;
        my $file = $1;
        $cnt_files++;
        $cnt_p0files++ if -f $file;
        CPAN->debug("file[$file]cnt_files[$cnt_files]cnt_p0files[$cnt_p0files]")
            if $CPAN::DEBUG;
    }
    return "-p1" unless $cnt_files;
    return $cnt_files==$cnt_p0files ? "-p0" : "-p1";
}

#-> sub CPAN::Distribution::_edge_cases
# with "configure" or "Makefile" or single file scripts
sub _edge_cases {
    my($self,$mpl,$local_file) = @_;
    $self->debug(sprintf("makefilepl[%s]anycwd[%s]",
                         $mpl,
                         CPAN::anycwd(),
                        )) if $CPAN::DEBUG;
    my $build_dir = $self->{build_dir};
    my($configure) = File::Spec->catfile($build_dir,"Configure");
    if (-f $configure) {
        # do we have anything to do?
        $self->{configure} = $configure;
    } elsif (-f File::Spec->catfile($build_dir,"Makefile")) {
        $CPAN::Frontend->mywarn(qq{
Package comes with a Makefile and without a Makefile.PL.
We\'ll try to build it with that Makefile then.
});
        $self->{writemakefile} = CPAN::Distrostatus->new("YES");
        $CPAN::Frontend->mysleep(2);
    } else {
        my $cf = $self->called_for || "unknown";
        if ($cf =~ m|/|) {
            $cf =~ s|.*/||;
            $cf =~ s|\W.*||;
        }
        $cf =~ s|[/\\:]||g;     # risk of filesystem damage
        $cf = "unknown" unless length($cf);
        $CPAN::Frontend->mywarn(qq{Package seems to come without Makefile.PL.
  (The test -f "$mpl" returned false.)
  Writing one on our own (setting NAME to $cf)\a\n});
        $self->{had_no_makefile_pl}++;
        $CPAN::Frontend->mysleep(3);

        # Writing our own Makefile.PL

        my $script = "";
        if ($self->{archived} eq "maybe_pl") {
            my $fh = FileHandle->new;
            my $script_file = File::Spec->catfile($build_dir,$local_file);
            $fh->open($script_file)
                or Carp::croak("Could not open script '$script_file': $!");
            local $/ = "\n";
            # name parsen und prereq
            my($state) = "poddir";
            my($name, $prereq) = ("", "");
            while (<$fh>) {
                if ($state eq "poddir" && /^=head\d\s+(\S+)/) {
                    if ($1 eq 'NAME') {
                        $state = "name";
                    } elsif ($1 eq 'PREREQUISITES') {
                        $state = "prereq";
                    }
                } elsif ($state =~ m{^(name|prereq)$}) {
                    if (/^=/) {
                        $state = "poddir";
                    } elsif (/^\s*$/) {
                        # nop
                    } elsif ($state eq "name") {
                        if ($name eq "") {
                            ($name) = /^(\S+)/;
                            $state = "poddir";
                        }
                    } elsif ($state eq "prereq") {
                        $prereq .= $_;
                    }
                } elsif (/^=cut\b/) {
                    last;
                }
            }
            $fh->close;

            for ($name) {
                s{.*<}{};       # strip X<...>
                s{>.*}{};
            }
            chomp $prereq;
            $prereq = join " ", split /\s+/, $prereq;
            my($PREREQ_PM) = join("\n", map {
                s{.*<}{};       # strip X<...>
                s{>.*}{};
                if (/[\s\'\"]/) { # prose?
                } else {
                    s/[^\w:]$//; # period?
                    " "x28 . "'$_' => 0,";
                }
            } split /\s*,\s*/, $prereq);

            $script = "
              EXE_FILES => ['$name'],
              PREREQ_PM => {
$PREREQ_PM
                           },
";
            if ($name) {
                my $to_file = File::Spec->catfile($build_dir, $name);
                rename $script_file, $to_file
                    or die "Can't rename $script_file to $to_file: $!";
            }
        }

        my $fh = FileHandle->new;
        $fh->open(">$mpl")
            or Carp::croak("Could not open >$mpl: $!");
        $fh->print(
                   qq{# This Makefile.PL has been autogenerated by the module CPAN.pm
# because there was no Makefile.PL supplied.
# Autogenerated on: }.scalar localtime().qq{

use ExtUtils::MakeMaker;
WriteMakefile(
              NAME => q[$cf],$script
             );
});
        $fh->close;
    }
}

#-> CPAN::Distribution::_signature_business
sub _signature_business {
    my($self) = @_;
    my $check_sigs = CPAN::HandleConfig->prefs_lookup($self,
                                                      q{check_sigs});
    if ($check_sigs) {
        if ($CPAN::META->has_inst("Module::Signature")) {
            if (-f "SIGNATURE") {
                $self->debug("Module::Signature is installed, verifying") if $CPAN::DEBUG;
                my $rv = Module::Signature::verify();
                if ($rv != Module::Signature::SIGNATURE_OK() and
                    $rv != Module::Signature::SIGNATURE_MISSING()) {
                    $CPAN::Frontend->mywarn(
                                            qq{\nSignature invalid for }.
                                            qq{distribution file. }.
                                            qq{Please investigate.\n\n}
                                           );

                    my $wrap =
                        sprintf(qq{I'd recommend removing %s. Some error occured    }.
                                qq{while checking its signature, so it could        }.
                                qq{be invalid. Maybe you have configured            }.
                                qq{your 'urllist' with a bad URL. Please check this }.
                                qq{array with 'o conf urllist' and retry. Or        }.
                                qq{examine the distribution in a subshell. Try
  look %s
and run
  cpansign -v
},
                                $self->{localfile},
                                $self->pretty_id,
                               );
                    $self->{signature_verify} = CPAN::Distrostatus->new("NO");
                    $CPAN::Frontend->mywarn(Text::Wrap::wrap("","",$wrap));
                    $CPAN::Frontend->mysleep(5) if $CPAN::Frontend->can("mysleep");
                } else {
                    $self->{signature_verify} = CPAN::Distrostatus->new("YES");
                    $self->debug("Module::Signature has verified") if $CPAN::DEBUG;
                }
            } else {
                $CPAN::Frontend->mywarn(qq{Package came without SIGNATURE\n\n});
            }
        } else {
            $self->debug("Module::Signature is NOT installed") if $CPAN::DEBUG;
        }
    }
}

#-> CPAN::Distribution::untar_me ;
sub untar_me {
    my($self,$ct) = @_;
    $self->{archived} = "tar";
    if ($ct->untar()) {
        $self->{unwrapped} = CPAN::Distrostatus->new("YES");
    } else {
        $self->{unwrapped} = CPAN::Distrostatus->new("NO -- untar failed");
    }
}

# CPAN::Distribution::unzip_me ;
sub unzip_me {
    my($self,$ct) = @_;
    $self->{archived} = "zip";
    if ($ct->unzip()) {
        $self->{unwrapped} = CPAN::Distrostatus->new("YES");
    } else {
        $self->{unwrapped} = CPAN::Distrostatus->new("NO -- unzip failed");
    }
    return;
}

sub handle_singlefile {
    my($self,$local_file) = @_;

    if ( $local_file =~ /\.pm(\.(gz|Z))?(?!\n)\Z/ ) {
        $self->{archived} = "pm";
    } elsif ( $local_file =~ /\.patch(\.(gz|bz2))?(?!\n)\Z/ ) {
        $self->{archived} = "patch";
    } else {
        $self->{archived} = "maybe_pl";
    }

    my $to = File::Basename::basename($local_file);
    if ($to =~ s/\.(gz|Z)(?!\n)\Z//) {
        if (eval{CPAN::Tarzip->new($local_file)->gunzip($to)}) {
            $self->{unwrapped} = CPAN::Distrostatus->new("YES");
        } else {
            $self->{unwrapped} = CPAN::Distrostatus->new("NO -- uncompressing failed");
        }
    } else {
        if (File::Copy::cp($local_file,".")) {
            $self->{unwrapped} = CPAN::Distrostatus->new("YES");
        } else {
            $self->{unwrapped} = CPAN::Distrostatus->new("NO -- copying failed");
        }
    }
    return $to;
}

#-> sub CPAN::Distribution::new ;
sub new {
    my($class,%att) = @_;

    # $CPAN::META->{cachemgr} ||= CPAN::CacheMgr->new();

    my $this = { %att };
    return bless $this, $class;
}

#-> sub CPAN::Distribution::look ;
sub look {
    my($self) = @_;

    if ($^O eq 'MacOS') {
      $self->Mac::BuildTools::look;
      return;
    }

    if (  $CPAN::Config->{'shell'} ) {
        $CPAN::Frontend->myprint(qq{
Trying to open a subshell in the build directory...
});
    } else {
        $CPAN::Frontend->myprint(qq{
Your configuration does not define a value for subshells.
Please define it with "o conf shell <your shell>"
});
        return;
    }
    my $dist = $self->id;
    my $dir;
    unless ($dir = $self->dir) {
        $self->get;
    }
    unless ($dir ||= $self->dir) {
        $CPAN::Frontend->mywarn(qq{
Could not determine which directory to use for looking at $dist.
});
        return;
    }
    my $pwd  = CPAN::anycwd();
    $self->safe_chdir($dir);
    $CPAN::Frontend->myprint(qq{Working directory is $dir\n});
    {
        local $ENV{CPAN_SHELL_LEVEL} = $ENV{CPAN_SHELL_LEVEL}||0;
        $ENV{CPAN_SHELL_LEVEL} += 1;
        my $shell = CPAN::HandleConfig->safe_quote($CPAN::Config->{'shell'});
        unless (system($shell) == 0) {
            my $code = $? >> 8;
            $CPAN::Frontend->mywarn("Subprocess shell exit code $code\n");
        }
    }
    $self->safe_chdir($pwd);
}

# CPAN::Distribution::cvs_import ;
sub cvs_import {
    my($self) = @_;
    $self->get;
    my $dir = $self->dir;

    my $package = $self->called_for;
    my $module = $CPAN::META->instance('CPAN::Module', $package);
    my $version = $module->cpan_version;

    my $userid = $self->cpan_userid;

    my $cvs_dir = (split /\//, $dir)[-1];
    $cvs_dir =~ s/-\d+[^-]+(?!\n)\Z//;
    my $cvs_root =
      $CPAN::Config->{cvsroot} || $ENV{CVSROOT};
    my $cvs_site_perl =
      $CPAN::Config->{cvs_site_perl} || $ENV{CVS_SITE_PERL};
    if ($cvs_site_perl) {
        $cvs_dir = "$cvs_site_perl/$cvs_dir";
    }
    my $cvs_log = qq{"imported $package $version sources"};
    $version =~ s/\./_/g;
    # XXX cvs: undocumented and unclear how it was meant to work
    my @cmd = ('cvs', '-d', $cvs_root, 'import', '-m', $cvs_log,
               "$cvs_dir", $userid, "v$version");

    my $pwd  = CPAN::anycwd();
    chdir($dir) or $CPAN::Frontend->mydie(qq{Could not chdir to "$dir": $!});

    $CPAN::Frontend->myprint(qq{Working directory is $dir\n});

    $CPAN::Frontend->myprint(qq{@cmd\n});
    system(@cmd) == 0 or
    # XXX cvs
        $CPAN::Frontend->mydie("cvs import failed");
    chdir($pwd) or $CPAN::Frontend->mydie(qq{Could not chdir to "$pwd": $!});
}

#-> sub CPAN::Distribution::readme ;
sub readme {
    my($self) = @_;
    my($dist) = $self->id;
    my($sans,$suffix) = $dist =~ /(.+)\.(tgz|tar[\._-]gz|tar\.Z|zip)$/;
    $self->debug("sans[$sans] suffix[$suffix]\n") if $CPAN::DEBUG;
    my($local_file);
    my($local_wanted) =
        File::Spec->catfile(
                            $CPAN::Config->{keep_source_where},
                            "authors",
                            "id",
                            split(/\//,"$sans.readme"),
                           );
    $self->debug("Doing localize") if $CPAN::DEBUG;
    $local_file = CPAN::FTP->localize("authors/id/$sans.readme",
                                      $local_wanted)
        or $CPAN::Frontend->mydie(qq{No $sans.readme found});;

    if ($^O eq 'MacOS') {
        Mac::BuildTools::launch_file($local_file);
        return;
    }

    my $fh_pager = FileHandle->new;
    local($SIG{PIPE}) = "IGNORE";
    my $pager = $CPAN::Config->{'pager'} || "cat";
    $fh_pager->open("|$pager")
        or die "Could not open pager $pager\: $!";
    my $fh_readme = FileHandle->new;
    $fh_readme->open($local_file)
        or $CPAN::Frontend->mydie(qq{Could not open "$local_file": $!});
    $CPAN::Frontend->myprint(qq{
Displaying file
  $local_file
with pager "$pager"
});
    $fh_pager->print(<$fh_readme>);
    $fh_pager->close;
}

#-> sub CPAN::Distribution::verifyCHECKSUM ;
sub verifyCHECKSUM {
    my($self) = @_;
  EXCUSE: {
        my @e;
        $self->{CHECKSUM_STATUS} ||= "";
        $self->{CHECKSUM_STATUS} eq "OK" and push @e, "Checksum was ok";
        $CPAN::Frontend->myprint(join "", map {"  $_\n"} @e) and return if @e;
    }
    my($lc_want,$lc_file,@local,$basename);
    @local = split(/\//,$self->id);
    pop @local;
    push @local, "CHECKSUMS";
    $lc_want =
        File::Spec->catfile($CPAN::Config->{keep_source_where},
                            "authors", "id", @local);
    local($") = "/";
    if (my $size = -s $lc_want) {
        $self->debug("lc_want[$lc_want]size[$size]") if $CPAN::DEBUG;
        if ($self->CHECKSUM_check_file($lc_want,1)) {
            return $self->{CHECKSUM_STATUS} = "OK";
        }
    }
    $lc_file = CPAN::FTP->localize("authors/id/@local",
                                   $lc_want,1);
    unless ($lc_file) {
        $CPAN::Frontend->myprint("Trying $lc_want.gz\n");
        $local[-1] .= ".gz";
        $lc_file = CPAN::FTP->localize("authors/id/@local",
                                       "$lc_want.gz",1);
        if ($lc_file) {
            $lc_file =~ s/\.gz(?!\n)\Z//;
            eval{CPAN::Tarzip->new("$lc_file.gz")->gunzip($lc_file)};
        } else {
            return;
        }
    }
    if ($self->CHECKSUM_check_file($lc_file)) {
        return $self->{CHECKSUM_STATUS} = "OK";
    }
}

#-> sub CPAN::Distribution::SIG_check_file ;
sub SIG_check_file {
    my($self,$chk_file) = @_;
    my $rv = eval { Module::Signature::_verify($chk_file) };

    if ($rv == Module::Signature::SIGNATURE_OK()) {
        $CPAN::Frontend->myprint("Signature for $chk_file ok\n");
        return $self->{SIG_STATUS} = "OK";
    } else {
        $CPAN::Frontend->myprint(qq{\nSignature invalid for }.
                                 qq{distribution file. }.
                                 qq{Please investigate.\n\n}.
                                 $self->as_string,
                                 $CPAN::META->instance(
                                                       'CPAN::Author',
                                                       $self->cpan_userid
                                                      )->as_string);

        my $wrap = qq{I\'d recommend removing $chk_file. Its signature
is invalid. Maybe you have configured your 'urllist' with
a bad URL. Please check this array with 'o conf urllist', and
retry.};

        $CPAN::Frontend->mydie(Text::Wrap::wrap("","",$wrap));
    }
}

#-> sub CPAN::Distribution::CHECKSUM_check_file ;

# sloppy is 1 when we have an old checksums file that maybe is good
# enough

sub CHECKSUM_check_file {
    my($self,$chk_file,$sloppy) = @_;
    my($cksum,$file,$basename);

    $sloppy ||= 0;
    $self->debug("chk_file[$chk_file]sloppy[$sloppy]") if $CPAN::DEBUG;
    my $check_sigs = CPAN::HandleConfig->prefs_lookup($self,
                                                      q{check_sigs});
    if ($check_sigs) {
        if ($CPAN::META->has_inst("Module::Signature")) {
            $self->debug("Module::Signature is installed, verifying") if $CPAN::DEBUG;
            $self->SIG_check_file($chk_file);
        } else {
            $self->debug("Module::Signature is NOT installed") if $CPAN::DEBUG;
        }
    }

    $file = $self->{localfile};
    $basename = File::Basename::basename($file);
    my $fh = FileHandle->new;
    if (open $fh, $chk_file) {
        local($/);
        my $eval = <$fh>;
        $eval =~ s/\015?\012/\n/g;
        close $fh;
        my($comp) = Safe->new();
        $cksum = $comp->reval($eval);
        if ($@) {
            rename $chk_file, "$chk_file.bad";
            Carp::confess($@) if $@;
        }
    } else {
        Carp::carp "Could not open $chk_file for reading";
    }

    if (! ref $cksum or ref $cksum ne "HASH") {
        $CPAN::Frontend->mywarn(qq{
Warning: checksum file '$chk_file' broken.

When trying to read that file I expected to get a hash reference
for further processing, but got garbage instead.
});
        my $answer = CPAN::Shell::colorable_makemaker_prompt("Proceed nonetheless?", "no");
        $answer =~ /^\s*y/i or $CPAN::Frontend->mydie("Aborted.\n");
        $self->{CHECKSUM_STATUS} = "NIL -- CHECKSUMS file broken";
        return;
    } elsif (exists $cksum->{$basename}{sha256}) {
        $self->debug("Found checksum for $basename:" .
                     "$cksum->{$basename}{sha256}\n") if $CPAN::DEBUG;

        open($fh, $file);
        binmode $fh;
        my $eq = $self->eq_CHECKSUM($fh,$cksum->{$basename}{sha256});
        $fh->close;
        $fh = CPAN::Tarzip->TIEHANDLE($file);

        unless ($eq) {
            my $dg = Digest::SHA->new(256);
            my($data,$ref);
            $ref = \$data;
            while ($fh->READ($ref, 4096) > 0) {
                $dg->add($data);
            }
            my $hexdigest = $dg->hexdigest;
            $eq += $hexdigest eq $cksum->{$basename}{'sha256-ungz'};
        }

        if ($eq) {
            $CPAN::Frontend->myprint("Checksum for $file ok\n");
            return $self->{CHECKSUM_STATUS} = "OK";
        } else {
            $CPAN::Frontend->myprint(qq{\nChecksum mismatch for }.
                                     qq{distribution file. }.
                                     qq{Please investigate.\n\n}.
                                     $self->as_string,
                                     $CPAN::META->instance(
                                                           'CPAN::Author',
                                                           $self->cpan_userid
                                                          )->as_string);

            my $wrap = qq{I\'d recommend removing $file. Its
checksum is incorrect. Maybe you have configured your 'urllist' with
a bad URL. Please check this array with 'o conf urllist', and
retry.};

            $CPAN::Frontend->mydie(Text::Wrap::wrap("","",$wrap));

            # former versions just returned here but this seems a
            # serious threat that deserves a die

            # $CPAN::Frontend->myprint("\n\n");
            # sleep 3;
            # return;
        }
        # close $fh if fileno($fh);
    } else {
        return if $sloppy;
        unless ($self->{CHECKSUM_STATUS}) {
            $CPAN::Frontend->mywarn(qq{
Warning: No checksum for $basename in $chk_file.

The cause for this may be that the file is very new and the checksum
has not yet been calculated, but it may also be that something is
going awry right now.
});
            my $answer = CPAN::Shell::colorable_makemaker_prompt("Proceed?", "yes");
            $answer =~ /^\s*y/i or $CPAN::Frontend->mydie("Aborted.\n");
        }
        $self->{CHECKSUM_STATUS} = "NIL -- distro not in CHECKSUMS file";
        return;
    }
}

#-> sub CPAN::Distribution::eq_CHECKSUM ;
sub eq_CHECKSUM {
    my($self,$fh,$expect) = @_;
    if ($CPAN::META->has_inst("Digest::SHA")) {
        my $dg = Digest::SHA->new(256);
        my($data);
        while (read($fh, $data, 4096)) {
            $dg->add($data);
        }
        my $hexdigest = $dg->hexdigest;
        # warn "fh[$fh] hex[$hexdigest] aexp[$expectMD5]";
        return $hexdigest eq $expect;
    }
    return 1;
}

#-> sub CPAN::Distribution::force ;

# Both CPAN::Modules and CPAN::Distributions know if "force" is in
# effect by autoinspection, not by inspecting a global variable. One
# of the reason why this was chosen to work that way was the treatment
# of dependencies. They should not automatically inherit the force
# status. But this has the downside that ^C and die() will return to
# the prompt but will not be able to reset the force_update
# attributes. We try to correct for it currently in the read_metadata
# routine, and immediately before we check for a Signal. I hope this
# works out in one of v1.57_53ff

# "Force get forgets previous error conditions"

#-> sub CPAN::Distribution::fforce ;
sub fforce {
  my($self, $method) = @_;
  $self->force($method,1);
}

#-> sub CPAN::Distribution::force ;
sub force {
  my($self, $method,$fforce) = @_;
  my %phase_map = (
                   get => [
                           "unwrapped",
                           "build_dir",
                           "archived",
                           "localfile",
                           "CHECKSUM_STATUS",
                           "signature_verify",
                           "prefs",
                           "prefs_file",
                           "prefs_file_doc",
                          ],
                   make => [
                            "writemakefile",
                            "make",
                            "modulebuild",
                            "prereq_pm",
                            "prereq_pm_detected",
                           ],
                   test => [
                            "badtestcnt",
                            "make_test",
                           ],
                   install => [
                               "install",
                              ],
                   unknown => [
                               "reqtype",
                               "yaml_content",
                              ],
                  );
  my $methodmatch = 0;
  my $ldebug = 0;
 PHASE: for my $phase (qw(unknown get make test install)) { # order matters
      $methodmatch = 1 if $fforce || $phase eq $method;
      next unless $methodmatch;
    ATTRIBUTE: for my $att (@{$phase_map{$phase}}) {
          if ($phase eq "get") {
              if (substr($self->id,-1,1) eq "."
                  && $att =~ /(unwrapped|build_dir|archived)/ ) {
                  # cannot be undone for local distros
                  next ATTRIBUTE;
              }
              if ($att eq "build_dir"
                  && $self->{build_dir}
                  && $CPAN::META->{is_tested}
                 ) {
                  delete $CPAN::META->{is_tested}{$self->{build_dir}};
              }
          } elsif ($phase eq "test") {
              if ($att eq "make_test"
                  && $self->{make_test}
                  && $self->{make_test}{COMMANDID}
                  && $self->{make_test}{COMMANDID} == $CPAN::CurrentCommandId
                 ) {
                  # endless loop too likely
                  next ATTRIBUTE;
              }
          }
          delete $self->{$att};
          if ($ldebug || $CPAN::DEBUG) {
              # local $CPAN::DEBUG = 16; # Distribution
              CPAN->debug(sprintf "id[%s]phase[%s]att[%s]", $self->id, $phase, $att);
          }
      }
  }
  if ($method && $method =~ /make|test|install/) {
    $self->{force_update} = 1; # name should probably have been force_install
  }
}

#-> sub CPAN::Distribution::notest ;
sub notest {
  my($self, $method) = @_;
  # $CPAN::Frontend->mywarn("XDEBUG: set notest for $self $method");
  $self->{"notest"}++; # name should probably have been force_install
}

#-> sub CPAN::Distribution::unnotest ;
sub unnotest {
  my($self) = @_;
  # warn "XDEBUG: deleting notest";
  delete $self->{notest};
}

#-> sub CPAN::Distribution::unforce ;
sub unforce {
  my($self) = @_;
  delete $self->{force_update};
}

#-> sub CPAN::Distribution::isa_perl ;
sub isa_perl {
  my($self) = @_;
  my $file = File::Basename::basename($self->id);
  if ($file =~ m{ ^ perl
                  -?
                  (5)
                  ([._-])
                  (
                   \d{3}(_[0-4][0-9])?
                   |
                   \d+\.\d+
                  )
                  \.tar[._-](?:gz|bz2)
                  (?!\n)\Z
                }xs) {
    return "$1.$3";
  } elsif ($self->cpan_comment
           &&
           $self->cpan_comment =~ /isa_perl\(.+?\)/) {
    return $1;
  }
}


#-> sub CPAN::Distribution::perl ;
sub perl {
    my ($self) = @_;
    if (! $self) {
        use Carp qw(carp);
        carp __PACKAGE__ . "::perl was called without parameters.";
    }
    return CPAN::HandleConfig->safe_quote($CPAN::Perl);
}


#-> sub CPAN::Distribution::make ;
sub make {
    my($self) = @_;
    if (my $goto = $self->prefs->{goto}) {
        return $self->goto($goto);
    }
    my $make = $self->{modulebuild} ? "Build" : "make";
    # Emergency brake if they said install Pippi and get newest perl
    if ($self->isa_perl) {
        if (
            $self->called_for ne $self->id &&
            ! $self->{force_update}
        ) {
            # if we die here, we break bundles
            $CPAN::Frontend
                ->mywarn(sprintf(
                            qq{The most recent version "%s" of the module "%s"
is part of the perl-%s distribution. To install that, you need to run
  force install %s   --or--
  install %s
},
                             $CPAN::META->instance(
                                                   'CPAN::Module',
                                                   $self->called_for
                                                  )->cpan_version,
                             $self->called_for,
                             $self->isa_perl,
                             $self->called_for,
                             $self->id,
                            ));
            $self->{make} = CPAN::Distrostatus->new("NO isa perl");
            $CPAN::Frontend->mysleep(1);
            return;
        }
    }
    $CPAN::Frontend->myprint(sprintf "Running %s for %s\n", $make, $self->id);
    $self->get;
    if ($self->{configure_requires_later}) {
        return;
    }
    local $ENV{PERL5LIB} = defined($ENV{PERL5LIB})
                           ? $ENV{PERL5LIB}
                           : ($ENV{PERLLIB} || "");
    $CPAN::META->set_perl5lib;
    local $ENV{MAKEFLAGS}; # protect us from outer make calls

    if ($CPAN::Signal) {
        delete $self->{force_update};
        return;
    }

    my $builddir;
  EXCUSE: {
        my @e;
        if (!$self->{archived} || $self->{archived} eq "NO") {
            push @e, "Is neither a tar nor a zip archive.";
        }

        if (!$self->{unwrapped}
            || (
                UNIVERSAL::can($self->{unwrapped},"failed") ?
                $self->{unwrapped}->failed :
                $self->{unwrapped} =~ /^NO/
               )) {
            push @e, "Had problems unarchiving. Please build manually";
        }

        unless ($self->{force_update}) {
            exists $self->{signature_verify} and
                (
                 UNIVERSAL::can($self->{signature_verify},"failed") ?
                 $self->{signature_verify}->failed :
                 $self->{signature_verify} =~ /^NO/
                )
                and push @e, "Did not pass the signature test.";
        }

        if (exists $self->{writemakefile} &&
            (
             UNIVERSAL::can($self->{writemakefile},"failed") ?
             $self->{writemakefile}->failed :
             $self->{writemakefile} =~ /^NO/
            )) {
            # XXX maybe a retry would be in order?
            my $err = UNIVERSAL::can($self->{writemakefile},"text") ?
                $self->{writemakefile}->text :
                    $self->{writemakefile};
            $err =~ s/^NO\s*//;
            $err ||= "Had some problem writing Makefile";
            $err .= ", won't make";
            push @e, $err;
        }

        if (defined $self->{make}) {
            if (UNIVERSAL::can($self->{make},"failed") ?
                $self->{make}->failed :
                $self->{make} =~ /^NO/) {
                if ($self->{force_update}) {
                    # Trying an already failed 'make' (unless somebody else blocks)
                } else {
                    # introduced for turning recursion detection into a distrostatus
                    my $error = length $self->{make}>3
                        ? substr($self->{make},3) : "Unknown error";
                    $CPAN::Frontend->mywarn("Could not make: $error\n");
                    $self->store_persistent_state;
                    return;
                }
            } else {
                push @e, "Has already been made";
            }
        }

        my $later = $self->{later} || $self->{configure_requires_later};
        if ($later) { # see also undelay
            if ($later) {
                push @e, $later;
            }
        }

        $CPAN::Frontend->myprint(join "", map {"  $_\n"} @e) and return if @e;
        $builddir = $self->dir or
            $CPAN::Frontend->mydie("PANIC: Cannot determine build directory\n");
        unless (chdir $builddir) {
            push @e, "Couldn't chdir to '$builddir': $!";
        }
        $CPAN::Frontend->mywarn(join "", map {"  $_\n"} @e) and return if @e;
    }
    if ($CPAN::Signal) {
        delete $self->{force_update};
        return;
    }
    $CPAN::Frontend->myprint("\n  CPAN.pm: Going to build ".$self->id."\n\n");
    $self->debug("Changed directory to $builddir") if $CPAN::DEBUG;

    if ($^O eq 'MacOS') {
        Mac::BuildTools::make($self);
        return;
    }

    my %env;
    while (my($k,$v) = each %ENV) {
        next unless defined $v;
        $env{$k} = $v;
    }
    local %ENV = %env;
    my $system;
    if (my $commandline = $self->prefs->{pl}{commandline}) {
        $system = $commandline;
        $ENV{PERL} = $^X;
    } elsif ($self->{'configure'}) {
        $system = $self->{'configure'};
    } elsif ($self->{modulebuild}) {
        my($perl) = $self->perl or die "Couldn\'t find executable perl\n";
        $system = "$perl Build.PL $CPAN::Config->{mbuildpl_arg}";
    } else {
        my($perl) = $self->perl or die "Couldn\'t find executable perl\n";
        my $switch = "";
# This needs a handler that can be turned on or off:
#        $switch = "-MExtUtils::MakeMaker ".
#            "-Mops=:default,:filesys_read,:filesys_open,require,chdir"
#            if $] > 5.00310;
        my $makepl_arg = $self->make_x_arg("pl");
        $ENV{PERL5_CPAN_IS_EXECUTING} = File::Spec->catfile($self->{build_dir},
                                                            "Makefile.PL");
        $system = sprintf("%s%s Makefile.PL%s",
                          $perl,
                          $switch ? " $switch" : "",
                          $makepl_arg ? " $makepl_arg" : "",
                         );
    }
    if (my $env = $self->prefs->{pl}{env}) {
        for my $e (keys %$env) {
            $ENV{$e} = $env->{$e};
        }
    }
    if (exists $self->{writemakefile}) {
    } else {
        local($SIG{ALRM}) = sub { die "inactivity_timeout reached\n" };
        my($ret,$pid,$output);
        $@ = "";
        my $go_via_alarm;
        if ($CPAN::Config->{inactivity_timeout}) {
            require Config;
            if ($Config::Config{d_alarm}
                &&
                $Config::Config{d_alarm} eq "define"
               ) {
                $go_via_alarm++
            } else {
                $CPAN::Frontend->mywarn("Warning: you have configured the config ".
                                        "variable 'inactivity_timeout' to ".
                                        "'$CPAN::Config->{inactivity_timeout}'. But ".
                                        "on this machine the system call 'alarm' ".
                                        "isn't available. This means that we cannot ".
                                        "provide the feature of intercepting long ".
                                        "waiting code and will turn this feature off.\n"
                                       );
                $CPAN::Config->{inactivity_timeout} = 0;
            }
        }
        if ($go_via_alarm) {
            if ( $self->_should_report('pl') ) {
                ($output, $ret) = CPAN::Reporter::record_command(
                    $system,
                    $CPAN::Config->{inactivity_timeout},
                );
                CPAN::Reporter::grade_PL( $self, $system, $output, $ret );
            }
            else {
                eval {
                    alarm $CPAN::Config->{inactivity_timeout};
                    local $SIG{CHLD}; # = sub { wait };
                    if (defined($pid = fork)) {
                        if ($pid) { #parent
                            # wait;
                            waitpid $pid, 0;
                        } else {    #child
                            # note, this exec isn't necessary if
                            # inactivity_timeout is 0. On the Mac I'd
                            # suggest, we set it always to 0.
                            exec $system;
                        }
                    } else {
                        $CPAN::Frontend->myprint("Cannot fork: $!");
                        return;
                    }
                };
                alarm 0;
                if ($@) {
                    kill 9, $pid;
                    waitpid $pid, 0;
                    my $err = "$@";
                    $CPAN::Frontend->myprint($err);
                    $self->{writemakefile} = CPAN::Distrostatus->new("NO $err");
                    $@ = "";
                    $self->store_persistent_state;
                    return $self->goodbye("$system -- TIMED OUT");
                }
            }
        } else {
            if (my $expect_model = $self->_prefs_with_expect("pl")) {
                # XXX probably want to check _should_report here and warn
                # about not being able to use CPAN::Reporter with expect
                $ret = $self->_run_via_expect($system,$expect_model);
                if (! defined $ret
                    && $self->{writemakefile}
                    && $self->{writemakefile}->failed) {
                    # timeout
                    return;
                }
            }
            elsif ( $self->_should_report('pl') ) {
                ($output, $ret) = CPAN::Reporter::record_command($system);
                CPAN::Reporter::grade_PL( $self, $system, $output, $ret );
            }
            else {
                $ret = system($system);
            }
            if ($ret != 0) {
                $self->{writemakefile} = CPAN::Distrostatus
                    ->new("NO '$system' returned status $ret");
                $CPAN::Frontend->mywarn("Warning: No success on command[$system]\n");
                $self->store_persistent_state;
                return $self->goodbye("$system -- NOT OK");
            }
        }
        if (-f "Makefile" || -f "Build") {
            $self->{writemakefile} = CPAN::Distrostatus->new("YES");
            delete $self->{make_clean}; # if cleaned before, enable next
        } else {
            my $makefile = $self->{modulebuild} ? "Build" : "Makefile";
            $self->{writemakefile} = CPAN::Distrostatus
                ->new(qq{NO -- No $makefile created});
            $self->store_persistent_state;
            return $self->goodbye("$system -- NO $makefile created");
        }
    }
    if ($CPAN::Signal) {
        delete $self->{force_update};
        return;
    }
    if (my @prereq = $self->unsat_prereq("later")) {
        if ($prereq[0][0] eq "perl") {
            my $need = "requires perl '$prereq[0][1]'";
            my $id = $self->pretty_id;
            $CPAN::Frontend->mywarn("$id $need; you have only $]; giving up\n");
            $self->{make} = CPAN::Distrostatus->new("NO $need");
            $self->store_persistent_state;
            return $self->goodbye("[prereq] -- NOT OK");
        } else {
            my $follow = eval { $self->follow_prereqs("later",@prereq); };
            if (0) {
            } elsif ($follow) {
                # signal success to the queuerunner
                return 1;
            } elsif ($@ && ref $@ && $@->isa("CPAN::Exception::RecursiveDependency")) {
                $CPAN::Frontend->mywarn($@);
                return $self->goodbye("[depend] -- NOT OK");
            }
        }
    }
    if ($CPAN::Signal) {
        delete $self->{force_update};
        return;
    }
    if (my $commandline = $self->prefs->{make}{commandline}) {
        $system = $commandline;
        $ENV{PERL} = CPAN::find_perl;
    } else {
        if ($self->{modulebuild}) {
            unless (-f "Build") {
                my $cwd = CPAN::anycwd();
                $CPAN::Frontend->mywarn("Alert: no Build file available for 'make $self->{id}'".
                                        " in cwd[$cwd]. Danger, Will Robinson!\n");
                $CPAN::Frontend->mysleep(5);
            }
            $system = join " ", $self->_build_command(), $CPAN::Config->{mbuild_arg};
        } else {
            $system = join " ", $self->_make_command(),  $CPAN::Config->{make_arg};
        }
        $system =~ s/\s+$//;
        my $make_arg = $self->make_x_arg("make");
        $system = sprintf("%s%s",
                          $system,
                          $make_arg ? " $make_arg" : "",
                         );
    }
    if (my $env = $self->prefs->{make}{env}) { # overriding the local
                                               # ENV of PL, not the
                                               # outer ENV, but
                                               # unlikely to be a risk
        for my $e (keys %$env) {
            $ENV{$e} = $env->{$e};
        }
    }
    my $expect_model = $self->_prefs_with_expect("make");
    my $want_expect = 0;
    if ( $expect_model && @{$expect_model->{talk}} ) {
        my $can_expect = $CPAN::META->has_inst("Expect");
        if ($can_expect) {
            $want_expect = 1;
        } else {
            $CPAN::Frontend->mywarn("Expect not installed, falling back to ".
                                    "system()\n");
        }
    }
    my $system_ok;
    if ($want_expect) {
        # XXX probably want to check _should_report here and
        # warn about not being able to use CPAN::Reporter with expect
        $system_ok = $self->_run_via_expect($system,$expect_model) == 0;
    }
    elsif ( $self->_should_report('make') ) {
        my ($output, $ret) = CPAN::Reporter::record_command($system);
        CPAN::Reporter::grade_make( $self, $system, $output, $ret );
        $system_ok = ! $ret;
    }
    else {
        $system_ok = system($system) == 0;
    }
    $self->introduce_myself;
    if ( $system_ok ) {
        $CPAN::Frontend->myprint("  $system -- OK\n");
        $self->{make} = CPAN::Distrostatus->new("YES");
    } else {
        $self->{writemakefile} ||= CPAN::Distrostatus->new("YES");
        $self->{make} = CPAN::Distrostatus->new("NO");
        $CPAN::Frontend->mywarn("  $system -- NOT OK\n");
    }
    $self->store_persistent_state;
}

# CPAN::Distribution::goodbye ;
sub goodbye {
    my($self,$goodbye) = @_;
    my $id = $self->pretty_id;
    $CPAN::Frontend->mywarn("  $id\n  $goodbye\n");
    return;
}

# CPAN::Distribution::_run_via_expect ;
sub _run_via_expect {
    my($self,$system,$expect_model) = @_;
    CPAN->debug("system[$system]expect_model[$expect_model]") if $CPAN::DEBUG;
    if ($CPAN::META->has_inst("Expect")) {
        my $expo = Expect->new;  # expo Expect object;
        $expo->spawn($system);
        $expect_model->{mode} ||= "deterministic";
        if ($expect_model->{mode} eq "deterministic") {
            return $self->_run_via_expect_deterministic($expo,$expect_model);
        } elsif ($expect_model->{mode} eq "anyorder") {
            return $self->_run_via_expect_anyorder($expo,$expect_model);
        } else {
            die "Panic: Illegal expect mode: $expect_model->{mode}";
        }
    } else {
        $CPAN::Frontend->mywarn("Expect not installed, falling back to system()\n");
        return system($system);
    }
}

sub _run_via_expect_anyorder {
    my($self,$expo,$expect_model) = @_;
    my $timeout = $expect_model->{timeout} || 5;
    my $reuse = $expect_model->{reuse};
    my @expectacopy = @{$expect_model->{talk}}; # we trash it!
    my $but = "";
  EXPECT: while () {
        my($eof,$ran_into_timeout);
        my @match = $expo->expect($timeout,
                                  [ eof => sub {
                                        $eof++;
                                    } ],
                                  [ timeout => sub {
                                        $ran_into_timeout++;
                                    } ],
                                  -re => eval"qr{.}",
                                 );
        if ($match[2]) {
            $but .= $match[2];
        }
        $but .= $expo->clear_accum;
        if ($eof) {
            $expo->soft_close;
            return $expo->exitstatus();
        } elsif ($ran_into_timeout) {
            # warn "DEBUG: they are asking a question, but[$but]";
            for (my $i = 0; $i <= $#expectacopy; $i+=2) {
                my($next,$send) = @expectacopy[$i,$i+1];
                my $regex = eval "qr{$next}";
                # warn "DEBUG: will compare with regex[$regex].";
                if ($but =~ /$regex/) {
                    # warn "DEBUG: will send send[$send]";
                    $expo->send($send);
                    # never allow reusing an QA pair unless they told us
                    splice @expectacopy, $i, 2 unless $reuse;
                    next EXPECT;
                }
            }
            my $why = "could not answer a question during the dialog";
            $CPAN::Frontend->mywarn("Failing: $why\n");
            $self->{writemakefile} =
                CPAN::Distrostatus->new("NO $why");
            return;
        }
    }
}

sub _run_via_expect_deterministic {
    my($self,$expo,$expect_model) = @_;
    my $ran_into_timeout;
    my $timeout = $expect_model->{timeout} || 15; # currently unsettable
    my $expecta = $expect_model->{talk};
  EXPECT: for (my $i = 0; $i <= $#$expecta; $i+=2) {
        my($re,$send) = @$expecta[$i,$i+1];
        CPAN->debug("timeout[$timeout]re[$re]") if $CPAN::DEBUG;
        my $regex = eval "qr{$re}";
        $expo->expect($timeout,
                      [ eof => sub {
                            my $but = $expo->clear_accum;
                            $CPAN::Frontend->mywarn("EOF (maybe harmless)
expected[$regex]\nbut[$but]\n\n");
                            last EXPECT;
                        } ],
                      [ timeout => sub {
                            my $but = $expo->clear_accum;
                            $CPAN::Frontend->mywarn("TIMEOUT
expected[$regex]\nbut[$but]\n\n");
                            $ran_into_timeout++;
                        } ],
                      -re => $regex);
        if ($ran_into_timeout) {
            # note that the caller expects 0 for success
            $self->{writemakefile} =
                CPAN::Distrostatus->new("NO timeout during expect dialog");
            return;
        }
        $expo->send($send);
    }
    $expo->soft_close;
    return $expo->exitstatus();
}

#-> CPAN::Distribution::_validate_distropref
sub _validate_distropref {
    my($self,@args) = @_;
    if (
        $CPAN::META->has_inst("CPAN::Kwalify")
        &&
        $CPAN::META->has_inst("Kwalify")
       ) {
        eval {CPAN::Kwalify::_validate("distroprefs",@args);};
        if ($@) {
            $CPAN::Frontend->mywarn($@);
        }
    } else {
        CPAN->debug("not validating '@args'") if $CPAN::DEBUG;
    }
}

#-> CPAN::Distribution::_find_prefs
sub _find_prefs {
    my($self) = @_;
    my $distroid = $self->pretty_id;
    #CPAN->debug("distroid[$distroid]") if $CPAN::DEBUG;
    my $prefs_dir = $CPAN::Config->{prefs_dir};
    return if $prefs_dir =~ /^\s*$/;
    eval { File::Path::mkpath($prefs_dir); };
    if ($@) {
        $CPAN::Frontend->mydie("Cannot create directory $prefs_dir");
    }
    my $yaml_module = CPAN::_yaml_module;
    my @extensions;
    if ($CPAN::META->has_inst($yaml_module)) {
        push @extensions, "yml";
    } else {
        my @fallbacks;
        if ($CPAN::META->has_inst("Data::Dumper")) {
            push @extensions, "dd";
            push @fallbacks, "Data::Dumper";
        }
        if ($CPAN::META->has_inst("Storable")) {
            push @extensions, "st";
            push @fallbacks, "Storable";
        }
        if (@fallbacks) {
            local $" = " and ";
            unless ($self->{have_complained_about_missing_yaml}++) {
                $CPAN::Frontend->mywarn("'$yaml_module' not installed, falling back ".
                                        "to @fallbacks to read prefs '$prefs_dir'\n");
            }
        } else {
            unless ($self->{have_complained_about_missing_yaml}++) {
                $CPAN::Frontend->mywarn("'$yaml_module' not installed, cannot ".
                                        "read prefs '$prefs_dir'\n");
            }
        }
    }
    if (@extensions) {
        my $dh = DirHandle->new($prefs_dir)
            or die Carp::croak("Couldn't open '$prefs_dir': $!");
      DIRENT: for (sort $dh->read) {
            next if $_ eq "." || $_ eq "..";
            my $exte = join "|", @extensions;
            next unless /\.($exte)$/;
            my $thisexte = $1;
            my $abs = File::Spec->catfile($prefs_dir, $_);
            if (-f $abs) {
                #CPAN->debug(sprintf "abs[%s]", $abs) if $CPAN::DEBUG;
                my @distropref;
                if ($thisexte eq "yml") {
                    # need no eval because if we have no YAML we do not try to read *.yml
                    #CPAN->debug(sprintf "before yaml load abs[%s]", $abs) if $CPAN::DEBUG;
                    @distropref = @{CPAN->_yaml_loadfile($abs)};
                    #CPAN->debug(sprintf "after yaml load abs[%s]", $abs) if $CPAN::DEBUG;
                } elsif ($thisexte eq "dd") {
                    package CPAN::Eval;
                    no strict;
                    open FH, "<$abs" or $CPAN::Frontend->mydie("Could not open '$abs': $!");
                    local $/;
                    my $eval = <FH>;
                    close FH;
                    eval $eval;
                    if ($@) {
                        $CPAN::Frontend->mydie("Error in distroprefs file $_\: $@");
                    }
                    my $i = 1;
                    while (${"VAR".$i}) {
                        push @distropref, ${"VAR".$i};
                        $i++;
                    }
                } elsif ($thisexte eq "st") {
                    # eval because Storable is never forward compatible
                    eval { @distropref = @{scalar Storable::retrieve($abs)}; };
                    if ($@) {
                        $CPAN::Frontend->mywarn("Error reading distroprefs file ".
                                                "$_, skipping\: $@");
                        $CPAN::Frontend->mysleep(4);
                        next DIRENT;
                    }
                }
                # $DB::single=1;
                #CPAN->debug(sprintf "#distropref[%d]", scalar @distropref) if $CPAN::DEBUG;
              ELEMENT: for my $y (0..$#distropref) {
                    my $distropref = $distropref[$y];
                    $self->_validate_distropref($distropref,$abs,$y);
                    my $match = $distropref->{match};
                    unless ($match) {
                        #CPAN->debug("no 'match' in abs[$abs], skipping") if $CPAN::DEBUG;
                        next ELEMENT;
                    }
                    my $ok = 1;
                    # do not take the order of C<keys %$match> because
                    # "module" is by far the slowest
                    my $saw_valid_subkeys = 0;
                    for my $sub_attribute (qw(distribution perl perlconfig module)) {
                        next unless exists $match->{$sub_attribute};
                        $saw_valid_subkeys++;
                        my $qr = eval "qr{$distropref->{match}{$sub_attribute}}";
                        if ($sub_attribute eq "module") {
                            my $okm = 0;
                            #CPAN->debug(sprintf "distropref[%d]", scalar @distropref) if $CPAN::DEBUG;
                            my @modules = $self->containsmods;
                            #CPAN->debug(sprintf "modules[%s]", join(",",@modules)) if $CPAN::DEBUG;
                          MODULE: for my $module (@modules) {
                                $okm ||= $module =~ /$qr/;
                                last MODULE if $okm;
                            }
                            $ok &&= $okm;
                        } elsif ($sub_attribute eq "distribution") {
                            my $okd = $distroid =~ /$qr/;
                            $ok &&= $okd;
                        } elsif ($sub_attribute eq "perl") {
                            my $okp = CPAN::find_perl =~ /$qr/;
                            $ok &&= $okp;
                        } elsif ($sub_attribute eq "perlconfig") {
                            for my $perlconfigkey (keys %{$match->{perlconfig}}) {
                                my $perlconfigval = $match->{perlconfig}->{$perlconfigkey};
                                # XXX should probably warn if Config does not exist
                                my $okpc = $Config::Config{$perlconfigkey} =~ /$perlconfigval/;
                                $ok &&= $okpc;
                                last if $ok == 0;
                            }
                        } else {
                            $CPAN::Frontend->mydie("Nonconforming .$thisexte file '$abs': ".
                                                   "unknown sub_attribut '$sub_attribute'. ".
                                                   "Please ".
                                                   "remove, cannot continue.");
                        }
                        last if $ok == 0; # short circuit
                    }
                    unless ($saw_valid_subkeys) {
                        $CPAN::Frontend->mydie("Nonconforming .$thisexte file '$abs': ".
                                               "missing match/* subattribute. ".
                                               "Please ".
                                               "remove, cannot continue.");
                    }
                    #CPAN->debug(sprintf "ok[%d]", $ok) if $CPAN::DEBUG;
                    if ($ok) {
                        return {
                                prefs => $distropref,
                                prefs_file => $abs,
                                prefs_file_doc => $y,
                               };
                    }

                }
            }
        }
        $dh->close;
    }
    return;
}

# CPAN::Distribution::prefs
sub prefs {
    my($self) = @_;
    if (exists $self->{negative_prefs_cache}
        &&
        $self->{negative_prefs_cache} != $CPAN::CurrentCommandId
       ) {
        delete $self->{negative_prefs_cache};
        delete $self->{prefs};
    }
    if (exists $self->{prefs}) {
        return $self->{prefs}; # XXX comment out during debugging
    }
    if ($CPAN::Config->{prefs_dir}) {
        CPAN->debug("prefs_dir[$CPAN::Config->{prefs_dir}]") if $CPAN::DEBUG;
        my $prefs = $self->_find_prefs();
        $prefs ||= ""; # avoid warning next line
        CPAN->debug("prefs[$prefs]") if $CPAN::DEBUG;
        if ($prefs) {
            for my $x (qw(prefs prefs_file prefs_file_doc)) {
                $self->{$x} = $prefs->{$x};
            }
            my $bs = sprintf(
                             "%s[%s]",
                             File::Basename::basename($self->{prefs_file}),
                             $self->{prefs_file_doc},
                            );
            my $filler1 = "_" x 22;
            my $filler2 = int(66 - length($bs))/2;
            $filler2 = 0 if $filler2 < 0;
            $filler2 = " " x $filler2;
            $CPAN::Frontend->myprint("
$filler1 D i s t r o P r e f s $filler1
$filler2 $bs $filler2
");
            $CPAN::Frontend->mysleep(1);
            return $self->{prefs};
        }
    }
    $self->{negative_prefs_cache} = $CPAN::CurrentCommandId;
    return $self->{prefs} = +{};
}

# CPAN::Distribution::make_x_arg
sub make_x_arg {
    my($self, $whixh) = @_;
    my $make_x_arg;
    my $prefs = $self->prefs;
    if (
        $prefs
        && exists $prefs->{$whixh}
        && exists $prefs->{$whixh}{args}
        && $prefs->{$whixh}{args}
       ) {
        $make_x_arg = join(" ",
                           map {CPAN::HandleConfig
                                 ->safe_quote($_)} @{$prefs->{$whixh}{args}},
                          );
    }
    my $what = sprintf "make%s_arg", $whixh eq "make" ? "" : $whixh;
    $make_x_arg ||= $CPAN::Config->{$what};
    return $make_x_arg;
}

# CPAN::Distribution::_make_command
sub _make_command {
    my ($self) = @_;
    if ($self) {
        return
            CPAN::HandleConfig
                ->safe_quote(
                             CPAN::HandleConfig->prefs_lookup($self,
                                                              q{make})
                             || $Config::Config{make}
                             || 'make'
                            );
    } else {
        # Old style call, without object. Deprecated
        Carp::confess("CPAN::_make_command() used as function. Don't Do That.");
        return
          safe_quote(undef,
                     CPAN::HandleConfig->prefs_lookup($self,q{make})
                     || $CPAN::Config->{make}
                     || $Config::Config{make}
                     || 'make');
    }
}

#-> sub CPAN::Distribution::follow_prereqs ;
sub follow_prereqs {
    my($self) = shift;
    my($slot) = shift;
    my(@prereq_tuples) = grep {$_->[0] ne "perl"} @_;
    return unless @prereq_tuples;
    my @prereq = map { $_->[0] } @prereq_tuples;
    my $pretty_id = $self->pretty_id;
    my %map = (
               b => "build_requires",
               r => "requires",
               c => "commandline",
              );
    my($filler1,$filler2,$filler3,$filler4);
    # $DB::single=1;
    my $unsat = "Unsatisfied dependencies detected during";
    my $w = length($unsat) > length($pretty_id) ? length($unsat) : length($pretty_id);
    {
        my $r = int(($w - length($unsat))/2);
        my $l = $w - length($unsat) - $r;
        $filler1 = "-"x4 . " "x$l;
        $filler2 = " "x$r . "-"x4 . "\n";
    }
    {
        my $r = int(($w - length($pretty_id))/2);
        my $l = $w - length($pretty_id) - $r;
        $filler3 = "-"x4 . " "x$l;
        $filler4 = " "x$r . "-"x4 . "\n";
    }
    $CPAN::Frontend->
        myprint("$filler1 $unsat $filler2".
                "$filler3 $pretty_id $filler4".
                join("", map {"    $_->[0] \[$map{$_->[1]}]\n"} @prereq_tuples),
               );
    my $follow = 0;
    if ($CPAN::Config->{prerequisites_policy} eq "follow") {
        $follow = 1;
    } elsif ($CPAN::Config->{prerequisites_policy} eq "ask") {
        my $answer = CPAN::Shell::colorable_makemaker_prompt(
"Shall I follow them and prepend them to the queue
of modules we are processing right now?", "yes");
        $follow = $answer =~ /^\s*y/i;
    } else {
        local($") = ", ";
        $CPAN::Frontend->
            myprint("  Ignoring dependencies on modules @prereq\n");
    }
    if ($follow) {
        my $id = $self->id;
        # color them as dirty
        for my $p (@prereq) {
            # warn "calling color_cmd_tmps(0,1)";
            my $any = CPAN::Shell->expandany($p);
            $self->{$slot . "_for"}{$any->id}++;
            if ($any) {
                $any->color_cmd_tmps(0,2);
            } else {
                $CPAN::Frontend->mywarn("Warning (maybe a bug): Cannot expand prereq '$p'\n");
                $CPAN::Frontend->mysleep(2);
            }
        }
        # queue them and re-queue yourself
        CPAN::Queue->jumpqueue({qmod => $id, reqtype => $self->{reqtype}},
                               map {+{qmod=>$_->[0],reqtype=>$_->[1]}} reverse @prereq_tuples);
        $self->{$slot} = "Delayed until after prerequisites";
        return 1; # signal success to the queuerunner
    }
    return;
}

#-> sub CPAN::Distribution::unsat_prereq ;
# return ([Foo=>1],[Bar=>1.2]) for normal modules
# return ([perl=>5.008]) if we need a newer perl than we are running under
sub unsat_prereq {
    my($self,$slot) = @_;
    my(%merged,$prereq_pm);
    my $prefs_depends = $self->prefs->{depends}||{};
    if ($slot eq "configure_requires_later") {
        my $meta_yml = $self->parse_meta_yml();
        %merged = (%{$meta_yml->{configure_requires}||{}},
                   %{$prefs_depends->{configure_requires}||{}});
        $prereq_pm = {}; # configure_requires defined as "b"
    } elsif ($slot eq "later") {
        my $prereq_pm_0 = $self->prereq_pm || {};
        for my $reqtype (qw(requires build_requires)) {
            $prereq_pm->{$reqtype} = {%{$prereq_pm_0->{$reqtype}||{}}}; # copy to not pollute it
            for my $k (keys %{$prefs_depends->{$reqtype}||{}}) {
                $prereq_pm->{$reqtype}{$k} = $prefs_depends->{$reqtype}{$k};
            }
        }
        %merged = (%{$prereq_pm->{requires}||{}},%{$prereq_pm->{build_requires}||{}});
    } else {
        die "Panic: illegal slot '$slot'";
    }
    my(@need);
    my @merged = %merged;
    CPAN->debug("all merged_prereqs[@merged]") if $CPAN::DEBUG;
  NEED: while (my($need_module, $need_version) = each %merged) {
        my($available_version,$available_file,$nmo);
        if ($need_module eq "perl") {
            $available_version = $];
            $available_file = CPAN::find_perl;
        } else {
            $nmo = $CPAN::META->instance("CPAN::Module",$need_module);
            next if $nmo->uptodate;
            $available_file = $nmo->available_file;

            # if they have not specified a version, we accept any installed one
            if (defined $available_file
                and ( # a few quick shortcurcuits
                     not defined $need_version
                     or $need_version eq '0'    # "==" would trigger warning when not numeric
                     or $need_version eq "undef"
                    )) {
                next NEED;
            }

            $available_version = $nmo->available_version;
        }

        # We only want to install prereqs if either they're not installed
        # or if the installed version is too old. We cannot omit this
        # check, because if 'force' is in effect, nobody else will check.
        if (defined $available_file) {
            my(@all_requirements) = split /\s*,\s*/, $need_version;
            local($^W) = 0;
            my $ok = 0;
          RQ: for my $rq (@all_requirements) {
                if ($rq =~ s|>=\s*||) {
                } elsif ($rq =~ s|>\s*||) {
                    # 2005-12: one user
                    if (CPAN::Version->vgt($available_version,$rq)) {
                        $ok++;
                    }
                    next RQ;
                } elsif ($rq =~ s|!=\s*||) {
                    # 2005-12: no user
                    if (CPAN::Version->vcmp($available_version,$rq)) {
                        $ok++;
                        next RQ;
                    } else {
                        last RQ;
                    }
                } elsif ($rq =~ m|<=?\s*|) {
                    # 2005-12: no user
                    $CPAN::Frontend->mywarn("Downgrading not supported (rq[$rq])\n");
                    $ok++;
                    next RQ;
                }
                if (! CPAN::Version->vgt($rq, $available_version)) {
                    $ok++;
                }
                CPAN->debug(sprintf("need_module[%s]available_file[%s]".
                                    "available_version[%s]rq[%s]ok[%d]",
                                    $need_module,
                                    $available_file,
                                    $available_version,
                                    CPAN::Version->readable($rq),
                                    $ok,
                                   )) if $CPAN::DEBUG;
            }
            next NEED if $ok == @all_requirements;
        }

        if ($need_module eq "perl") {
            return ["perl", $need_version];
        }
        $self->{sponsored_mods}{$need_module} ||= 0;
        CPAN->debug("need_module[$need_module]s/s/n[$self->{sponsored_mods}{$need_module}]") if $CPAN::DEBUG;
        if ($self->{sponsored_mods}{$need_module}++) {
            # We have already sponsored it and for some reason it's still
            # not available. So we do ... what??

            # if we push it again, we have a potential infinite loop

            # The following "next" was a very problematic construct.
            # It helped a lot but broke some day and had to be
            # replaced.

            # We must be able to deal with modules that come again and
            # again as a prereq and have themselves prereqs and the
            # queue becomes long but finally we would find the correct
            # order. The RecursiveDependency check should trigger a
            # die when it's becoming too weird. Unfortunately removing
            # this next breaks many other things.

            # The bug that brought this up is described in Todo under
            # "5.8.9 cannot install Compress::Zlib"

            # next; # this is the next that had to go away

            # The following "next NEED" are fine and the error message
            # explains well what is going on. For example when the DBI
            # fails and consequently DBD::SQLite fails and now we are
            # processing CPAN::SQLite. Then we must have a "next" for
            # DBD::SQLite. How can we get it and how can we identify
            # all other cases we must identify?

            my $do = $nmo->distribution;
            next NEED unless $do; # not on CPAN
            if (CPAN::Version->vcmp($need_version, $nmo->ro->{CPAN_VERSION}) > 0){
                $CPAN::Frontend->mywarn("Warning: Prerequisite ".
                                        "'$need_module => $need_version' ".
                                        "for '$self->{ID}' seems ".
                                        "not available according to the indexes\n"
                                       );
                next NEED;
            }
          NOSAYER: for my $nosayer (
                                    "unwrapped",
                                    "writemakefile",
                                    "signature_verify",
                                    "make",
                                    "make_test",
                                    "install",
                                    "make_clean",
                                   ) {
                if ($do->{$nosayer}) {
                    if (UNIVERSAL::can($do->{$nosayer},"failed") ?
                        $do->{$nosayer}->failed :
                        $do->{$nosayer} =~ /^NO/) {
                        if ($nosayer eq "make_test"
                            &&
                            $do->{make_test}{COMMANDID} != $CPAN::CurrentCommandId
                           ) {
                            next NOSAYER;
                        }
                        $CPAN::Frontend->mywarn("Warning: Prerequisite ".
                                                "'$need_module => $need_version' ".
                                                "for '$self->{ID}' failed when ".
                                                "processing '$do->{ID}' with ".
                                                "'$nosayer => $do->{$nosayer}'. Continuing, ".
                                                "but chances to succeed are limited.\n"
                                               );
                        next NEED;
                    } else { # the other guy succeeded
                        if ($nosayer eq "install") {
                            # we had this with
                            # DMAKI/DateTime-Calendar-Chinese-0.05.tar.gz
                            # 2007-03
                            $CPAN::Frontend->mywarn("Warning: Prerequisite ".
                                                    "'$need_module => $need_version' ".
                                                    "for '$self->{ID}' already installed ".
                                                    "but installation looks suspicious. ".
                                                    "Skipping another installation attempt, ".
                                                    "to prevent looping endlessly.\n"
                                                   );
                            next NEED;
                        }
                    }
                }
            }
        }
        my $needed_as = exists $prereq_pm->{requires}{$need_module} ? "r" : "b";
        push @need, [$need_module,$needed_as];
    }
    my @unfolded = map { "[".join(",",@$_)."]" } @need;
    CPAN->debug("returning from unsat_prereq[@unfolded]") if $CPAN::DEBUG;
    @need;
}

#-> sub CPAN::Distribution::read_yaml ;
sub read_yaml {
    my($self) = @_;
    return $self->{yaml_content} if exists $self->{yaml_content};
    my $build_dir = $self->{build_dir};
    my $yaml = File::Spec->catfile($build_dir,"META.yml");
    $self->debug("yaml[$yaml]") if $CPAN::DEBUG;
    return unless -f $yaml;
    eval { $self->{yaml_content} = CPAN->_yaml_loadfile($yaml)->[0]; };
    if ($@) {
        $CPAN::Frontend->mywarn("Could not read ".
                                "'$yaml'. Falling back to other ".
                                "methods to determine prerequisites\n");
        return $self->{yaml_content} = undef; # if we die, then we
                                              # cannot read YAML's own
                                              # META.yml
    }
    # not "authoritative"
    if (not exists $self->{yaml_content}{dynamic_config}
        or $self->{yaml_content}{dynamic_config}
       ) {
        $self->{yaml_content} = undef;
    }
    $self->debug(sprintf "yaml_content[%s]", $self->{yaml_content} || "UNDEF")
        if $CPAN::DEBUG;
    return $self->{yaml_content};
}

#-> sub CPAN::Distribution::prereq_pm ;
sub prereq_pm {
    my($self) = @_;
    $self->{prereq_pm_detected} ||= 0;
    CPAN->debug("ID[$self->{ID}]prereq_pm_detected[$self->{prereq_pm_detected}]") if $CPAN::DEBUG;
    return $self->{prereq_pm} if $self->{prereq_pm_detected};
    return unless $self->{writemakefile}  # no need to have succeeded
                                          # but we must have run it
        || $self->{modulebuild};
    CPAN->debug(sprintf "writemakefile[%s]modulebuild[%s]",
                $self->{writemakefile}||"",
                $self->{modulebuild}||"",
               ) if $CPAN::DEBUG;
    my($req,$breq);
    if (my $yaml = $self->read_yaml) { # often dynamic_config prevents a result here
        $req =  $yaml->{requires} || {};
        $breq =  $yaml->{build_requires} || {};
        undef $req unless ref $req eq "HASH" && %$req;
        if ($req) {
            if ($yaml->{generated_by} &&
                $yaml->{generated_by} =~ /ExtUtils::MakeMaker version ([\d\._]+)/) {
                my $eummv = do { local $^W = 0; $1+0; };
                if ($eummv < 6.2501) {
                    # thanks to Slaven for digging that out: MM before
                    # that could be wrong because it could reflect a
                    # previous release
                    undef $req;
                }
            }
            my $areq;
            my $do_replace;
            while (my($k,$v) = each %{$req||{}}) {
                if ($v =~ /\d/) {
                    $areq->{$k} = $v;
                } elsif ($k =~ /[A-Za-z]/ &&
                         $v =~ /[A-Za-z]/ &&
                         $CPAN::META->exists("Module",$v)
                        ) {
                    $CPAN::Frontend->mywarn("Suspicious key-value pair in META.yml's ".
                                            "requires hash: $k => $v; I'll take both ".
                                            "key and value as a module name\n");
                    $CPAN::Frontend->mysleep(1);
                    $areq->{$k} = 0;
                    $areq->{$v} = 0;
                    $do_replace++;
                }
            }
            $req = $areq if $do_replace;
        }
    }
    unless ($req || $breq) {
        my $build_dir = $self->{build_dir} or die "Panic: no build_dir?";
        my $makefile = File::Spec->catfile($build_dir,"Makefile");
        my $fh;
        if (-f $makefile
            and
            $fh = FileHandle->new("<$makefile\0")) {
            CPAN->debug("Getting prereq from Makefile") if $CPAN::DEBUG;
            local($/) = "\n";
            while (<$fh>) {
                last if /MakeMaker post_initialize section/;
                my($p) = m{^[\#]
                           \s+PREREQ_PM\s+=>\s+(.+)
                       }x;
                next unless $p;
                # warn "Found prereq expr[$p]";

                #  Regexp modified by A.Speer to remember actual version of file
                #  PREREQ_PM hash key wants, then add to
                while ( $p =~ m/(?:\s)([\w\:]+)=>(q\[.*?\]|undef),?/g ) {
                    # In case a prereq is mentioned twice, complain.
                    if ( defined $req->{$1} ) {
                        warn "Warning: PREREQ_PM mentions $1 more than once, ".
                            "last mention wins";
                    }
                    my($m,$n) = ($1,$2);
                    if ($n =~ /^q\[(.*?)\]$/) {
                        $n = $1;
                    }
                    $req->{$m} = $n;
                }
                last;
            }
        }
    }
    unless ($req || $breq) {
        my $build_dir = $self->{build_dir} or die "Panic: no build_dir?";
        my $buildfile = File::Spec->catfile($build_dir,"Build");
        if (-f $buildfile) {
            CPAN->debug("Found '$buildfile'") if $CPAN::DEBUG;
            my $build_prereqs = File::Spec->catfile($build_dir,"_build","prereqs");
            if (-f $build_prereqs) {
                CPAN->debug("Getting prerequisites from '$build_prereqs'") if $CPAN::DEBUG;
                my $content = do { local *FH;
                                   open FH, $build_prereqs
                                       or $CPAN::Frontend->mydie("Could not open ".
                                                                 "'$build_prereqs': $!");
                                   local $/;
                                   <FH>;
                               };
                my $bphash = eval $content;
                if ($@) {
                } else {
                    $req  = $bphash->{requires} || +{};
                    $breq = $bphash->{build_requires} || +{};
                }
            }
        }
    }
    if (-f "Build.PL"
        && ! -f "Makefile.PL"
        && ! exists $req->{"Module::Build"}
        && ! $CPAN::META->has_inst("Module::Build")) {
        $CPAN::Frontend->mywarn("  Warning: CPAN.pm discovered Module::Build as ".
                                "undeclared prerequisite.\n".
                                "  Adding it now as such.\n"
                               );
        $CPAN::Frontend->mysleep(5);
        $req->{"Module::Build"} = 0;
        delete $self->{writemakefile};
    }
    if ($req || $breq) {
        $self->{prereq_pm_detected}++;
        return $self->{prereq_pm} = { requires => $req, build_requires => $breq };
    }
}

#-> sub CPAN::Distribution::test ;
sub test {
    my($self) = @_;
    if (my $goto = $self->prefs->{goto}) {
        return $self->goto($goto);
    }
    $self->make;
    if ($CPAN::Signal) {
      delete $self->{force_update};
      return;
    }
    # warn "XDEBUG: checking for notest: $self->{notest} $self";
    if ($self->{notest}) {
        $CPAN::Frontend->myprint("Skipping test because of notest pragma\n");
        return 1;
    }

    my $make = $self->{modulebuild} ? "Build" : "make";

    local $ENV{PERL5LIB} = defined($ENV{PERL5LIB})
                           ? $ENV{PERL5LIB}
                           : ($ENV{PERLLIB} || "");

    $CPAN::META->set_perl5lib;
    local $ENV{MAKEFLAGS}; # protect us from outer make calls

    $CPAN::Frontend->myprint("Running $make test\n");

  EXCUSE: {
        my @e;
        if ($self->{make} or $self->{later}) {
            # go ahead
        } else {
            push @e,
                "Make had some problems, won't test";
        }

        exists $self->{make} and
            (
             UNIVERSAL::can($self->{make},"failed") ?
             $self->{make}->failed :
             $self->{make} =~ /^NO/
            ) and push @e, "Can't test without successful make";
        $self->{badtestcnt} ||= 0;
        if ($self->{badtestcnt} > 0) {
            require Data::Dumper;
            CPAN->debug(sprintf "NOREPEAT[%s]", Data::Dumper::Dumper($self)) if $CPAN::DEBUG;
            push @e, "Won't repeat unsuccessful test during this command";
        }

        push @e, $self->{later} if $self->{later};
        push @e, $self->{configure_requires_later} if $self->{configure_requires_later};

        if (exists $self->{build_dir}) {
            if (exists $self->{make_test}) {
                if (
                    UNIVERSAL::can($self->{make_test},"failed") ?
                    $self->{make_test}->failed :
                    $self->{make_test} =~ /^NO/
                   ) {
                    if (
                        UNIVERSAL::can($self->{make_test},"commandid")
                        &&
                        $self->{make_test}->commandid == $CPAN::CurrentCommandId
                       ) {
                        push @e, "Has already been tested within this command";
                    }
                } else {
                    push @e, "Has already been tested successfully";
                }
            }
        } elsif (!@e) {
            push @e, "Has no own directory";
        }
        $CPAN::Frontend->myprint(join "", map {"  $_\n"} @e) and return if @e;
        unless (chdir $self->{build_dir}) {
            push @e, "Couldn't chdir to '$self->{build_dir}': $!";
        }
        $CPAN::Frontend->mywarn(join "", map {"  $_\n"} @e) and return if @e;
    }
    $self->debug("Changed directory to $self->{build_dir}")
        if $CPAN::DEBUG;

    if ($^O eq 'MacOS') {
        Mac::BuildTools::make_test($self);
        return;
    }

    if ($self->{modulebuild}) {
        my $v = CPAN::Shell->expand("Module","Test::Harness")->inst_version;
        if (CPAN::Version->vlt($v,2.62)) {
            $CPAN::Frontend->mywarn(qq{The version of your Test::Harness is only
  '$v', you need at least '2.62'. Please upgrade your Test::Harness.\n});
            $self->{make_test} = CPAN::Distrostatus->new("NO Test::Harness too old");
            return;
        }
    }

    my $system;
    my $prefs_test = $self->prefs->{test};
    if (my $commandline
        = exists $prefs_test->{commandline} ? $prefs_test->{commandline} : "") {
        $system = $commandline;
        $ENV{PERL} = CPAN::find_perl;
    } elsif ($self->{modulebuild}) {
        $system = sprintf "%s test", $self->_build_command();
    } else {
        $system = join " ", $self->_make_command(), "test";
    }
    my $make_test_arg = $self->make_x_arg("test");
    $system = sprintf("%s%s",
                      $system,
                      $make_test_arg ? " $make_test_arg" : "",
                     );
    my($tests_ok);
    my %env;
    while (my($k,$v) = each %ENV) {
        next unless defined $v;
        $env{$k} = $v;
    }
    local %ENV = %env;
    if (my $env = $self->prefs->{test}{env}) {
        for my $e (keys %$env) {
            $ENV{$e} = $env->{$e};
        }
    }
    my $expect_model = $self->_prefs_with_expect("test");
    my $want_expect = 0;
    if ( $expect_model && @{$expect_model->{talk}} ) {
        my $can_expect = $CPAN::META->has_inst("Expect");
        if ($can_expect) {
            $want_expect = 1;
        } else {
            $CPAN::Frontend->mywarn("Expect not installed, falling back to ".
                                    "testing without\n");
        }
    }
    if ($want_expect) {
        if ($self->_should_report('test')) {
            $CPAN::Frontend->mywarn("Reporting via CPAN::Reporter is currently ".
                                    "not supported when distroprefs specify ".
                                    "an interactive test\n");
        }
        $tests_ok = $self->_run_via_expect($system,$expect_model) == 0;
    } elsif ( $self->_should_report('test') ) {
        $tests_ok = CPAN::Reporter::test($self, $system);
    } else {
        $tests_ok = system($system) == 0;
    }
    $self->introduce_myself;
    if ( $tests_ok ) {
        {
            my @prereq;

            # local $CPAN::DEBUG = 16; # Distribution
            for my $m (keys %{$self->{sponsored_mods}}) {
                next unless $self->{sponsored_mods}{$m} > 0;
                my $m_obj = CPAN::Shell->expand("Module",$m) or next;
                # XXX we need available_version which reflects
                # $ENV{PERL5LIB} so that already tested but not yet
                # installed modules are counted.
                my $available_version = $m_obj->available_version;
                my $available_file = $m_obj->available_file;
                if ($available_version &&
                    !CPAN::Version->vlt($available_version,$self->{prereq_pm}{$m})
                   ) {
                    CPAN->debug("m[$m] good enough available_version[$available_version]")
                        if $CPAN::DEBUG;
                } elsif ($available_file
                         && (
                             !$self->{prereq_pm}{$m}
                             ||
                             $self->{prereq_pm}{$m} == 0
                            )
                        ) {
                    # lex Class::Accessor::Chained::Fast which has no $VERSION
                    CPAN->debug("m[$m] have available_file[$available_file]")
                        if $CPAN::DEBUG;
                } else {
                    push @prereq, $m;
                }
            }
            if (@prereq) {
                my $cnt = @prereq;
                my $which = join ",", @prereq;
                my $but = $cnt == 1 ? "one dependency not OK ($which)" :
                    "$cnt dependencies missing ($which)";
                $CPAN::Frontend->mywarn("Tests succeeded but $but\n");
                $self->{make_test} = CPAN::Distrostatus->new("NO $but");
                $self->store_persistent_state;
                return $self->goodbye("[dependencies] -- NA");
            }
        }

        $CPAN::Frontend->myprint("  $system -- OK\n");
        $self->{make_test} = CPAN::Distrostatus->new("YES");
        $CPAN::META->is_tested($self->{build_dir},$self->{make_test}{TIME});
        # probably impossible to need the next line because badtestcnt
        # has a lifespan of one command
        delete $self->{badtestcnt};
    } else {
        $self->{make_test} = CPAN::Distrostatus->new("NO");
        $self->{badtestcnt}++;
        $CPAN::Frontend->mywarn("  $system -- NOT OK\n");
        CPAN::Shell->optprint
              ("hint",
               sprintf
               ("//hint// to see the cpan-testers results for installing this module, try:
  reports %s\n",
                $self->pretty_id));
    }
    $self->store_persistent_state;
}

sub _prefs_with_expect {
    my($self,$where) = @_;
    return unless my $prefs = $self->prefs;
    return unless my $where_prefs = $prefs->{$where};
    if ($where_prefs->{expect}) {
        return {
                mode => "deterministic",
                timeout => 15,
                talk => $where_prefs->{expect},
               };
    } elsif ($where_prefs->{"eexpect"}) {
        return $where_prefs->{"eexpect"};
    }
    return;
}

#-> sub CPAN::Distribution::clean ;
sub clean {
    my($self) = @_;
    my $make = $self->{modulebuild} ? "Build" : "make";
    $CPAN::Frontend->myprint("Running $make clean\n");
    unless (exists $self->{archived}) {
        $CPAN::Frontend->mywarn("Distribution seems to have never been unzipped".
                                "/untarred, nothing done\n");
        return 1;
    }
    unless (exists $self->{build_dir}) {
        $CPAN::Frontend->mywarn("Distribution has no own directory, nothing to do.\n");
        return 1;
    }
    if (exists $self->{writemakefile}
        and $self->{writemakefile}->failed
       ) {
        $CPAN::Frontend->mywarn("No Makefile, don't know how to 'make clean'\n");
        return 1;
    }
  EXCUSE: {
        my @e;
        exists $self->{make_clean} and $self->{make_clean} eq "YES" and
            push @e, "make clean already called once";
        $CPAN::Frontend->myprint(join "", map {"  $_\n"} @e) and return if @e;
    }
    chdir $self->{build_dir} or
        Carp::confess("Couldn't chdir to $self->{build_dir}: $!");
    $self->debug("Changed directory to $self->{build_dir}") if $CPAN::DEBUG;

    if ($^O eq 'MacOS') {
        Mac::BuildTools::make_clean($self);
        return;
    }

    my $system;
    if ($self->{modulebuild}) {
        unless (-f "Build") {
            my $cwd = CPAN::anycwd();
            $CPAN::Frontend->mywarn("Alert: no Build file available for 'clean $self->{id}".
                                    " in cwd[$cwd]. Danger, Will Robinson!");
            $CPAN::Frontend->mysleep(5);
        }
        $system = sprintf "%s clean", $self->_build_command();
    } else {
        $system  = join " ", $self->_make_command(), "clean";
    }
    my $system_ok = system($system) == 0;
    $self->introduce_myself;
    if ( $system_ok ) {
      $CPAN::Frontend->myprint("  $system -- OK\n");

      # $self->force;

      # Jost Krieger pointed out that this "force" was wrong because
      # it has the effect that the next "install" on this distribution
      # will untar everything again. Instead we should bring the
      # object's state back to where it is after untarring.

      for my $k (qw(
                    force_update
                    install
                    writemakefile
                    make
                    make_test
                   )) {
          delete $self->{$k};
      }
      $self->{make_clean} = CPAN::Distrostatus->new("YES");

    } else {
      # Hmmm, what to do if make clean failed?

      $self->{make_clean} = CPAN::Distrostatus->new("NO");
      $CPAN::Frontend->mywarn(qq{  $system -- NOT OK\n});

      # 2006-02-27: seems silly to me to force a make now
      # $self->force("make"); # so that this directory won't be used again

    }
    $self->store_persistent_state;
}

#-> sub CPAN::Distribution::goto ;
sub goto {
    my($self,$goto) = @_;
    $goto = $self->normalize($goto);
    my $why = sprintf(
                      "Goto '$goto' via prefs file '%s' doc %d",
                      $self->{prefs_file},
                      $self->{prefs_file_doc},
                     );
    $self->{unwrapped} = CPAN::Distrostatus->new("NO $why");
    # 2007-07-16 akoenig : Better than NA would be if we could inherit
    # the status of the $goto distro but given the exceptional nature
    # of 'goto' I feel reluctant to implement it
    my $goodbye_message = "[goto] -- NA $why";
    $self->goodbye($goodbye_message);

    # inject into the queue

    CPAN::Queue->delete($self->id);
    CPAN::Queue->jumpqueue({qmod => $goto, reqtype => $self->{reqtype}});

    # and run where we left off

    my($method) = (caller(1))[3];
    CPAN->instance("CPAN::Distribution",$goto)->$method();
    CPAN::Queue->delete_first($goto);
}

#-> sub CPAN::Distribution::install ;
sub install {
    my($self) = @_;
    if (my $goto = $self->prefs->{goto}) {
        return $self->goto($goto);
    }
    # $DB::single=1;
    unless ($self->{badtestcnt}) {
        $self->test;
    }
    if ($CPAN::Signal) {
      delete $self->{force_update};
      return;
    }
    my $make = $self->{modulebuild} ? "Build" : "make";
    $CPAN::Frontend->myprint("Running $make install\n");
  EXCUSE: {
        my @e;
        if ($self->{make} or $self->{later}) {
            # go ahead
        } else {
            push @e,
                "Make had some problems, won't install";
        }

        exists $self->{make} and
            (
             UNIVERSAL::can($self->{make},"failed") ?
             $self->{make}->failed :
             $self->{make} =~ /^NO/
            ) and
            push @e, "Make had returned bad status, install seems impossible";

        if (exists $self->{build_dir}) {
        } elsif (!@e) {
            push @e, "Has no own directory";
        }

        if (exists $self->{make_test} and
            (
             UNIVERSAL::can($self->{make_test},"failed") ?
             $self->{make_test}->failed :
             $self->{make_test} =~ /^NO/
            )) {
            if ($self->{force_update}) {
                $self->{make_test}->text("FAILED but failure ignored because ".
                                         "'force' in effect");
            } else {
                push @e, "make test had returned bad status, ".
                    "won't install without force"
            }
        }
        if (exists $self->{install}) {
            if (UNIVERSAL::can($self->{install},"text") ?
                $self->{install}->text eq "YES" :
                $self->{install} =~ /^YES/
               ) {
                $CPAN::Frontend->myprint("  Already done\n");
                $CPAN::META->is_installed($self->{build_dir});
                return 1;
            } else {
                # comment in Todo on 2006-02-11; maybe retry?
                push @e, "Already tried without success";
            }
        }

        push @e, $self->{later} if $self->{later};
        push @e, $self->{configure_requires_later} if $self->{configure_requires_later};

        $CPAN::Frontend->myprint(join "", map {"  $_\n"} @e) and return if @e;
        unless (chdir $self->{build_dir}) {
            push @e, "Couldn't chdir to '$self->{build_dir}': $!";
        }
        $CPAN::Frontend->mywarn(join "", map {"  $_\n"} @e) and return if @e;
    }
    $self->debug("Changed directory to $self->{build_dir}")
        if $CPAN::DEBUG;

    if ($^O eq 'MacOS') {
        Mac::BuildTools::make_install($self);
        return;
    }

    my $system;
    if (my $commandline = $self->prefs->{install}{commandline}) {
        $system = $commandline;
        $ENV{PERL} = CPAN::find_perl;
    } elsif ($self->{modulebuild}) {
        my($mbuild_install_build_command) =
            exists $CPAN::HandleConfig::keys{mbuild_install_build_command} &&
                $CPAN::Config->{mbuild_install_build_command} ?
                    $CPAN::Config->{mbuild_install_build_command} :
                        $self->_build_command();
        $system = sprintf("%s install %s",
                          $mbuild_install_build_command,
                          $CPAN::Config->{mbuild_install_arg},
                         );
    } else {
        my($make_install_make_command) =
            CPAN::HandleConfig->prefs_lookup($self,
                                             q{make_install_make_command})
                  || $self->_make_command();
        $system = sprintf("%s install %s",
                          $make_install_make_command,
                          $CPAN::Config->{make_install_arg},
                         );
    }

    my($stderr) = $^O eq "MSWin32" ? "" : " 2>&1 ";
    my $brip = CPAN::HandleConfig->prefs_lookup($self,
                                                q{build_requires_install_policy});
    $brip ||="ask/yes";
    my $id = $self->id;
    my $reqtype = $self->{reqtype} ||= "c"; # in doubt it was a command
    my $want_install = "yes";
    if ($reqtype eq "b") {
        if ($brip eq "no") {
            $want_install = "no";
        } elsif ($brip =~ m|^ask/(.+)|) {
            my $default = $1;
            $default = "yes" unless $default =~ /^(y|n)/i;
            $want_install =
                CPAN::Shell::colorable_makemaker_prompt
                      ("$id is just needed temporarily during building or testing. ".
                       "Do you want to install it permanently? (Y/n)",
                       $default);
        }
    }
    unless ($want_install =~ /^y/i) {
        my $is_only = "is only 'build_requires'";
        $CPAN::Frontend->mywarn("Not installing because $is_only\n");
        $self->{install} = CPAN::Distrostatus->new("NO -- $is_only");
        delete $self->{force_update};
        return;
    }
    local $ENV{PERL5LIB} = defined($ENV{PERL5LIB})
                           ? $ENV{PERL5LIB}
                           : ($ENV{PERLLIB} || "");

    $CPAN::META->set_perl5lib;
    my($pipe) = FileHandle->new("$system $stderr |");
    my($makeout) = "";
    while (<$pipe>) {
        print $_; # intentionally NOT use Frontend->myprint because it
                  # looks irritating when we markup in color what we
                  # just pass through from an external program
        $makeout .= $_;
    }
    $pipe->close;
    my $close_ok = $? == 0;
    $self->introduce_myself;
    if ( $close_ok ) {
        $CPAN::Frontend->myprint("  $system -- OK\n");
        $CPAN::META->is_installed($self->{build_dir});
        $self->{install} = CPAN::Distrostatus->new("YES");
    } else {
        $self->{install} = CPAN::Distrostatus->new("NO");
        $CPAN::Frontend->mywarn("  $system -- NOT OK\n");
        my $mimc =
            CPAN::HandleConfig->prefs_lookup($self,
                                             q{make_install_make_command});
        if (
            $makeout =~ /permission/s
            && $> > 0
            && (
                ! $mimc
                || $mimc eq (CPAN::HandleConfig->prefs_lookup($self,
                                                              q{make}))
               )
           ) {
            $CPAN::Frontend->myprint(
                                     qq{----\n}.
                                     qq{  You may have to su }.
                                     qq{to root to install the package\n}.
                                     qq{  (Or you may want to run something like\n}.
                                     qq{    o conf make_install_make_command 'sudo make'\n}.
                                     qq{  to raise your permissions.}
                                    );
        }
    }
    delete $self->{force_update};
    # $DB::single = 1;
    $self->store_persistent_state;
}

sub introduce_myself {
    my($self) = @_;
    $CPAN::Frontend->myprint(sprintf("  %s\n",$self->pretty_id));
}

#-> sub CPAN::Distribution::dir ;
sub dir {
    shift->{build_dir};
}

#-> sub CPAN::Distribution::perldoc ;
sub perldoc {
    my($self) = @_;

    my($dist) = $self->id;
    my $package = $self->called_for;

    $self->_display_url( $CPAN::Defaultdocs . $package );
}

#-> sub CPAN::Distribution::_check_binary ;
sub _check_binary {
    my ($dist,$shell,$binary) = @_;
    my ($pid,$out);

    $CPAN::Frontend->myprint(qq{ + _check_binary($binary)\n})
      if $CPAN::DEBUG;

    if ($CPAN::META->has_inst("File::Which")) {
        return File::Which::which($binary);
    } else {
        local *README;
        $pid = open README, "which $binary|"
            or $CPAN::Frontend->mywarn(qq{Could not fork 'which $binary': $!\n});
        return unless $pid;
        while (<README>) {
            $out .= $_;
        }
        close README
            or $CPAN::Frontend->mywarn("Could not run 'which $binary': $!\n")
                and return;
    }

    $CPAN::Frontend->myprint(qq{   + $out \n})
      if $CPAN::DEBUG && $out;

    return $out;
}

#-> sub CPAN::Distribution::_display_url ;
sub _display_url {
    my($self,$url) = @_;
    my($res,$saved_file,$pid,$out);

    $CPAN::Frontend->myprint(qq{ + _display_url($url)\n})
      if $CPAN::DEBUG;

    # should we define it in the config instead?
    my $html_converter = "html2text.pl";

    my $web_browser = $CPAN::Config->{'lynx'} || undef;
    my $web_browser_out = $web_browser
        ? CPAN::Distribution->_check_binary($self,$web_browser)
        : undef;

    if ($web_browser_out) {
        # web browser found, run the action
        my $browser = CPAN::HandleConfig->safe_quote($CPAN::Config->{'lynx'});
        $CPAN::Frontend->myprint(qq{system[$browser $url]})
            if $CPAN::DEBUG;
        $CPAN::Frontend->myprint(qq{
Displaying URL
  $url
with browser $browser
});
        $CPAN::Frontend->mysleep(1);
        system("$browser $url");
        if ($saved_file) { 1 while unlink($saved_file) }
    } else {
        # web browser not found, let's try text only
        my $html_converter_out =
            CPAN::Distribution->_check_binary($self,$html_converter);
        $html_converter_out = CPAN::HandleConfig->safe_quote($html_converter_out);

        if ($html_converter_out ) {
            # html2text found, run it
            $saved_file = CPAN::Distribution->_getsave_url( $self, $url );
            $CPAN::Frontend->mydie(qq{ERROR: problems while getting $url\n})
                unless defined($saved_file);

            local *README;
            $pid = open README, "$html_converter $saved_file |"
                or $CPAN::Frontend->mydie(qq{
Could not fork '$html_converter $saved_file': $!});
            my($fh,$filename);
            if ($CPAN::META->has_usable("File::Temp")) {
                $fh = File::Temp->new(
                                      dir      => File::Spec->tmpdir,
                                      template => 'cpan_htmlconvert_XXXX',
                                      suffix => '.txt',
                                      unlink => 0,
                                     );
                $filename = $fh->filename;
            } else {
                $filename = "cpan_htmlconvert_$$.txt";
                $fh = FileHandle->new();
                open $fh, ">$filename" or die;
            }
            while (<README>) {
                $fh->print($_);
            }
            close README or
                $CPAN::Frontend->mydie(qq{Could not run '$html_converter $saved_file': $!});
            my $tmpin = $fh->filename;
            $CPAN::Frontend->myprint(sprintf(qq{
Run '%s %s' and
saved output to %s\n},
                                             $html_converter,
                                             $saved_file,
                                             $tmpin,
                                            )) if $CPAN::DEBUG;
            close $fh;
            local *FH;
            open FH, $tmpin
                or $CPAN::Frontend->mydie(qq{Could not open "$tmpin": $!});
            my $fh_pager = FileHandle->new;
            local($SIG{PIPE}) = "IGNORE";
            my $pager = $CPAN::Config->{'pager'} || "cat";
            $fh_pager->open("|$pager")
                or $CPAN::Frontend->mydie(qq{
Could not open pager '$pager': $!});
            $CPAN::Frontend->myprint(qq{
Displaying URL
  $url
with pager "$pager"
});
            $CPAN::Frontend->mysleep(1);
            $fh_pager->print(<FH>);
            $fh_pager->close;
        } else {
            # coldn't find the web browser or html converter
            $CPAN::Frontend->myprint(qq{
You need to install lynx or $html_converter to use this feature.});
        }
    }
}

#-> sub CPAN::Distribution::_getsave_url ;
sub _getsave_url {
    my($dist, $shell, $url) = @_;

    $CPAN::Frontend->myprint(qq{ + _getsave_url($url)\n})
      if $CPAN::DEBUG;

    my($fh,$filename);
    if ($CPAN::META->has_usable("File::Temp")) {
        $fh = File::Temp->new(
                              dir      => File::Spec->tmpdir,
                              template => "cpan_getsave_url_XXXX",
                              suffix => ".html",
                              unlink => 0,
                             );
        $filename = $fh->filename;
    } else {
        $fh = FileHandle->new;
        $filename = "cpan_getsave_url_$$.html";
    }
    my $tmpin = $filename;
    if ($CPAN::META->has_usable('LWP')) {
        $CPAN::Frontend->myprint("Fetching with LWP:
  $url
");
        my $Ua;
        CPAN::LWP::UserAgent->config;
        eval { $Ua = CPAN::LWP::UserAgent->new; };
        if ($@) {
            $CPAN::Frontend->mywarn("ERROR: CPAN::LWP::UserAgent->new dies with $@\n");
            return;
        } else {
            my($var);
            $Ua->proxy('http', $var)
                if $var = $CPAN::Config->{http_proxy} || $ENV{http_proxy};
            $Ua->no_proxy($var)
                if $var = $CPAN::Config->{no_proxy} || $ENV{no_proxy};
        }

        my $req = HTTP::Request->new(GET => $url);
        $req->header('Accept' => 'text/html');
        my $res = $Ua->request($req);
        if ($res->is_success) {
            $CPAN::Frontend->myprint(" + request successful.\n")
                if $CPAN::DEBUG;
            print $fh $res->content;
            close $fh;
            $CPAN::Frontend->myprint(qq{ + saved content to $tmpin \n})
                if $CPAN::DEBUG;
            return $tmpin;
        } else {
            $CPAN::Frontend->myprint(sprintf(
                                             "LWP failed with code[%s], message[%s]\n",
                                             $res->code,
                                             $res->message,
                                            ));
            return;
        }
    } else {
        $CPAN::Frontend->mywarn("  LWP not available\n");
        return;
    }
}

#-> sub CPAN::Distribution::_build_command
sub _build_command {
    my($self) = @_;
    if ($^O eq "MSWin32") { # special code needed at least up to
                            # Module::Build 0.2611 and 0.2706; a fix
                            # in M:B has been promised 2006-01-30
        my($perl) = $self->perl or $CPAN::Frontend->mydie("Couldn't find executable perl\n");
        return "$perl ./Build";
    }
    return "./Build";
}

#-> sub CPAN::Distribution::_should_report
sub _should_report {
    my($self, $phase) = @_;
    die "_should_report() requires a 'phase' argument"
        if ! defined $phase;

    # configured
    my $test_report = CPAN::HandleConfig->prefs_lookup($self,
                                                       q{test_report});
    return unless $test_report;

    # don't repeat if we cached a result
    return $self->{should_report}
        if exists $self->{should_report};

    # available
    if ( ! $CPAN::META->has_inst("CPAN::Reporter")) {
        $CPAN::Frontend->mywarn(
            "CPAN::Reporter not installed.  No reports will be sent.\n"
        );
        return $self->{should_report} = 0;
    }

    # capable
    my $crv = CPAN::Reporter->VERSION;
    if ( CPAN::Version->vlt( $crv, 0.99 ) ) {
        # don't cache $self->{should_report} -- need to check each phase
        if ( $phase eq 'test' ) {
            return 1;
        }
        else {
            $CPAN::Frontend->mywarn(
                "Reporting on the '$phase' phase requires CPAN::Reporter 0.99, but \n" .
                "you only have version $crv\.  Only 'test' phase reports will be sent.\n"
            );
            return;
        }
    }

    # appropriate
    if ($self->is_dot_dist) {
        $CPAN::Frontend->mywarn("Reporting via CPAN::Reporter is disabled ".
                                "for local directories\n");
        return $self->{should_report} = 0;
    }
    if ($self->prefs->{patches}
        &&
        @{$self->prefs->{patches}}
        &&
        $self->{patched}
       ) {
        $CPAN::Frontend->mywarn("Reporting via CPAN::Reporter is disabled ".
                                "when the source has been patched\n");
        return $self->{should_report} = 0;
    }

    # proceed and cache success
    return $self->{should_report} = 1;
}

#-> sub CPAN::Distribution::reports
sub reports {
    my($self) = @_;
    my $pathname = $self->id;
    $CPAN::Frontend->myprint("Distribution: $pathname\n");

    unless ($CPAN::META->has_inst("CPAN::DistnameInfo")) {
        $CPAN::Frontend->mydie("CPAN::DistnameInfo not installed; cannot continue");
    }
    unless ($CPAN::META->has_usable("LWP")) {
        $CPAN::Frontend->mydie("LWP not installed; cannot continue");
    }
    unless ($CPAN::META->has_usable("File::Temp")) {
        $CPAN::Frontend->mydie("File::Temp not installed; cannot continue");
    }

    my $d = CPAN::DistnameInfo->new($pathname);

    my $dist      = $d->dist;      # "CPAN-DistnameInfo"
    my $version   = $d->version;   # "0.02"
    my $maturity  = $d->maturity;  # "released"
    my $filename  = $d->filename;  # "CPAN-DistnameInfo-0.02.tar.gz"
    my $cpanid    = $d->cpanid;    # "GBARR"
    my $distvname = $d->distvname; # "CPAN-DistnameInfo-0.02"

    my $url = sprintf "http://cpantesters.perl.org/show/%s.yaml", $dist;

    CPAN::LWP::UserAgent->config;
    my $Ua;
    eval { $Ua = CPAN::LWP::UserAgent->new; };
    if ($@) {
        $CPAN::Frontend->mydie("CPAN::LWP::UserAgent->new dies with $@\n");
    }
    $CPAN::Frontend->myprint("Fetching '$url'...");
    my $resp = $Ua->get($url);
    unless ($resp->is_success) {
        $CPAN::Frontend->mydie(sprintf "Could not download '%s': %s\n", $url, $resp->code);
    }
    $CPAN::Frontend->myprint("DONE\n\n");
    my $yaml = $resp->content;
    # was fuer ein Umweg!
    my $fh = File::Temp->new(
                             dir      => File::Spec->tmpdir,
                             template => 'cpan_reports_XXXX',
                             suffix => '.yaml',
                             unlink => 0,
                            );
    my $tfilename = $fh->filename;
    print $fh $yaml;
    close $fh or $CPAN::Frontend->mydie("Could not close '$tfilename': $!");
    my $unserialized = CPAN->_yaml_loadfile($tfilename)->[0];
    unlink $tfilename or $CPAN::Frontend->mydie("Could not unlink '$tfilename': $!");
    my %other_versions;
    my $this_version_seen;
    for my $rep (@$unserialized) {
        my $rversion = $rep->{version};
        if ($rversion eq $version) {
            unless ($this_version_seen++) {
                $CPAN::Frontend->myprint ("$rep->{version}:\n");
            }
            $CPAN::Frontend->myprint
                (sprintf("%1s%1s%-4s %s on %s %s (%s)\n",
                         $rep->{archname} eq $Config::Config{archname}?"*":"",
                         $rep->{action}eq"PASS"?"+":$rep->{action}eq"FAIL"?"-":"",
                         $rep->{action},
                         $rep->{perl},
                         ucfirst $rep->{osname},
                         $rep->{osvers},
                         $rep->{archname},
                        ));
        } else {
            $other_versions{$rep->{version}}++;
        }
    }
    unless ($this_version_seen) {
        $CPAN::Frontend->myprint("No reports found for version '$version'
Reports for other versions:\n");
        for my $v (sort keys %other_versions) {
            $CPAN::Frontend->myprint(" $v\: $other_versions{$v}\n");
        }
    }
    $url =~ s/\.yaml/.html/;
    $CPAN::Frontend->myprint("See $url for details\n");
}

package CPAN::Bundle;
use strict;

sub look {
    my $self = shift;
    $CPAN::Frontend->myprint($self->as_string);
}

#-> CPAN::Bundle::undelay
sub undelay {
    my $self = shift;
    delete $self->{later};
    for my $c ( $self->contains ) {
        my $obj = CPAN::Shell->expandany($c) or next;
        $obj->undelay;
    }
}

# mark as dirty/clean
#-> sub CPAN::Bundle::color_cmd_tmps ;
sub color_cmd_tmps {
    my($self) = shift;
    my($depth) = shift || 0;
    my($color) = shift || 0;
    my($ancestors) = shift || [];
    # a module needs to recurse to its cpan_file, a distribution needs
    # to recurse into its prereq_pms, a bundle needs to recurse into its modules

    return if exists $self->{incommandcolor}
        && $color==1
        && $self->{incommandcolor}==$color;
    if ($depth>=$CPAN::MAX_RECURSION) {
        die(CPAN::Exception::RecursiveDependency->new($ancestors));
    }
    # warn "color_cmd_tmps $depth $color " . $self->id; # sleep 1;

    for my $c ( $self->contains ) {
        my $obj = CPAN::Shell->expandany($c) or next;
        CPAN->debug("c[$c]obj[$obj]") if $CPAN::DEBUG;
        $obj->color_cmd_tmps($depth+1,$color,[@$ancestors, $self->id]);
    }
    # never reached code?
    #if ($color==0) {
      #delete $self->{badtestcnt};
    #}
    $self->{incommandcolor} = $color;
}

#-> sub CPAN::Bundle::as_string ;
sub as_string {
    my($self) = @_;
    $self->contains;
    # following line must be "=", not "||=" because we have a moving target
    $self->{INST_VERSION} = $self->inst_version;
    return $self->SUPER::as_string;
}

#-> sub CPAN::Bundle::contains ;
sub contains {
    my($self) = @_;
    my($inst_file) = $self->inst_file || "";
    my($id) = $self->id;
    $self->debug("inst_file[$inst_file]id[$id]") if $CPAN::DEBUG;
    if ($inst_file && CPAN::Version->vlt($self->inst_version,$self->cpan_version)) {
        undef $inst_file;
    }
    unless ($inst_file) {
        # Try to get at it in the cpan directory
        $self->debug("no inst_file") if $CPAN::DEBUG;
        my $cpan_file;
        $CPAN::Frontend->mydie("I don't know a bundle with ID $id\n") unless
              $cpan_file = $self->cpan_file;
        if ($cpan_file eq "N/A") {
            $CPAN::Frontend->mydie("Bundle $id not found on disk and not on CPAN.
  Maybe stale symlink? Maybe removed during session? Giving up.\n");
        }
        my $dist = $CPAN::META->instance('CPAN::Distribution',
                                         $self->cpan_file);
        $self->debug("before get id[$dist->{ID}]") if $CPAN::DEBUG;
        $dist->get;
        $self->debug("after get id[$dist->{ID}]") if $CPAN::DEBUG;
        my($todir) = $CPAN::Config->{'cpan_home'};
        my(@me,$from,$to,$me);
        @me = split /::/, $self->id;
        $me[-1] .= ".pm";
        $me = File::Spec->catfile(@me);
        $from = $self->find_bundle_file($dist->{build_dir},join('/',@me));
        $to = File::Spec->catfile($todir,$me);
        File::Path::mkpath(File::Basename::dirname($to));
        File::Copy::copy($from, $to)
              or Carp::confess("Couldn't copy $from to $to: $!");
        $inst_file = $to;
    }
    my @result;
    my $fh = FileHandle->new;
    local $/ = "\n";
    open($fh,$inst_file) or die "Could not open '$inst_file': $!";
    my $in_cont = 0;
    $self->debug("inst_file[$inst_file]") if $CPAN::DEBUG;
    while (<$fh>) {
        $in_cont = m/^=(?!head1\s+CONTENTS)/ ? 0 :
            m/^=head1\s+CONTENTS/ ? 1 : $in_cont;
        next unless $in_cont;
        next if /^=/;
        s/\#.*//;
        next if /^\s+$/;
        chomp;
        push @result, (split " ", $_, 2)[0];
    }
    close $fh;
    delete $self->{STATUS};
    $self->{CONTAINS} = \@result;
    $self->debug("CONTAINS[@result]") if $CPAN::DEBUG;
    unless (@result) {
        $CPAN::Frontend->mywarn(qq{
The bundle file "$inst_file" may be a broken
bundlefile. It seems not to contain any bundle definition.
Please check the file and if it is bogus, please delete it.
Sorry for the inconvenience.
});
    }
    @result;
}

#-> sub CPAN::Bundle::find_bundle_file
# $where is in local format, $what is in unix format
sub find_bundle_file {
    my($self,$where,$what) = @_;
    $self->debug("where[$where]what[$what]") if $CPAN::DEBUG;
### The following two lines let CPAN.pm become Bundle/CPAN.pm :-(
###    my $bu = File::Spec->catfile($where,$what);
###    return $bu if -f $bu;
    my $manifest = File::Spec->catfile($where,"MANIFEST");
    unless (-f $manifest) {
        require ExtUtils::Manifest;
        my $cwd = CPAN::anycwd();
        $self->safe_chdir($where);
        ExtUtils::Manifest::mkmanifest();
        $self->safe_chdir($cwd);
    }
    my $fh = FileHandle->new($manifest)
        or Carp::croak("Couldn't open $manifest: $!");
    local($/) = "\n";
    my $bundle_filename = $what;
    $bundle_filename =~ s|Bundle.*/||;
    my $bundle_unixpath;
    while (<$fh>) {
        next if /^\s*\#/;
        my($file) = /(\S+)/;
        if ($file =~ m|\Q$what\E$|) {
            $bundle_unixpath = $file;
            # return File::Spec->catfile($where,$bundle_unixpath); # bad
            last;
        }
        # retry if she managed to have no Bundle directory
        $bundle_unixpath = $file if $file =~ m|\Q$bundle_filename\E$|;
    }
    return File::Spec->catfile($where, split /\//, $bundle_unixpath)
        if $bundle_unixpath;
    Carp::croak("Couldn't find a Bundle file in $where");
}

# needs to work quite differently from Module::inst_file because of
# cpan_home/Bundle/ directory and the possibility that we have
# shadowing effect. As it makes no sense to take the first in @INC for
# Bundles, we parse them all for $VERSION and take the newest.

#-> sub CPAN::Bundle::inst_file ;
sub inst_file {
    my($self) = @_;
    my($inst_file);
    my(@me);
    @me = split /::/, $self->id;
    $me[-1] .= ".pm";
    my($incdir,$bestv);
    foreach $incdir ($CPAN::Config->{'cpan_home'},@INC) {
        my $bfile = File::Spec->catfile($incdir, @me);
        CPAN->debug("bfile[$bfile]") if $CPAN::DEBUG;
        next unless -f $bfile;
        my $foundv = MM->parse_version($bfile);
        if (!$bestv || CPAN::Version->vgt($foundv,$bestv)) {
            $self->{INST_FILE} = $bfile;
            $self->{INST_VERSION} = $bestv = $foundv;
        }
    }
    $self->{INST_FILE};
}

#-> sub CPAN::Bundle::inst_version ;
sub inst_version {
    my($self) = @_;
    $self->inst_file; # finds INST_VERSION as side effect
    $self->{INST_VERSION};
}

#-> sub CPAN::Bundle::rematein ;
sub rematein {
    my($self,$meth) = @_;
    $self->debug("self[$self] meth[$meth]") if $CPAN::DEBUG;
    my($id) = $self->id;
    Carp::croak "Can't $meth $id, don't have an associated bundle file. :-(\n"
        unless $self->inst_file || $self->cpan_file;
    my($s,%fail);
    for $s ($self->contains) {
        my($type) = $s =~ m|/| ? 'CPAN::Distribution' :
            $s =~ m|^Bundle::| ? 'CPAN::Bundle' : 'CPAN::Module';
        if ($type eq 'CPAN::Distribution') {
            $CPAN::Frontend->mywarn(qq{
The Bundle }.$self->id.qq{ contains
explicitly a file '$s'.
Going to $meth that.
});
            $CPAN::Frontend->mysleep(5);
        }
        # possibly noisy action:
        $self->debug("type[$type] s[$s]") if $CPAN::DEBUG;
        my $obj = $CPAN::META->instance($type,$s);
        $obj->{reqtype} = $self->{reqtype};
        $obj->$meth();
    }
}

# If a bundle contains another that contains an xs_file we have here,
# we just don't bother I suppose
#-> sub CPAN::Bundle::xs_file
sub xs_file {
    return 0;
}

#-> sub CPAN::Bundle::force ;
sub fforce   { shift->rematein('fforce',@_); }
#-> sub CPAN::Bundle::force ;
sub force   { shift->rematein('force',@_); }
#-> sub CPAN::Bundle::notest ;
sub notest  { shift->rematein('notest',@_); }
#-> sub CPAN::Bundle::get ;
sub get     { shift->rematein('get',@_); }
#-> sub CPAN::Bundle::make ;
sub make    { shift->rematein('make',@_); }
#-> sub CPAN::Bundle::test ;
sub test    {
    my $self = shift;
    # $self->{badtestcnt} ||= 0;
    $self->rematein('test',@_);
}
#-> sub CPAN::Bundle::install ;
sub install {
  my $self = shift;
  $self->rematein('install',@_);
}
#-> sub CPAN::Bundle::clean ;
sub clean   { shift->rematein('clean',@_); }

#-> sub CPAN::Bundle::uptodate ;
sub uptodate {
    my($self) = @_;
    return 0 unless $self->SUPER::uptodate; # we mut have the current Bundle def
    my $c;
    foreach $c ($self->contains) {
        my $obj = CPAN::Shell->expandany($c);
        return 0 unless $obj->uptodate;
    }
    return 1;
}

#-> sub CPAN::Bundle::readme ;
sub readme  {
    my($self) = @_;
    my($file) = $self->cpan_file or $CPAN::Frontend->myprint(qq{
No File found for bundle } . $self->id . qq{\n}), return;
    $self->debug("self[$self] file[$file]") if $CPAN::DEBUG;
    $CPAN::META->instance('CPAN::Distribution',$file)->readme;
}

package CPAN::Module;
use strict;

# Accessors
#-> sub CPAN::Module::userid
sub userid {
    my $self = shift;
    my $ro = $self->ro;
    return unless $ro;
    return $ro->{userid} || $ro->{CPAN_USERID};
}
#-> sub CPAN::Module::description
sub description {
    my $self = shift;
    my $ro = $self->ro or return "";
    $ro->{description}
}

#-> sub CPAN::Module::distribution
sub distribution {
    my($self) = @_;
    CPAN::Shell->expand("Distribution",$self->cpan_file);
}

#-> sub CPAN::Module::undelay
sub undelay {
    my $self = shift;
    delete $self->{later};
    if ( my $dist = CPAN::Shell->expand("Distribution", $self->cpan_file) ) {
        $dist->undelay;
    }
}

# mark as dirty/clean
#-> sub CPAN::Module::color_cmd_tmps ;
sub color_cmd_tmps {
    my($self) = shift;
    my($depth) = shift || 0;
    my($color) = shift || 0;
    my($ancestors) = shift || [];
    # a module needs to recurse to its cpan_file

    return if exists $self->{incommandcolor}
        && $color==1
        && $self->{incommandcolor}==$color;
    return if $color==0 && !$self->{incommandcolor};
    if ($color>=1) {
        if ( $self->uptodate ) {
            $self->{incommandcolor} = $color;
            return;
        } elsif (my $have_version = $self->available_version) {
            # maybe what we have is good enough
            if (@$ancestors) {
                my $who_asked_for_me = $ancestors->[-1];
                my $obj = CPAN::Shell->expandany($who_asked_for_me);
                if (0) {
                } elsif ($obj->isa("CPAN::Bundle")) {
                    # bundles cannot specify a minimum version
                    return;
                } elsif ($obj->isa("CPAN::Distribution")) {
                    if (my $prereq_pm = $obj->prereq_pm) {
                        for my $k (keys %$prereq_pm) {
                            if (my $want_version = $prereq_pm->{$k}{$self->id}) {
                                if (CPAN::Version->vcmp($have_version,$want_version) >= 0) {
                                    $self->{incommandcolor} = $color;
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        $self->{incommandcolor} = $color; # set me before recursion,
                                          # so we can break it
    }
    if ($depth>=$CPAN::MAX_RECURSION) {
        die(CPAN::Exception::RecursiveDependency->new($ancestors));
    }
    # warn "color_cmd_tmps $depth $color " . $self->id; # sleep 1;

    if ( my $dist = CPAN::Shell->expand("Distribution", $self->cpan_file) ) {
        $dist->color_cmd_tmps($depth+1,$color,[@$ancestors, $self->id]);
    }
    # unreached code?
    # if ($color==0) {
    #    delete $self->{badtestcnt};
    # }
    $self->{incommandcolor} = $color;
}

#-> sub CPAN::Module::as_glimpse ;
sub as_glimpse {
    my($self) = @_;
    my(@m);
    my $class = ref($self);
    $class =~ s/^CPAN:://;
    my $color_on = "";
    my $color_off = "";
    if (
        $CPAN::Shell::COLOR_REGISTERED
        &&
        $CPAN::META->has_inst("Term::ANSIColor")
        &&
        $self->description
       ) {
        $color_on = Term::ANSIColor::color("green");
        $color_off = Term::ANSIColor::color("reset");
    }
    my $uptodateness = " ";
    unless ($class eq "Bundle") {
        my $u = $self->uptodate;
        $uptodateness = $u ? "=" : "<" if defined $u;
    };
    my $id = do {
        my $d = $self->distribution;
        $d ? $d -> pretty_id : $self->cpan_userid;
    };
    push @m, sprintf("%-7s %1s %s%-22s%s (%s)\n",
                     $class,
                     $uptodateness,
                     $color_on,
                     $self->id,
                     $color_off,
                     $id,
                    );
    join "", @m;
}

#-> sub CPAN::Module::dslip_status
sub dslip_status {
    my($self) = @_;
    my($stat);
    # development status
    @{$stat->{D}}{qw,i c a b R M S,}     = qw,idea
                                              pre-alpha alpha beta released
                                              mature standard,;
    # support level
    @{$stat->{S}}{qw,m d u n a,}         = qw,mailing-list
                                              developer comp.lang.perl.*
                                              none abandoned,;
    # language
    @{$stat->{L}}{qw,p c + o h,}         = qw,perl C C++ other hybrid,;
    # interface
    @{$stat->{I}}{qw,f r O p h n,}       = qw,functions
                                              references+ties
                                              object-oriented pragma
                                              hybrid none,;
    # public licence
    @{$stat->{P}}{qw,p g l b a 2 o d r n,} = qw,Standard-Perl
                                              GPL LGPL
                                              BSD Artistic Artistic_2
                                              open-source
                                              distribution_allowed
                                              restricted_distribution
                                              no_licence,;
    for my $x (qw(d s l i p)) {
        $stat->{$x}{' '} = 'unknown';
        $stat->{$x}{'?'} = 'unknown';
    }
    my $ro = $self->ro;
    return +{} unless $ro && $ro->{statd};
    return {
            D  => $ro->{statd},
            S  => $ro->{stats},
            L  => $ro->{statl},
            I  => $ro->{stati},
            P  => $ro->{statp},
            DV => $stat->{D}{$ro->{statd}},
            SV => $stat->{S}{$ro->{stats}},
            LV => $stat->{L}{$ro->{statl}},
            IV => $stat->{I}{$ro->{stati}},
            PV => $stat->{P}{$ro->{statp}},
           };
}

#-> sub CPAN::Module::as_string ;
sub as_string {
    my($self) = @_;
    my(@m);
    CPAN->debug("$self entering as_string") if $CPAN::DEBUG;
    my $class = ref($self);
    $class =~ s/^CPAN:://;
    local($^W) = 0;
    push @m, $class, " id = $self->{ID}\n";
    my $sprintf = "    %-12s %s\n";
    push @m, sprintf($sprintf, 'DESCRIPTION', $self->description)
        if $self->description;
    my $sprintf2 = "    %-12s %s (%s)\n";
    my($userid);
    $userid = $self->userid;
    if ( $userid ) {
        my $author;
        if ($author = CPAN::Shell->expand('Author',$userid)) {
            my $email = "";
            my $m; # old perls
            if ($m = $author->email) {
                $email = " <$m>";
            }
            push @m, sprintf(
                             $sprintf2,
                             'CPAN_USERID',
                             $userid,
                             $author->fullname . $email
                            );
        }
    }
    push @m, sprintf($sprintf, 'CPAN_VERSION', $self->cpan_version)
        if $self->cpan_version;
    if (my $cpan_file = $self->cpan_file) {
        push @m, sprintf($sprintf, 'CPAN_FILE', $cpan_file);
        if (my $dist = CPAN::Shell->expand("Distribution",$cpan_file)) {
            my $upload_date = $dist->upload_date;
            if ($upload_date) {
                push @m, sprintf($sprintf, 'UPLOAD_DATE', $upload_date);
            }
        }
    }
    my $sprintf3 = "    %-12s %1s%1s%1s%1s%1s (%s,%s,%s,%s,%s)\n";
    my $dslip = $self->dslip_status;
    push @m, sprintf(
                     $sprintf3,
                     'DSLIP_STATUS',
                     @{$dslip}{qw(D S L I P DV SV LV IV PV)},
                    ) if $dslip->{D};
    my $local_file = $self->inst_file;
    unless ($self->{MANPAGE}) {
        my $manpage;
        if ($local_file) {
            $manpage = $self->manpage_headline($local_file);
        } else {
            # If we have already untarred it, we should look there
            my $dist = $CPAN::META->instance('CPAN::Distribution',
                                             $self->cpan_file);
            # warn "dist[$dist]";
            # mff=manifest file; mfh=manifest handle
            my($mff,$mfh);
            if (
                $dist->{build_dir}
                and
                (-f  ($mff = File::Spec->catfile($dist->{build_dir}, "MANIFEST")))
                and
                $mfh = FileHandle->new($mff)
               ) {
                CPAN->debug("mff[$mff]") if $CPAN::DEBUG;
                my $lfre = $self->id; # local file RE
                $lfre =~ s/::/./g;
                $lfre .= "\\.pm\$";
                my($lfl); # local file file
                local $/ = "\n";
                my(@mflines) = <$mfh>;
                for (@mflines) {
                    s/^\s+//;
                    s/\s.*//s;
                }
                while (length($lfre)>5 and !$lfl) {
                    ($lfl) = grep /$lfre/, @mflines;
                    CPAN->debug("lfl[$lfl]lfre[$lfre]") if $CPAN::DEBUG;
                    $lfre =~ s/.+?\.//;
                }
                $lfl =~ s/\s.*//; # remove comments
                $lfl =~ s/\s+//g; # chomp would maybe be too system-specific
                my $lfl_abs = File::Spec->catfile($dist->{build_dir},$lfl);
                # warn "lfl_abs[$lfl_abs]";
                if (-f $lfl_abs) {
                    $manpage = $self->manpage_headline($lfl_abs);
                }
            }
        }
        $self->{MANPAGE} = $manpage if $manpage;
    }
    my($item);
    for $item (qw/MANPAGE/) {
        push @m, sprintf($sprintf, $item, $self->{$item})
            if exists $self->{$item};
    }
    for $item (qw/CONTAINS/) {
        push @m, sprintf($sprintf, $item, join(" ",@{$self->{$item}}))
            if exists $self->{$item} && @{$self->{$item}};
    }
    push @m, sprintf($sprintf, 'INST_FILE',
                     $local_file || "(not installed)");
    push @m, sprintf($sprintf, 'INST_VERSION',
                     $self->inst_version) if $local_file;
    join "", @m, "\n";
}

#-> sub CPAN::Module::manpage_headline
sub manpage_headline {
    my($self,$local_file) = @_;
    my(@local_file) = $local_file;
    $local_file =~ s/\.pm(?!\n)\Z/.pod/;
    push @local_file, $local_file;
    my(@result,$locf);
    for $locf (@local_file) {
        next unless -f $locf;
        my $fh = FileHandle->new($locf)
            or $Carp::Frontend->mydie("Couldn't open $locf: $!");
        my $inpod = 0;
        local $/ = "\n";
        while (<$fh>) {
            $inpod = m/^=(?!head1\s+NAME\s*$)/ ? 0 :
                m/^=head1\s+NAME\s*$/ ? 1 : $inpod;
            next unless $inpod;
            next if /^=/;
            next if /^\s+$/;
            chomp;
            push @result, $_;
        }
        close $fh;
        last if @result;
    }
    for (@result) {
        s/^\s+//;
        s/\s+$//;
    }
    join " ", @result;
}

#-> sub CPAN::Module::cpan_file ;
# Note: also inherited by CPAN::Bundle
sub cpan_file {
    my $self = shift;
    # CPAN->debug(sprintf "id[%s]", $self->id) if $CPAN::DEBUG;
    unless ($self->ro) {
        CPAN::Index->reload;
    }
    my $ro = $self->ro;
    if ($ro && defined $ro->{CPAN_FILE}) {
        return $ro->{CPAN_FILE};
    } else {
        my $userid = $self->userid;
        if ( $userid ) {
            if ($CPAN::META->exists("CPAN::Author",$userid)) {
                my $author = $CPAN::META->instance("CPAN::Author",
                                                   $userid);
                my $fullname = $author->fullname;
                my $email = $author->email;
                unless (defined $fullname && defined $email) {
                    return sprintf("Contact Author %s",
                                   $userid,
                                  );
                }
                return "Contact Author $fullname <$email>";
            } else {
                return "Contact Author $userid (Email address not available)";
            }
        } else {
            return "N/A";
        }
    }
}

#-> sub CPAN::Module::cpan_version ;
sub cpan_version {
    my $self = shift;

    my $ro = $self->ro;
    unless ($ro) {
        # Can happen with modules that are not on CPAN
        $ro = {};
    }
    $ro->{CPAN_VERSION} = 'undef'
        unless defined $ro->{CPAN_VERSION};
    $ro->{CPAN_VERSION};
}

#-> sub CPAN::Module::force ;
sub force {
    my($self) = @_;
    $self->{force_update} = 1;
}

#-> sub CPAN::Module::fforce ;
sub fforce {
    my($self) = @_;
    $self->{force_update} = 2;
}

#-> sub CPAN::Module::notest ;
sub notest {
    my($self) = @_;
    # $CPAN::Frontend->mywarn("XDEBUG: set notest for Module");
    $self->{notest}++;
}

#-> sub CPAN::Module::rematein ;
sub rematein {
    my($self,$meth) = @_;
    $CPAN::Frontend->myprint(sprintf("Running %s for module '%s'\n",
                                     $meth,
                                     $self->id));
    my $cpan_file = $self->cpan_file;
    if ($cpan_file eq "N/A" || $cpan_file =~ /^Contact Author/) {
        $CPAN::Frontend->mywarn(sprintf qq{
  The module %s isn\'t available on CPAN.

  Either the module has not yet been uploaded to CPAN, or it is
  temporary unavailable. Please contact the author to find out
  more about the status. Try 'i %s'.
},
                                $self->id,
                                $self->id,
                               );
        return;
    }
    my $pack = $CPAN::META->instance('CPAN::Distribution',$cpan_file);
    $pack->called_for($self->id);
    if (exists $self->{force_update}) {
        if ($self->{force_update} == 2) {
            $pack->fforce($meth);
        } else {
            $pack->force($meth);
        }
    }
    $pack->notest($meth) if exists $self->{notest} && $self->{notest};

    $pack->{reqtype} ||= "";
    CPAN->debug("dist-reqtype[$pack->{reqtype}]".
                "self-reqtype[$self->{reqtype}]") if $CPAN::DEBUG;
        if ($pack->{reqtype}) {
            if ($pack->{reqtype} eq "b" && $self->{reqtype} =~ /^[rc]$/) {
                $pack->{reqtype} = $self->{reqtype};
                if (
                    exists $pack->{install}
                    &&
                    (
                     UNIVERSAL::can($pack->{install},"failed") ?
                     $pack->{install}->failed :
                     $pack->{install} =~ /^NO/
                    )
                   ) {
                    delete $pack->{install};
                    $CPAN::Frontend->mywarn
                        ("Promoting $pack->{ID} from 'build_requires' to 'requires'");
                }
            }
        } else {
            $pack->{reqtype} = $self->{reqtype};
        }

    my $success = eval {
        $pack->$meth();
    };
    my $err = $@;
    $pack->unforce if $pack->can("unforce") && exists $self->{force_update};
    $pack->unnotest if $pack->can("unnotest") && exists $self->{notest};
    delete $self->{force_update};
    delete $self->{notest};
    if ($err) {
        die $err;
    }
    return $success;
}

#-> sub CPAN::Module::perldoc ;
sub perldoc { shift->rematein('perldoc') }
#-> sub CPAN::Module::readme ;
sub readme  { shift->rematein('readme') }
#-> sub CPAN::Module::look ;
sub look    { shift->rematein('look') }
#-> sub CPAN::Module::cvs_import ;
sub cvs_import { shift->rematein('cvs_import') }
#-> sub CPAN::Module::get ;
sub get     { shift->rematein('get',@_) }
#-> sub CPAN::Module::make ;
sub make    { shift->rematein('make') }
#-> sub CPAN::Module::test ;
sub test   {
    my $self = shift;
    # $self->{badtestcnt} ||= 0;
    $self->rematein('test',@_);
}

#-> sub CPAN::Module::uptodate ;
sub uptodate {
    my ($self) = @_;
    local ($_);
    my $inst = $self->inst_version or return undef;
    my $cpan = $self->cpan_version;
    local ($^W) = 0;
    CPAN::Version->vgt($cpan,$inst) and return 0;
    CPAN->debug(join("",
                     "returning uptodate. inst_file[",
                     $self->inst_file,
                     "cpan[$cpan] inst[$inst]")) if $CPAN::DEBUG;
    return 1;
}

#-> sub CPAN::Module::install ;
sub install {
    my($self) = @_;
    my($doit) = 0;
    if ($self->uptodate
        &&
        not exists $self->{force_update}
       ) {
        $CPAN::Frontend->myprint(sprintf("%s is up to date (%s).\n",
                                         $self->id,
                                         $self->inst_version,
                                        ));
    } else {
        $doit = 1;
    }
    my $ro = $self->ro;
    if ($ro && $ro->{stats} && $ro->{stats} eq "a") {
        $CPAN::Frontend->mywarn(qq{
\n\n\n     ***WARNING***
     The module $self->{ID} has no active maintainer.\n\n\n
});
        $CPAN::Frontend->mysleep(5);
    }
    $self->rematein('install') if $doit;
}
#-> sub CPAN::Module::clean ;
sub clean  { shift->rematein('clean') }

#-> sub CPAN::Module::inst_file ;
sub inst_file {
    my($self) = @_;
    $self->_file_in_path([@INC]);
}

#-> sub CPAN::Module::available_file ;
sub available_file {
    my($self) = @_;
    my $sep = $Config::Config{path_sep};
    my $perllib = $ENV{PERL5LIB};
    $perllib = $ENV{PERLLIB} unless defined $perllib;
    my @perllib = split(/$sep/,$perllib) if defined $perllib;
    $self->_file_in_path([@perllib,@INC]);
}

#-> sub CPAN::Module::file_in_path ;
sub _file_in_path {
    my($self,$path) = @_;
    my($dir,@packpath);
    @packpath = split /::/, $self->{ID};
    $packpath[-1] .= ".pm";
    if (@packpath == 1 && $packpath[0] eq "readline.pm") {
        unshift @packpath, "Term", "ReadLine"; # historical reasons
    }
    foreach $dir (@$path) {
        my $pmfile = File::Spec->catfile($dir,@packpath);
        if (-f $pmfile) {
            return $pmfile;
        }
    }
    return;
}

#-> sub CPAN::Module::xs_file ;
sub xs_file {
    my($self) = @_;
    my($dir,@packpath);
    @packpath = split /::/, $self->{ID};
    push @packpath, $packpath[-1];
    $packpath[-1] .= "." . $Config::Config{'dlext'};
    foreach $dir (@INC) {
        my $xsfile = File::Spec->catfile($dir,'auto',@packpath);
        if (-f $xsfile) {
            return $xsfile;
        }
    }
    return;
}

#-> sub CPAN::Module::inst_version ;
sub inst_version {
    my($self) = @_;
    my $parsefile = $self->inst_file or return;
    my $have = $self->parse_version($parsefile);
    $have;
}

#-> sub CPAN::Module::inst_version ;
sub available_version {
    my($self) = @_;
    my $parsefile = $self->available_file or return;
    my $have = $self->parse_version($parsefile);
    $have;
}

#-> sub CPAN::Module::parse_version ;
sub parse_version {
    my($self,$parsefile) = @_;
    my $have = MM->parse_version($parsefile);
    $have = "undef" unless defined $have && length $have;
    $have =~ s/^ //; # since the %vd hack these two lines here are needed
    $have =~ s/ $//; # trailing whitespace happens all the time

    $have = CPAN::Version->readable($have);

    $have =~ s/\s*//g; # stringify to float around floating point issues
    $have; # no stringify needed, \s* above matches always
}

#-> sub CPAN::Module::reports
sub reports {
    my($self) = @_;
    $self->distribution->reports;
}

package CPAN;
use strict;

1;


__END__

=head1 NAME

CPAN - query, download and build perl modules from CPAN sites

=head1 SYNOPSIS

Interactive mode:

  perl -MCPAN -e shell

--or--

  cpan

Basic commands:

  # Modules:

  cpan> install Acme::Meta                       # in the shell

  CPAN::Shell->install("Acme::Meta");            # in perl

  # Distributions:

  cpan> install NWCLARK/Acme-Meta-0.02.tar.gz    # in the shell

  CPAN::Shell->
    install("NWCLARK/Acme-Meta-0.02.tar.gz");    # in perl

  # module objects:

  $mo = CPAN::Shell->expandany($mod);
  $mo = CPAN::Shell->expand("Module",$mod);      # same thing

  # distribution objects:

  $do = CPAN::Shell->expand("Module",$mod)->distribution;
  $do = CPAN::Shell->expandany($distro);         # same thing
  $do = CPAN::Shell->expand("Distribution",
                            $distro);            # same thing

=head1 DESCRIPTION

The CPAN module automates or at least simplifies the make and install
of perl modules and extensions. It includes some primitive searching
capabilities and knows how to use Net::FTP or LWP or some external
download clients to fetch the distributions from the net.

These are fetched from one or more of the mirrored CPAN (Comprehensive
Perl Archive Network) sites and unpacked in a dedicated directory.

The CPAN module also supports the concept of named and versioned
I<bundles> of modules. Bundles simplify the handling of sets of
related modules. See Bundles below.

The package contains a session manager and a cache manager. The
session manager keeps track of what has been fetched, built and
installed in the current session. The cache manager keeps track of the
disk space occupied by the make processes and deletes excess space
according to a simple FIFO mechanism.

All methods provided are accessible in a programmer style and in an
interactive shell style.

=head2 CPAN::shell([$prompt, $command]) Starting Interactive Mode

The interactive mode is entered by running

    perl -MCPAN -e shell

or

    cpan

which puts you into a readline interface. If C<Term::ReadKey> and
either C<Term::ReadLine::Perl> or C<Term::ReadLine::Gnu> are installed
it supports both history and command completion.

Once you are on the command line, type C<h> to get a one page help
screen and the rest should be self-explanatory.

The function call C<shell> takes two optional arguments, one is the
prompt, the second is the default initial command line (the latter
only works if a real ReadLine interface module is installed).

The most common uses of the interactive modes are

=over 2

=item Searching for authors, bundles, distribution files and modules

There are corresponding one-letter commands C<a>, C<b>, C<d>, and C<m>
for each of the four categories and another, C<i> for any of the
mentioned four. Each of the four entities is implemented as a class
with slightly differing methods for displaying an object.

Arguments you pass to these commands are either strings exactly matching
the identification string of an object or regular expressions that are
then matched case-insensitively against various attributes of the
objects. The parser recognizes a regular expression only if you
enclose it between two slashes.

The principle is that the number of found objects influences how an
item is displayed. If the search finds one item, the result is
displayed with the rather verbose method C<as_string>, but if we find
more than one, we display each object with the terse method
C<as_glimpse>.

=item C<get>, C<make>, C<test>, C<install>, C<clean> modules or distributions

These commands take any number of arguments and investigate what is
necessary to perform the action. If the argument is a distribution
file name (recognized by embedded slashes), it is processed. If it is
a module, CPAN determines the distribution file in which this module
is included and processes that, following any dependencies named in
the module's META.yml or Makefile.PL (this behavior is controlled by
the configuration parameter C<prerequisites_policy>.)

C<get> downloads a distribution file and untars or unzips it, C<make>
builds it, C<test> runs the test suite, and C<install> installs it.

Any C<make> or C<test> are run unconditionally. An

  install <distribution_file>

also is run unconditionally. But for

  install <module>

CPAN checks if an install is actually needed for it and prints
I<module up to date> in the case that the distribution file containing
the module doesn't need to be updated.

CPAN also keeps track of what it has done within the current session
and doesn't try to build a package a second time regardless if it
succeeded or not. It does not repeat a test run if the test
has been run successfully before. Same for install runs.

The C<force> pragma may precede another command (currently: C<get>,
C<make>, C<test>, or C<install>) and executes the command from scratch
and tries to continue in case of some errors. See the section below on
the C<force> and the C<fforce> pragma.

The C<notest> pragma may be used to skip the test part in the build
process.

Example:

    cpan> notest install Tk

A C<clean> command results in a

  make clean

being executed within the distribution file's working directory.

=item C<readme>, C<perldoc>, C<look> module or distribution

C<readme> displays the README file of the associated distribution.
C<Look> gets and untars (if not yet done) the distribution file,
changes to the appropriate directory and opens a subshell process in
that directory. C<perldoc> displays the pod documentation of the
module in html or plain text format.

=item C<ls> author

=item C<ls> globbing_expression

The first form lists all distribution files in and below an author's
CPAN directory as they are stored in the CHECKUMS files distributed on
CPAN. The listing goes recursive into all subdirectories.

The second form allows to limit or expand the output with shell
globbing as in the following examples:

      ls JV/make*
      ls GSAR/*make*
      ls */*make*

The last example is very slow and outputs extra progress indicators
that break the alignment of the result.

Note that globbing only lists directories explicitly asked for, for
example FOO/* will not list FOO/bar/Acme-Sthg-n.nn.tar.gz. This may be
regarded as a bug and may be changed in future versions.

=item C<failed>

The C<failed> command reports all distributions that failed on one of
C<make>, C<test> or C<install> for some reason in the currently
running shell session.

=item Persistence between sessions

If the C<YAML> or the C<YAML::Syck> module is installed a record of
the internal state of all modules is written to disk after each step.
The files contain a signature of the currently running perl version
for later perusal.

If the configurations variable C<build_dir_reuse> is set to a true
value, then CPAN.pm reads the collected YAML files. If the stored
signature matches the currently running perl the stored state is
loaded into memory such that effectively persistence between sessions
is established.

=item The C<force> and the C<fforce> pragma

To speed things up in complex installation scenarios, CPAN.pm keeps
track of what it has already done and refuses to do some things a
second time. A C<get>, a C<make>, and an C<install> are not repeated.
A C<test> is only repeated if the previous test was unsuccessful. The
diagnostic message when CPAN.pm refuses to do something a second time
is one of I<Has already been >C<unwrapped|made|tested successfully> or
something similar. Another situation where CPAN refuses to act is an
C<install> if the according C<test> was not successful.

In all these cases, the user can override the goatish behaviour by
prepending the command with the word force, for example:

  cpan> force get Foo
  cpan> force make AUTHOR/Bar-3.14.tar.gz
  cpan> force test Baz
  cpan> force install Acme::Meta

Each I<forced> command is executed with the according part of its
memory erased.

The C<fforce> pragma is a variant that emulates a C<force get> which
erases the entire memory followed by the action specified, effectively
restarting the whole get/make/test/install procedure from scratch.

=item Lockfile

Interactive sessions maintain a lockfile, per default C<~/.cpan/.lock>.
Batch jobs can run without a lockfile and do not disturb each other.

The shell offers to run in I<degraded mode> when another process is
holding the lockfile. This is an experimental feature that is not yet
tested very well. This second shell then does not write the history
file, does not use the metadata file and has a different prompt.

=item Signals

CPAN.pm installs signal handlers for SIGINT and SIGTERM. While you are
in the cpan-shell it is intended that you can press C<^C> anytime and
return to the cpan-shell prompt. A SIGTERM will cause the cpan-shell
to clean up and leave the shell loop. You can emulate the effect of a
SIGTERM by sending two consecutive SIGINTs, which usually means by
pressing C<^C> twice.

CPAN.pm ignores a SIGPIPE. If the user sets C<inactivity_timeout>, a
SIGALRM is used during the run of the C<perl Makefile.PL> or C<perl
Build.PL> subprocess.

=back

=head2 CPAN::Shell

The commands that are available in the shell interface are methods in
the package CPAN::Shell. If you enter the shell command, all your
input is split by the Text::ParseWords::shellwords() routine which
acts like most shells do. The first word is being interpreted as the
method to be called and the rest of the words are treated as arguments
to this method. Continuation lines are supported if a line ends with a
literal backslash.

=head2 autobundle

C<autobundle> writes a bundle file into the
C<$CPAN::Config-E<gt>{cpan_home}/Bundle> directory. The file contains
a list of all modules that are both available from CPAN and currently
installed within @INC. The name of the bundle file is based on the
current date and a counter.

=head2 hosts

Note: this feature is still in alpha state and may change in future
versions of CPAN.pm

This commands provides a statistical overview over recent download
activities. The data for this is collected in the YAML file
C<FTPstats.yml> in your C<cpan_home> directory. If no YAML module is
configured or YAML not installed, then no stats are provided.

=head2 mkmyconfig

mkmyconfig() writes your own CPAN::MyConfig file into your ~/.cpan/
directory so that you can save your own preferences instead of the
system wide ones.

=head2 recent ***EXPERIMENTAL COMMAND***

The C<recent> command downloads a list of recent uploads to CPAN and
displays them I<slowly>. While the command is running $SIG{INT} is
defined to mean that the loop shall be left after having displayed the
current item.

B<Note>: This command requires XML::LibXML installed.

B<Note>: This whole command currently is a bit klunky and will
probably change in future versions of CPAN.pm but the general
approach will likely stay.

B<Note>: See also L<smoke>

=head2 recompile

recompile() is a very special command in that it takes no argument and
runs the make/test/install cycle with brute force over all installed
dynamically loadable extensions (aka XS modules) with 'force' in
effect. The primary purpose of this command is to finish a network
installation. Imagine, you have a common source tree for two different
architectures. You decide to do a completely independent fresh
installation. You start on one architecture with the help of a Bundle
file produced earlier. CPAN installs the whole Bundle for you, but
when you try to repeat the job on the second architecture, CPAN
responds with a C<"Foo up to date"> message for all modules. So you
invoke CPAN's recompile on the second architecture and you're done.

Another popular use for C<recompile> is to act as a rescue in case your
perl breaks binary compatibility. If one of the modules that CPAN uses
is in turn depending on binary compatibility (so you cannot run CPAN
commands), then you should try the CPAN::Nox module for recovery.

=head2 report Bundle|Distribution|Module

The C<report> command temporarily turns on the C<test_report> config
variable, then runs the C<force test> command with the given
arguments. The C<force> pragma is used to re-run the tests and repeat
every step that might have failed before.

=head2 smoke ***EXPERIMENTAL COMMAND***

B<*** WARNING: this command downloads and executes software from CPAN to
your computer of completely unknown status. You should never do
this with your normal account and better have a dedicated well
separated and secured machine to do this. ***>

The C<smoke> command takes the list of recent uploads to CPAN as
provided by the C<recent> command and tests them all. While the
command is running $SIG{INT} is defined to mean that the current item
shall be skipped.

B<Note>: This whole command currently is a bit klunky and will
probably change in future versions of CPAN.pm but the general
approach will likely stay.

B<Note>: See also L<recent>

=head2 upgrade [Module|/Regex/]...

The C<upgrade> command first runs an C<r> command with the given
arguments and then installs the newest versions of all modules that
were listed by that.

=head2 The four C<CPAN::*> Classes: Author, Bundle, Module, Distribution

Although it may be considered internal, the class hierarchy does matter
for both users and programmer. CPAN.pm deals with above mentioned four
classes, and all those classes share a set of methods. A classical
single polymorphism is in effect. A metaclass object registers all
objects of all kinds and indexes them with a string. The strings
referencing objects have a separated namespace (well, not completely
separated):

         Namespace                         Class

   words containing a "/" (slash)      Distribution
    words starting with Bundle::          Bundle
          everything else            Module or Author

Modules know their associated Distribution objects. They always refer
to the most recent official release. Developers may mark their releases
as unstable development versions (by inserting an underbar into the
module version number which will also be reflected in the distribution
name when you run 'make dist'), so the really hottest and newest
distribution is not always the default.  If a module Foo circulates
on CPAN in both version 1.23 and 1.23_90, CPAN.pm offers a convenient
way to install version 1.23 by saying

    install Foo

This would install the complete distribution file (say
BAR/Foo-1.23.tar.gz) with all accompanying material. But if you would
like to install version 1.23_90, you need to know where the
distribution file resides on CPAN relative to the authors/id/
directory. If the author is BAR, this might be BAR/Foo-1.23_90.tar.gz;
so you would have to say

    install BAR/Foo-1.23_90.tar.gz

The first example will be driven by an object of the class
CPAN::Module, the second by an object of class CPAN::Distribution.

=head2 Integrating local directories

Note: this feature is still in alpha state and may change in future
versions of CPAN.pm

Distribution objects are normally distributions from the CPAN, but
there is a slightly degenerate case for Distribution objects, too, of
projects held on the local disk. These distribution objects have the
same name as the local directory and end with a dot. A dot by itself
is also allowed for the current directory at the time CPAN.pm was
used. All actions such as C<make>, C<test>, and C<install> are applied
directly to that directory. This gives the command C<cpan .> an
interesting touch: while the normal mantra of installing a CPAN module
without CPAN.pm is one of

    perl Makefile.PL                 perl Build.PL
           ( go and get prerequisites )
    make                             ./Build
    make test                        ./Build test
    make install                     ./Build install

the command C<cpan .> does all of this at once. It figures out which
of the two mantras is appropriate, fetches and installs all
prerequisites, cares for them recursively and finally finishes the
installation of the module in the current directory, be it a CPAN
module or not.

The typical usage case is for private modules or working copies of
projects from remote repositories on the local disk.

=head1 CONFIGURATION

When the CPAN module is used for the first time, a configuration
dialog tries to determine a couple of site specific options. The
result of the dialog is stored in a hash reference C< $CPAN::Config >
in a file CPAN/Config.pm.

The default values defined in the CPAN/Config.pm file can be
overridden in a user specific file: CPAN/MyConfig.pm. Such a file is
best placed in $HOME/.cpan/CPAN/MyConfig.pm, because $HOME/.cpan is
added to the search path of the CPAN module before the use() or
require() statements. The mkmyconfig command writes this file for you.

The C<o conf> command has various bells and whistles:

=over

=item completion support

If you have a ReadLine module installed, you can hit TAB at any point
of the commandline and C<o conf> will offer you completion for the
built-in subcommands and/or config variable names.

=item displaying some help: o conf help

Displays a short help

=item displaying current values: o conf [KEY]

Displays the current value(s) for this config variable. Without KEY
displays all subcommands and config variables.

Example:

  o conf shell

If KEY starts and ends with a slash the string in between is
interpreted as a regular expression and only keys matching this regex
are displayed

Example:

  o conf /color/

=item changing of scalar values: o conf KEY VALUE

Sets the config variable KEY to VALUE. The empty string can be
specified as usual in shells, with C<''> or C<"">

Example:

  o conf wget /usr/bin/wget

=item changing of list values: o conf KEY SHIFT|UNSHIFT|PUSH|POP|SPLICE|LIST

If a config variable name ends with C<list>, it is a list. C<o conf
KEY shift> removes the first element of the list, C<o conf KEY pop>
removes the last element of the list. C<o conf KEYS unshift LIST>
prepends a list of values to the list, C<o conf KEYS push LIST>
appends a list of valued to the list.

Likewise, C<o conf KEY splice LIST> passes the LIST to the according
splice command.

Finally, any other list of arguments is taken as a new list value for
the KEY variable discarding the previous value.

Examples:

  o conf urllist unshift http://cpan.dev.local/CPAN
  o conf urllist splice 3 1
  o conf urllist http://cpan1.local http://cpan2.local ftp://ftp.perl.org

=item reverting to saved: o conf defaults

Reverts all config variables to the state in the saved config file.

=item saving the config: o conf commit

Saves all config variables to the current config file (CPAN/Config.pm
or CPAN/MyConfig.pm that was loaded at start).

=back

The configuration dialog can be started any time later again by
issuing the command C< o conf init > in the CPAN shell. A subset of
the configuration dialog can be run by issuing C<o conf init WORD>
where WORD is any valid config variable or a regular expression.

=head2 Config Variables

Currently the following keys in the hash reference $CPAN::Config are
defined:

  applypatch         path to external prg
  auto_commit        commit all changes to config variables to disk
  build_cache        size of cache for directories to build modules
  build_dir          locally accessible directory to build modules
  build_dir_reuse    boolean if distros in build_dir are persistent
  build_requires_install_policy
                     to install or not to install when a module is
                     only needed for building. yes|no|ask/yes|ask/no
  bzip2              path to external prg
  cache_metadata     use serializer to cache metadata
  commands_quote     prefered character to use for quoting external
                     commands when running them. Defaults to double
                     quote on Windows, single tick everywhere else;
                     can be set to space to disable quoting
  check_sigs         if signatures should be verified
  colorize_debug     Term::ANSIColor attributes for debugging output
  colorize_output    boolean if Term::ANSIColor should colorize output
  colorize_print     Term::ANSIColor attributes for normal output
  colorize_warn      Term::ANSIColor attributes for warnings
  commandnumber_in_prompt
                     boolean if you want to see current command number
  cpan_home          local directory reserved for this package
  curl               path to external prg
  dontload_hash      DEPRECATED
  dontload_list      arrayref: modules in the list will not be
                     loaded by the CPAN::has_inst() routine
  ftp                path to external prg
  ftp_passive        if set, the envariable FTP_PASSIVE is set for downloads
  ftp_proxy          proxy host for ftp requests
  getcwd             see below
  gpg                path to external prg
  gzip               location of external program gzip
  histfile           file to maintain history between sessions
  histsize           maximum number of lines to keep in histfile
  http_proxy         proxy host for http requests
  inactivity_timeout breaks interactive Makefile.PLs or Build.PLs
                     after this many seconds inactivity. Set to 0 to
                     never break.
  index_expire       after this many days refetch index files
  inhibit_startup_message
                     if true, does not print the startup message
  keep_source_where  directory in which to keep the source (if we do)
  load_module_verbosity
                     report loading of optional modules used by CPAN.pm
  lynx               path to external prg
  make               location of external make program
  make_arg           arguments that should always be passed to 'make'
  make_install_make_command
                     the make command for running 'make install', for
                     example 'sudo make'
  make_install_arg   same as make_arg for 'make install'
  makepl_arg         arguments passed to 'perl Makefile.PL'
  mbuild_arg         arguments passed to './Build'
  mbuild_install_arg arguments passed to './Build install'
  mbuild_install_build_command
                     command to use instead of './Build' when we are
                     in the install stage, for example 'sudo ./Build'
  mbuildpl_arg       arguments passed to 'perl Build.PL'
  ncftp              path to external prg
  ncftpget           path to external prg
  no_proxy           don't proxy to these hosts/domains (comma separated list)
  pager              location of external program more (or any pager)
  password           your password if you CPAN server wants one
  patch              path to external prg
  prefer_installer   legal values are MB and EUMM: if a module comes
                     with both a Makefile.PL and a Build.PL, use the
                     former (EUMM) or the latter (MB); if the module
                     comes with only one of the two, that one will be
                     used in any case
  prerequisites_policy
                     what to do if you are missing module prerequisites
                     ('follow' automatically, 'ask' me, or 'ignore')
  prefs_dir          local directory to store per-distro build options
  proxy_user         username for accessing an authenticating proxy
  proxy_pass         password for accessing an authenticating proxy
  randomize_urllist  add some randomness to the sequence of the urllist
  scan_cache         controls scanning of cache ('atstart' or 'never')
  shell              your favorite shell
  show_unparsable_versions
                     boolean if r command tells which modules are versionless
  show_upload_date   boolean if commands should try to determine upload date
  show_zero_versions boolean if r command tells for which modules $version==0
  tar                location of external program tar
  tar_verbosity      verbosity level for the tar command
  term_is_latin      deprecated: if true Unicode is translated to ISO-8859-1
                     (and nonsense for characters outside latin range)
  term_ornaments     boolean to turn ReadLine ornamenting on/off
  test_report        email test reports (if CPAN::Reporter is installed)
  unzip              location of external program unzip
  urllist            arrayref to nearby CPAN sites (or equivalent locations)
  use_sqlite         use CPAN::SQLite for metadata storage (fast and lean)
  username           your username if you CPAN server wants one
  wait_list          arrayref to a wait server to try (See CPAN::WAIT)
  wget               path to external prg
  yaml_load_code     enable YAML code deserialisation
  yaml_module        which module to use to read/write YAML files

You can set and query each of these options interactively in the cpan
shell with the C<o conf> or the C<o conf init> command as specified below.

=over 2

=item C<o conf E<lt>scalar optionE<gt>>

prints the current value of the I<scalar option>

=item C<o conf E<lt>scalar optionE<gt> E<lt>valueE<gt>>

Sets the value of the I<scalar option> to I<value>

=item C<o conf E<lt>list optionE<gt>>

prints the current value of the I<list option> in MakeMaker's
neatvalue format.

=item C<o conf E<lt>list optionE<gt> [shift|pop]>

shifts or pops the array in the I<list option> variable

=item C<o conf E<lt>list optionE<gt> [unshift|push|splice] E<lt>listE<gt>>

works like the corresponding perl commands.

=item interactive editing: o conf init [MATCH|LIST]

Runs an interactive configuration dialog for matching variables.
Without argument runs the dialog over all supported config variables.
To specify a MATCH the argument must be enclosed by slashes.

Examples:

  o conf init ftp_passive ftp_proxy
  o conf init /color/

Note: this method of setting config variables often provides more
explanation about the functioning of a variable than the manpage.

=back

=head2 CPAN::anycwd($path): Note on config variable getcwd

CPAN.pm changes the current working directory often and needs to
determine its own current working directory. Per default it uses
Cwd::cwd but if this doesn't work on your system for some reason,
alternatives can be configured according to the following table:

=over 4

=item cwd

Calls Cwd::cwd

=item getcwd

Calls Cwd::getcwd

=item fastcwd

Calls Cwd::fastcwd

=item backtickcwd

Calls the external command cwd.

=back

=head2 Note on the format of the urllist parameter

urllist parameters are URLs according to RFC 1738. We do a little
guessing if your URL is not compliant, but if you have problems with
C<file> URLs, please try the correct format. Either:

    file://localhost/whatever/ftp/pub/CPAN/

or

    file:///home/ftp/pub/CPAN/

=head2 The urllist parameter has CD-ROM support

The C<urllist> parameter of the configuration table contains a list of
URLs that are to be used for downloading. If the list contains any
C<file> URLs, CPAN always tries to get files from there first. This
feature is disabled for index files. So the recommendation for the
owner of a CD-ROM with CPAN contents is: include your local, possibly
outdated CD-ROM as a C<file> URL at the end of urllist, e.g.

  o conf urllist push file://localhost/CDROM/CPAN

CPAN.pm will then fetch the index files from one of the CPAN sites
that come at the beginning of urllist. It will later check for each
module if there is a local copy of the most recent version.

Another peculiarity of urllist is that the site that we could
successfully fetch the last file from automatically gets a preference
token and is tried as the first site for the next request. So if you
add a new site at runtime it may happen that the previously preferred
site will be tried another time. This means that if you want to disallow
a site for the next transfer, it must be explicitly removed from
urllist.

=head2 Maintaining the urllist parameter

If you have YAML.pm (or some other YAML module configured in
C<yaml_module>) installed, CPAN.pm collects a few statistical data
about recent downloads. You can view the statistics with the C<hosts>
command or inspect them directly by looking into the C<FTPstats.yml>
file in your C<cpan_home> directory.

To get some interesting statistics it is recommended to set the
C<randomize_urllist> parameter that introduces some amount of
randomness into the URL selection.

=head2 The C<requires> and C<build_requires> dependency declarations

Since CPAN.pm version 1.88_51 modules declared as C<build_requires> by
a distribution are treated differently depending on the config
variable C<build_requires_install_policy>. By setting
C<build_requires_install_policy> to C<no> such a module is not being
installed. It is only built and tested and then kept in the list of
tested but uninstalled modules. As such it is available during the
build of the dependent module by integrating the path to the
C<blib/arch> and C<blib/lib> directories in the environment variable
PERL5LIB. If C<build_requires_install_policy> is set ti C<yes>, then
both modules declared as C<requires> and those declared as
C<build_requires> are treated alike. By setting to C<ask/yes> or
C<ask/no>, CPAN.pm asks the user and sets the default accordingly.

=head2 Configuration for individual distributions (I<Distroprefs>)

(B<Note:> This feature has been introduced in CPAN.pm 1.8854 and is
still considered beta quality)

Distributions on the CPAN usually behave according to what we call the
CPAN mantra. Or since the event of Module::Build we should talk about
two mantras:

    perl Makefile.PL     perl Build.PL
    make                 ./Build
    make test            ./Build test
    make install         ./Build install

But some modules cannot be built with this mantra. They try to get
some extra data from the user via the environment, extra arguments or
interactively thus disturbing the installation of large bundles like
Phalanx100 or modules with many dependencies like Plagger.

The distroprefs system of C<CPAN.pm> addresses this problem by
allowing the user to specify extra informations and recipes in YAML
files to either

=over

=item

pass additional arguments to one of the four commands,

=item

set environment variables

=item

instantiate an Expect object that reads from the console, waits for
some regular expressions and enters some answers

=item

temporarily override assorted C<CPAN.pm> configuration variables

=item

specify dependencies that the original maintainer forgot to specify

=item

disable the installation of an object altogether

=back

See the YAML and Data::Dumper files that come with the C<CPAN.pm>
distribution in the C<distroprefs/> directory for examples.

=head2 Filenames

The YAML files themselves must have the C<.yml> extension, all other
files are ignored (for two exceptions see I<Fallback Data::Dumper and
Storable> below). The containing directory can be specified in
C<CPAN.pm> in the C<prefs_dir> config variable. Try C<o conf init
prefs_dir> in the CPAN shell to set and activate the distroprefs
system.

Every YAML file may contain arbitrary documents according to the YAML
specification and every single document is treated as an entity that
can specify the treatment of a single distribution.

The names of the files can be picked freely, C<CPAN.pm> always reads
all files (in alphabetical order) and takes the key C<match> (see
below in I<Language Specs>) as a hashref containing match criteria
that determine if the current distribution matches the YAML document
or not.

=head2 Fallback Data::Dumper and Storable

If neither your configured C<yaml_module> nor YAML.pm is installed
CPAN.pm falls back to using Data::Dumper and Storable and looks for
files with the extensions C<.dd> or C<.st> in the C<prefs_dir>
directory. These files are expected to contain one or more hashrefs.
For Data::Dumper generated files, this is expected to be done with by
defining C<$VAR1>, C<$VAR2>, etc. The YAML shell would produce these
with the command

    ysh < somefile.yml > somefile.dd

For Storable files the rule is that they must be constructed such that
C<Storable::retrieve(file)> returns an array reference and the array
elements represent one distropref object each. The conversion from
YAML would look like so:

    perl -MYAML=LoadFile -MStorable=nstore -e '
        @y=LoadFile(shift);
        nstore(\@y, shift)' somefile.yml somefile.st

In bootstrapping situations it is usually sufficient to translate only
a few YAML files to Data::Dumper for the crucial modules like
C<YAML::Syck>, C<YAML.pm> and C<Expect.pm>. If you prefer Storable
over Data::Dumper, remember to pull out a Storable version that writes
an older format than all the other Storable versions that will need to
read them.

=head2 Blueprint

The following example contains all supported keywords and structures
with the exception of C<eexpect> which can be used instead of
C<expect>.

  ---
  comment: "Demo"
  match:
    module: "Dancing::Queen"
    distribution: "^CHACHACHA/Dancing-"
    perl: "/usr/local/cariba-perl/bin/perl"
    perlconfig:
      archname: "freebsd"
  disabled: 1
  cpanconfig:
    make: gmake
  pl:
    args:
      - "--somearg=specialcase"

    env: {}

    expect:
      - "Which is your favorite fruit"
      - "apple\n"

  make:
    args:
      - all
      - extra-all

    env: {}

    expect: []

    commendline: "echo SKIPPING make"

  test:
    args: []

    env: {}

    expect: []

  install:
    args: []

    env:
      WANT_TO_INSTALL: YES

    expect:
      - "Do you really want to install"
      - "y\n"

  patches:
    - "ABCDE/Fedcba-3.14-ABCDE-01.patch"

  depends:
    configure_requires:
      LWP: 5.8
    build_requires:
      Test::Exception: 0.25
    requires:
      Spiffy: 0.30


=head2 Language Specs

Every YAML document represents a single hash reference. The valid keys
in this hash are as follows:

=over

=item comment [scalar]

A comment

=item cpanconfig [hash]

Temporarily override assorted C<CPAN.pm> configuration variables.

Supported are: C<build_requires_install_policy>, C<check_sigs>,
C<make>, C<make_install_make_command>, C<prefer_installer>,
C<test_report>. Please report as a bug when you need another one
supported.

=item depends [hash] *** EXPERIMENTAL FEATURE ***

All three types, namely C<configure_requires>, C<build_requires>, and
C<requires> are supported in the way specified in the META.yml
specification. The current implementation I<merges> the specified
dependencies with those declared by the package maintainer. In a
future implementation this may be changed to override the original
declaration.

=item disabled [boolean]

Specifies that this distribution shall not be processed at all.

=item goto [string]

The canonical name of a delegate distribution that shall be installed
instead. Useful when a new version, although it tests OK itself,
breaks something else or a developer release or a fork is already
uploaded that is better than the last released version.

=item install [hash]

Processing instructions for the C<make install> or C<./Build install>
phase of the CPAN mantra. See below under I<Processiong Instructions>.

=item make [hash]

Processing instructions for the C<make> or C<./Build> phase of the
CPAN mantra. See below under I<Processiong Instructions>.

=item match [hash]

A hashref with one or more of the keys C<distribution>, C<modules>,
C<perl>, and C<perlconfig> that specify if a document is targeted at a
specific CPAN distribution or installation.

The corresponding values are interpreted as regular expressions. The
C<distribution> related one will be matched against the canonical
distribution name, e.g. "AUTHOR/Foo-Bar-3.14.tar.gz".

The C<module> related one will be matched against I<all> modules
contained in the distribution until one module matches.

The C<perl> related one will be matched against C<$^X> (but with the
absolute path).

The value associated with C<perlconfig> is itself a hashref that is
matched against corresponding values in the C<%Config::Config> hash
living in the C< Config.pm > module.

If more than one restriction of C<module>, C<distribution>, and
C<perl> is specified, the results of the separately computed match
values must all match. If this is the case then the hashref
represented by the YAML document is returned as the preference
structure for the current distribution.

=item patches [array]

An array of patches on CPAN or on the local disk to be applied in
order via the external patch program. If the value for the C<-p>
parameter is C<0> or C<1> is determined by reading the patch
beforehand.

Note: if the C<applypatch> program is installed and C<CPAN::Config>
knows about it B<and> a patch is written by the C<makepatch> program,
then C<CPAN.pm> lets C<applypatch> apply the patch. Both C<makepatch>
and C<applypatch> are available from CPAN in the C<JV/makepatch-*>
distribution.

=item pl [hash]

Processing instructions for the C<perl Makefile.PL> or C<perl
Build.PL> phase of the CPAN mantra. See below under I<Processiong
Instructions>.

=item test [hash]

Processing instructions for the C<make test> or C<./Build test> phase
of the CPAN mantra. See below under I<Processiong Instructions>.

=back

=head2 Processing Instructions

=over

=item args [array]

Arguments to be added to the command line

=item commandline

A full commandline that will be executed as it stands by a system
call. During the execution the environment variable PERL will is set
to $^X (but with an absolute path). If C<commandline> is specified,
the content of C<args> is not used.

=item eexpect [hash]

Extended C<expect>. This is a hash reference with four allowed keys,
C<mode>, C<timeout>, C<reuse>, and C<talk>.

C<mode> may have the values C<deterministic> for the case where all
questions come in the order written down and C<anyorder> for the case
where the questions may come in any order. The default mode is
C<deterministic>.

C<timeout> denotes a timeout in seconds. Floating point timeouts are
OK. In the case of a C<mode=deterministic> the timeout denotes the
timeout per question, in the case of C<mode=anyorder> it denotes the
timeout per byte received from the stream or questions.

C<talk> is a reference to an array that contains alternating questions
and answers. Questions are regular expressions and answers are literal
strings. The Expect module will then watch the stream coming from the
execution of the external program (C<perl Makefile.PL>, C<perl
Build.PL>, C<make>, etc.).

In the case of C<mode=deterministic> the CPAN.pm will inject the
according answer as soon as the stream matches the regular expression.

In the case of C<mode=anyorder> CPAN.pm will answer a question as soon
as the timeout is reached for the next byte in the input stream. In
this mode you can use the C<reuse> parameter to decide what shall
happen with a question-answer pair after it has been used. In the
default case (reuse=0) it is removed from the array, so it cannot be
used again accidentally. In this case, if you want to answer the
question C<Do you really want to do that> several times, then it must
be included in the array at least as often as you want this answer to
be given. Setting the parameter C<reuse> to 1 makes this repetition
unnecessary.

=item env [hash]

Environment variables to be set during the command

=item expect [array]

C<< expect: <array> >> is a short notation for

  eexpect:
    mode: deterministic
    timeout: 15
    talk: <array>

=back

=head2 Schema verification with C<Kwalify>

If you have the C<Kwalify> module installed (which is part of the
Bundle::CPANxxl), then all your distroprefs files are checked for
syntactical correctness.

=head2 Example Distroprefs Files

C<CPAN.pm> comes with a collection of example YAML files. Note that these
are really just examples and should not be used without care because
they cannot fit everybody's purpose. After all the authors of the
packages that ask questions had a need to ask, so you should watch
their questions and adjust the examples to your environment and your
needs. You have beend warned:-)

=head1 PROGRAMMER'S INTERFACE

If you do not enter the shell, the available shell commands are both
available as methods (C<CPAN::Shell-E<gt>install(...)>) and as
functions in the calling package (C<install(...)>).  Before calling low-level
commands it makes sense to initialize components of CPAN you need, e.g.:

  CPAN::HandleConfig->load;
  CPAN::Shell::setup_output;
  CPAN::Index->reload;

High-level commands do such initializations automatically.

There's currently only one class that has a stable interface -
CPAN::Shell. All commands that are available in the CPAN shell are
methods of the class CPAN::Shell. Each of the commands that produce
listings of modules (C<r>, C<autobundle>, C<u>) also return a list of
the IDs of all modules within the list.

=over 2

=item expand($type,@things)

The IDs of all objects available within a program are strings that can
be expanded to the corresponding real objects with the
C<CPAN::Shell-E<gt>expand("Module",@things)> method. Expand returns a
list of CPAN::Module objects according to the C<@things> arguments
given. In scalar context it only returns the first element of the
list.

=item expandany(@things)

Like expand, but returns objects of the appropriate type, i.e.
CPAN::Bundle objects for bundles, CPAN::Module objects for modules and
CPAN::Distribution objects for distributions. Note: it does not expand
to CPAN::Author objects.

=item Programming Examples

This enables the programmer to do operations that combine
functionalities that are available in the shell.

    # install everything that is outdated on my disk:
    perl -MCPAN -e 'CPAN::Shell->install(CPAN::Shell->r)'

    # install my favorite programs if necessary:
    for $mod (qw(Net::FTP Digest::SHA Data::Dumper)) {
        CPAN::Shell->install($mod);
    }

    # list all modules on my disk that have no VERSION number
    for $mod (CPAN::Shell->expand("Module","/./")) {
        next unless $mod->inst_file;
        # MakeMaker convention for undefined $VERSION:
        next unless $mod->inst_version eq "undef";
        print "No VERSION in ", $mod->id, "\n";
    }

    # find out which distribution on CPAN contains a module:
    print CPAN::Shell->expand("Module","Apache::Constants")->cpan_file

Or if you want to write a cronjob to watch The CPAN, you could list
all modules that need updating. First a quick and dirty way:

    perl -e 'use CPAN; CPAN::Shell->r;'

If you don't want to get any output in the case that all modules are
up to date, you can parse the output of above command for the regular
expression //modules are up to date// and decide to mail the output
only if it doesn't match. Ick?

If you prefer to do it more in a programmer style in one single
process, maybe something like this suits you better:

  # list all modules on my disk that have newer versions on CPAN
  for $mod (CPAN::Shell->expand("Module","/./")) {
    next unless $mod->inst_file;
    next if $mod->uptodate;
    printf "Module %s is installed as %s, could be updated to %s from CPAN\n",
        $mod->id, $mod->inst_version, $mod->cpan_version;
  }

If that gives you too much output every day, you maybe only want to
watch for three modules. You can write

  for $mod (CPAN::Shell->expand("Module","/Apache|LWP|CGI/")) {

as the first line instead. Or you can combine some of the above
tricks:

  # watch only for a new mod_perl module
  $mod = CPAN::Shell->expand("Module","mod_perl");
  exit if $mod->uptodate;
  # new mod_perl arrived, let me know all update recommendations
  CPAN::Shell->r;

=back

=head2 Methods in the other Classes

=over 4

=item CPAN::Author::as_glimpse()

Returns a one-line description of the author

=item CPAN::Author::as_string()

Returns a multi-line description of the author

=item CPAN::Author::email()

Returns the author's email address

=item CPAN::Author::fullname()

Returns the author's name

=item CPAN::Author::name()

An alias for fullname

=item CPAN::Bundle::as_glimpse()

Returns a one-line description of the bundle

=item CPAN::Bundle::as_string()

Returns a multi-line description of the bundle

=item CPAN::Bundle::clean()

Recursively runs the C<clean> method on all items contained in the bundle.

=item CPAN::Bundle::contains()

Returns a list of objects' IDs contained in a bundle. The associated
objects may be bundles, modules or distributions.

=item CPAN::Bundle::force($method,@args)

Forces CPAN to perform a task that it normally would have refused to
do. Force takes as arguments a method name to be called and any number
of additional arguments that should be passed to the called method.
The internals of the object get the needed changes so that CPAN.pm
does not refuse to take the action. The C<force> is passed recursively
to all contained objects. See also the section above on the C<force>
and the C<fforce> pragma.

=item CPAN::Bundle::get()

Recursively runs the C<get> method on all items contained in the bundle

=item CPAN::Bundle::inst_file()

Returns the highest installed version of the bundle in either @INC or
C<$CPAN::Config->{cpan_home}>. Note that this is different from
CPAN::Module::inst_file.

=item CPAN::Bundle::inst_version()

Like CPAN::Bundle::inst_file, but returns the $VERSION

=item CPAN::Bundle::uptodate()

Returns 1 if the bundle itself and all its members are uptodate.

=item CPAN::Bundle::install()

Recursively runs the C<install> method on all items contained in the bundle

=item CPAN::Bundle::make()

Recursively runs the C<make> method on all items contained in the bundle

=item CPAN::Bundle::readme()

Recursively runs the C<readme> method on all items contained in the bundle

=item CPAN::Bundle::test()

Recursively runs the C<test> method on all items contained in the bundle

=item CPAN::Distribution::as_glimpse()

Returns a one-line description of the distribution

=item CPAN::Distribution::as_string()

Returns a multi-line description of the distribution

=item CPAN::Distribution::author

Returns the CPAN::Author object of the maintainer who uploaded this
distribution

=item CPAN::Distribution::pretty_id()

Returns a string of the form "AUTHORID/TARBALL", where AUTHORID is the
author's PAUSE ID and TARBALL is the distribution filename.

=item CPAN::Distribution::base_id()

Returns the distribution filename without any archive suffix.  E.g
"Foo-Bar-0.01"

=item CPAN::Distribution::clean()

Changes to the directory where the distribution has been unpacked and
runs C<make clean> there.

=item CPAN::Distribution::containsmods()

Returns a list of IDs of modules contained in a distribution file.
Only works for distributions listed in the 02packages.details.txt.gz
file. This typically means that only the most recent version of a
distribution is covered.

=item CPAN::Distribution::cvs_import()

Changes to the directory where the distribution has been unpacked and
runs something like

    cvs -d $cvs_root import -m $cvs_log $cvs_dir $userid v$version

there.

=item CPAN::Distribution::dir()

Returns the directory into which this distribution has been unpacked.

=item CPAN::Distribution::force($method,@args)

Forces CPAN to perform a task that it normally would have refused to
do. Force takes as arguments a method name to be called and any number
of additional arguments that should be passed to the called method.
The internals of the object get the needed changes so that CPAN.pm
does not refuse to take the action. See also the section above on the
C<force> and the C<fforce> pragma.

=item CPAN::Distribution::get()

Downloads the distribution from CPAN and unpacks it. Does nothing if
the distribution has already been downloaded and unpacked within the
current session.

=item CPAN::Distribution::install()

Changes to the directory where the distribution has been unpacked and
runs the external command C<make install> there. If C<make> has not
yet been run, it will be run first. A C<make test> will be issued in
any case and if this fails, the install will be canceled. The
cancellation can be avoided by letting C<force> run the C<install> for
you.

This install method has only the power to install the distribution if
there are no dependencies in the way. To install an object and all of
its dependencies, use CPAN::Shell->install.

Note that install() gives no meaningful return value. See uptodate().

=item CPAN::Distribution::install_tested()

Install all the distributions that have been tested sucessfully but
not yet installed. See also C<is_tested>.

=item CPAN::Distribution::isa_perl()

Returns 1 if this distribution file seems to be a perl distribution.
Normally this is derived from the file name only, but the index from
CPAN can contain a hint to achieve a return value of true for other
filenames too.

=item CPAN::Distribution::is_tested()

List all the distributions that have been tested sucessfully but not
yet installed. See also C<install_tested>.

=item CPAN::Distribution::look()

Changes to the directory where the distribution has been unpacked and
opens a subshell there. Exiting the subshell returns.

=item CPAN::Distribution::make()

First runs the C<get> method to make sure the distribution is
downloaded and unpacked. Changes to the directory where the
distribution has been unpacked and runs the external commands C<perl
Makefile.PL> or C<perl Build.PL> and C<make> there.

=item CPAN::Distribution::perldoc()

Downloads the pod documentation of the file associated with a
distribution (in html format) and runs it through the external
command lynx specified in C<$CPAN::Config->{lynx}>. If lynx
isn't available, it converts it to plain text with external
command html2text and runs it through the pager specified
in C<$CPAN::Config->{pager}>

=item CPAN::Distribution::prefs()

Returns the hash reference from the first matching YAML file that the
user has deposited in the C<prefs_dir/> directory. The first
succeeding match wins. The files in the C<prefs_dir/> are processed
alphabetically and the canonical distroname (e.g.
AUTHOR/Foo-Bar-3.14.tar.gz) is matched against the regular expressions
stored in the $root->{match}{distribution} attribute value.
Additionally all module names contained in a distribution are matched
agains the regular expressions in the $root->{match}{module} attribute
value. The two match values are ANDed together. Each of the two
attributes are optional.

=item CPAN::Distribution::prereq_pm()

Returns the hash reference that has been announced by a distribution
as the the C<requires> and C<build_requires> elements. These can be
declared either by the C<META.yml> (if authoritative) or can be
deposited after the run of C<Build.PL> in the file C<./_build/prereqs>
or after the run of C<Makfile.PL> written as the C<PREREQ_PM> hash in
a comment in the produced C<Makefile>. I<Note>: this method only works
after an attempt has been made to C<make> the distribution. Returns
undef otherwise.

=item CPAN::Distribution::readme()

Downloads the README file associated with a distribution and runs it
through the pager specified in C<$CPAN::Config->{pager}>.

=item CPAN::Distribution::reports()

Downloads report data for this distribution from cpantesters.perl.org
and displays a subset of them.

=item CPAN::Distribution::read_yaml()

Returns the content of the META.yml of this distro as a hashref. Note:
works only after an attempt has been made to C<make> the distribution.
Returns undef otherwise. Also returns undef if the content of META.yml
is not authoritative. (The rules about what exactly makes the content
authoritative are still in flux.)

=item CPAN::Distribution::test()

Changes to the directory where the distribution has been unpacked and
runs C<make test> there.

=item CPAN::Distribution::uptodate()

Returns 1 if all the modules contained in the distribution are
uptodate. Relies on containsmods.

=item CPAN::Index::force_reload()

Forces a reload of all indices.

=item CPAN::Index::reload()

Reloads all indices if they have not been read for more than
C<$CPAN::Config->{index_expire}> days.

=item CPAN::InfoObj::dump()

CPAN::Author, CPAN::Bundle, CPAN::Module, and CPAN::Distribution
inherit this method. It prints the data structure associated with an
object. Useful for debugging. Note: the data structure is considered
internal and thus subject to change without notice.

=item CPAN::Module::as_glimpse()

Returns a one-line description of the module in four columns: The
first column contains the word C<Module>, the second column consists
of one character: an equals sign if this module is already installed
and uptodate, a less-than sign if this module is installed but can be
upgraded, and a space if the module is not installed. The third column
is the name of the module and the fourth column gives maintainer or
distribution information.

=item CPAN::Module::as_string()

Returns a multi-line description of the module

=item CPAN::Module::clean()

Runs a clean on the distribution associated with this module.

=item CPAN::Module::cpan_file()

Returns the filename on CPAN that is associated with the module.

=item CPAN::Module::cpan_version()

Returns the latest version of this module available on CPAN.

=item CPAN::Module::cvs_import()

Runs a cvs_import on the distribution associated with this module.

=item CPAN::Module::description()

Returns a 44 character description of this module. Only available for
modules listed in The Module List (CPAN/modules/00modlist.long.html
or 00modlist.long.txt.gz)

=item CPAN::Module::distribution()

Returns the CPAN::Distribution object that contains the current
version of this module.

=item CPAN::Module::dslip_status()

Returns a hash reference. The keys of the hash are the letters C<D>,
C<S>, C<L>, C<I>, and <P>, for development status, support level,
language, interface and public licence respectively. The data for the
DSLIP status are collected by pause.perl.org when authors register
their namespaces. The values of the 5 hash elements are one-character
words whose meaning is described in the table below. There are also 5
hash elements C<DV>, C<SV>, C<LV>, C<IV>, and <PV> that carry a more
verbose value of the 5 status variables.

Where the 'DSLIP' characters have the following meanings:

  D - Development Stage  (Note: *NO IMPLIED TIMESCALES*):
    i   - Idea, listed to gain consensus or as a placeholder
    c   - under construction but pre-alpha (not yet released)
    a/b - Alpha/Beta testing
    R   - Released
    M   - Mature (no rigorous definition)
    S   - Standard, supplied with Perl 5

  S - Support Level:
    m   - Mailing-list
    d   - Developer
    u   - Usenet newsgroup comp.lang.perl.modules
    n   - None known, try comp.lang.perl.modules
    a   - abandoned; volunteers welcome to take over maintainance

  L - Language Used:
    p   - Perl-only, no compiler needed, should be platform independent
    c   - C and perl, a C compiler will be needed
    h   - Hybrid, written in perl with optional C code, no compiler needed
    +   - C++ and perl, a C++ compiler will be needed
    o   - perl and another language other than C or C++

  I - Interface Style
    f   - plain Functions, no references used
    h   - hybrid, object and function interfaces available
    n   - no interface at all (huh?)
    r   - some use of unblessed References or ties
    O   - Object oriented using blessed references and/or inheritance

  P - Public License
    p   - Standard-Perl: user may choose between GPL and Artistic
    g   - GPL: GNU General Public License
    l   - LGPL: "GNU Lesser General Public License" (previously known as
          "GNU Library General Public License")
    b   - BSD: The BSD License
    a   - Artistic license alone
    2   - Artistic license 2.0 or later
    o   - open source: appoved by www.opensource.org
    d   - allows distribution without restrictions
    r   - restricted distribtion
    n   - no license at all

=item CPAN::Module::force($method,@args)

Forces CPAN to perform a task that it normally would have refused to
do. Force takes as arguments a method name to be called and any number
of additional arguments that should be passed to the called method.
The internals of the object get the needed changes so that CPAN.pm
does not refuse to take the action. See also the section above on the
C<force> and the C<fforce> pragma.

=item CPAN::Module::get()

Runs a get on the distribution associated with this module.

=item CPAN::Module::inst_file()

Returns the filename of the module found in @INC. The first file found
is reported just like perl itself stops searching @INC when it finds a
module.

=item CPAN::Module::available_file()

Returns the filename of the module found in PERL5LIB or @INC. The
first file found is reported. The advantage of this method over
C<inst_file> is that modules that have been tested but not yet
installed are included because PERL5LIB keeps track of tested modules.

=item CPAN::Module::inst_version()

Returns the version number of the installed module in readable format.

=item CPAN::Module::available_version()

Returns the version number of the available module in readable format.

=item CPAN::Module::install()

Runs an C<install> on the distribution associated with this module.

=item CPAN::Module::look()

Changes to the directory where the distribution associated with this
module has been unpacked and opens a subshell there. Exiting the
subshell returns.

=item CPAN::Module::make()

Runs a C<make> on the distribution associated with this module.

=item CPAN::Module::manpage_headline()

If module is installed, peeks into the module's manpage, reads the
headline and returns it. Moreover, if the module has been downloaded
within this session, does the equivalent on the downloaded module even
if it is not installed.

=item CPAN::Module::perldoc()

Runs a C<perldoc> on this module.

=item CPAN::Module::readme()

Runs a C<readme> on the distribution associated with this module.

=item CPAN::Module::reports()

Calls the reports() method on the associated distribution object.

=item CPAN::Module::test()

Runs a C<test> on the distribution associated with this module.

=item CPAN::Module::uptodate()

Returns 1 if the module is installed and up-to-date.

=item CPAN::Module::userid()

Returns the author's ID of the module.

=back

=head2 Cache Manager

Currently the cache manager only keeps track of the build directory
($CPAN::Config->{build_dir}). It is a simple FIFO mechanism that
deletes complete directories below C<build_dir> as soon as the size of
all directories there gets bigger than $CPAN::Config->{build_cache}
(in MB). The contents of this cache may be used for later
re-installations that you intend to do manually, but will never be
trusted by CPAN itself. This is due to the fact that the user might
use these directories for building modules on different architectures.

There is another directory ($CPAN::Config->{keep_source_where}) where
the original distribution files are kept. This directory is not
covered by the cache manager and must be controlled by the user. If
you choose to have the same directory as build_dir and as
keep_source_where directory, then your sources will be deleted with
the same fifo mechanism.

=head2 Bundles

A bundle is just a perl module in the namespace Bundle:: that does not
define any functions or methods. It usually only contains documentation.

It starts like a perl module with a package declaration and a $VERSION
variable. After that the pod section looks like any other pod with the
only difference being that I<one special pod section> exists starting with
(verbatim):

    =head1 CONTENTS

In this pod section each line obeys the format

        Module_Name [Version_String] [- optional text]

The only required part is the first field, the name of a module
(e.g. Foo::Bar, ie. I<not> the name of the distribution file). The rest
of the line is optional. The comment part is delimited by a dash just
as in the man page header.

The distribution of a bundle should follow the same convention as
other distributions.

Bundles are treated specially in the CPAN package. If you say 'install
Bundle::Tkkit' (assuming such a bundle exists), CPAN will install all
the modules in the CONTENTS section of the pod. You can install your
own Bundles locally by placing a conformant Bundle file somewhere into
your @INC path. The autobundle() command which is available in the
shell interface does that for you by including all currently installed
modules in a snapshot bundle file.

=head1 PREREQUISITES

If you have a local mirror of CPAN and can access all files with
"file:" URLs, then you only need a perl better than perl5.003 to run
this module. Otherwise Net::FTP is strongly recommended. LWP may be
required for non-UNIX systems or if your nearest CPAN site is
associated with a URL that is not C<ftp:>.

If you have neither Net::FTP nor LWP, there is a fallback mechanism
implemented for an external ftp command or for an external lynx
command.

=head1 UTILITIES

=head2 Finding packages and VERSION

This module presumes that all packages on CPAN

=over 2

=item *

declare their $VERSION variable in an easy to parse manner. This
prerequisite can hardly be relaxed because it consumes far too much
memory to load all packages into the running program just to determine
the $VERSION variable. Currently all programs that are dealing with
version use something like this

    perl -MExtUtils::MakeMaker -le \
        'print MM->parse_version(shift)' filename

If you are author of a package and wonder if your $VERSION can be
parsed, please try the above method.

=item *

come as compressed or gzipped tarfiles or as zip files and contain a
C<Makefile.PL> or C<Build.PL> (well, we try to handle a bit more, but
without much enthusiasm).

=back

=head2 Debugging

The debugging of this module is a bit complex, because we have
interferences of the software producing the indices on CPAN, of the
mirroring process on CPAN, of packaging, of configuration, of
synchronicity, and of bugs within CPAN.pm.

For debugging the code of CPAN.pm itself in interactive mode some more
or less useful debugging aid can be turned on for most packages within
CPAN.pm with one of

=over 2

=item o debug package...

sets debug mode for packages.

=item o debug -package...

unsets debug mode for packages.

=item o debug all

turns debugging on for all packages.

=item o debug number

=back

which sets the debugging packages directly. Note that C<o debug 0>
turns debugging off.

What seems quite a successful strategy is the combination of C<reload
cpan> and the debugging switches. Add a new debug statement while
running in the shell and then issue a C<reload cpan> and see the new
debugging messages immediately without losing the current context.

C<o debug> without an argument lists the valid package names and the
current set of packages in debugging mode. C<o debug> has built-in
completion support.

For debugging of CPAN data there is the C<dump> command which takes
the same arguments as make/test/install and outputs each object's
Data::Dumper dump. If an argument looks like a perl variable and
contains one of C<$>, C<@> or C<%>, it is eval()ed and fed to
Data::Dumper directly.

=head2 Floppy, Zip, Offline Mode

CPAN.pm works nicely without network too. If you maintain machines
that are not networked at all, you should consider working with file:
URLs. Of course, you have to collect your modules somewhere first. So
you might use CPAN.pm to put together all you need on a networked
machine. Then copy the $CPAN::Config->{keep_source_where} (but not
$CPAN::Config->{build_dir}) directory on a floppy. This floppy is kind
of a personal CPAN. CPAN.pm on the non-networked machines works nicely
with this floppy. See also below the paragraph about CD-ROM support.

=head2 Basic Utilities for Programmers

=over 2

=item has_inst($module)

Returns true if the module is installed. Used to load all modules into
the running CPAN.pm which are considered optional. The config variable
C<dontload_list> can be used to intercept the C<has_inst()> call such
that an optional module is not loaded despite being available. For
example the following command will prevent that C<YAML.pm> is being
loaded:

    cpan> o conf dontload_list push YAML

See the source for details.

=item has_usable($module)

Returns true if the module is installed and is in a usable state. Only
useful for a handful of modules that are used internally. See the
source for details.

=item instance($module)

The constructor for all the singletons used to represent modules,
distributions, authors and bundles. If the object already exists, this
method returns the object, otherwise it calls the constructor.

=back

=head1 SECURITY

There's no strong security layer in CPAN.pm. CPAN.pm helps you to
install foreign, unmasked, unsigned code on your machine. We compare
to a checksum that comes from the net just as the distribution file
itself. But we try to make it easy to add security on demand:

=head2 Cryptographically signed modules

Since release 1.77 CPAN.pm has been able to verify cryptographically
signed module distributions using Module::Signature.  The CPAN modules
can be signed by their authors, thus giving more security.  The simple
unsigned MD5 checksums that were used before by CPAN protect mainly
against accidental file corruption.

You will need to have Module::Signature installed, which in turn
requires that you have at least one of Crypt::OpenPGP module or the
command-line F<gpg> tool installed.

You will also need to be able to connect over the Internet to the public
keyservers, like pgp.mit.edu, and their port 11731 (the HKP protocol).

The configuration parameter check_sigs is there to turn signature
checking on or off.

=head1 EXPORT

Most functions in package CPAN are exported per default. The reason
for this is that the primary use is intended for the cpan shell or for
one-liners.

=head1 ENVIRONMENT

When the CPAN shell enters a subshell via the look command, it sets
the environment CPAN_SHELL_LEVEL to 1 or increments it if it is
already set.

When CPAN runs, it sets the environment variable PERL5_CPAN_IS_RUNNING
to the ID of the running process. It also sets
PERL5_CPANPLUS_IS_RUNNING to prevent runaway processes which could
happen with older versions of Module::Install.

When running C<perl Makefile.PL>, the environment variable
C<PERL5_CPAN_IS_EXECUTING> is set to the full path of the
C<Makefile.PL> that is being executed. This prevents runaway processes
with newer versions of Module::Install.

When the config variable ftp_passive is set, all downloads will be run
with the environment variable FTP_PASSIVE set to this value. This is
in general a good idea as it influences both Net::FTP and LWP based
connections. The same effect can be achieved by starting the cpan
shell with this environment variable set. For Net::FTP alone, one can
also always set passive mode by running libnetcfg.

=head1 POPULATE AN INSTALLATION WITH LOTS OF MODULES

Populating a freshly installed perl with my favorite modules is pretty
easy if you maintain a private bundle definition file. To get a useful
blueprint of a bundle definition file, the command autobundle can be used
on the CPAN shell command line. This command writes a bundle definition
file for all modules that are installed for the currently running perl
interpreter. It's recommended to run this command only once and from then
on maintain the file manually under a private name, say
Bundle/my_bundle.pm. With a clever bundle file you can then simply say

    cpan> install Bundle::my_bundle

then answer a few questions and then go out for a coffee.

Maintaining a bundle definition file means keeping track of two
things: dependencies and interactivity. CPAN.pm sometimes fails on
calculating dependencies because not all modules define all MakeMaker
attributes correctly, so a bundle definition file should specify
prerequisites as early as possible. On the other hand, it's a bit
annoying that many distributions need some interactive configuring. So
what I try to accomplish in my private bundle file is to have the
packages that need to be configured early in the file and the gentle
ones later, so I can go out after a few minutes and leave CPAN.pm
untended.

=head1 WORKING WITH CPAN.pm BEHIND FIREWALLS

Thanks to Graham Barr for contributing the following paragraphs about
the interaction between perl, and various firewall configurations. For
further information on firewalls, it is recommended to consult the
documentation that comes with the ncftp program. If you are unable to
go through the firewall with a simple Perl setup, it is very likely
that you can configure ncftp so that it works for your firewall.

=head2 Three basic types of firewalls

Firewalls can be categorized into three basic types.

=over 4

=item http firewall

This is where the firewall machine runs a web server and to access the
outside world you must do it via the web server. If you set environment
variables like http_proxy or ftp_proxy to a values beginning with http://
or in your web browser you have to set proxy information then you know
you are running an http firewall.

To access servers outside these types of firewalls with perl (even for
ftp) you will need to use LWP.

=item ftp firewall

This where the firewall machine runs an ftp server. This kind of
firewall will only let you access ftp servers outside the firewall.
This is usually done by connecting to the firewall with ftp, then
entering a username like "user@outside.host.com"

To access servers outside these type of firewalls with perl you
will need to use Net::FTP.

=item One way visibility

I say one way visibility as these firewalls try to make themselves look
invisible to the users inside the firewall. An FTP data connection is
normally created by sending the remote server your IP address and then
listening for the connection. But the remote server will not be able to
connect to you because of the firewall. So for these types of firewall
FTP connections need to be done in a passive mode.

There are two that I can think off.

=over 4

=item SOCKS

If you are using a SOCKS firewall you will need to compile perl and link
it with the SOCKS library, this is what is normally called a 'socksified'
perl. With this executable you will be able to connect to servers outside
the firewall as if it is not there.

=item IP Masquerade

This is the firewall implemented in the Linux kernel, it allows you to
hide a complete network behind one IP address. With this firewall no
special compiling is needed as you can access hosts directly.

For accessing ftp servers behind such firewalls you usually need to
set the environment variable C<FTP_PASSIVE> or the config variable
ftp_passive to a true value.

=back

=back

=head2 Configuring lynx or ncftp for going through a firewall

If you can go through your firewall with e.g. lynx, presumably with a
command such as

    /usr/local/bin/lynx -pscott:tiger

then you would configure CPAN.pm with the command

    o conf lynx "/usr/local/bin/lynx -pscott:tiger"

That's all. Similarly for ncftp or ftp, you would configure something
like

    o conf ncftp "/usr/bin/ncftp -f /home/scott/ncftplogin.cfg"

Your mileage may vary...

=head1 FAQ

=over 4

=item 1)

I installed a new version of module X but CPAN keeps saying,
I have the old version installed

Most probably you B<do> have the old version installed. This can
happen if a module installs itself into a different directory in the
@INC path than it was previously installed. This is not really a
CPAN.pm problem, you would have the same problem when installing the
module manually. The easiest way to prevent this behaviour is to add
the argument C<UNINST=1> to the C<make install> call, and that is why
many people add this argument permanently by configuring

  o conf make_install_arg UNINST=1

=item 2)

So why is UNINST=1 not the default?

Because there are people who have their precise expectations about who
may install where in the @INC path and who uses which @INC array. In
fine tuned environments C<UNINST=1> can cause damage.

=item 3)

I want to clean up my mess, and install a new perl along with
all modules I have. How do I go about it?

Run the autobundle command for your old perl and optionally rename the
resulting bundle file (e.g. Bundle/mybundle.pm), install the new perl
with the Configure option prefix, e.g.

    ./Configure -Dprefix=/usr/local/perl-5.6.78.9

Install the bundle file you produced in the first step with something like

    cpan> install Bundle::mybundle

and you're done.

=item 4)

When I install bundles or multiple modules with one command
there is too much output to keep track of.

You may want to configure something like

  o conf make_arg "| tee -ai /root/.cpan/logs/make.out"
  o conf make_install_arg "| tee -ai /root/.cpan/logs/make_install.out"

so that STDOUT is captured in a file for later inspection.


=item 5)

I am not root, how can I install a module in a personal directory?

First of all, you will want to use your own configuration, not the one
that your root user installed. If you do not have permission to write
in the cpan directory that root has configured, you will be asked if
you want to create your own config. Answering "yes" will bring you into
CPAN's configuration stage, using the system config for all defaults except
things that have to do with CPAN's work directory, saving your choices to
your MyConfig.pm file.

You can also manually initiate this process with the following command:

    % perl -MCPAN -e 'mkmyconfig'

or by running

    mkmyconfig

from the CPAN shell.

You will most probably also want to configure something like this:

  o conf makepl_arg "LIB=~/myperl/lib \
                    INSTALLMAN1DIR=~/myperl/man/man1 \
                    INSTALLMAN3DIR=~/myperl/man/man3 \
                    INSTALLSCRIPT=~/myperl/bin \
                    INSTALLBIN=~/myperl/bin"

and then (oh joy) the equivalent command for Module::Build. That would
be

  o conf mbuildpl_arg "--lib=~/myperl/lib \
                    --installman1dir=~/myperl/man/man1 \
                    --installman3dir=~/myperl/man/man3 \
                    --installscript=~/myperl/bin \
                    --installbin=~/myperl/bin"

You can make this setting permanent like all C<o conf> settings with
C<o conf commit> or by setting C<auto_commit> beforehand.

You will have to add ~/myperl/man to the MANPATH environment variable
and also tell your perl programs to look into ~/myperl/lib, e.g. by
including

  use lib "$ENV{HOME}/myperl/lib";

or setting the PERL5LIB environment variable.

While we're speaking about $ENV{HOME}, it might be worth mentioning,
that for Windows we use the File::HomeDir module that provides an
equivalent to the concept of the home directory on Unix.

Another thing you should bear in mind is that the UNINST parameter can
be dangerous when you are installing into a private area because you
might accidentally remove modules that other people depend on that are
not using the private area.

=item 6)

How to get a package, unwrap it, and make a change before building it?

Have a look at the C<look> (!) command.

=item 7)

I installed a Bundle and had a couple of fails. When I
retried, everything resolved nicely. Can this be fixed to work
on first try?

The reason for this is that CPAN does not know the dependencies of all
modules when it starts out. To decide about the additional items to
install, it just uses data found in the META.yml file or the generated
Makefile. An undetected missing piece breaks the process. But it may
well be that your Bundle installs some prerequisite later than some
depending item and thus your second try is able to resolve everything.
Please note, CPAN.pm does not know the dependency tree in advance and
cannot sort the queue of things to install in a topologically correct
order. It resolves perfectly well IF all modules declare the
prerequisites correctly with the PREREQ_PM attribute to MakeMaker or
the C<requires> stanza of Module::Build. For bundles which fail and
you need to install often, it is recommended to sort the Bundle
definition file manually.

=item 8)

In our intranet we have many modules for internal use. How
can I integrate these modules with CPAN.pm but without uploading
the modules to CPAN?

Have a look at the CPAN::Site module.

=item 9)

When I run CPAN's shell, I get an error message about things in my
/etc/inputrc (or ~/.inputrc) file.

These are readline issues and can only be fixed by studying readline
configuration on your architecture and adjusting the referenced file
accordingly. Please make a backup of the /etc/inputrc or ~/.inputrc
and edit them. Quite often harmless changes like uppercasing or
lowercasing some arguments solves the problem.

=item 10)

Some authors have strange characters in their names.

Internally CPAN.pm uses the UTF-8 charset. If your terminal is
expecting ISO-8859-1 charset, a converter can be activated by setting
term_is_latin to a true value in your config file. One way of doing so
would be

    cpan> o conf term_is_latin 1

If other charset support is needed, please file a bugreport against
CPAN.pm at rt.cpan.org and describe your needs. Maybe we can extend
the support or maybe UTF-8 terminals become widely available.

Note: this config variable is deprecated and will be removed in a
future version of CPAN.pm. It will be replaced with the conventions
around the family of $LANG and $LC_* environment variables.

=item 11)

When an install fails for some reason and then I correct the error
condition and retry, CPAN.pm refuses to install the module, saying
C<Already tried without success>.

Use the force pragma like so

  force install Foo::Bar

Or you can use

  look Foo::Bar

and then 'make install' directly in the subshell.

=item 12)

How do I install a "DEVELOPER RELEASE" of a module?

By default, CPAN will install the latest non-developer release of a
module. If you want to install a dev release, you have to specify the
partial path starting with the author id to the tarball you wish to
install, like so:

    cpan> install KWILLIAMS/Module-Build-0.27_07.tar.gz

Note that you can use the C<ls> command to get this path listed.

=item 13)

How do I install a module and all its dependencies from the commandline,
without being prompted for anything, despite my CPAN configuration
(or lack thereof)?

CPAN uses ExtUtils::MakeMaker's prompt() function to ask its questions, so
if you set the PERL_MM_USE_DEFAULT environment variable, you shouldn't be
asked any questions at all (assuming the modules you are installing are
nice about obeying that variable as well):

    % PERL_MM_USE_DEFAULT=1 perl -MCPAN -e 'install My::Module'

=item 14)

How do I create a Module::Build based Build.PL derived from an
ExtUtils::MakeMaker focused Makefile.PL?

http://search.cpan.org/search?query=Module::Build::Convert

http://www.refcnt.org/papers/module-build-convert

=item 15)

What's the best CPAN site for me?

The urllist config parameter is yours. You can add and remove sites at
will. You should find out which sites have the best uptodateness,
bandwidth, reliability, etc. and are topologically close to you. Some
people prefer fast downloads, others uptodateness, others reliability.
You decide which to try in which order.

Henk P. Penning maintains a site that collects data about CPAN sites:

  http://www.cs.uu.nl/people/henkp/mirmon/cpan.html

=item 16)

Why do I get asked the same questions every time I start the shell?

You can make your configuration changes permanent by calling the
command C<o conf commit>. Alternatively set the C<auto_commit>
variable to true by running C<o conf init auto_commit> and answering
the following question with yes.

=back

=head1 COMPATIBILITY

=head2 OLD PERL VERSIONS

CPAN.pm is regularly tested to run under 5.004, 5.005, and assorted
newer versions. It is getting more and more difficult to get the
minimal prerequisites working on older perls. It is close to
impossible to get the whole Bundle::CPAN working there. If you're in
the position to have only these old versions, be advised that CPAN is
designed to work fine without the Bundle::CPAN installed.

To get things going, note that GBARR/Scalar-List-Utils-1.18.tar.gz is
compatible with ancient perls and that File::Temp is listed as a
prerequisite but CPAN has reasonable workarounds if it is missing.

=head2 CPANPLUS

This module and its competitor, the CPANPLUS module, are both much
cooler than the other. CPAN.pm is older. CPANPLUS was designed to be
more modular but it was never tried to make it compatible with CPAN.pm.

=head1 SECURITY ADVICE

This software enables you to upgrade software on your computer and so
is inherently dangerous because the newly installed software may
contain bugs and may alter the way your computer works or even make it
unusable. Please consider backing up your data before every upgrade.

=head1 BUGS

Please report bugs via L<http://rt.cpan.org/>

Before submitting a bug, please make sure that the traditional method
of building a Perl module package from a shell by following the
installation instructions of that package still works in your
environment.

=head1 AUTHOR

Andreas Koenig C<< <andk@cpan.org> >>

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=head1 TRANSLATIONS

Kawai,Takanori provides a Japanese translation of this manpage at
L<http://homepage3.nifty.com/hippo2000/perltips/CPAN.htm>

=head1 SEE ALSO

L<cpan>, L<CPAN::Nox>, L<CPAN::Version>

=cut


