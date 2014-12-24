package ExtUtils::MM_Any;

use strict;
our $VERSION = '6.44';

use Carp;
use File::Spec;
BEGIN { our @ISA = qw(File::Spec); }

# We need $Verbose
use ExtUtils::MakeMaker qw($Verbose);

use ExtUtils::MakeMaker::Config;


# So we don't have to keep calling the methods over and over again,
# we have these globals to cache the values.  Faster and shrtr.
my $Curdir  = __PACKAGE__->curdir;
my $Rootdir = __PACKAGE__->rootdir;
my $Updir   = __PACKAGE__->updir;


=head1 NAME

ExtUtils::MM_Any - Platform-agnostic MM methods

=head1 SYNOPSIS

  FOR INTERNAL USE ONLY!

  package ExtUtils::MM_SomeOS;

  # Temporarily, you have to subclass both.  Put MM_Any first.
  require ExtUtils::MM_Any;
  require ExtUtils::MM_Unix;
  @ISA = qw(ExtUtils::MM_Any ExtUtils::Unix);

=head1 DESCRIPTION

B<FOR INTERNAL USE ONLY!>

ExtUtils::MM_Any is a superclass for the ExtUtils::MM_* set of
modules.  It contains methods which are either inherently
cross-platform or are written in a cross-platform manner.

Subclass off of ExtUtils::MM_Any I<and> ExtUtils::MM_Unix.  This is a
temporary solution.

B<THIS MAY BE TEMPORARY!>


=head1 METHODS

Any methods marked I<Abstract> must be implemented by subclasses.


=head2 Cross-platform helper methods

These are methods which help writing cross-platform code.



=head3 os_flavor  I<Abstract>

    my @os_flavor = $mm->os_flavor;

@os_flavor is the style of operating system this is, usually
corresponding to the MM_*.pm file we're using.  

The first element of @os_flavor is the major family (ie. Unix,
Windows, VMS, OS/2, etc...) and the rest are sub families.

Some examples:

    Cygwin98       ('Unix',  'Cygwin', 'Cygwin9x')
    Windows NT     ('Win32', 'WinNT')
    Win98          ('Win32', 'Win9x')
    Linux          ('Unix',  'Linux')
    MacOS X        ('Unix',  'Darwin', 'MacOS', 'MacOS X')
    OS/2           ('OS/2')

This is used to write code for styles of operating system.  
See os_flavor_is() for use.


=head3 os_flavor_is

    my $is_this_flavor = $mm->os_flavor_is($this_flavor);
    my $is_this_flavor = $mm->os_flavor_is(@one_of_these_flavors);

Checks to see if the current operating system is one of the given flavors.

This is useful for code like:

    if( $mm->os_flavor_is('Unix') ) {
        $out = `foo 2>&1`;
    }
    else {
        $out = `foo`;
    }

=cut

sub os_flavor_is {
    my $self = shift;
    my %flavors = map { ($_ => 1) } $self->os_flavor;
    return (grep { $flavors{$_} } @_) ? 1 : 0;
}


=head3 split_command

    my @cmds = $MM->split_command($cmd, @args);

Most OS have a maximum command length they can execute at once.  Large
modules can easily generate commands well past that limit.  Its
necessary to split long commands up into a series of shorter commands.

C<split_command> will return a series of @cmds each processing part of
the args.  Collectively they will process all the arguments.  Each
individual line in @cmds will not be longer than the
$self->max_exec_len being careful to take into account macro expansion.

$cmd should include any switches and repeated initial arguments.

If no @args are given, no @cmds will be returned.

Pairs of arguments will always be preserved in a single command, this
is a heuristic for things like pm_to_blib and pod2man which work on
pairs of arguments.  This makes things like this safe:

    $self->split_command($cmd, %pod2man);


=cut

sub split_command {
    my($self, $cmd, @args) = @_;

    my @cmds = ();
    return(@cmds) unless @args;

    # If the command was given as a here-doc, there's probably a trailing
    # newline.
    chomp $cmd;

    # set aside 30% for macro expansion.
    my $len_left = int($self->max_exec_len * 0.70);
    $len_left -= length $self->_expand_macros($cmd);

    do {
        my $arg_str = '';
        my @next_args;
        while( @next_args = splice(@args, 0, 2) ) {
            # Two at a time to preserve pairs.
            my $next_arg_str = "\t  ". join ' ', @next_args, "\n";

            if( !length $arg_str ) {
                $arg_str .= $next_arg_str
            }
            elsif( length($arg_str) + length($next_arg_str) > $len_left ) {
                unshift @args, @next_args;
                last;
            }
            else {
                $arg_str .= $next_arg_str;
            }
        }
        chop $arg_str;

        push @cmds, $self->escape_newlines("$cmd \n$arg_str");
    } while @args;

    return @cmds;
}


sub _expand_macros {
    my($self, $cmd) = @_;

    $cmd =~ s{\$\((\w+)\)}{
        defined $self->{$1} ? $self->{$1} : "\$($1)"
    }e;
    return $cmd;
}


=head3 echo

    my @commands = $MM->echo($text);
    my @commands = $MM->echo($text, $file);
    my @commands = $MM->echo($text, $file, $appending);

Generates a set of @commands which print the $text to a $file.

If $file is not given, output goes to STDOUT.

If $appending is true the $file will be appended to rather than
overwritten.

=cut

