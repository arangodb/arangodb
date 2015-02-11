package CPAN::HandleConfig;
use strict;
use vars qw(%can %keys $loading $VERSION);

$VERSION = sprintf "%.6f", substr(q$Rev: 2212 $,4)/1000000 + 5.4;

%can = (
        commit   => "Commit changes to disk",
        defaults => "Reload defaults from disk",
        help     => "Short help about 'o conf' usage",
        init     => "Interactive setting of all options",
);

# Q: where is the "How do I add a new config option" HOWTO?
# A1: svn diff -r 757:758 # where dagolden added test_report
# A2: svn diff -r 985:986 # where andk added yaml_module
%keys = map { $_ => undef }
    (
     "applypatch",
     "auto_commit",
     "build_cache",
     "build_dir",
     "build_dir_reuse",
     "build_requires_install_policy",
     "bzip2",
     "cache_metadata",
     "check_sigs",
     "colorize_debug",
     "colorize_output",
     "colorize_print",
     "colorize_warn",
     "commandnumber_in_prompt",
     "commands_quote",
     "cpan_home",
     "curl",
     "dontload_hash", # deprecated after 1.83_68 (rev. 581)
     "dontload_list",
     "ftp",
     "ftp_passive",
     "ftp_proxy",
     "getcwd",
     "gpg",
     "gzip",
     "histfile",
     "histsize",
     "http_proxy",
     "inactivity_timeout",
     "index_expire",
     "inhibit_startup_message",
     "keep_source_where",
     "load_module_verbosity",
     "lynx",
     "make",
     "make_arg",
     "make_install_arg",
     "make_install_make_command",
     "makepl_arg",
     "mbuild_arg",
     "mbuild_install_arg",
     "mbuild_install_build_command",
     "mbuildpl_arg",
     "ncftp",
     "ncftpget",
     "no_proxy",
     "pager",
     "password",
     "patch",
     "prefer_installer",
     "prefs_dir",
     "prerequisites_policy",
     "proxy_pass",
     "proxy_user",
     "randomize_urllist",
     "scan_cache",
     "shell",
     "show_unparsable_versions",
     "show_upload_date",
     "show_zero_versions",
     "tar",
     "tar_verbosity",
     "term_is_latin",
     "term_ornaments",
     "test_report",
     "unzip",
     "urllist",
     "use_sqlite",
     "username",
     "wait_list",
     "wget",
     "yaml_load_code",
     "yaml_module",
    );

my %prefssupport = map { $_ => 1 }
    (
     "build_requires_install_policy",
     "check_sigs",
     "make",
     "make_install_make_command",
     "prefer_installer",
     "test_report",
    );

if ($^O eq "MSWin32") {
    for my $k (qw(
                  mbuild_install_build_command
                  make_install_make_command
                 )) {
        delete $keys{$k};
        if (exists $CPAN::Config->{$k}) {
            for ("deleting previously set config variable '$k' => '$CPAN::Config->{$k}'") {
                $CPAN::Frontend ? $CPAN::Frontend->mywarn($_) : warn $_;
            }
            delete $CPAN::Config->{$k};
        }
    }
}