sub echo {
    my($self, $text, $file, $appending) = @_;
    $appending ||= 0;

    my @cmds = map { '$(NOECHO) $(ECHO) '.$self->quote_literal($_) } 
               split /\n/, $text;
    if( $file ) {
        my $redirect = $appending ? '>>' : '>';
        $cmds[0] .= " $redirect $file";
        $_ .= " >> $file" foreach @cmds[1..$#cmds];
    }

    return @cmds;
}


=head3 wraplist

  my $args = $mm->wraplist(@list);

Takes an array of items and turns them into a well-formatted list of
arguments.  In most cases this is simply something like:

    FOO \
    BAR \
    BAZ

=cut

sub wraplist {
    my $self = shift;
    return join " \\\n\t", @_;
}


=head3 maketext_filter

    my $filter_make_text = $mm->maketext_filter($make_text);

The text of the Makefile is run through this method before writing to
disk.  It allows systems a chance to make portability fixes to the
Makefile.

By default it does nothing.

This method is protected and not intended to be called outside of
MakeMaker.

=cut

sub maketext_filter { return $_[1] }


=head3 cd  I<Abstract>

  my $subdir_cmd = $MM->cd($subdir, @cmds);

This will generate a make fragment which runs the @cmds in the given
$dir.  The rough equivalent to this, except cross platform.

  cd $subdir && $cmd

Currently $dir can only go down one level.  "foo" is fine.  "foo/bar" is
not.  "../foo" is right out.

The resulting $subdir_cmd has no leading tab nor trailing newline.  This
makes it easier to embed in a make string.  For example.

      my $make = sprintf <<'CODE', $subdir_cmd;
  foo :
      $(ECHO) what
      %s
      $(ECHO) mouche
  CODE


=head3 oneliner  I<Abstract>

  my $oneliner = $MM->oneliner($perl_code);
  my $oneliner = $MM->oneliner($perl_code, \@switches);

This will generate a perl one-liner safe for the particular platform
you're on based on the given $perl_code and @switches (a -e is
assumed) suitable for using in a make target.  It will use the proper
shell quoting and escapes.

$(PERLRUN) will be used as perl.

Any newlines in $perl_code will be escaped.  Leading and trailing
newlines will be stripped.  Makes this idiom much easier:

    my $code = $MM->oneliner(<<'CODE', [...switches...]);
some code here
another line here
CODE

Usage might be something like:

    # an echo emulation
    $oneliner = $MM->oneliner('print "Foo\n"');
    $make = '$oneliner > somefile';

All dollar signs must be doubled in the $perl_code if you expect them
to be interpreted normally, otherwise it will be considered a make
macro.  Also remember to quote make macros else it might be used as a
bareword.  For example:

    # Assign the value of the $(VERSION_FROM) make macro to $vf.
    $oneliner = $MM->oneliner('$$vf = "$(VERSION_FROM)"');

Its currently very simple and may be expanded sometime in the figure
to include more flexible code and switches.


=head3 quote_literal  I<Abstract>

    my $safe_text = $MM->quote_literal($text);

This will quote $text so it is interpreted literally in the shell.

For example, on Unix this would escape any single-quotes in $text and
put single-quotes around the whole thing.


=head3 escape_newlines  I<Abstract>

    my $escaped_text = $MM->escape_newlines($text);

Shell escapes newlines in $text.


=head3 max_exec_len  I<Abstract>

    my $max_exec_len = $MM->max_exec_len;

Calculates the maximum command size the OS can exec.  Effectively,
this is the max size of a shell command line.

=for _private
$self->{_MAX_EXEC_LEN} is set by this method, but only for testing purposes.


=head3 make

    my $make = $MM->make;

Returns the make variant we're generating the Makefile for.  This attempts
to do some normalization on the information from %Config or the user.

=cut

sub make {
    my $self = shift;

    my $make = lc $self->{MAKE};

    # Truncate anything like foomake6 to just foomake.
    $make =~ s/^(\w+make).*/$1/;

    # Turn gnumake into gmake.
    $make =~ s/^gnu/g/;

    return $make;
}


=head2 Targets

These are methods which produce make targets.


=head3 all_target

Generate the default target 'all'.

=cut

sub all_target {
    my $self = shift;

    return <<'MAKE_EXT';
all :: pure_all
	$(NOECHO) $(NOOP)
MAKE_EXT

}


=head3 blibdirs_target

    my $make_frag = $mm->blibdirs_target;

Creates the blibdirs target which creates all the directories we use
in blib/.

The blibdirs.ts target is deprecated.  Depend on blibdirs instead.


=cut

sub blibdirs_target {
    my $self = shift;

    my @dirs = map { uc "\$(INST_$_)" } qw(libdir archlib
                                           autodir archautodir
                                           bin script
                                           man1dir man3dir
                                          );

    my @exists = map { $_.'$(DFSEP).exists' } @dirs;

    my $make = sprintf <<'MAKE', join(' ', @exists);
blibdirs : %s
	$(NOECHO) $(NOOP)

# Backwards compat with 6.18 through 6.25
blibdirs.ts : blibdirs
	$(NOECHO) $(NOOP)

MAKE

    $make .= $self->dir_target(@dirs);

    return $make;
}


=head3 clean (o)

Defines the clean target.

=cut

sub clean {
# --- Cleanup and Distribution Sections ---

    my($self, %attribs) = @_;
    my @m;
    push(@m, '
# Delete temporary files but do not touch installed files. We don\'t delete
# the Makefile here so a later make realclean still has a makefile to use.

clean :: clean_subdirs
');

    my @files = values %{$self->{XS}}; # .c files from *.xs files
    my @dirs  = qw(blib);

    # Normally these are all under blib but they might have been
    # redefined.
    # XXX normally this would be a good idea, but the Perl core sets
    # INST_LIB = ../../lib rather than actually installing the files.
    # So a "make clean" in an ext/ directory would blow away lib.
    # Until the core is adjusted let's leave this out.
#     push @dirs, qw($(INST_ARCHLIB) $(INST_LIB)
#                    $(INST_BIN) $(INST_SCRIPT)
#                    $(INST_MAN1DIR) $(INST_MAN3DIR)
#                    $(INST_LIBDIR) $(INST_ARCHLIBDIR) $(INST_AUTODIR) 
#                    $(INST_STATIC) $(INST_DYNAMIC)
#                 );
                  

    if( $attribs{FILES} ) {
        # Use @dirs because we don't know what's in here.
        push @dirs, ref $attribs{FILES}                ?
                        @{$attribs{FILES}}             :
                        split /\s+/, $attribs{FILES}   ;
    }

    push(@files, qw[$(MAKE_APERL_FILE) 
                    perlmain.c tmon.out mon.out so_locations 
                    blibdirs.ts pm_to_blib pm_to_blib.ts
                    *$(OBJ_EXT) *$(LIB_EXT) perl.exe perl perl$(EXE_EXT)
                    $(BOOTSTRAP) $(BASEEXT).bso
                    $(BASEEXT).def lib$(BASEEXT).def
                    $(BASEEXT).exp $(BASEEXT).x
                   ]);

    push(@files, $self->catfile('$(INST_ARCHAUTODIR)','extralibs.all'));
    push(@files, $self->catfile('$(INST_ARCHAUTODIR)','extralibs.ld'));

    # core files
    push(@files, qw[core core.*perl.*.? *perl.core]);
    push(@files, map { "core." . "[0-9]"x$_ } (1..5));

    # OS specific things to clean up.  Use @dirs since we don't know
    # what might be in here.
    push @dirs, $self->extra_clean_files;

    # Occasionally files are repeated several times from different sources
    { my(%f) = map { ($_ => 1) } @files; @files = keys %f; }
    { my(%d) = map { ($_ => 1) } @dirs;  @dirs  = keys %d; }

    push @m, map "\t$_\n", $self->split_command('- $(RM_F)',  @files);
    push @m, map "\t$_\n", $self->split_command('- $(RM_RF)', @dirs);

    # Leave Makefile.old around for realclean
    push @m, <<'MAKE';
	- $(MV) $(FIRST_MAKEFILE) $(MAKEFILE_OLD) $(DEV_NULL)
MAKE

    push(@m, "\t$attribs{POSTOP}\n")   if $attribs{POSTOP};

    join("", @m);
}


=head3 clean_subdirs_target

  my $make_frag = $MM->clean_subdirs_target;

Returns the clean_subdirs target.  This is used by the clean target to
call clean on any subdirectories which contain Makefiles.

=cut

sub clean_subdirs_target {
    my($self) = shift;

    # No subdirectories, no cleaning.
    return <<'NOOP_FRAG' unless @{$self->{DIR}};
clean_subdirs :
	$(NOECHO) $(NOOP)
NOOP_FRAG


    my $clean = "clean_subdirs :\n";

    for my $dir (@{$self->{DIR}}) {
        my $subclean = $self->oneliner(sprintf <<'CODE', $dir);
chdir '%s';  system '$(MAKE) clean' if -f '$(FIRST_MAKEFILE)';
CODE

        $clean .= "\t$subclean\n";
    }

    return $clean;
}


=head3 dir_target

    my $make_frag = $mm->dir_target(@directories);

Generates targets to create the specified directories and set its
permission to 0755.

Because depending on a directory to just ensure it exists doesn't work
too well (the modified time changes too often) dir_target() creates a
.exists file in the created directory.  It is this you should depend on.
For portability purposes you should use the $(DIRFILESEP) macro rather
than a '/' to seperate the directory from the file.

    yourdirectory$(DIRFILESEP).exists

=cut

sub dir_target {
    my($self, @dirs) = @_;

    my $make = '';
    foreach my $dir (@dirs) {
        $make .= sprintf <<'MAKE', ($dir) x 7;
%s$(DFSEP).exists :: Makefile.PL
	$(NOECHO) $(MKPATH) %s
	$(NOECHO) $(CHMOD) 755 %s
	$(NOECHO) $(TOUCH) %s$(DFSEP).exists

MAKE

    }

    return $make;
}


=head3 distdir

Defines the scratch directory target that will hold the distribution
before tar-ing (or shar-ing).

=cut

# For backwards compatibility.
*dist_dir = *distdir;

sub distdir {
    my($self) = shift;

    my $meta_target = $self->{NO_META} ? '' : 'distmeta';
    my $sign_target = !$self->{SIGN}   ? '' : 'distsignature';

    return sprintf <<'MAKE_FRAG', $meta_target, $sign_target;
create_distdir :
	$(RM_RF) $(DISTVNAME)
	$(PERLRUN) "-MExtUtils::Manifest=manicopy,maniread" \
		-e "manicopy(maniread(),'$(DISTVNAME)', '$(DIST_CP)');"

distdir : create_distdir %s %s
	$(NOECHO) $(NOOP)

MAKE_FRAG

}


=head3 dist_test

Defines a target that produces the distribution in the
scratchdirectory, and runs 'perl Makefile.PL; make ;make test' in that
subdirectory.

=cut

sub dist_test {
    my($self) = shift;

    my $mpl_args = join " ", map qq["$_"], @ARGV;

    my $test = $self->cd('$(DISTVNAME)',
                         '$(ABSPERLRUN) Makefile.PL '.$mpl_args,
                         '$(MAKE) $(PASTHRU)',
                         '$(MAKE) test $(PASTHRU)'
                        );

    return sprintf <<'MAKE_FRAG', $test;
disttest : distdir
	%s

MAKE_FRAG


}


=head3 dynamic (o)

Defines the dynamic target.

=cut

sub dynamic {
# --- Dynamic Loading Sections ---
# Note:  $(INST_BOOT) moved to dynamic_lib to get rid of empty .bs

    my($self) = shift;
    '
dynamic :: $(FIRST_MAKEFILE) $(BOOTSTRAP) $(INST_DYNAMIC)
	$(NOECHO) $(NOOP)
';
}


=head3 makemakerdflt_target

  my $make_frag = $mm->makemakerdflt_target

Returns a make fragment with the makemakerdeflt_target specified.
This target is the first target in the Makefile, is the default target
and simply points off to 'all' just in case any make variant gets
confused or something gets snuck in before the real 'all' target.

=cut

sub makemakerdflt_target {
    return <<'MAKE_FRAG';
makemakerdflt : all
	$(NOECHO) $(NOOP)
MAKE_FRAG

}


=head3 manifypods_target

  my $manifypods_target = $self->manifypods_target;

Generates the manifypods target.  This target generates man pages from
all POD files in MAN1PODS and MAN3PODS.

=cut

sub manifypods_target {
    my($self) = shift;

    my $man1pods      = '';
    my $man3pods      = '';
    my $dependencies  = '';

    # populate manXpods & dependencies:
    foreach my $name (keys %{$self->{MAN1PODS}}, keys %{$self->{MAN3PODS}}) {
        $dependencies .= " \\\n\t$name";
    }

    my $manify = <<END;
manifypods : pure_all $dependencies
END

    my @man_cmds;
    foreach my $section (qw(1 3)) {
        my $pods = $self->{"MAN${section}PODS"};
        push @man_cmds, $self->split_command(<<CMD, %$pods);
	\$(NOECHO) \$(POD2MAN) --section=$section --perm_rw=\$(PERM_RW)
CMD
    }

    $manify .= "\t\$(NOECHO) \$(NOOP)\n" unless @man_cmds;
    $manify .= join '', map { "$_\n" } @man_cmds;

    return $manify;
}


=head3 metafile_target

    my $target = $mm->metafile_target;

Generate the metafile target.

Writes the file META.yml YAML encoded meta-data about the module in
the distdir.  The format follows Module::Build's as closely as
possible.

=cut

sub metafile_target {
    my $self = shift;

    return <<'MAKE_FRAG' if $self->{NO_META};
metafile :
	$(NOECHO) $(NOOP)
MAKE_FRAG

    my $prereq_pm = '';
    foreach my $mod ( sort { lc $a cmp lc $b } keys %{$self->{PREREQ_PM}} ) {
        my $ver = $self->{PREREQ_PM}{$mod};
        $prereq_pm .= sprintf "\n    %-30s %s", "$mod:", $ver;
    }

    my $author_value = defined $self->{AUTHOR}
        ? "\n    - $self->{AUTHOR}"
        : undef;

    # Use a list to preserve order.
    my @meta_to_mm = (
        name         => $self->{DISTNAME},
        version      => $self->{VERSION},
        abstract     => $self->{ABSTRACT},
        license      => $self->{LICENSE},
        author       => $author_value,
        generated_by => 
                "ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION",
        distribution_type => $self->{PM} ? 'module' : 'script',
    );

    my $meta = "--- #YAML:1.0\n";

    while( @meta_to_mm ) {
        my($key, $val) = splice @meta_to_mm, 0, 2;

        $val = '~' unless defined $val;

        $meta .= sprintf "%-20s %s\n", "$key:", $val;
    };

    $meta .= <<"YAML";
requires:     $prereq_pm
meta-spec:
    url:     http://module-build.sourceforge.net/META-spec-v1.3.html
    version: 1.3
YAML

    $meta .= $self->{EXTRA_META} if $self->{EXTRA_META};

    my @write_meta = $self->echo($meta, 'META_new.yml');

    return sprintf <<'MAKE_FRAG', join("\n\t", @write_meta);
metafile : create_distdir
	$(NOECHO) $(ECHO) Generating META.yml
	%s
	-$(NOECHO) $(MV) META_new.yml $(DISTVNAME)/META.yml
MAKE_FRAG

}


=head3 distmeta_target

    my $make_frag = $mm->distmeta_target;

Generates the distmeta target to add META.yml to the MANIFEST in the
distdir.

=cut

sub distmeta_target {
    my $self = shift;

    my $add_meta = $self->oneliner(<<'CODE', ['-MExtUtils::Manifest=maniadd']);
eval { maniadd({q{META.yml} => q{Module meta-data (added by MakeMaker)}}) } 
    or print "Could not add META.yml to MANIFEST: $${'@'}\n"
CODE

    my $add_meta_to_distdir = $self->cd('$(DISTVNAME)', $add_meta);

    return sprintf <<'MAKE', $add_meta_to_distdir;
distmeta : create_distdir metafile
	$(NOECHO) %s

MAKE

}


=head3 realclean (o)

Defines the realclean target.

=cut

sub realclean {
    my($self, %attribs) = @_;

    my @dirs  = qw($(DISTVNAME));
    my @files = qw($(FIRST_MAKEFILE) $(MAKEFILE_OLD));

    # Special exception for the perl core where INST_* is not in blib.
    # This cleans up the files built from the ext/ directory (all XS).
    if( $self->{PERL_CORE} ) {
	push @dirs, qw($(INST_AUTODIR) $(INST_ARCHAUTODIR));
        push @files, values %{$self->{PM}};
    }

    if( $self->has_link_code ){
        push @files, qw($(OBJECT));
    }

    if( $attribs{FILES} ) {
        if( ref $attribs{FILES} ) {
            push @dirs, @{ $attribs{FILES} };
        }
        else {
            push @dirs, split /\s+/, $attribs{FILES};
        }
    }

    # Occasionally files are repeated several times from different sources
    { my(%f) = map { ($_ => 1) } @files;  @files = keys %f; }
    { my(%d) = map { ($_ => 1) } @dirs;   @dirs  = keys %d; }

    my $rm_cmd  = join "\n\t", map { "$_" } 
                    $self->split_command('- $(RM_F)',  @files);
    my $rmf_cmd = join "\n\t", map { "$_" } 
                    $self->split_command('- $(RM_RF)', @dirs);

    my $m = sprintf <<'MAKE', $rm_cmd, $rmf_cmd;
# Delete temporary files (via clean) and also delete dist files
realclean purge ::  clean realclean_subdirs
	%s
	%s
MAKE

    $m .= "\t$attribs{POSTOP}\n" if $attribs{POSTOP};

    return $m;
}


=head3 realclean_subdirs_target

  my $make_frag = $MM->realclean_subdirs_target;

Returns the realclean_subdirs target.  This is used by the realclean
target to call realclean on any subdirectories which contain Makefiles.

=cut

sub realclean_subdirs_target {
    my $self = shift;

    return <<'NOOP_FRAG' unless @{$self->{DIR}};
realclean_subdirs :
	$(NOECHO) $(NOOP)
NOOP_FRAG

    my $rclean = "realclean_subdirs :\n";

    foreach my $dir (@{$self->{DIR}}) {
        foreach my $makefile ('$(MAKEFILE_OLD)', '$(FIRST_MAKEFILE)' ) {
            my $subrclean .= $self->oneliner(sprintf <<'CODE', $dir, ($makefile) x 2);
chdir '%s';  system '$(MAKE) $(USEMAKEFILE) %s realclean' if -f '%s';
CODE

            $rclean .= sprintf <<'RCLEAN', $subrclean;
	- %s
RCLEAN

        }
    }

    return $rclean;
}


=head3 signature_target

    my $target = $mm->signature_target;

Generate the signature target.

Writes the file SIGNATURE with "cpansign -s".

=cut

sub signature_target {
    my $self = shift;

    return <<'MAKE_FRAG';
signature :
	cpansign -s
MAKE_FRAG

}


=head3 distsignature_target

    my $make_frag = $mm->distsignature_target;

Generates the distsignature target to add SIGNATURE to the MANIFEST in the
distdir.

=cut

sub distsignature_target {
    my $self = shift;

    my $add_sign = $self->oneliner(<<'CODE', ['-MExtUtils::Manifest=maniadd']);
eval { maniadd({q{SIGNATURE} => q{Public-key signature (added by MakeMaker)}}) } 
    or print "Could not add SIGNATURE to MANIFEST: $${'@'}\n"
CODE

    my $sign_dist        = $self->cd('$(DISTVNAME)' => 'cpansign -s');

    # cpansign -s complains if SIGNATURE is in the MANIFEST yet does not
    # exist
    my $touch_sig        = $self->cd('$(DISTVNAME)' => '$(TOUCH) SIGNATURE');
    my $add_sign_to_dist = $self->cd('$(DISTVNAME)' => $add_sign );

    return sprintf <<'MAKE', $add_sign_to_dist, $touch_sig, $sign_dist
distsignature : create_distdir
	$(NOECHO) %s
	$(NOECHO) %s
	%s

MAKE

}


=head3 special_targets

  my $make_frag = $mm->special_targets

Returns a make fragment containing any targets which have special
meaning to make.  For example, .SUFFIXES and .PHONY.

=cut

sub special_targets {
    my $make_frag = <<'MAKE_FRAG';
.SUFFIXES : .xs .c .C .cpp .i .s .cxx .cc $(OBJ_EXT)

.PHONY: all config static dynamic test linkext manifest blibdirs clean realclean disttest distdir

MAKE_FRAG

    $make_frag .= <<'MAKE_FRAG' if $ENV{CLEARCASE_ROOT};
.NO_CONFIG_REC: Makefile

MAKE_FRAG

    return $make_frag;
}




=head2 Init methods

Methods which help initialize the MakeMaker object and macros.


=head3 init_ABSTRACT

    $mm->init_ABSTRACT

=cut

sub init_ABSTRACT {
    my $self = shift;

    if( $self->{ABSTRACT_FROM} and $self->{ABSTRACT} ) {
        warn "Both ABSTRACT_FROM and ABSTRACT are set.  ".
             "Ignoring ABSTRACT_FROM.\n";
        return;
    }

    if ($self->{ABSTRACT_FROM}){
        $self->{ABSTRACT} = $self->parse_abstract($self->{ABSTRACT_FROM}) or
            carp "WARNING: Setting ABSTRACT via file ".
                 "'$self->{ABSTRACT_FROM}' failed\n";
    }
}

=head3 init_INST

    $mm->init_INST;

Called by init_main.  Sets up all INST_* variables except those related
to XS code.  Those are handled in init_xs.

=cut

sub init_INST {
    my($self) = shift;

    $self->{INST_ARCHLIB} ||= $self->catdir($Curdir,"blib","arch");
    $self->{INST_BIN}     ||= $self->catdir($Curdir,'blib','bin');

    # INST_LIB typically pre-set if building an extension after
    # perl has been built and installed. Setting INST_LIB allows
    # you to build directly into, say $Config{privlibexp}.
    unless ($self->{INST_LIB}){
	if ($self->{PERL_CORE}) {
            if (defined $Cross::platform) {
                $self->{INST_LIB} = $self->{INST_ARCHLIB} = 
                  $self->catdir($self->{PERL_LIB},"..","xlib",
                                     $Cross::platform);
            }
            else {
                $self->{INST_LIB} = $self->{INST_ARCHLIB} = $self->{PERL_LIB};
            }
	} else {
	    $self->{INST_LIB} = $self->catdir($Curdir,"blib","lib");
	}
    }

    my @parentdir = split(/::/, $self->{PARENT_NAME});
    $self->{INST_LIBDIR}      = $self->catdir('$(INST_LIB)',     @parentdir);
    $self->{INST_ARCHLIBDIR}  = $self->catdir('$(INST_ARCHLIB)', @parentdir);
    $self->{INST_AUTODIR}     = $self->catdir('$(INST_LIB)', 'auto', 
                                              '$(FULLEXT)');
    $self->{INST_ARCHAUTODIR} = $self->catdir('$(INST_ARCHLIB)', 'auto',
                                              '$(FULLEXT)');

    $self->{INST_SCRIPT}  ||= $self->catdir($Curdir,'blib','script');

    $self->{INST_MAN1DIR} ||= $self->catdir($Curdir,'blib','man1');
    $self->{INST_MAN3DIR} ||= $self->catdir($Curdir,'blib','man3');

    return 1;
}


=head3 init_INSTALL

    $mm->init_INSTALL;

Called by init_main.  Sets up all INSTALL_* variables (except
INSTALLDIRS) and *PREFIX.

=cut

sub init_INSTALL {
    my($self) = shift;

    if( $self->{ARGS}{INSTALL_BASE} and $self->{ARGS}{PREFIX} ) {
        die "Only one of PREFIX or INSTALL_BASE can be given.  Not both.\n";
    }

    if( $self->{ARGS}{INSTALL_BASE} ) {
        $self->init_INSTALL_from_INSTALL_BASE;
    }
    else {
        $self->init_INSTALL_from_PREFIX;
    }
}


=head3 init_INSTALL_from_PREFIX

  $mm->init_INSTALL_from_PREFIX;

=cut

sub init_INSTALL_from_PREFIX {
    my $self = shift;

    $self->init_lib2arch;

    # There are often no Config.pm defaults for these new man variables so 
    # we fall back to the old behavior which is to use installman*dir
    foreach my $num (1, 3) {
        my $k = 'installsiteman'.$num.'dir';

        $self->{uc $k} ||= uc "\$(installman${num}dir)"
          unless $Config{$k};
    }

    foreach my $num (1, 3) {
        my $k = 'installvendorman'.$num.'dir';

        unless( $Config{$k} ) {
            $self->{uc $k}  ||= $Config{usevendorprefix}
                              ? uc "\$(installman${num}dir)"
                              : '';
        }
    }

    $self->{INSTALLSITEBIN} ||= '$(INSTALLBIN)'
      unless $Config{installsitebin};
    $self->{INSTALLSITESCRIPT} ||= '$(INSTALLSCRIPT)'
      unless $Config{installsitescript};

    unless( $Config{installvendorbin} ) {
        $self->{INSTALLVENDORBIN} ||= $Config{usevendorprefix} 
                                    ? $Config{installbin}
                                    : '';
    }
    unless( $Config{installvendorscript} ) {
        $self->{INSTALLVENDORSCRIPT} ||= $Config{usevendorprefix}
                                       ? $Config{installscript}
                                       : '';
    }


    my $iprefix = $Config{installprefixexp} || $Config{installprefix} || 
                  $Config{prefixexp}        || $Config{prefix} || '';
    my $vprefix = $Config{usevendorprefix}  ? $Config{vendorprefixexp} : '';
    my $sprefix = $Config{siteprefixexp}    || '';

    # 5.005_03 doesn't have a siteprefix.
    $sprefix = $iprefix unless $sprefix;


    $self->{PREFIX}       ||= '';

    if( $self->{PREFIX} ) {
        @{$self}{qw(PERLPREFIX SITEPREFIX VENDORPREFIX)} =
          ('$(PREFIX)') x 3;
    }
    else {
        $self->{PERLPREFIX}   ||= $iprefix;
        $self->{SITEPREFIX}   ||= $sprefix;
        $self->{VENDORPREFIX} ||= $vprefix;

        # Lots of MM extension authors like to use $(PREFIX) so we
        # put something sensible in there no matter what.
        $self->{PREFIX} = '$('.uc $self->{INSTALLDIRS}.'PREFIX)';
    }

    my $arch    = $Config{archname};
    my $version = $Config{version};

    # default style
    my $libstyle = $Config{installstyle} || 'lib/perl5';
    my $manstyle = '';

    if( $self->{LIBSTYLE} ) {
        $libstyle = $self->{LIBSTYLE};
        $manstyle = $self->{LIBSTYLE} eq 'lib/perl5' ? 'lib/perl5' : '';
    }

    # Some systems, like VOS, set installman*dir to '' if they can't
    # read man pages.
    for my $num (1, 3) {
        $self->{'INSTALLMAN'.$num.'DIR'} ||= 'none'
          unless $Config{'installman'.$num.'dir'};
    }

    my %bin_layouts = 
    (
        bin         => { s => $iprefix,
                         t => 'perl',
                         d => 'bin' },
        vendorbin   => { s => $vprefix,
                         t => 'vendor',
                         d => 'bin' },
        sitebin     => { s => $sprefix,
                         t => 'site',
                         d => 'bin' },
        script      => { s => $iprefix,
                         t => 'perl',
                         d => 'bin' },
        vendorscript=> { s => $vprefix,
                         t => 'vendor',
                         d => 'bin' },
        sitescript  => { s => $sprefix,
                         t => 'site',
                         d => 'bin' },
    );
    
    my %man_layouts =
    (
        man1dir         => { s => $iprefix,
                             t => 'perl',
                             d => 'man/man1',
                             style => $manstyle, },
        siteman1dir     => { s => $sprefix,
                             t => 'site',
                             d => 'man/man1',
                             style => $manstyle, },
        vendorman1dir   => { s => $vprefix,
                             t => 'vendor',
                             d => 'man/man1',
                             style => $manstyle, },

        man3dir         => { s => $iprefix,
                             t => 'perl',
                             d => 'man/man3',
                             style => $manstyle, },
        siteman3dir     => { s => $sprefix,
                             t => 'site',
                             d => 'man/man3',
                             style => $manstyle, },
        vendorman3dir   => { s => $vprefix,
                             t => 'vendor',
                             d => 'man/man3',
                             style => $manstyle, },
    );

    my %lib_layouts =
    (
        privlib     => { s => $iprefix,
                         t => 'perl',
                         d => '',
                         style => $libstyle, },
        vendorlib   => { s => $vprefix,
                         t => 'vendor',
                         d => '',
                         style => $libstyle, },
        sitelib     => { s => $sprefix,
                         t => 'site',
                         d => 'site_perl',
                         style => $libstyle, },
        
        archlib     => { s => $iprefix,
                         t => 'perl',
                         d => "$version/$arch",
                         style => $libstyle },
        vendorarch  => { s => $vprefix,
                         t => 'vendor',
                         d => "$version/$arch",
                         style => $libstyle },
        sitearch    => { s => $sprefix,
                         t => 'site',
                         d => "site_perl/$version/$arch",
                         style => $libstyle },
    );


    # Special case for LIB.
    if( $self->{LIB} ) {
        foreach my $var (keys %lib_layouts) {
            my $Installvar = uc "install$var";

            if( $var =~ /arch/ ) {
                $self->{$Installvar} ||= 
                  $self->catdir($self->{LIB}, $Config{archname});
            }
            else {
                $self->{$Installvar} ||= $self->{LIB};
            }
        }
    }

    my %type2prefix = ( perl    => 'PERLPREFIX',
                        site    => 'SITEPREFIX',
                        vendor  => 'VENDORPREFIX'
                      );

    my %layouts = (%bin_layouts, %man_layouts, %lib_layouts);
    while( my($var, $layout) = each(%layouts) ) {
        my($s, $t, $d, $style) = @{$layout}{qw(s t d style)};
        my $r = '$('.$type2prefix{$t}.')';

        print STDERR "Prefixing $var\n" if $Verbose >= 2;

        my $installvar = "install$var";
        my $Installvar = uc $installvar;
        next if $self->{$Installvar};

        $d = "$style/$d" if $style;
        $self->prefixify($installvar, $s, $r, $d);

        print STDERR "  $Installvar == $self->{$Installvar}\n" 
          if $Verbose >= 2;
    }

    # Generate these if they weren't figured out.
    $self->{VENDORARCHEXP} ||= $self->{INSTALLVENDORARCH};
    $self->{VENDORLIBEXP}  ||= $self->{INSTALLVENDORLIB};

    return 1;
}


=head3 init_from_INSTALL_BASE

    $mm->init_from_INSTALL_BASE

=cut

my %map = (
           lib      => [qw(lib perl5)],
           arch     => [('lib', 'perl5', $Config{archname})],
           bin      => [qw(bin)],
           man1dir  => [qw(man man1)],
           man3dir  => [qw(man man3)]
          );
$map{script} = $map{bin};

sub init_INSTALL_from_INSTALL_BASE {
    my $self = shift;

    @{$self}{qw(PREFIX VENDORPREFIX SITEPREFIX PERLPREFIX)} = 
                                                         '$(INSTALL_BASE)';

    my %install;
    foreach my $thing (keys %map) {
        foreach my $dir (('', 'SITE', 'VENDOR')) {
            my $uc_thing = uc $thing;
            my $key = "INSTALL".$dir.$uc_thing;

            $install{$key} ||= 
              $self->catdir('$(INSTALL_BASE)', @{$map{$thing}});
        }
    }

    # Adjust for variable quirks.
    $install{INSTALLARCHLIB} ||= delete $install{INSTALLARCH};
    $install{INSTALLPRIVLIB} ||= delete $install{INSTALLLIB};

    foreach my $key (keys %install) {
        $self->{$key} ||= $install{$key};
    }

    return 1;
}


=head3 init_VERSION  I<Abstract>

    $mm->init_VERSION

Initialize macros representing versions of MakeMaker and other tools

MAKEMAKER: path to the MakeMaker module.

MM_VERSION: ExtUtils::MakeMaker Version

MM_REVISION: ExtUtils::MakeMaker version control revision (for backwards 
             compat)

VERSION: version of your module

VERSION_MACRO: which macro represents the version (usually 'VERSION')

VERSION_SYM: like version but safe for use as an RCS revision number

DEFINE_VERSION: -D line to set the module version when compiling

XS_VERSION: version in your .xs file.  Defaults to $(VERSION)

XS_VERSION_MACRO: which macro represents the XS version.

XS_DEFINE_VERSION: -D line to set the xs version when compiling.

Called by init_main.

=cut

sub init_VERSION {
    my($self) = shift;

    $self->{MAKEMAKER}  = $ExtUtils::MakeMaker::Filename;
    $self->{MM_VERSION} = $ExtUtils::MakeMaker::VERSION;
    $self->{MM_REVISION}= $ExtUtils::MakeMaker::Revision;
    $self->{VERSION_FROM} ||= '';

    if ($self->{VERSION_FROM}){
        $self->{VERSION} = $self->parse_version($self->{VERSION_FROM});
        if( $self->{VERSION} eq 'undef' ) {
            carp("WARNING: Setting VERSION via file ".
                 "'$self->{VERSION_FROM}' failed\n");
        }
    }

    # strip blanks
    if (defined $self->{VERSION}) {
        $self->{VERSION} =~ s/^\s+//;
        $self->{VERSION} =~ s/\s+$//;
    }
    else {
        $self->{VERSION} = '';
    }


    $self->{VERSION_MACRO}  = 'VERSION';
    ($self->{VERSION_SYM} = $self->{VERSION}) =~ s/\W/_/g;
    $self->{DEFINE_VERSION} = '-D$(VERSION_MACRO)=\"$(VERSION)\"';


    # Graham Barr and Paul Marquess had some ideas how to ensure
    # version compatibility between the *.pm file and the
    # corresponding *.xs file. The bottomline was, that we need an
    # XS_VERSION macro that defaults to VERSION:
    $self->{XS_VERSION} ||= $self->{VERSION};

    $self->{XS_VERSION_MACRO}  = 'XS_VERSION';
    $self->{XS_DEFINE_VERSION} = '-D$(XS_VERSION_MACRO)=\"$(XS_VERSION)\"';

}


=head3 init_others  I<Abstract>

    $MM->init_others();

Initializes the macro definitions used by tools_other() and places them
in the $MM object.

If there is no description, its the same as the parameter to
WriteMakefile() documented in ExtUtils::MakeMaker.

Defines at least these macros.

  Macro             Description

  NOOP              Do nothing
  NOECHO            Tell make not to display the command itself

  MAKEFILE
  FIRST_MAKEFILE
  MAKEFILE_OLD
  MAKE_APERL_FILE   File used by MAKE_APERL

  SHELL             Program used to run shell commands

  ECHO              Print text adding a newline on the end
  RM_F              Remove a file 
  RM_RF             Remove a directory          
  TOUCH             Update a file's timestamp   
  TEST_F            Test for a file's existence 
  CP                Copy a file                 
  MV                Move a file                 
  CHMOD             Change permissions on a     
                    file

  UMASK_NULL        Nullify umask
  DEV_NULL          Suppress all command output


=head3 init_DIRFILESEP  I<Abstract>

  $MM->init_DIRFILESEP;
  my $dirfilesep = $MM->{DIRFILESEP};

Initializes the DIRFILESEP macro which is the seperator between the
directory and filename in a filepath.  ie. / on Unix, \ on Win32 and
nothing on VMS.

For example:

    # instead of $(INST_ARCHAUTODIR)/extralibs.ld
    $(INST_ARCHAUTODIR)$(DIRFILESEP)extralibs.ld

Something of a hack but it prevents a lot of code duplication between
MM_* variants.

Do not use this as a seperator between directories.  Some operating
systems use different seperators between subdirectories as between
directories and filenames (for example:  VOLUME:[dir1.dir2]file on VMS).

=head3 init_linker  I<Abstract>

    $mm->init_linker;

Initialize macros which have to do with linking.

PERL_ARCHIVE: path to libperl.a equivalent to be linked to dynamic
extensions.

PERL_ARCHIVE_AFTER: path to a library which should be put on the
linker command line I<after> the external libraries to be linked to
dynamic extensions.  This may be needed if the linker is one-pass, and
Perl includes some overrides for C RTL functions, such as malloc().

EXPORT_LIST: name of a file that is passed to linker to define symbols
to be exported.

Some OSes do not need these in which case leave it blank.


=head3 init_platform

    $mm->init_platform

Initialize any macros which are for platform specific use only.

A typical one is the version number of your OS specific mocule.
(ie. MM_Unix_VERSION or MM_VMS_VERSION).

=cut

sub init_platform {
    return '';
}


=head3 init_MAKE

    $mm->init_MAKE

Initialize MAKE from either a MAKE environment variable or $Config{make}.

=cut

sub init_MAKE {
    my $self = shift;

    $self->{MAKE} ||= $ENV{MAKE} || $Config{make};
}


=head2 Tools

A grab bag of methods to generate specific macros and commands.



=head3 manifypods

Defines targets and routines to translate the pods into manpages and
put them into the INST_* directories.

=cut

sub manifypods {
    my $self          = shift;

    my $POD2MAN_macro = $self->POD2MAN_macro();
    my $manifypods_target = $self->manifypods_target();

    return <<END_OF_TARGET;

$POD2MAN_macro

$manifypods_target

END_OF_TARGET

}


=head3 POD2MAN_macro

  my $pod2man_macro = $self->POD2MAN_macro

Returns a definition for the POD2MAN macro.  This is a program
which emulates the pod2man utility.  You can add more switches to the
command by simply appending them on the macro.

Typical usage:

    $(POD2MAN) --section=3 --perm_rw=$(PERM_RW) podfile1 man_page1 ...

=cut

sub POD2MAN_macro {
    my $self = shift;

# Need the trailing '--' so perl stops gobbling arguments and - happens
# to be an alternative end of line seperator on VMS so we quote it
    return <<'END_OF_DEF';
POD2MAN_EXE = $(PERLRUN) "-MExtUtils::Command::MM" -e pod2man "--"
POD2MAN = $(POD2MAN_EXE)
END_OF_DEF
}


=head3 test_via_harness

  my $command = $mm->test_via_harness($perl, $tests);

Returns a $command line which runs the given set of $tests with
Test::Harness and the given $perl.

Used on the t/*.t files.

=cut

sub test_via_harness {
    my($self, $perl, $tests) = @_;

    return qq{\t$perl "-MExtUtils::Command::MM" }.
           qq{"-e" "test_harness(\$(TEST_VERBOSE), '\$(INST_LIB)', '\$(INST_ARCHLIB)')" $tests\n};
}

=head3 test_via_script

  my $command = $mm->test_via_script($perl, $script);

Returns a $command line which just runs a single test without
Test::Harness.  No checks are done on the results, they're just
printed.

Used for test.pl, since they don't always follow Test::Harness
formatting.

=cut

sub test_via_script {
    my($self, $perl, $script) = @_;
    return qq{\t$perl "-I\$(INST_LIB)" "-I\$(INST_ARCHLIB)" $script\n};
}


=head3 tool_autosplit

Defines a simple perl call that runs autosplit. May be deprecated by
pm_to_blib soon.

=cut

sub tool_autosplit {
    my($self, %attribs) = @_;

    my $maxlen = $attribs{MAXLEN} ? '$$AutoSplit::Maxlen=$attribs{MAXLEN};' 
                                  : '';

    my $asplit = $self->oneliner(sprintf <<'PERL_CODE', $maxlen);
use AutoSplit; %s autosplit($$ARGV[0], $$ARGV[1], 0, 1, 1)
PERL_CODE

    return sprintf <<'MAKE_FRAG', $asplit;
# Usage: $(AUTOSPLITFILE) FileToSplit AutoDirToSplitInto
AUTOSPLITFILE = %s

MAKE_FRAG

}




=head2 File::Spec wrappers

ExtUtils::MM_Any is a subclass of File::Spec.  The methods noted here
override File::Spec.



=head3 catfile

File::Spec <= 0.83 has a bug where the file part of catfile is not
canonicalized.  This override fixes that bug.

=cut

sub catfile {
    my $self = shift;
    return $self->canonpath($self->SUPER::catfile(@_));
}



=head2 Misc

Methods I can't really figure out where they should go yet.


=head3 find_tests

  my $test = $mm->find_tests;

Returns a string suitable for feeding to the shell to return all
tests in t/*.t.

=cut

sub find_tests {
    my($self) = shift;
    return -d 't' ? 't/*.t' : '';
}


=head3 extra_clean_files

    my @files_to_clean = $MM->extra_clean_files;

Returns a list of OS specific files to be removed in the clean target in
addition to the usual set.

=cut

# An empty method here tickled a perl 5.8.1 bug and would return its object.
sub extra_clean_files { 
    return;
}


=head3 installvars

    my @installvars = $mm->installvars;

A list of all the INSTALL* variables without the INSTALL prefix.  Useful
for iteration or building related variable sets.

=cut

sub installvars {
    return qw(PRIVLIB SITELIB  VENDORLIB
              ARCHLIB SITEARCH VENDORARCH
              BIN     SITEBIN  VENDORBIN
              SCRIPT  SITESCRIPT  VENDORSCRIPT
              MAN1DIR SITEMAN1DIR VENDORMAN1DIR
              MAN3DIR SITEMAN3DIR VENDORMAN3DIR
             );
}


=head3 libscan

  my $wanted = $self->libscan($path);

Takes a path to a file or dir and returns an empty string if we don't
want to include this file in the library.  Otherwise it returns the
the $path unchanged.

Mainly used to exclude version control administrative directories from
installation.

=cut

sub libscan {
    my($self,$path) = @_;
    my($dirs,$file) = ($self->splitpath($path))[1,2];
    return '' if grep /^(?:RCS|CVS|SCCS|\.svn|_darcs)$/, 
                     $self->splitdir($dirs), $file;

    return $path;
}


=head3 platform_constants

    my $make_frag = $mm->platform_constants

Returns a make fragment defining all the macros initialized in
init_platform() rather than put them in constants().

=cut

sub platform_constants {
    return '';
}


=head1 AUTHOR

Michael G Schwern <schwern@pobox.com> and the denizens of
makemaker@perl.org with code from ExtUtils::MM_Unix and
ExtUtils::MM_Win32.


=cut

1;