# returns true on successful action
sub edit {
    my($self,@args) = @_;
    return unless @args;
    CPAN->debug("self[$self]args[".join(" | ",@args)."]");
    my($o,$str,$func,$args,$key_exists);
    $o = shift @args;
    $DB::single = 1;
    if($can{$o}) {
        $self->$o(args => \@args); # o conf init => sub init => sub load
        return 1;
    } else {
        CPAN->debug("o[$o]") if $CPAN::DEBUG;
        unless (exists $keys{$o}) {
            $CPAN::Frontend->mywarn("Warning: unknown configuration variable '$o'\n");
        }
        my $changed;


        # one day I used randomize_urllist for a boolean, so we must
        # list them explicitly --ak
        if (0) {
        } elsif ($o =~ /^(wait_list|urllist|dontload_list)$/) {

            #
            # ARRAYS
            #

            $func = shift @args;
            $func ||= "";
            CPAN->debug("func[$func]args[@args]") if $CPAN::DEBUG;
            # Let's avoid eval, it's easier to comprehend without.
            if ($func eq "push") {
                push @{$CPAN::Config->{$o}}, @args;
                $changed = 1;
            } elsif ($func eq "pop") {
                pop @{$CPAN::Config->{$o}};
                $changed = 1;
            } elsif ($func eq "shift") {
                shift @{$CPAN::Config->{$o}};
                $changed = 1;
            } elsif ($func eq "unshift") {
                unshift @{$CPAN::Config->{$o}}, @args;
                $changed = 1;
            } elsif ($func eq "splice") {
                my $offset = shift @args || 0;
                my $length = shift @args || 0;
                splice @{$CPAN::Config->{$o}}, $offset, $length, @args; # may warn
                $changed = 1;
            } elsif ($func) {
                $CPAN::Config->{$o} = [$func, @args];
                $changed = 1;
            } else {
                $self->prettyprint($o);
            }
            if ($changed) {
                if ($o eq "urllist") {
                    # reset the cached values
                    undef $CPAN::FTP::Thesite;
                    undef $CPAN::FTP::Themethod;
                    $CPAN::Index::LAST_TIME = 0;
                } elsif ($o eq "dontload_list") {
                    # empty it, it will be built up again
                    $CPAN::META->{dontload_hash} = {};
                }
            }
        } elsif ($o =~ /_hash$/) {

            #
            # HASHES
            #

            if (@args==1 && $args[0] eq "") {
                @args = ();
            } elsif (@args % 2) {
                push @args, "";
            }
            $CPAN::Config->{$o} = { @args };
            $changed = 1;
        } else {

            #
            # SCALARS
            #

            if (defined $args[0]) {
                $CPAN::CONFIG_DIRTY = 1;
                $CPAN::Config->{$o} = $args[0];
                $changed = 1;
            }
            $self->prettyprint($o)
                if exists $keys{$o} or defined $CPAN::Config->{$o};
        }
        if ($changed) {
            if ($CPAN::Config->{auto_commit}) {
                $self->commit;
            } else {
                $CPAN::CONFIG_DIRTY = 1;
                $CPAN::Frontend->myprint("Please use 'o conf commit' to ".
                                         "make the config permanent!\n\n");
            }
        }
    }
}

sub prettyprint {
    my($self,$k) = @_;
    my $v = $CPAN::Config->{$k};
    if (ref $v) {
        my(@report);
        if (ref $v eq "ARRAY") {
            @report = map {"\t$_ \[$v->[$_]]\n"} 0..$#$v;
        } else {
            @report = map
                {
                    sprintf "\t%-18s => %s\n",
                               "[$_]",
                                        defined $v->{$_} ? "[$v->{$_}]" : "undef"
                } keys %$v;
        }
        $CPAN::Frontend->myprint(
                                 join(
                                      "",
                                      sprintf(
                                              "    %-18s\n",
                                              $k
                                             ),
                                      @report
                                     )
                                );
    } elsif (defined $v) {
        $CPAN::Frontend->myprint(sprintf "    %-18s [%s]\n", $k, $v);
    } else {
        $CPAN::Frontend->myprint(sprintf "    %-18s undef\n", $k);
    }
}

sub commit {
    my($self,@args) = @_;
    CPAN->debug("args[@args]") if $CPAN::DEBUG;
    if ($CPAN::RUN_DEGRADED) {
                             $CPAN::Frontend->mydie(
                                                    "'o conf commit' disabled in ".
                                                    "degraded mode. Maybe try\n".
                                                    " !undef \$CPAN::RUN_DEGRADED\n"
                                                   );
    }
    my $configpm;
    if (@args) {
      if ($args[0] eq "args") {
        # we have not signed that contract
      } else {
        $configpm = $args[0];
      }
    }
    unless (defined $configpm) {
        $configpm ||= $INC{"CPAN/MyConfig.pm"};
        $configpm ||= $INC{"CPAN/Config.pm"};
        $configpm || Carp::confess(q{
CPAN::Config::commit called without an argument.
Please specify a filename where to save the configuration or try
"o conf init" to have an interactive course through configing.
});
    }
    my($mode);
    if (-f $configpm) {
        $mode = (stat $configpm)[2];
        if ($mode && ! -w _) {
            Carp::confess("$configpm is not writable");
        }
    }

    my $msg;
    $msg = <<EOF unless $configpm =~ /MyConfig/;

# This is CPAN.pm's systemwide configuration file. This file provides
# defaults for users, and the values can be changed in a per-user
# configuration file. The user-config file is being looked for as
# ~/.cpan/CPAN/MyConfig.pm.

EOF
    $msg ||= "\n";
    my($fh) = FileHandle->new;
    rename $configpm, "$configpm~" if -f $configpm;
    open $fh, ">$configpm" or
        $CPAN::Frontend->mydie("Couldn't open >$configpm: $!");
    $fh->print(qq[$msg\$CPAN::Config = \{\n]);
    foreach (sort keys %$CPAN::Config) {
        unless (exists $keys{$_}) {
            # do not drop them: forward compatibility!
            $CPAN::Frontend->mywarn("Unknown config variable '$_'\n");
            next;
        }
        $fh->print(
            "  '$_' => ",
            $self->neatvalue($CPAN::Config->{$_}),
            ",\n"
        );
    }

    $fh->print("};\n1;\n__END__\n");
    close $fh;

    #$mode = 0444 | ( $mode & 0111 ? 0111 : 0 );
    #chmod $mode, $configpm;
###why was that so?    $self->defaults;
    $CPAN::Frontend->myprint("commit: wrote '$configpm'\n");
    $CPAN::CONFIG_DIRTY = 0;
    1;
}

# stolen from MakeMaker; not taking the original because it is buggy;
# bugreport will have to say: keys of hashes remain unquoted and can
# produce syntax errors
sub neatvalue {
    my($self, $v) = @_;
    return "undef" unless defined $v;
    my($t) = ref $v;
    unless ($t) {
        $v =~ s/\\/\\\\/g;
        return "q[$v]";
    }
    if ($t eq 'ARRAY') {
        my(@m, @neat);
        push @m, "[";
        foreach my $elem (@$v) {
            push @neat, "q[$elem]";
        }
        push @m, join ", ", @neat;
        push @m, "]";
        return join "", @m;
    }
    return "$v" unless $t eq 'HASH';
    my(@m, $key, $val);
    while (($key,$val) = each %$v) {
        last unless defined $key; # cautious programming in case (undef,undef) is true
        push(@m,"q[$key]=>".$self->neatvalue($val)) ;
    }
    return "{ ".join(', ',@m)." }";
}

sub defaults {
    my($self) = @_;
    if ($CPAN::RUN_DEGRADED) {
                             $CPAN::Frontend->mydie(
                                                    "'o conf defaults' disabled in ".
                                                    "degraded mode. Maybe try\n".
                                                    " !undef \$CPAN::RUN_DEGRADED\n"
                                                   );
    }
    my $done;
    for my $config (qw(CPAN/MyConfig.pm CPAN/Config.pm)) {
        if ($INC{$config}) {
            CPAN->debug("INC{'$config'}[$INC{$config}]") if $CPAN::DEBUG;
            CPAN::Shell->_reload_this($config,{reloforce => 1});
            $CPAN::Frontend->myprint("'$INC{$config}' reread\n");
            last;
        }
    }
    $CPAN::CONFIG_DIRTY = 0;
    1;
}

=head2 C<< CLASS->safe_quote ITEM >>

Quotes an item to become safe against spaces
in shell interpolation. An item is enclosed
in double quotes if:

  - the item contains spaces in the middle
  - the item does not start with a quote

This happens to avoid shell interpolation
problems when whitespace is present in
directory names.

This method uses C<commands_quote> to determine
the correct quote. If C<commands_quote> is
a space, no quoting will take place.


if it starts and ends with the same quote character: leave it as it is

if it contains no whitespace: leave it as it is

if it contains whitespace, then

if it contains quotes: better leave it as it is

else: quote it with the correct quote type for the box we're on

=cut

{
    # Instead of patching the guess, set commands_quote
    # to the right value
    my ($quotes,$use_quote)
        = $^O eq 'MSWin32'
            ? ('"', '"')
                : (q{"'}, "'")
                    ;

    sub safe_quote {
        my ($self, $command) = @_;
        # Set up quote/default quote
        my $quote = $CPAN::Config->{commands_quote} || $quotes;

        if ($quote ne ' '
            and defined($command )
            and $command =~ /\s/
            and $command !~ /[$quote]/) {
            return qq<$use_quote$command$use_quote>
        }
        return $command;
    }
}

sub init {
    my($self,@args) = @_;
    CPAN->debug("self[$self]args[".join(",",@args)."]");
    $self->load(doit => 1, @args);
    1;
}

# This is a piece of repeated code that is abstracted here for
# maintainability.  RMB
#
sub _configpmtest {
    my($configpmdir, $configpmtest) = @_;
    if (-w $configpmtest) {
        return $configpmtest;
    } elsif (-w $configpmdir) {
        #_#_# following code dumped core on me with 5.003_11, a.k.
        my $configpm_bak = "$configpmtest.bak";
        unlink $configpm_bak if -f $configpm_bak;
        if( -f $configpmtest ) {
            if( rename $configpmtest, $configpm_bak ) {
                $CPAN::Frontend->mywarn(<<END);
Old configuration file $configpmtest
    moved to $configpm_bak
END
            }
        }
        my $fh = FileHandle->new;
        if ($fh->open(">$configpmtest")) {
            $fh->print("1;\n");
            return $configpmtest;
        } else {
            # Should never happen
            Carp::confess("Cannot open >$configpmtest");
        }
    } else { return }
}

sub require_myconfig_or_config () {
    return if $INC{"CPAN/MyConfig.pm"};
    local @INC = @INC;
    my $home = home();
    unshift @INC, File::Spec->catdir($home,'.cpan');
    eval { require CPAN::MyConfig };
    my $err_myconfig = $@;
    if ($err_myconfig and $err_myconfig !~ m#locate CPAN/MyConfig\.pm#) {
        die "Error while requiring CPAN::MyConfig:\n$err_myconfig";
    }
    unless ($INC{"CPAN/MyConfig.pm"}) { # this guy has settled his needs already
      eval {require CPAN::Config;}; # not everybody has one
      my $err_config = $@;
      if ($err_config and $err_config !~ m#locate CPAN/Config\.pm#) {
          die "Error while requiring CPAN::Config:\n$err_config";
      }
    }
}

sub home () {
    my $home;
    if ($CPAN::META->has_usable("File::HomeDir")) {
        $home = File::HomeDir->my_data;
        unless (defined $home) {
            $home = File::HomeDir->my_home
        }
    }
    unless (defined $home) {
        $home = $ENV{HOME};
    }
    $home;
}

sub load {
    my($self, %args) = @_;
    $CPAN::Be_Silent++ if $args{be_silent};
    my $doit;
    $doit = delete $args{doit};

    use Carp;
    require_myconfig_or_config;
    my @miss = $self->missing_config_data;
    return unless $doit || @miss;
    return if $loading;
    $loading++;

    require CPAN::FirstTime;
    my($configpm,$fh,$redo);
    $redo ||= "";
    if (defined $INC{"CPAN/Config.pm"} && -w $INC{"CPAN/Config.pm"}) {
        $configpm = $INC{"CPAN/Config.pm"};
        $redo++;
    } elsif (defined $INC{"CPAN/MyConfig.pm"} && -w $INC{"CPAN/MyConfig.pm"}) {
        $configpm = $INC{"CPAN/MyConfig.pm"};
        $redo++;
    } else {
        my($path_to_cpan) = File::Basename::dirname($INC{"CPAN.pm"});
        my($configpmdir) = File::Spec->catdir($path_to_cpan,"CPAN");
        my($configpmtest) = File::Spec->catfile($configpmdir,"Config.pm");
        my $inc_key;
        if (-d $configpmdir or File::Path::mkpath($configpmdir)) {
            $configpm = _configpmtest($configpmdir,$configpmtest);
            $inc_key = "CPAN/Config.pm";
        }
        unless ($configpm) {
            $configpmdir = File::Spec->catdir(home,".cpan","CPAN");
            File::Path::mkpath($configpmdir);
            $configpmtest = File::Spec->catfile($configpmdir,"MyConfig.pm");
            $configpm = _configpmtest($configpmdir,$configpmtest);
            $inc_key = "CPAN/MyConfig.pm";
        }
        if ($configpm) {
          $INC{$inc_key} = $configpm;
        } else {
          my $text = qq{WARNING: CPAN.pm is unable to } .
              qq{create a configuration file.};
          output($text, 'confess');
        }

    }
    local($") = ", ";
    if ($redo && !$doit) {
        $CPAN::Frontend->myprint(<<END);
Sorry, we have to rerun the configuration dialog for CPAN.pm due to
some missing parameters...

END
        $args{args} = \@miss;
    }
    CPAN::FirstTime::init($configpm, %args);
    $loading--;
    return;
}


# returns mandatory but missing entries in the Config
sub missing_config_data {
    my(@miss);
    for (
         "auto_commit",
         "build_cache",
         "build_dir",
         "cache_metadata",
         "cpan_home",
         "ftp_proxy",
         #"gzip",
         "http_proxy",
         "index_expire",
         #"inhibit_startup_message",
         "keep_source_where",
         #"make",
         "make_arg",
         "make_install_arg",
         "makepl_arg",
         "mbuild_arg",
         "mbuild_install_arg",
         "mbuild_install_build_command",
         "mbuildpl_arg",
         "no_proxy",
         #"pager",
         "prerequisites_policy",
         "scan_cache",
         #"tar",
         #"unzip",
         "urllist",
        ) {
        next unless exists $keys{$_};
        push @miss, $_ unless defined $CPAN::Config->{$_};
    }
    return @miss;
}

sub help {
    $CPAN::Frontend->myprint(q[
Known options:
  commit    commit session changes to disk
  defaults  reload default config values from disk
  help      this help
  init      enter a dialog to set all or a set of parameters

Edit key values as in the following (the "o" is a literal letter o):
  o conf build_cache 15
  o conf build_dir "/foo/bar"
  o conf urllist shift
  o conf urllist unshift ftp://ftp.foo.bar/
  o conf inhibit_startup_message 1

]);
    undef; #don't reprint CPAN::Config
}

sub cpl {
    my($word,$line,$pos) = @_;
    $word ||= "";
    CPAN->debug("word[$word] line[$line] pos[$pos]") if $CPAN::DEBUG;
    my(@words) = split " ", substr($line,0,$pos+1);
    if (
        defined($words[2])
        and
        $words[2] =~ /list$/
        and
        (
        @words == 3
        ||
        @words == 4 && length($word)
        )
       ) {
        return grep /^\Q$word\E/, qw(splice shift unshift pop push);
    } elsif (defined($words[2])
             and
             $words[2] eq "init"
             and
            (
             @words == 3
             ||
             @words >= 4 && length($word)
            )) {
        return sort grep /^\Q$word\E/, keys %keys;
    } elsif (@words >= 4) {
        return ();
    }
    my %seen;
    my(@o_conf) =  sort grep { !$seen{$_}++ }
        keys %can,
            keys %$CPAN::Config,
                keys %keys;
    return grep /^\Q$word\E/, @o_conf;
}

sub prefs_lookup {
    my($self,$distro,$what) = @_;

    if ($prefssupport{$what}) {
        return $CPAN::Config->{$what} unless
            $distro
                and $distro->prefs
                    and $distro->prefs->{cpanconfig}
                        and defined $distro->prefs->{cpanconfig}{$what};
        return $distro->prefs->{cpanconfig}{$what};
    } else {
        $CPAN::Frontend->mywarn("Warning: $what not yet officially ".
                                "supported for distroprefs, doing a normal lookup");
        return $CPAN::Config->{$what};
    }
}


{
    package
        CPAN::Config; ####::###### #hide from indexer
    # note: J. Nick Koston wrote me that they are using
    # CPAN::Config->commit although undocumented. I suggested
    # CPAN::Shell->o("conf","commit") even when ugly it is at least
    # documented

    # that's why I added the CPAN::Config class with autoload and
    # deprecated warning

    use strict;
    use vars qw($AUTOLOAD $VERSION);
    $VERSION = sprintf "%.2f", substr(q$Rev: 2212 $,4)/100;

    # formerly CPAN::HandleConfig was known as CPAN::Config
    sub AUTOLOAD {
        my $class = shift; # e.g. in dh-make-perl: CPAN::Config
        my($l) = $AUTOLOAD;
        $CPAN::Frontend->mywarn("Dispatching deprecated method '$l' to CPAN::HandleConfig\n");
        $l =~ s/.*:://;
        CPAN::HandleConfig->$l(@_);
    }
}

1;

__END__

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut

# Local Variables:
# mode: cperl
# cperl-indent-level: 4
# End:
