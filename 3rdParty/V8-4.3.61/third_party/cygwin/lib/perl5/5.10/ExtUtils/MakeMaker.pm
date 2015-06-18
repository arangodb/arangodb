# $Id: /local/ExtUtils-MakeMaker/lib/ExtUtils/MakeMaker.pm 54639 2008-02-29T00:06:55.056100Z schwern  $
package ExtUtils::MakeMaker;

use strict;

BEGIN {require 5.006;}

require Exporter;
use ExtUtils::MakeMaker::Config;
use Carp ();
use File::Path;

our $Verbose = 0;       # exported
our @Parent;            # needs to be localized
our @Get_from_Config;   # referenced by MM_Unix
my @MM_Sections;
my @Overridable;
my @Prepend_parent;
my %Recognized_Att_Keys;

our $VERSION = '6.44';
our ($Revision) = q$Revision: 54639 $ =~ /Revision:\s+(\S+)/;
our $Filename = __FILE__;   # referenced outside MakeMaker

our @ISA = qw(Exporter);
our @EXPORT    = qw(&WriteMakefile &writeMakefile $Verbose &prompt);
our @EXPORT_OK = qw($VERSION &neatvalue &mkbootstrap &mksymlists
                    &WriteEmptyMakefile);

# These will go away once the last of the Win32 & VMS specific code is 
# purged.
my $Is_VMS     = $^O eq 'VMS';
my $Is_Win32   = $^O eq 'MSWin32';

full_setup();

require ExtUtils::MM;  # Things like CPAN assume loading ExtUtils::MakeMaker
                       # will give them MM.

require ExtUtils::MY;  # XXX pre-5.8 versions of ExtUtils::Embed expect
                       # loading ExtUtils::MakeMaker will give them MY.
                       # This will go when Embed is it's own CPAN module.


sub WriteMakefile {
    Carp::croak "WriteMakefile: Need even number of args" if @_ % 2;

    require ExtUtils::MY;
    my %att = @_;

    _verify_att(\%att);

    my $mm = MM->new(\%att);
    $mm->flush;

    return $mm;
}


# Basic signatures of the attributes WriteMakefile takes.  Each is the
# reference type.  Empty value indicate it takes a non-reference
# scalar.
my %Att_Sigs;
my %Special_Sigs = (
 C                  => 'ARRAY',
 CONFIG             => 'ARRAY',
 CONFIGURE          => 'CODE',
 DIR                => 'ARRAY',
 DL_FUNCS           => 'HASH',
 DL_VARS            => 'ARRAY',
 EXCLUDE_EXT        => 'ARRAY',
 EXE_FILES          => 'ARRAY',
 FUNCLIST           => 'ARRAY',
 H                  => 'ARRAY',
 IMPORTS            => 'HASH',
 INCLUDE_EXT        => 'ARRAY',
 LIBS               => ['ARRAY',''],
 MAN1PODS           => 'HASH',
 MAN3PODS           => 'HASH',
 PL_FILES           => 'HASH',
 PM                 => 'HASH',
 PMLIBDIRS          => 'ARRAY',
 PMLIBPARENTDIRS    => 'ARRAY',
 PREREQ_PM          => 'HASH',
 SKIP               => 'ARRAY',
 TYPEMAPS           => 'ARRAY',
 XS                 => 'HASH',
 VERSION            => ['version',''],
 _KEEP_AFTER_FLUSH  => '',

 clean      => 'HASH',
 depend     => 'HASH',
 dist       => 'HASH',
 dynamic_lib=> 'HASH',
 linkext    => 'HASH',
 macro      => 'HASH',
 postamble  => 'HASH',
 realclean  => 'HASH',
 test       => 'HASH',
 tool_autosplit => 'HASH',
);

@Att_Sigs{keys %Recognized_Att_Keys} = ('') x keys %Recognized_Att_Keys;
@Att_Sigs{keys %Special_Sigs} = values %Special_Sigs;


sub _verify_att {
    my($att) = @_;

    while( my($key, $val) = each %$att ) {
        my $sig = $Att_Sigs{$key};
        unless( defined $sig ) {
            warn "WARNING: $key is not a known parameter.\n";
            next;
        }

        my @sigs   = ref $sig ? @$sig : $sig;
        my $given  = ref $val;
        unless( grep { $given eq $_ || ($_ && eval{$val->isa($_)}) } @sigs ) {
            my $takes = join " or ", map { _format_att($_) } @sigs;

            my $has = _format_att($given);
            warn "WARNING: $key takes a $takes not a $has.\n".
                 "         Please inform the author.\n";
        }
    }
}


sub _format_att {
    my $given = shift;
    
    return $given eq ''        ? "string/number"
         : uc $given eq $given ? "$given reference"
         :                       "$given object"
         ;
}


sub prompt ($;$) {  ## no critic
    my($mess, $def) = @_;
    Carp::confess("prompt function called without an argument") 
        unless defined $mess;

    my $isa_tty = -t STDIN && (-t STDOUT || !(-f STDOUT || -c STDOUT)) ;

    my $dispdef = defined $def ? "[$def] " : " ";
    $def = defined $def ? $def : "";

    local $|=1;
    local $\;
    print "$mess $dispdef";

    my $ans;
    if ($ENV{PERL_MM_USE_DEFAULT} || (!$isa_tty && eof STDIN)) {
        print "$def\n";
    }
    else {
        $ans = <STDIN>;
        if( defined $ans ) {
            chomp $ans;
        }
        else { # user hit ctrl-D
            print "\n";
        }
    }

    return (!defined $ans || $ans eq '') ? $def : $ans;
}

sub eval_in_subdirs {
    my($self) = @_;
    use Cwd qw(cwd abs_path);
    my $pwd = cwd() || die "Can't figure out your cwd!";

    local @INC = map eval {abs_path($_) if -e} || $_, @INC;
    push @INC, '.';     # '.' has to always be at the end of @INC

    foreach my $dir (@{$self->{DIR}}){
        my($abs) = $self->catdir($pwd,$dir);
        eval { $self->eval_in_x($abs); };
        last if $@;
    }
    chdir $pwd;
    die $@ if $@;
}

sub eval_in_x {
    my($self,$dir) = @_;
    chdir $dir or Carp::carp("Couldn't change to directory $dir: $!");

    {
        package main;
        do './Makefile.PL';
    };
    if ($@) {
#         if ($@ =~ /prerequisites/) {
#             die "MakeMaker WARNING: $@";
#         } else {
#             warn "WARNING from evaluation of $dir/Makefile.PL: $@";
#         }
        die "ERROR from evaluation of $dir/Makefile.PL: $@";
    }
}


# package name for the classes into which the first object will be blessed
my $PACKNAME = 'PACK000';

sub full_setup {
    $Verbose ||= 0;

    my @attrib_help = qw/

    AUTHOR ABSTRACT ABSTRACT_FROM BINARY_LOCATION
    C CAPI CCFLAGS CONFIG CONFIGURE DEFINE DIR DISTNAME DL_FUNCS DL_VARS
    EXCLUDE_EXT EXE_FILES EXTRA_META FIRST_MAKEFILE
    FULLPERL FULLPERLRUN FULLPERLRUNINST
    FUNCLIST H IMPORTS

    INST_ARCHLIB INST_SCRIPT INST_BIN INST_LIB INST_MAN1DIR INST_MAN3DIR
    INSTALLDIRS
    DESTDIR PREFIX INSTALL_BASE
    PERLPREFIX      SITEPREFIX      VENDORPREFIX
    INSTALLPRIVLIB  INSTALLSITELIB  INSTALLVENDORLIB
    INSTALLARCHLIB  INSTALLSITEARCH INSTALLVENDORARCH
    INSTALLBIN      INSTALLSITEBIN  INSTALLVENDORBIN
    INSTALLMAN1DIR          INSTALLMAN3DIR
    INSTALLSITEMAN1DIR      INSTALLSITEMAN3DIR
    INSTALLVENDORMAN1DIR    INSTALLVENDORMAN3DIR
    INSTALLSCRIPT   INSTALLSITESCRIPT  INSTALLVENDORSCRIPT
    PERL_LIB        PERL_ARCHLIB 
    SITELIBEXP      SITEARCHEXP 

    INC INCLUDE_EXT LDFROM LIB LIBPERL_A LIBS LICENSE
    LINKTYPE MAKE MAKEAPERL MAKEFILE MAKEFILE_OLD MAN1PODS MAN3PODS MAP_TARGET 
    MYEXTLIB NAME NEEDS_LINKING NOECHO NO_META NORECURS NO_VC OBJECT OPTIMIZE 
    PERL_MALLOC_OK PERL PERLMAINCC PERLRUN PERLRUNINST PERL_CORE
    PERL_SRC PERM_RW PERM_RWX
    PL_FILES PM PM_FILTER PMLIBDIRS PMLIBPARENTDIRS POLLUTE PPM_INSTALL_EXEC
    PPM_INSTALL_SCRIPT PREREQ_FATAL PREREQ_PM PREREQ_PRINT PRINT_PREREQ
    SIGN SKIP TYPEMAPS VERSION VERSION_FROM XS XSOPT XSPROTOARG
    XS_VERSION clean depend dist dynamic_lib linkext macro realclean
    tool_autosplit

    MACPERL_SRC MACPERL_LIB MACLIBS_68K MACLIBS_PPC MACLIBS_SC MACLIBS_MRC
    MACLIBS_ALL_68K MACLIBS_ALL_PPC MACLIBS_SHARED
        /;

    # IMPORTS is used under OS/2 and Win32

    # @Overridable is close to @MM_Sections but not identical.  The
    # order is important. Many subroutines declare macros. These
    # depend on each other. Let's try to collect the macros up front,
    # then pasthru, then the rules.

    # MM_Sections are the sections we have to call explicitly
    # in Overridable we have subroutines that are used indirectly


    @MM_Sections = 
        qw(

 post_initialize const_config constants platform_constants 
 tool_autosplit tool_xsubpp tools_other 

 makemakerdflt

 dist macro depend cflags const_loadlibs const_cccmd
 post_constants

 pasthru

 special_targets
 c_o xs_c xs_o
 top_targets blibdirs linkext dlsyms dynamic dynamic_bs
 dynamic_lib static static_lib manifypods processPL
 installbin subdirs
 clean_subdirs clean realclean_subdirs realclean 
 metafile signature
 dist_basics dist_core distdir dist_test dist_ci distmeta distsignature
 install force perldepend makefile staticmake test ppd

          ); # loses section ordering

    @Overridable = @MM_Sections;
    push @Overridable, qw[

 libscan makeaperl needs_linking perm_rw perm_rwx
 subdir_x test_via_harness test_via_script 

 init_VERSION init_dist init_INST init_INSTALL init_DEST init_dirscan
 init_PM init_MANPODS init_xs init_PERL init_DIRFILESEP init_linker
                         ];

    push @MM_Sections, qw[

 pm_to_blib selfdocument

                         ];

    # Postamble needs to be the last that was always the case
    push @MM_Sections, "postamble";
    push @Overridable, "postamble";

    # All sections are valid keys.
    @Recognized_Att_Keys{@MM_Sections} = (1) x @MM_Sections;

    # we will use all these variables in the Makefile
    @Get_from_Config = 
        qw(
           ar cc cccdlflags ccdlflags dlext dlsrc exe_ext full_ar ld 
           lddlflags ldflags libc lib_ext obj_ext osname osvers ranlib 
           sitelibexp sitearchexp so
          );

    # 5.5.3 doesn't have any concept of vendor libs
    push @Get_from_Config, qw( vendorarchexp vendorlibexp ) if $] >= 5.006;

    foreach my $item (@attrib_help){
        $Recognized_Att_Keys{$item} = 1;
    }
    foreach my $item (@Get_from_Config) {
        $Recognized_Att_Keys{uc $item} = $Config{$item};
        print "Attribute '\U$item\E' => '$Config{$item}'\n"
            if ($Verbose >= 2);
    }

    #
    # When we eval a Makefile.PL in a subdirectory, that one will ask
    # us (the parent) for the values and will prepend "..", so that
    # all files to be installed end up below OUR ./blib
    #
    @Prepend_parent = qw(
           INST_BIN INST_LIB INST_ARCHLIB INST_SCRIPT
           MAP_TARGET INST_MAN1DIR INST_MAN3DIR PERL_SRC
           PERL FULLPERL
    );
}

sub writeMakefile {
    die <<END;

The extension you are trying to build apparently is rather old and
most probably outdated. We detect that from the fact, that a
subroutine "writeMakefile" is called, and this subroutine is not
supported anymore since about October 1994.

Please contact the author or look into CPAN (details about CPAN can be
found in the FAQ and at http:/www.perl.com) for a more recent version
of the extension. If you're really desperate, you can try to change
the subroutine name from writeMakefile to WriteMakefile and rerun
'perl Makefile.PL', but you're most probably left alone, when you do
so.

The MakeMaker team

END
}

sub new {
    my($class,$self) = @_;
    my($key);

    # Store the original args passed to WriteMakefile()
    foreach my $k (keys %$self) {
        $self->{ARGS}{$k} = $self->{$k};
    }

    if ("@ARGV" =~ /\bPREREQ_PRINT\b/) {
        require Data::Dumper;
        print Data::Dumper->Dump([$self->{PREREQ_PM}], [qw(PREREQ_PM)]);
        exit 0;
    }

    # PRINT_PREREQ is RedHatism.
    if ("@ARGV" =~ /\bPRINT_PREREQ\b/) {
        print join(" ", map { "perl($_)>=$self->{PREREQ_PM}->{$_} " } 
                        sort keys %{$self->{PREREQ_PM}}), "\n";
        exit 0;
   }

    print STDOUT "MakeMaker (v$VERSION)\n" if $Verbose;
    if (-f "MANIFEST" && ! -f "Makefile"){
        check_manifest();
    }

    $self = {} unless (defined $self);

    check_hints($self);

    my %configure_att;         # record &{$self->{CONFIGURE}} attributes
    my(%initial_att) = %$self; # record initial attributes

    my(%unsatisfied) = ();
    foreach my $prereq (sort keys %{$self->{PREREQ_PM}}) {
        # 5.8.0 has a bug with require Foo::Bar alone in an eval, so an
        # extra statement is a workaround.
        my $file = "$prereq.pm";
        $file =~ s{::}{/}g;
        eval { require $file };

        my $pr_version = $prereq->VERSION || 0;

        # convert X.Y_Z alpha version #s to X.YZ for easier comparisons
        $pr_version =~ s/(\d+)\.(\d+)_(\d+)/$1.$2$3/;

        if ($@) {
            warn sprintf "Warning: prerequisite %s %s not found.\n", 
              $prereq, $self->{PREREQ_PM}{$prereq} 
                   unless $self->{PREREQ_FATAL};
            $unsatisfied{$prereq} = 'not installed';
        } elsif ($pr_version < $self->{PREREQ_PM}->{$prereq} ){
            warn sprintf "Warning: prerequisite %s %s not found. We have %s.\n",
              $prereq, $self->{PREREQ_PM}{$prereq}, 
                ($pr_version || 'unknown version') 
                  unless $self->{PREREQ_FATAL};
            $unsatisfied{$prereq} = $self->{PREREQ_PM}->{$prereq} ? 
              $self->{PREREQ_PM}->{$prereq} : 'unknown version' ;
        }
    }
    
     if (%unsatisfied && $self->{PREREQ_FATAL}){
        my $failedprereqs = join "\n", map {"    $_ $unsatisfied{$_}"} 
                            sort { $a cmp $b } keys %unsatisfied;
        die <<"END";
MakeMaker FATAL: prerequisites not found.
$failedprereqs

Please install these modules first and rerun 'perl Makefile.PL'.
END
    }
    
    if (defined $self->{CONFIGURE}) {
        if (ref $self->{CONFIGURE} eq 'CODE') {
            %configure_att = %{&{$self->{CONFIGURE}}};
            $self = { %$self, %configure_att };
        } else {
            Carp::croak "Attribute 'CONFIGURE' to WriteMakefile() not a code reference\n";
        }
    }

    # This is for old Makefiles written pre 5.00, will go away
    if ( Carp::longmess("") =~ /runsubdirpl/s ){
        Carp::carp("WARNING: Please rerun 'perl Makefile.PL' to regenerate your Makefiles\n");
    }

    my $newclass = ++$PACKNAME;
    local @Parent = @Parent;    # Protect against non-local exits
    {
        print "Blessing Object into class [$newclass]\n" if $Verbose>=2;
        mv_all_methods("MY",$newclass);
        bless $self, $newclass;
        push @Parent, $self;
        require ExtUtils::MY;

        no strict 'refs';   ## no critic;
        @{"$newclass\:\:ISA"} = 'MM';
    }

    if (defined $Parent[-2]){
        $self->{PARENT} = $Parent[-2];
        for my $key (@Prepend_parent) {
            next unless defined $self->{PARENT}{$key};

            # Don't stomp on WriteMakefile() args.
            next if defined $self->{ARGS}{$key} and
                    $self->{ARGS}{$key} eq $self->{$key};

            $self->{$key} = $self->{PARENT}{$key};

            unless ($Is_VMS && $key =~ /PERL$/) {
                $self->{$key} = $self->catdir("..",$self->{$key})
                  unless $self->file_name_is_absolute($self->{$key});
            } else {
                # PERL or FULLPERL will be a command verb or even a
                # command with an argument instead of a full file
                # specification under VMS.  So, don't turn the command
                # into a filespec, but do add a level to the path of
                # the argument if not already absolute.
                my @cmd = split /\s+/, $self->{$key};
                $cmd[1] = $self->catfile('[-]',$cmd[1])
                  unless (@cmd < 2) || $self->file_name_is_absolute($cmd[1]);
                $self->{$key} = join(' ', @cmd);
            }
        }
        if ($self->{PARENT}) {
            $self->{PARENT}->{CHILDREN}->{$newclass} = $self;
            foreach my $opt (qw(POLLUTE PERL_CORE LINKTYPE)) {
                if (exists $self->{PARENT}->{$opt}
                    and not exists $self->{$opt})
                    {
                        # inherit, but only if already unspecified
                        $self->{$opt} = $self->{PARENT}->{$opt};
                    }
            }
        }
        my @fm = grep /^FIRST_MAKEFILE=/, @ARGV;
        parse_args($self,@fm) if @fm;
    } else {
        parse_args($self,split(' ', $ENV{PERL_MM_OPT} || ''),@ARGV);
    }


    $self->{NAME} ||= $self->guess_name;

    ($self->{NAME_SYM} = $self->{NAME}) =~ s/\W+/_/g;

    $self->init_MAKE;
    $self->init_main;
    $self->init_VERSION;
    $self->init_dist;
    $self->init_INST;
    $self->init_INSTALL;
    $self->init_DEST;
    $self->init_dirscan;
    $self->init_PM;
    $self->init_MANPODS;
    $self->init_xs;
    $self->init_PERL;
    $self->init_DIRFILESEP;
    $self->init_linker;
    $self->init_ABSTRACT;

    if (! $self->{PERL_SRC} ) {
        require VMS::Filespec if $Is_VMS;
        my($pthinks) = $self->canonpath($INC{'Config.pm'});
        my($cthinks) = $self->catfile($Config{'archlibexp'},'Config.pm');
        $pthinks = VMS::Filespec::vmsify($pthinks) if $Is_VMS;
        if ($pthinks ne $cthinks &&
            !($Is_Win32 and lc($pthinks) eq lc($cthinks))) {
            print "Have $pthinks expected $cthinks\n";
            if ($Is_Win32) {
                $pthinks =~ s![/\\]Config\.pm$!!i; $pthinks =~ s!.*[/\\]!!;
            }
            else {
                $pthinks =~ s!/Config\.pm$!!; $pthinks =~ s!.*/!!;
            }
            print STDOUT <<END unless $self->{UNINSTALLED_PERL};
Your perl and your Config.pm seem to have different ideas about the 
architecture they are running on.
Perl thinks: [$pthinks]
Config says: [$Config{archname}]
This may or may not cause problems. Please check your installation of perl 
if you have problems building this extension.
END
        }
    }

    $self->init_others();
    $self->init_platform();
    $self->init_PERM();
    my($argv) = neatvalue(\@ARGV);
    $argv =~ s/^\[/(/;
    $argv =~ s/\]$/)/;

    push @{$self->{RESULT}}, <<END;
# This Makefile is for the $self->{NAME} extension to perl.
#
# It was generated automatically by MakeMaker version
# $VERSION (Revision: $Revision) from the contents of
# Makefile.PL. Don't edit this file, edit Makefile.PL instead.
#
#       ANY CHANGES MADE HERE WILL BE LOST!
#
#   MakeMaker ARGV: $argv
#
#   MakeMaker Parameters:
END

    foreach my $key (sort keys %initial_att){
        next if $key eq 'ARGS';

        my($v) = neatvalue($initial_att{$key});
        $v =~ s/(CODE|HASH|ARRAY|SCALAR)\([\dxa-f]+\)/$1\(...\)/;
        $v =~ tr/\n/ /s;
        push @{$self->{RESULT}}, "#     $key => $v";
    }
    undef %initial_att;        # free memory

    if (defined $self->{CONFIGURE}) {
       push @{$self->{RESULT}}, <<END;

#   MakeMaker 'CONFIGURE' Parameters:
END
        if (scalar(keys %configure_att) > 0) {
            foreach my $key (sort keys %configure_att){
               next if $key eq 'ARGS';
               my($v) = neatvalue($configure_att{$key});
               $v =~ s/(CODE|HASH|ARRAY|SCALAR)\([\dxa-f]+\)/$1\(...\)/;
               $v =~ tr/\n/ /s;
               push @{$self->{RESULT}}, "#     $key => $v";
            }
        }
        else
        {
           push @{$self->{RESULT}}, "# no values returned";
        }
        undef %configure_att;  # free memory
    }

    # turn the SKIP array into a SKIPHASH hash
    for my $skip (@{$self->{SKIP} || []}) {
        $self->{SKIPHASH}{$skip} = 1;
    }
    delete $self->{SKIP}; # free memory

    if ($self->{PARENT}) {
        for (qw/install dist dist_basics dist_core distdir dist_test dist_ci/) {
            $self->{SKIPHASH}{$_} = 1;
        }
    }

    # We run all the subdirectories now. They don't have much to query
    # from the parent, but the parent has to query them: if they need linking!
    unless ($self->{NORECURS}) {
        $self->eval_in_subdirs if @{$self->{DIR}};
    }

    foreach my $section ( @MM_Sections ){
        # Support for new foo_target() methods.
        my $method = $section;
        $method .= '_target' unless $self->can($method);

        print "Processing Makefile '$section' section\n" if ($Verbose >= 2);
        my($skipit) = $self->skipcheck($section);
        if ($skipit){
            push @{$self->{RESULT}}, "\n# --- MakeMaker $section section $skipit.";
        } else {
            my(%a) = %{$self->{$section} || {}};
            push @{$self->{RESULT}}, "\n# --- MakeMaker $section section:";
            push @{$self->{RESULT}}, "# " . join ", ", %a if $Verbose && %a;
            push @{$self->{RESULT}}, $self->maketext_filter(
                $self->$method( %a )
            );
        }
    }

    push @{$self->{RESULT}}, "\n# End.";

    $self;
}

sub WriteEmptyMakefile {
    Carp::croak "WriteEmptyMakefile: Need an even number of args" if @_ % 2;

    my %att = @_;
    my $self = MM->new(\%att);
    
    my $new = $self->{MAKEFILE};
    my $old = $self->{MAKEFILE_OLD};
    if (-f $old) {
        _unlink($old) or warn "unlink $old: $!";
    }
    if ( -f $new ) {
        _rename($new, $old) or warn "rename $new => $old: $!"
    }
    open my $mfh, '>', $new or die "open $new for write: $!";
    print $mfh <<'EOP';
all :

clean :

install :

makemakerdflt :

test :

EOP
    close $mfh or die "close $new for write: $!";
}

sub check_manifest {
    print STDOUT "Checking if your kit is complete...\n";
    require ExtUtils::Manifest;
    # avoid warning
    $ExtUtils::Manifest::Quiet = $ExtUtils::Manifest::Quiet = 1;
    my(@missed) = ExtUtils::Manifest::manicheck();
    if (@missed) {
        print STDOUT "Warning: the following files are missing in your kit:\n";
        print "\t", join "\n\t", @missed;
        print STDOUT "\n";
        print STDOUT "Please inform the author.\n";
    } else {
        print STDOUT "Looks good\n";
    }
}

sub parse_args{
    my($self, @args) = @_;
    foreach (@args) {
        unless (m/(.*?)=(.*)/) {
            ++$Verbose if m/^verb/;
            next;
        }
        my($name, $value) = ($1, $2);
        if ($value =~ m/^~(\w+)?/) { # tilde with optional username
            $value =~ s [^~(\w*)]
                [$1 ?
                 ((getpwnam($1))[7] || "~$1") :
                 (getpwuid($>))[7]
                 ]ex;
        }

        # Remember the original args passed it.  It will be useful later.
        $self->{ARGS}{uc $name} = $self->{uc $name} = $value;
    }

    # catch old-style 'potential_libs' and inform user how to 'upgrade'
    if (defined $self->{potential_libs}){
        my($msg)="'potential_libs' => '$self->{potential_libs}' should be";
        if ($self->{potential_libs}){
            print STDOUT "$msg changed to:\n\t'LIBS' => ['$self->{potential_libs}']\n";
        } else {
            print STDOUT "$msg deleted.\n";
        }
        $self->{LIBS} = [$self->{potential_libs}];
        delete $self->{potential_libs};
    }
    # catch old-style 'ARMAYBE' and inform user how to 'upgrade'
    if (defined $self->{ARMAYBE}){
        my($armaybe) = $self->{ARMAYBE};
        print STDOUT "ARMAYBE => '$armaybe' should be changed to:\n",
                        "\t'dynamic_lib' => {ARMAYBE => '$armaybe'}\n";
        my(%dl) = %{$self->{dynamic_lib} || {}};
        $self->{dynamic_lib} = { %dl, ARMAYBE => $armaybe};
        delete $self->{ARMAYBE};
    }
    if (defined $self->{LDTARGET}){
        print STDOUT "LDTARGET should be changed to LDFROM\n";
        $self->{LDFROM} = $self->{LDTARGET};
        delete $self->{LDTARGET};
    }
    # Turn a DIR argument on the command line into an array
    if (defined $self->{DIR} && ref \$self->{DIR} eq 'SCALAR') {
        # So they can choose from the command line, which extensions they want
        # the grep enables them to have some colons too much in case they
        # have to build a list with the shell
        $self->{DIR} = [grep $_, split ":", $self->{DIR}];
    }
    # Turn a INCLUDE_EXT argument on the command line into an array
    if (defined $self->{INCLUDE_EXT} && ref \$self->{INCLUDE_EXT} eq 'SCALAR') {
        $self->{INCLUDE_EXT} = [grep $_, split '\s+', $self->{INCLUDE_EXT}];
    }
    # Turn a EXCLUDE_EXT argument on the command line into an array
    if (defined $self->{EXCLUDE_EXT} && ref \$self->{EXCLUDE_EXT} eq 'SCALAR') {
        $self->{EXCLUDE_EXT} = [grep $_, split '\s+', $self->{EXCLUDE_EXT}];
    }

    foreach my $mmkey (sort keys %$self){
        next if $mmkey eq 'ARGS';
        print STDOUT "  $mmkey => ", neatvalue($self->{$mmkey}), "\n" if $Verbose;
        print STDOUT "'$mmkey' is not a known MakeMaker parameter name.\n"
            unless exists $Recognized_Att_Keys{$mmkey};
    }
    $| = 1 if $Verbose;
}

sub check_hints {
    my($self) = @_;
    # We allow extension-specific hints files.

    require File::Spec;
    my $curdir = File::Spec->curdir;

    my $hint_dir = File::Spec->catdir($curdir, "hints");
    return unless -d $hint_dir;

    # First we look for the best hintsfile we have
    my($hint)="${^O}_$Config{osvers}";
    $hint =~ s/\./_/g;
    $hint =~ s/_$//;
    return unless $hint;

    # Also try without trailing minor version numbers.
    while (1) {
        last if -f File::Spec->catfile($hint_dir, "$hint.pl");  # found
    } continue {
        last unless $hint =~ s/_[^_]*$//; # nothing to cut off
    }
    my $hint_file = File::Spec->catfile($hint_dir, "$hint.pl");

    return unless -f $hint_file;    # really there

    _run_hintfile($self, $hint_file);
}

sub _run_hintfile {
    our $self;
    local($self) = shift;       # make $self available to the hint file.
    my($hint_file) = shift;

    local($@, $!);
    print STDERR "Processing hints file $hint_file\n";

    # Just in case the ./ isn't on the hint file, which File::Spec can
    # often strip off, we bung the curdir into @INC
    local @INC = (File::Spec->curdir, @INC);
    my $ret = do $hint_file;
    if( !defined $ret ) {
        my $error = $@ || $!;
        print STDERR $error;
    }
}

sub mv_all_methods {
    my($from,$to) = @_;

    # Here you see the *current* list of methods that are overridable
    # from Makefile.PL via MY:: subroutines. As of VERSION 5.07 I'm
    # still trying to reduce the list to some reasonable minimum --
    # because I want to make it easier for the user. A.K.

    local $SIG{__WARN__} = sub { 
        # can't use 'no warnings redefined', 5.6 only
        warn @_ unless $_[0] =~ /^Subroutine .* redefined/ 
    };
    foreach my $method (@Overridable) {

        # We cannot say "next" here. Nick might call MY->makeaperl
        # which isn't defined right now

        # Above statement was written at 4.23 time when Tk-b8 was
        # around. As Tk-b9 only builds with 5.002something and MM 5 is
        # standard, we try to enable the next line again. It was
        # commented out until MM 5.23

        next unless defined &{"${from}::$method"};

        {
            no strict 'refs';   ## no critic
            *{"${to}::$method"} = \&{"${from}::$method"};

            # If we delete a method, then it will be undefined and cannot
            # be called.  But as long as we have Makefile.PLs that rely on
            # %MY:: being intact, we have to fill the hole with an
            # inheriting method:

            {
                package MY;
                my $super = "SUPER::".$method;
                *{$method} = sub {
                    shift->$super(@_);
                };
            }
        }
    }

    # We have to clean out %INC also, because the current directory is
    # changed frequently and Graham Barr prefers to get his version
    # out of a History.pl file which is "required" so woudn't get
    # loaded again in another extension requiring a History.pl

    # With perl5.002_01 the deletion of entries in %INC caused Tk-b11
    # to core dump in the middle of a require statement. The required
    # file was Tk/MMutil.pm.  The consequence is, we have to be
    # extremely careful when we try to give perl a reason to reload a
    # library with same name.  The workaround prefers to drop nothing
    # from %INC and teach the writers not to use such libraries.

#    my $inc;
#    foreach $inc (keys %INC) {
#       #warn "***$inc*** deleted";
#       delete $INC{$inc};
#    }
}

sub skipcheck {
    my($self) = shift;
    my($section) = @_;
    if ($section eq 'dynamic') {
        print STDOUT "Warning (non-fatal): Target 'dynamic' depends on targets ",
        "in skipped section 'dynamic_bs'\n"
            if $self->{SKIPHASH}{dynamic_bs} && $Verbose;
        print STDOUT "Warning (non-fatal): Target 'dynamic' depends on targets ",
        "in skipped section 'dynamic_lib'\n"
            if $self->{SKIPHASH}{dynamic_lib} && $Verbose;
    }
    if ($section eq 'dynamic_lib') {
        print STDOUT "Warning (non-fatal): Target '\$(INST_DYNAMIC)' depends on ",
        "targets in skipped section 'dynamic_bs'\n"
            if $self->{SKIPHASH}{dynamic_bs} && $Verbose;
    }
    if ($section eq 'static') {
        print STDOUT "Warning (non-fatal): Target 'static' depends on targets ",
        "in skipped section 'static_lib'\n"
            if $self->{SKIPHASH}{static_lib} && $Verbose;
    }
    return 'skipped' if $self->{SKIPHASH}{$section};
    return '';
}

sub flush {
    my $self = shift;

    my $finalname = $self->{MAKEFILE};
    print STDOUT "Writing $finalname for $self->{NAME}\n";

    unlink($finalname, "MakeMaker.tmp", $Is_VMS ? 'Descrip.MMS' : ());
    open(my $fh,">", "MakeMaker.tmp")
        or die "Unable to open MakeMaker.tmp: $!";

    for my $chunk (@{$self->{RESULT}}) {
        print $fh "$chunk\n";
    }

    close $fh;
    _rename("MakeMaker.tmp", $finalname) or
      warn "rename MakeMaker.tmp => $finalname: $!";
    chmod 0644, $finalname unless $Is_VMS;

    my %keep = map { ($_ => 1) } qw(NEEDS_LINKING HAS_LINK_CODE);

    if ($self->{PARENT} && !$self->{_KEEP_AFTER_FLUSH}) {
        foreach (keys %$self) { # safe memory
            delete $self->{$_} unless $keep{$_};
        }
    }

    system("$Config::Config{eunicefix} $finalname") unless $Config::Config{eunicefix} eq ":";
}


# This is a rename for OS's where the target must be unlinked first.
sub _rename {
    my($src, $dest) = @_;
    chmod 0666, $dest;
    unlink $dest;
    return rename $src, $dest;
}

# This is an unlink for OS's where the target must be writable first.
sub _unlink {
    my @files = @_;
    chmod 0666, @files;
    return unlink @files;
}


# The following mkbootstrap() is only for installations that are calling
# the pre-4.1 mkbootstrap() from their old Makefiles. This MakeMaker
# writes Makefiles, that use ExtUtils::Mkbootstrap directly.
sub mkbootstrap {
    die <<END;
!!! Your Makefile has been built such a long time ago, !!!
!!! that is unlikely to work with current MakeMaker.   !!!
!!! Please rebuild your Makefile                       !!!
END
}

# Ditto for mksymlists() as of MakeMaker 5.17
sub mksymlists {
    die <<END;
!!! Your Makefile has been built such a long time ago, !!!
!!! that is unlikely to work with current MakeMaker.   !!!
!!! Please rebuild your Makefile                       !!!
END
}

sub neatvalue {
    my($v) = @_;
    return "undef" unless defined $v;
    my($t) = ref $v;
    return "q[$v]" unless $t;
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
    while (($key,$val) = each %$v){
        last unless defined $key; # cautious programming in case (undef,undef) is true
        push(@m,"$key=>".neatvalue($val)) ;
    }
    return "{ ".join(', ',@m)." }";
}

sub selfdocument {
    my($self) = @_;
    my(@m);
    if ($Verbose){
        push @m, "\n# Full list of MakeMaker attribute values:";
        foreach my $key (sort keys %$self){
            next if $key eq 'RESULT' || $key =~ /^[A-Z][a-z]/;
            my($v) = neatvalue($self->{$key});
            $v =~ s/(CODE|HASH|ARRAY|SCALAR)\([\dxa-f]+\)/$1\(...\)/;
            $v =~ tr/\n/ /s;
            push @m, "# $key => $v";
        }
    }
    join "\n", @m;
}

1;

__END__

=head1 NAME

ExtUtils::MakeMaker - Create a module Makefile

=head1 SYNOPSIS

  use ExtUtils::MakeMaker;

  WriteMakefile( ATTRIBUTE => VALUE [, ...] );

=head1 DESCRIPTION

This utility is designed to write a Makefile for an extension module
from a Makefile.PL. It is based on the Makefile.SH model provided by
Andy Dougherty and the perl5-porters.

It splits the task of generating the Makefile into several subroutines
that can be individually overridden.  Each subroutine returns the text
it wishes to have written to the Makefile.

MakeMaker is object oriented. Each directory below the current
directory that contains a Makefile.PL is treated as a separate
object. This makes it possible to write an unlimited number of
Makefiles with a single invocation of WriteMakefile().

=head2 How To Write A Makefile.PL

See ExtUtils::MakeMaker::Tutorial.

The long answer is the rest of the manpage :-)

=head2 Default Makefile Behaviour

The generated Makefile enables the user of the extension to invoke

  perl Makefile.PL # optionally "perl Makefile.PL verbose"
  make
  make test        # optionally set TEST_VERBOSE=1
  make install     # See below

The Makefile to be produced may be altered by adding arguments of the
form C<KEY=VALUE>. E.g.

  perl Makefile.PL INSTALL_BASE=~

Other interesting targets in the generated Makefile are

  make config     # to check if the Makefile is up-to-date
  make clean      # delete local temp files (Makefile gets renamed)
  make realclean  # delete derived files (including ./blib)
  make ci         # check in all the files in the MANIFEST file
  make dist       # see below the Distribution Support section

=head2 make test

MakeMaker checks for the existence of a file named F<test.pl> in the
current directory and if it exists it execute the script with the
proper set of perl C<-I> options.

MakeMaker also checks for any files matching glob("t/*.t"). It will
execute all matching files in alphabetical order via the
L<Test::Harness> module with the C<-I> switches set correctly.

If you'd like to see the raw output of your tests, set the
C<TEST_VERBOSE> variable to true.

  make test TEST_VERBOSE=1

=head2 make testdb

A useful variation of the above is the target C<testdb>. It runs the
test under the Perl debugger (see L<perldebug>). If the file
F<test.pl> exists in the current directory, it is used for the test.

If you want to debug some other testfile, set the C<TEST_FILE> variable
thusly:

  make testdb TEST_FILE=t/mytest.t

By default the debugger is called using C<-d> option to perl. If you
want to specify some other option, set the C<TESTDB_SW> variable:

  make testdb TESTDB_SW=-Dx

=head2 make install

make alone puts all relevant files into directories that are named by
the macros INST_LIB, INST_ARCHLIB, INST_SCRIPT, INST_MAN1DIR and
INST_MAN3DIR.  All these default to something below ./blib if you are
I<not> building below the perl source directory. If you I<are>
building below the perl source, INST_LIB and INST_ARCHLIB default to
../../lib, and INST_SCRIPT is not defined.

The I<install> target of the generated Makefile copies the files found
below each of the INST_* directories to their INSTALL*
counterparts. Which counterparts are chosen depends on the setting of
INSTALLDIRS according to the following table:

                                 INSTALLDIRS set to
                           perl        site          vendor

                 PERLPREFIX      SITEPREFIX          VENDORPREFIX
  INST_ARCHLIB   INSTALLARCHLIB  INSTALLSITEARCH     INSTALLVENDORARCH
  INST_LIB       INSTALLPRIVLIB  INSTALLSITELIB      INSTALLVENDORLIB
  INST_BIN       INSTALLBIN      INSTALLSITEBIN      INSTALLVENDORBIN
  INST_SCRIPT    INSTALLSCRIPT   INSTALLSITESCRIPT   INSTALLVENDORSCRIPT
  INST_MAN1DIR   INSTALLMAN1DIR  INSTALLSITEMAN1DIR  INSTALLVENDORMAN1DIR
  INST_MAN3DIR   INSTALLMAN3DIR  INSTALLSITEMAN3DIR  INSTALLVENDORMAN3DIR

The INSTALL... macros in turn default to their %Config
($Config{installprivlib}, $Config{installarchlib}, etc.) counterparts.

You can check the values of these variables on your system with

    perl '-V:install.*'

And to check the sequence in which the library directories are
searched by perl, run

    perl -le 'print join $/, @INC'

Sometimes older versions of the module you're installing live in other
directories in @INC.  Because Perl loads the first version of a module it 
finds, not the newest, you might accidentally get one of these older
versions even after installing a brand new version.  To delete I<all other
versions of the module you're installing> (not simply older ones) set the
C<UNINST> variable.

    make install UNINST=1


=head2 INSTALL_BASE

INSTALL_BASE can be passed into Makefile.PL to change where your
module will be installed.  INSTALL_BASE is more like what everyone
else calls "prefix" than PREFIX is.

To have everything installed in your home directory, do the following.

    # Unix users, INSTALL_BASE=~ works fine
    perl Makefile.PL INSTALL_BASE=/path/to/your/home/dir

Like PREFIX, it sets several INSTALL* attributes at once.  Unlike
PREFIX it is easy to predict where the module will end up.  The
installation pattern looks like this:

    INSTALLARCHLIB     INSTALL_BASE/lib/perl5/$Config{archname}
    INSTALLPRIVLIB     INSTALL_BASE/lib/perl5
    INSTALLBIN         INSTALL_BASE/bin
    INSTALLSCRIPT      INSTALL_BASE/bin
    INSTALLMAN1DIR     INSTALL_BASE/man/man1
    INSTALLMAN3DIR     INSTALL_BASE/man/man3

INSTALL_BASE in MakeMaker and C<--install_base> in Module::Build (as
of 0.28) install to the same location.  If you want MakeMaker and
Module::Build to install to the same location simply set INSTALL_BASE
and C<--install_base> to the same location.

INSTALL_BASE was added in 6.31.


=head2 PREFIX and LIB attribute

PREFIX and LIB can be used to set several INSTALL* attributes in one
go.  Here's an example for installing into your home directory.

    # Unix users, PREFIX=~ works fine
    perl Makefile.PL PREFIX=/path/to/your/home/dir

This will install all files in the module under your home directory,
with man pages and libraries going into an appropriate place (usually
~/man and ~/lib).  How the exact location is determined is complicated
and depends on how your Perl was configured.  INSTALL_BASE works more
like what other build systems call "prefix" than PREFIX and we
recommend you use that instead.

Another way to specify many INSTALL directories with a single
parameter is LIB.

    perl Makefile.PL LIB=~/lib

This will install the module's architecture-independent files into
~/lib, the architecture-dependent files into ~/lib/$archname.

Note, that in both cases the tilde expansion is done by MakeMaker, not
by perl by default, nor by make.

Conflicts between parameters LIB, PREFIX and the various INSTALL*
arguments are resolved so that:

=over 4

=item *

setting LIB overrides any setting of INSTALLPRIVLIB, INSTALLARCHLIB,
INSTALLSITELIB, INSTALLSITEARCH (and they are not affected by PREFIX);

=item *

without LIB, setting PREFIX replaces the initial C<$Config{prefix}>
part of those INSTALL* arguments, even if the latter are explicitly
set (but are set to still start with C<$Config{prefix}>).

=back

If the user has superuser privileges, and is not working on AFS or
relatives, then the defaults for INSTALLPRIVLIB, INSTALLARCHLIB,
INSTALLSCRIPT, etc. will be appropriate, and this incantation will be
the best:

    perl Makefile.PL; 
    make; 
    make test
    make install

make install per default writes some documentation of what has been
done into the file C<$(INSTALLARCHLIB)/perllocal.pod>. This feature
can be bypassed by calling make pure_install.

=head2 AFS users

will have to specify the installation directories as these most
probably have changed since perl itself has been installed. They will
have to do this by calling

    perl Makefile.PL INSTALLSITELIB=/afs/here/today \
        INSTALLSCRIPT=/afs/there/now INSTALLMAN3DIR=/afs/for/manpages
    make

Be careful to repeat this procedure every time you recompile an
extension, unless you are sure the AFS installation directories are
still valid.

=head2 Static Linking of a new Perl Binary

An extension that is built with the above steps is ready to use on
systems supporting dynamic loading. On systems that do not support
dynamic loading, any newly created extension has to be linked together
with the available resources. MakeMaker supports the linking process
by creating appropriate targets in the Makefile whenever an extension
is built. You can invoke the corresponding section of the makefile with

    make perl

That produces a new perl binary in the current directory with all
extensions linked in that can be found in INST_ARCHLIB, SITELIBEXP,
and PERL_ARCHLIB. To do that, MakeMaker writes a new Makefile, on
UNIX, this is called Makefile.aperl (may be system dependent). If you
want to force the creation of a new perl, it is recommended, that you
delete this Makefile.aperl, so the directories are searched-through
for linkable libraries again.

The binary can be installed into the directory where perl normally
resides on your machine with

    make inst_perl

To produce a perl binary with a different name than C<perl>, either say

    perl Makefile.PL MAP_TARGET=myperl
    make myperl
    make inst_perl

or say

    perl Makefile.PL
    make myperl MAP_TARGET=myperl
    make inst_perl MAP_TARGET=myperl

In any case you will be prompted with the correct invocation of the
C<inst_perl> target that installs the new binary into INSTALLBIN.

make inst_perl per default writes some documentation of what has been
done into the file C<$(INSTALLARCHLIB)/perllocal.pod>. This
can be bypassed by calling make pure_inst_perl.

Warning: the inst_perl: target will most probably overwrite your
existing perl binary. Use with care!

Sometimes you might want to build a statically linked perl although
your system supports dynamic loading. In this case you may explicitly
set the linktype with the invocation of the Makefile.PL or make:

    perl Makefile.PL LINKTYPE=static    # recommended

or

    make LINKTYPE=static                # works on most systems

=head2 Determination of Perl Library and Installation Locations

MakeMaker needs to know, or to guess, where certain things are
located.  Especially INST_LIB and INST_ARCHLIB (where to put the files
during the make(1) run), PERL_LIB and PERL_ARCHLIB (where to read
existing modules from), and PERL_INC (header files and C<libperl*.*>).

Extensions may be built either using the contents of the perl source
directory tree or from the installed perl library. The recommended way
is to build extensions after you have run 'make install' on perl
itself. You can do that in any directory on your hard disk that is not
below the perl source tree. The support for extensions below the ext
directory of the perl distribution is only good for the standard
extensions that come with perl.

If an extension is being built below the C<ext/> directory of the perl
source then MakeMaker will set PERL_SRC automatically (e.g.,
C<../..>).  If PERL_SRC is defined and the extension is recognized as
a standard extension, then other variables default to the following:

  PERL_INC     = PERL_SRC
  PERL_LIB     = PERL_SRC/lib
  PERL_ARCHLIB = PERL_SRC/lib
  INST_LIB     = PERL_LIB
  INST_ARCHLIB = PERL_ARCHLIB

If an extension is being built away from the perl source then MakeMaker
will leave PERL_SRC undefined and default to using the installed copy
of the perl library. The other variables default to the following:

  PERL_INC     = $archlibexp/CORE
  PERL_LIB     = $privlibexp
  PERL_ARCHLIB = $archlibexp
  INST_LIB     = ./blib/lib
  INST_ARCHLIB = ./blib/arch

If perl has not yet been installed then PERL_SRC can be defined on the
command line as shown in the previous section.


=head2 Which architecture dependent directory?

If you don't want to keep the defaults for the INSTALL* macros,
MakeMaker helps you to minimize the typing needed: the usual
relationship between INSTALLPRIVLIB and INSTALLARCHLIB is determined
by Configure at perl compilation time. MakeMaker supports the user who
sets INSTALLPRIVLIB. If INSTALLPRIVLIB is set, but INSTALLARCHLIB not,
then MakeMaker defaults the latter to be the same subdirectory of
INSTALLPRIVLIB as Configure decided for the counterparts in %Config ,
otherwise it defaults to INSTALLPRIVLIB. The same relationship holds
for INSTALLSITELIB and INSTALLSITEARCH.

MakeMaker gives you much more freedom than needed to configure
internal variables and get different results. It is worth to mention,
that make(1) also lets you configure most of the variables that are
used in the Makefile. But in the majority of situations this will not
be necessary, and should only be done if the author of a package
recommends it (or you know what you're doing).

=head2 Using Attributes and Parameters

The following attributes may be specified as arguments to WriteMakefile()
or as NAME=VALUE pairs on the command line.

=over 2

=item ABSTRACT

One line description of the module. Will be included in PPD file.

=item ABSTRACT_FROM

Name of the file that contains the package description. MakeMaker looks
for a line in the POD matching /^($package\s-\s)(.*)/. This is typically
the first line in the "=head1 NAME" section. $2 becomes the abstract.

=item AUTHOR

String containing name (and email address) of package author(s). Is used
in PPD (Perl Package Description) files for PPM (Perl Package Manager).

=item BINARY_LOCATION

Used when creating PPD files for binary packages.  It can be set to a
full or relative path or URL to the binary archive for a particular
architecture.  For example:

        perl Makefile.PL BINARY_LOCATION=x86/Agent.tar.gz

builds a PPD package that references a binary of the C<Agent> package,
located in the C<x86> directory relative to the PPD itself.

=item C

Ref to array of *.c file names. Initialised from a directory scan
and the values portion of the XS attribute hash. This is not
currently used by MakeMaker but may be handy in Makefile.PLs.

=item CCFLAGS

String that will be included in the compiler call command line between
the arguments INC and OPTIMIZE.

=item CONFIG

Arrayref. E.g. [qw(archname manext)] defines ARCHNAME & MANEXT from
config.sh. MakeMaker will add to CONFIG the following values anyway:
ar
cc
cccdlflags
ccdlflags
dlext
dlsrc
ld
lddlflags
ldflags
libc
lib_ext
obj_ext
ranlib
sitelibexp
sitearchexp
so

=item CONFIGURE

CODE reference. The subroutine should return a hash reference. The
hash may contain further attributes, e.g. {LIBS =E<gt> ...}, that have to
be determined by some evaluation method.

=item DEFINE

Something like C<"-DHAVE_UNISTD_H">

=item DESTDIR

This is the root directory into which the code will be installed.  It
I<prepends itself to the normal prefix>.  For example, if your code
would normally go into F</usr/local/lib/perl> you could set DESTDIR=~/tmp/
and installation would go into F<~/tmp/usr/local/lib/perl>.

This is primarily of use for people who repackage Perl modules.

NOTE: Due to the nature of make, it is important that you put the trailing
slash on your DESTDIR.  F<~/tmp/> not F<~/tmp>.

=item DIR

Ref to array of subdirectories containing Makefile.PLs e.g. [ 'sdbm'
] in ext/SDBM_File

=item DISTNAME

A safe filename for the package. 

Defaults to NAME above but with :: replaced with -.

For example, Foo::Bar becomes Foo-Bar.

=item DISTVNAME

Your name for distributing the package with the version number
included.  This is used by 'make dist' to name the resulting archive
file.

Defaults to DISTNAME-VERSION.

For example, version 1.04 of Foo::Bar becomes Foo-Bar-1.04.

On some OS's where . has special meaning VERSION_SYM may be used in
place of VERSION.

=item DL_FUNCS

Hashref of symbol names for routines to be made available as universal
symbols.  Each key/value pair consists of the package name and an
array of routine names in that package.  Used only under AIX, OS/2,
VMS and Win32 at present.  The routine names supplied will be expanded
in the same way as XSUB names are expanded by the XS() macro.
Defaults to

  {"$(NAME)" => ["boot_$(NAME)" ] }

e.g.

  {"RPC" => [qw( boot_rpcb rpcb_gettime getnetconfigent )],
   "NetconfigPtr" => [ 'DESTROY'] }

Please see the L<ExtUtils::Mksymlists> documentation for more information
about the DL_FUNCS, DL_VARS and FUNCLIST attributes.

=item DL_VARS

Array of symbol names for variables to be made available as universal symbols.
Used only under AIX, OS/2, VMS and Win32 at present.  Defaults to [].
(e.g. [ qw(Foo_version Foo_numstreams Foo_tree ) ])

=item EXCLUDE_EXT

Array of extension names to exclude when doing a static build.  This
is ignored if INCLUDE_EXT is present.  Consult INCLUDE_EXT for more
details.  (e.g.  [ qw( Socket POSIX ) ] )

This attribute may be most useful when specified as a string on the
command line:  perl Makefile.PL EXCLUDE_EXT='Socket Safe'

=item EXE_FILES

Ref to array of executable files. The files will be copied to the
INST_SCRIPT directory. Make realclean will delete them from there
again.

If your executables start with something like #!perl or
#!/usr/bin/perl MakeMaker will change this to the path of the perl
'Makefile.PL' was invoked with so the programs will be sure to run
properly even if perl is not in /usr/bin/perl.

=item FIRST_MAKEFILE

The name of the Makefile to be produced.  This is used for the second
Makefile that will be produced for the MAP_TARGET.

Defaults to 'Makefile' or 'Descrip.MMS' on VMS.

(Note: we couldn't use MAKEFILE because dmake uses this for something
else).

=item FULLPERL

Perl binary able to run this extension, load XS modules, etc...

=item FULLPERLRUN

Like PERLRUN, except it uses FULLPERL.

=item FULLPERLRUNINST

Like PERLRUNINST, except it uses FULLPERL.

=item FUNCLIST

This provides an alternate means to specify function names to be
exported from the extension.  Its value is a reference to an
array of function names to be exported by the extension.  These
names are passed through unaltered to the linker options file.

=item H

Ref to array of *.h file names. Similar to C.

=item IMPORTS

This attribute is used to specify names to be imported into the
extension. Takes a hash ref.

It is only used on OS/2 and Win32.

=item INC

Include file dirs eg: C<"-I/usr/5include -I/path/to/inc">

=item INCLUDE_EXT

Array of extension names to be included when doing a static build.
MakeMaker will normally build with all of the installed extensions when
doing a static build, and that is usually the desired behavior.  If
INCLUDE_EXT is present then MakeMaker will build only with those extensions
which are explicitly mentioned. (e.g.  [ qw( Socket POSIX ) ])

It is not necessary to mention DynaLoader or the current extension when
filling in INCLUDE_EXT.  If the INCLUDE_EXT is mentioned but is empty then
only DynaLoader and the current extension will be included in the build.

This attribute may be most useful when specified as a string on the
command line:  perl Makefile.PL INCLUDE_EXT='POSIX Socket Devel::Peek'

=item INSTALLARCHLIB

Used by 'make install', which copies files from INST_ARCHLIB to this
directory if INSTALLDIRS is set to perl.

=item INSTALLBIN

Directory to install binary files (e.g. tkperl) into if
INSTALLDIRS=perl.

=item INSTALLDIRS

Determines which of the sets of installation directories to choose:
perl, site or vendor.  Defaults to site.

=item INSTALLMAN1DIR

=item INSTALLMAN3DIR

These directories get the man pages at 'make install' time if
INSTALLDIRS=perl.  Defaults to $Config{installman*dir}.

If set to 'none', no man pages will be installed.

=item INSTALLPRIVLIB

Used by 'make install', which copies files from INST_LIB to this
directory if INSTALLDIRS is set to perl.

Defaults to $Config{installprivlib}.

=item INSTALLSCRIPT

Used by 'make install' which copies files from INST_SCRIPT to this
directory if INSTALLDIRS=perl.

=item INSTALLSITEARCH

Used by 'make install', which copies files from INST_ARCHLIB to this
directory if INSTALLDIRS is set to site (default).

=item INSTALLSITEBIN

Used by 'make install', which copies files from INST_BIN to this
directory if INSTALLDIRS is set to site (default).

=item INSTALLSITELIB

Used by 'make install', which copies files from INST_LIB to this
directory if INSTALLDIRS is set to site (default).

=item INSTALLSITEMAN1DIR

=item INSTALLSITEMAN3DIR

These directories get the man pages at 'make install' time if
INSTALLDIRS=site (default).  Defaults to 
$(SITEPREFIX)/man/man$(MAN*EXT).

If set to 'none', no man pages will be installed.

=item INSTALLSITESCRIPT

Used by 'make install' which copies files from INST_SCRIPT to this
directory if INSTALLDIRS is set to site (default).

=item INSTALLVENDORARCH

Used by 'make install', which copies files from INST_ARCHLIB to this
directory if INSTALLDIRS is set to vendor.

=item INSTALLVENDORBIN

Used by 'make install', which copies files from INST_BIN to this
directory if INSTALLDIRS is set to vendor.

=item INSTALLVENDORLIB

Used by 'make install', which copies files from INST_LIB to this
directory if INSTALLDIRS is set to vendor.

=item INSTALLVENDORMAN1DIR

=item INSTALLVENDORMAN3DIR

These directories get the man pages at 'make install' time if
INSTALLDIRS=vendor.  Defaults to $(VENDORPREFIX)/man/man$(MAN*EXT).

If set to 'none', no man pages will be installed.

=item INSTALLVENDORSCRIPT

Used by 'make install' which copies files from INST_SCRIPT to this
directory if INSTALLDIRS is set to is set to vendor.

=item INST_ARCHLIB

Same as INST_LIB for architecture dependent files.

=item INST_BIN

Directory to put real binary files during 'make'. These will be copied
to INSTALLBIN during 'make install'

=item INST_LIB

Directory where we put library files of this extension while building
it.

=item INST_MAN1DIR

Directory to hold the man pages at 'make' time

=item INST_MAN3DIR

Directory to hold the man pages at 'make' time

=item INST_SCRIPT

Directory, where executable files should be installed during
'make'. Defaults to "./blib/script", just to have a dummy location during
testing. make install will copy the files in INST_SCRIPT to
INSTALLSCRIPT.

=item LD

Program to be used to link libraries for dynamic loading.

Defaults to $Config{ld}.

=item LDDLFLAGS

Any special flags that might need to be passed to ld to create a
shared library suitable for dynamic loading.  It is up to the makefile
to use it.  (See L<Config/lddlflags>)

Defaults to $Config{lddlflags}.

=item LDFROM

Defaults to "$(OBJECT)" and is used in the ld command to specify
what files to link/load from (also see dynamic_lib below for how to
specify ld flags)

=item LIB

LIB should only be set at C<perl Makefile.PL> time but is allowed as a
MakeMaker argument. It has the effect of setting both INSTALLPRIVLIB
and INSTALLSITELIB to that value regardless any explicit setting of
those arguments (or of PREFIX).  INSTALLARCHLIB and INSTALLSITEARCH
are set to the corresponding architecture subdirectory.

=item LIBPERL_A

The filename of the perllibrary that will be used together with this
extension. Defaults to libperl.a.

=item LIBS

An anonymous array of alternative library
specifications to be searched for (in order) until
at least one library is found. E.g.

  'LIBS' => ["-lgdbm", "-ldbm -lfoo", "-L/path -ldbm.nfs"]

Mind, that any element of the array
contains a complete set of arguments for the ld
command. So do not specify

  'LIBS' => ["-ltcl", "-ltk", "-lX11"]

See ODBM_File/Makefile.PL for an example, where an array is needed. If
you specify a scalar as in

  'LIBS' => "-ltcl -ltk -lX11"

MakeMaker will turn it into an array with one element.

=item LICENSE

The licensing terms of your distribution.  Generally its "perl" for the
same license as Perl itself.

See L<Module::Build::API> for the list of options.

Defaults to "unknown".

=item LINKTYPE

'static' or 'dynamic' (default unless usedl=undef in
config.sh). Should only be used to force static linking (also see
linkext below).

=item MAKE

Variant of make you intend to run the generated Makefile with.  This
parameter lets Makefile.PL know what make quirks to account for when
generating the Makefile.

MakeMaker also honors the MAKE environment variable.  This parameter
takes precedent.

Currently the only significant values are 'dmake' and 'nmake' for Windows
users.

Defaults to $Config{make}.

=item MAKEAPERL

Boolean which tells MakeMaker, that it should include the rules to
make a perl. This is handled automatically as a switch by
MakeMaker. The user normally does not need it.

=item MAKEFILE_OLD

When 'make clean' or similar is run, the $(FIRST_MAKEFILE) will be
backed up at this location.

Defaults to $(FIRST_MAKEFILE).old or $(FIRST_MAKEFILE)_old on VMS.

=item MAN1PODS

Hashref of pod-containing files. MakeMaker will default this to all
EXE_FILES files that include POD directives. The files listed
here will be converted to man pages and installed as was requested
at Configure time.

=item MAN3PODS

Hashref that assigns to *.pm and *.pod files the files into which the
manpages are to be written. MakeMaker parses all *.pod and *.pm files
for POD directives. Files that contain POD will be the default keys of
the MAN3PODS hashref. These will then be converted to man pages during
C<make> and will be installed during C<make install>.

=item MAP_TARGET

If it is intended, that a new perl binary be produced, this variable
may hold a name for that binary. Defaults to perl

=item MYEXTLIB

If the extension links to a library that it builds set this to the
name of the library (see SDBM_File)

=item NAME

Perl module name for this extension (DBD::Oracle). This will default
to the directory name but should be explicitly defined in the
Makefile.PL.

=item NEEDS_LINKING

MakeMaker will figure out if an extension contains linkable code
anywhere down the directory tree, and will set this variable
accordingly, but you can speed it up a very little bit if you define
this boolean variable yourself.

=item NOECHO

Command so make does not print the literal commands its running.

By setting it to an empty string you can generate a Makefile that
prints all commands. Mainly used in debugging MakeMaker itself.

Defaults to C<@>.

=item NORECURS

Boolean.  Attribute to inhibit descending into subdirectories.

=item NO_META

When true, suppresses the generation and addition to the MANIFEST of
the META.yml module meta-data file during 'make distdir'.

Defaults to false.

=item NO_VC

In general, any generated Makefile checks for the current version of
MakeMaker and the version the Makefile was built under. If NO_VC is
set, the version check is neglected. Do not write this into your
Makefile.PL, use it interactively instead.

=item OBJECT

List of object files, defaults to '$(BASEEXT)$(OBJ_EXT)', but can be a long
string containing all object files, e.g. "tkpBind.o
tkpButton.o tkpCanvas.o"

(Where BASEEXT is the last component of NAME, and OBJ_EXT is $Config{obj_ext}.)

=item OPTIMIZE

Defaults to C<-O>. Set it to C<-g> to turn debugging on. The flag is
passed to subdirectory makes.

=item PERL

Perl binary for tasks that can be done by miniperl

=item PERL_CORE

Set only when MakeMaker is building the extensions of the Perl core
distribution.

=item PERLMAINCC

The call to the program that is able to compile perlmain.c. Defaults
to $(CC).

=item PERL_ARCHLIB

Same as for PERL_LIB, but for architecture dependent files.

Used only when MakeMaker is building the extensions of the Perl core
distribution (because normally $(PERL_ARCHLIB) is automatically in @INC,
and adding it would get in the way of PERL5LIB).

=item PERL_LIB

Directory containing the Perl library to use.

Used only when MakeMaker is building the extensions of the Perl core
distribution (because normally $(PERL_LIB) is automatically in @INC,
and adding it would get in the way of PERL5LIB).

=item PERL_MALLOC_OK

defaults to 0.  Should be set to TRUE if the extension can work with
the memory allocation routines substituted by the Perl malloc() subsystem.
This should be applicable to most extensions with exceptions of those

=over 4

=item *

with bugs in memory allocations which are caught by Perl's malloc();

=item *

which interact with the memory allocator in other ways than via
malloc(), realloc(), free(), calloc(), sbrk() and brk();

=item *

which rely on special alignment which is not provided by Perl's malloc().

=back

B<NOTE.>  Negligence to set this flag in I<any one> of loaded extension
nullifies many advantages of Perl's malloc(), such as better usage of
system resources, error detection, memory usage reporting, catchable failure
of memory allocations, etc.

=item PERLPREFIX

Directory under which core modules are to be installed.

Defaults to $Config{installprefixexp} falling back to
$Config{installprefix}, $Config{prefixexp} or $Config{prefix} should
$Config{installprefixexp} not exist.

Overridden by PREFIX.

=item PERLRUN

Use this instead of $(PERL) when you wish to run perl.  It will set up
extra necessary flags for you.

=item PERLRUNINST

Use this instead of $(PERL) when you wish to run perl to work with
modules.  It will add things like -I$(INST_ARCH) and other necessary
flags so perl can see the modules you're about to install.

=item PERL_SRC

Directory containing the Perl source code (use of this should be
avoided, it may be undefined)

=item PERM_RW

Desired permission for read/writable files. Defaults to C<644>.
See also L<MM_Unix/perm_rw>.

=item PERM_RWX

Desired permission for executable files. Defaults to C<755>.
See also L<MM_Unix/perm_rwx>.

=item PL_FILES

MakeMaker can run programs to generate files for you at build time.
By default any file named *.PL (except Makefile.PL and Build.PL) in
the top level directory will be assumed to be a Perl program and run
passing its own basename in as an argument.  For example...

    perl foo.PL foo

This behavior can be overridden by supplying your own set of files to
search.  PL_FILES accepts a hash ref, the key being the file to run
and the value is passed in as the first argument when the PL file is run.

    PL_FILES => {'bin/foobar.PL' => 'bin/foobar'}

Would run bin/foobar.PL like this:

    perl bin/foobar.PL bin/foobar

If multiple files from one program are desired an array ref can be used.

    PL_FILES => {'bin/foobar.PL' => [qw(bin/foobar1 bin/foobar2)]}

In this case the program will be run multiple times using each target file.

    perl bin/foobar.PL bin/foobar1
    perl bin/foobar.PL bin/foobar2

PL files are normally run B<after> pm_to_blib and include INST_LIB and
INST_ARCH in its C<@INC> so the just built modules can be
accessed... unless the PL file is making a module (or anything else in
PM) in which case it is run B<before> pm_to_blib and does not include
INST_LIB and INST_ARCH in its C<@INC>.  This apparently odd behavior
is there for backwards compatibility (and its somewhat DWIM).


=item PM

Hashref of .pm files and *.pl files to be installed.  e.g.

  {'name_of_file.pm' => '$(INST_LIBDIR)/install_as.pm'}

By default this will include *.pm and *.pl and the files found in
the PMLIBDIRS directories.  Defining PM in the
Makefile.PL will override PMLIBDIRS.

=item PMLIBDIRS

Ref to array of subdirectories containing library files.  Defaults to
[ 'lib', $(BASEEXT) ]. The directories will be scanned and I<any> files
they contain will be installed in the corresponding location in the
library.  A libscan() method can be used to alter the behaviour.
Defining PM in the Makefile.PL will override PMLIBDIRS.

(Where BASEEXT is the last component of NAME.)

=item PM_FILTER

A filter program, in the traditional Unix sense (input from stdin, output
to stdout) that is passed on each .pm file during the build (in the
pm_to_blib() phase).  It is empty by default, meaning no filtering is done.

Great care is necessary when defining the command if quoting needs to be
done.  For instance, you would need to say:

  {'PM_FILTER' => 'grep -v \\"^\\#\\"'}

to remove all the leading comments on the fly during the build.  The
extra \\ are necessary, unfortunately, because this variable is interpolated
within the context of a Perl program built on the command line, and double
quotes are what is used with the -e switch to build that command line.  The
# is escaped for the Makefile, since what is going to be generated will then
be:

  PM_FILTER = grep -v \"^\#\"

Without the \\ before the #, we'd have the start of a Makefile comment,
and the macro would be incorrectly defined.

=item POLLUTE

Release 5.005 grandfathered old global symbol names by providing preprocessor
macros for extension source compatibility.  As of release 5.6, these
preprocessor definitions are not available by default.  The POLLUTE flag
specifies that the old names should still be defined:

  perl Makefile.PL POLLUTE=1

Please inform the module author if this is necessary to successfully install
a module under 5.6 or later.

=item PPM_INSTALL_EXEC

Name of the executable used to run C<PPM_INSTALL_SCRIPT> below. (e.g. perl)

=item PPM_INSTALL_SCRIPT

Name of the script that gets executed by the Perl Package Manager after
the installation of a package.

=item PREFIX

This overrides all the default install locations.  Man pages,
libraries, scripts, etc...  MakeMaker will try to make an educated
guess about where to place things under the new PREFIX based on your
Config defaults.  Failing that, it will fall back to a structure
which should be sensible for your platform.

If you specify LIB or any INSTALL* variables they will not be effected
by the PREFIX.

=item PREREQ_FATAL

Bool. If this parameter is true, failing to have the required modules
(or the right versions thereof) will be fatal. C<perl Makefile.PL>
will C<die> instead of simply informing the user of the missing dependencies.

It is I<extremely> rare to have to use C<PREREQ_FATAL>. Its use by module
authors is I<strongly discouraged> and should never be used lightly.
Module installation tools have ways of resolving umet dependencies but
to do that they need a F<Makefile>.  Using C<PREREQ_FATAL> breaks this.
That's bad.

The only situation where it is appropriate is when you have
dependencies that are indispensible to actually I<write> a
F<Makefile>. For example, MakeMaker's F<Makefile.PL> needs L<File::Spec>.
If its not available it cannot write the F<Makefile>.

Note: see L<Test::Harness> for a shortcut for stopping tests early
if you are missing dependencies and are afraid that users might
use your module with an incomplete environment.

=item PREREQ_PM

Hashref: Names of modules that need to be available to run this
extension (e.g. Fcntl for SDBM_File) are the keys of the hash and the
desired version is the value. If the required version number is 0, we
only check if any version is installed already.

=item PREREQ_PRINT

Bool.  If this parameter is true, the prerequisites will be printed to
stdout and MakeMaker will exit.  The output format is an evalable hash
ref.

$PREREQ_PM = {
               'A::B' => Vers1,
               'C::D' => Vers2,
               ...
             };

=item PRINT_PREREQ

RedHatism for C<PREREQ_PRINT>.  The output format is different, though:

    perl(A::B)>=Vers1 perl(C::D)>=Vers2 ...

=item SITEPREFIX

Like PERLPREFIX, but only for the site install locations.

Defaults to $Config{siteprefixexp}.  Perls prior to 5.6.0 didn't have
an explicit siteprefix in the Config.  In those cases
$Config{installprefix} will be used.

Overridable by PREFIX

=item SIGN

When true, perform the generation and addition to the MANIFEST of the
SIGNATURE file in the distdir during 'make distdir', via 'cpansign
-s'.

Note that you need to install the Module::Signature module to
perform this operation.

Defaults to false.

=item SKIP

Arrayref. E.g. [qw(name1 name2)] skip (do not write) sections of the
Makefile. Caution! Do not use the SKIP attribute for the negligible
speedup. It may seriously damage the resulting Makefile. Only use it
if you really need it.

=item TYPEMAPS

Ref to array of typemap file names.  Use this when the typemaps are
in some directory other than the current directory or when they are
not named B<typemap>.  The last typemap in the list takes
precedence.  A typemap in the current directory has highest
precedence, even if it isn't listed in TYPEMAPS.  The default system
typemap has lowest precedence.

=item VENDORPREFIX

Like PERLPREFIX, but only for the vendor install locations.

Defaults to $Config{vendorprefixexp}.

Overridable by PREFIX

=item VERBINST

If true, make install will be verbose

=item VERSION

Your version number for distributing the package.  This defaults to
0.1.

=item VERSION_FROM

Instead of specifying the VERSION in the Makefile.PL you can let
MakeMaker parse a file to determine the version number. The parsing
routine requires that the file named by VERSION_FROM contains one
single line to compute the version number. The first line in the file
that contains the regular expression

    /([\$*])(([\w\:\']*)\bVERSION)\b.*\=/

will be evaluated with eval() and the value of the named variable
B<after> the eval() will be assigned to the VERSION attribute of the
MakeMaker object. The following lines will be parsed o.k.:

    $VERSION   = '1.00';
    *VERSION   = \'1.01';
    ($VERSION) = q$Revision: 54639 $ =~ /(\d+)/g;
    $FOO::VERSION = '1.10';
    *FOO::VERSION = \'1.11';

but these will fail:

    # Bad
    my $VERSION         = '1.01';
    local $VERSION      = '1.02';
    local $FOO::VERSION = '1.30';

"Version strings" are incompatible should not be used.

    # Bad
    $VERSION = 1.2.3;
    $VERSION = v1.2.3;

L<version> objects are fine.  As of MakeMaker 6.35 version.pm will be
automatically loaded, but you must declare the dependency on version.pm.
For compatibility with older MakeMaker you should load on the same line 
as $VERSION is declared.

    # All on one line
    use version; our $VERSION = qv(1.2.3);

(Putting C<my> or C<local> on the preceding line will work o.k.)

The file named in VERSION_FROM is not added as a dependency to
Makefile. This is not really correct, but it would be a major pain
during development to have to rewrite the Makefile for any smallish
change in that file. If you want to make sure that the Makefile
contains the correct VERSION macro after any change of the file, you
would have to do something like

    depend => { Makefile => '$(VERSION_FROM)' }

See attribute C<depend> below.

=item VERSION_SYM

A sanitized VERSION with . replaced by _.  For places where . has
special meaning (some filesystems, RCS labels, etc...)

=item XS

Hashref of .xs files. MakeMaker will default this.  e.g.

  {'name_of_file.xs' => 'name_of_file.c'}

The .c files will automatically be included in the list of files
deleted by a make clean.

=item XSOPT

String of options to pass to xsubpp.  This might include C<-C++> or
C<-extern>.  Do not include typemaps here; the TYPEMAP parameter exists for
that purpose.

=item XSPROTOARG

May be set to an empty string, which is identical to C<-prototypes>, or
C<-noprototypes>. See the xsubpp documentation for details. MakeMaker
defaults to the empty string.

=item XS_VERSION

Your version number for the .xs file of this package.  This defaults
to the value of the VERSION attribute.

=back

=head2 Additional lowercase attributes

can be used to pass parameters to the methods which implement that
part of the Makefile.  Parameters are specified as a hash ref but are
passed to the method as a hash.

=over 2

=item clean

  {FILES => "*.xyz foo"}

=item depend

  {ANY_TARGET => ANY_DEPENDENCY, ...}

(ANY_TARGET must not be given a double-colon rule by MakeMaker.)

=item dist

  {TARFLAGS => 'cvfF', COMPRESS => 'gzip', SUFFIX => '.gz',
  SHAR => 'shar -m', DIST_CP => 'ln', ZIP => '/bin/zip',
  ZIPFLAGS => '-rl', DIST_DEFAULT => 'private tardist' }

If you specify COMPRESS, then SUFFIX should also be altered, as it is
needed to tell make the target file of the compression. Setting
DIST_CP to ln can be useful, if you need to preserve the timestamps on
your files. DIST_CP can take the values 'cp', which copies the file,
'ln', which links the file, and 'best' which copies symbolic links and
links the rest. Default is 'best'.

=item dynamic_lib

  {ARMAYBE => 'ar', OTHERLDFLAGS => '...', INST_DYNAMIC_DEP => '...'}

=item linkext

  {LINKTYPE => 'static', 'dynamic' or ''}

NB: Extensions that have nothing but *.pm files had to say

  {LINKTYPE => ''}

with Pre-5.0 MakeMakers. Since version 5.00 of MakeMaker such a line
can be deleted safely. MakeMaker recognizes when there's nothing to
be linked.

=item macro

  {ANY_MACRO => ANY_VALUE, ...}

=item postamble

Anything put here will be passed to MY::postamble() if you have one.

=item realclean

  {FILES => '$(INST_ARCHAUTODIR)/*.xyz'}

=item test

  {TESTS => 't/*.t'}

=item tool_autosplit

  {MAXLEN => 8}

=back

=head2 Overriding MakeMaker Methods

If you cannot achieve the desired Makefile behaviour by specifying
attributes you may define private subroutines in the Makefile.PL.
Each subroutine returns the text it wishes to have written to
the Makefile. To override a section of the Makefile you can
either say:

        sub MY::c_o { "new literal text" }

or you can edit the default by saying something like:

        package MY; # so that "SUPER" works right
        sub c_o {
            my $inherited = shift->SUPER::c_o(@_);
            $inherited =~ s/old text/new text/;
            $inherited;
        }

If you are running experiments with embedding perl as a library into
other applications, you might find MakeMaker is not sufficient. You'd
better have a look at ExtUtils::Embed which is a collection of utilities
for embedding.

If you still need a different solution, try to develop another
subroutine that fits your needs and submit the diffs to
C<makemaker@perl.org>

For a complete description of all MakeMaker methods see
L<ExtUtils::MM_Unix>.

Here is a simple example of how to add a new target to the generated
Makefile:

    sub MY::postamble {
        return <<'MAKE_FRAG';
    $(MYEXTLIB): sdbm/Makefile
            cd sdbm && $(MAKE) all

    MAKE_FRAG
    }

=head2 The End Of Cargo Cult Programming

WriteMakefile() now does some basic sanity checks on its parameters to
protect against typos and malformatted values.  This means some things
which happened to work in the past will now throw warnings and
possibly produce internal errors.

Some of the most common mistakes:

=over 2

=item C<< MAN3PODS => ' ' >>

This is commonly used to suppress the creation of man pages.  MAN3PODS
takes a hash ref not a string, but the above worked by accident in old
versions of MakeMaker.

The correct code is C<< MAN3PODS => { } >>.

=back


=head2 Hintsfile support

MakeMaker.pm uses the architecture specific information from
Config.pm. In addition it evaluates architecture specific hints files
in a C<hints/> directory. The hints files are expected to be named
like their counterparts in C<PERL_SRC/hints>, but with an C<.pl> file
name extension (eg. C<next_3_2.pl>). They are simply C<eval>ed by
MakeMaker within the WriteMakefile() subroutine, and can be used to
execute commands as well as to include special variables. The rules
which hintsfile is chosen are the same as in Configure.

The hintsfile is eval()ed immediately after the arguments given to
WriteMakefile are stuffed into a hash reference $self but before this
reference becomes blessed. So if you want to do the equivalent to
override or create an attribute you would say something like

    $self->{LIBS} = ['-ldbm -lucb -lc'];

=head2 Distribution Support

For authors of extensions MakeMaker provides several Makefile
targets. Most of the support comes from the ExtUtils::Manifest module,
where additional documentation can be found.

=over 4

=item    make distcheck

reports which files are below the build directory but not in the
MANIFEST file and vice versa. (See ExtUtils::Manifest::fullcheck() for
details)

=item    make skipcheck

reports which files are skipped due to the entries in the
C<MANIFEST.SKIP> file (See ExtUtils::Manifest::skipcheck() for
details)

=item    make distclean

does a realclean first and then the distcheck. Note that this is not
needed to build a new distribution as long as you are sure that the
MANIFEST file is ok.

=item    make manifest

rewrites the MANIFEST file, adding all remaining files found (See
ExtUtils::Manifest::mkmanifest() for details)

=item    make distdir

Copies all the files that are in the MANIFEST file to a newly created
directory with the name C<$(DISTNAME)-$(VERSION)>. If that directory
exists, it will be removed first.

Additionally, it will create a META.yml module meta-data file in the
distdir and add this to the distdir's MANIFEST.  You can shut this
behavior off with the NO_META flag.

=item   make disttest

Makes a distdir first, and runs a C<perl Makefile.PL>, a make, and
a make test in that directory.

=item    make tardist

First does a distdir. Then a command $(PREOP) which defaults to a null
command, followed by $(TO_UNIX), which defaults to a null command under
UNIX, and will convert files in distribution directory to UNIX format
otherwise. Next it runs C<tar> on that directory into a tarfile and
deletes the directory. Finishes with a command $(POSTOP) which
defaults to a null command.

=item    make dist

Defaults to $(DIST_DEFAULT) which in turn defaults to tardist.

=item    make uutardist

Runs a tardist first and uuencodes the tarfile.

=item    make shdist

First does a distdir. Then a command $(PREOP) which defaults to a null
command. Next it runs C<shar> on that directory into a sharfile and
deletes the intermediate directory again. Finishes with a command
$(POSTOP) which defaults to a null command.  Note: For shdist to work
properly a C<shar> program that can handle directories is mandatory.

=item    make zipdist

First does a distdir. Then a command $(PREOP) which defaults to a null
command. Runs C<$(ZIP) $(ZIPFLAGS)> on that directory into a
zipfile. Then deletes that directory. Finishes with a command
$(POSTOP) which defaults to a null command.

=item    make ci

Does a $(CI) and a $(RCS_LABEL) on all files in the MANIFEST file.

=back

Customization of the dist targets can be done by specifying a hash
reference to the dist attribute of the WriteMakefile call. The
following parameters are recognized:

    CI           ('ci -u')
    COMPRESS     ('gzip --best')
    POSTOP       ('@ :')
    PREOP        ('@ :')
    TO_UNIX      (depends on the system)
    RCS_LABEL    ('rcs -q -Nv$(VERSION_SYM):')
    SHAR         ('shar')
    SUFFIX       ('.gz')
    TAR          ('tar')
    TARFLAGS     ('cvf')
    ZIP          ('zip')
    ZIPFLAGS     ('-r')

An example:

    WriteMakefile( 'dist' => { COMPRESS=>"bzip2", SUFFIX=>".bz2" })


=head2 Module Meta-Data

Long plaguing users of MakeMaker based modules has been the problem of
getting basic information about the module out of the sources
I<without> running the F<Makefile.PL> and doing a bunch of messy
heuristics on the resulting F<Makefile>.  To this end a simple module
meta-data file has been introduced, F<META.yml>.

F<META.yml> is a YAML document (see http://www.yaml.org) containing
basic information about the module (name, version, prerequisites...)
in an easy to read format.  The format is developed and defined by the
Module::Build developers (see 
http://module-build.sourceforge.net/META-spec.html)

MakeMaker will automatically generate a F<META.yml> file for you and
add it to your F<MANIFEST> as part of the 'distdir' target (and thus
the 'dist' target).  This is intended to seamlessly and rapidly
populate CPAN with module meta-data.  If you wish to shut this feature
off, set the C<NO_META> C<WriteMakefile()> flag to true.


=head2 Disabling an extension

If some events detected in F<Makefile.PL> imply that there is no way
to create the Module, but this is a normal state of things, then you
can create a F<Makefile> which does nothing, but succeeds on all the
"usual" build targets.  To do so, use

    use ExtUtils::MakeMaker qw(WriteEmptyMakefile);
    WriteEmptyMakefile();

instead of WriteMakefile().

This may be useful if other modules expect this module to be I<built>
OK, as opposed to I<work> OK (say, this system-dependent module builds
in a subdirectory of some other distribution, or is listed as a
dependency in a CPAN::Bundle, but the functionality is supported by
different means on the current architecture).

=head2 Other Handy Functions

=over 4

=item prompt

    my $value = prompt($message);
    my $value = prompt($message, $default);

The C<prompt()> function provides an easy way to request user input
used to write a makefile.  It displays the $message as a prompt for
input.  If a $default is provided it will be used as a default.  The
function returns the $value selected by the user.

If C<prompt()> detects that it is not running interactively and there
is nothing on STDIN or if the PERL_MM_USE_DEFAULT environment variable
is set to true, the $default will be used without prompting.  This
prevents automated processes from blocking on user input. 

If no $default is provided an empty string will be used instead.

=back


=head1 ENVIRONMENT

=over 4

=item PERL_MM_OPT

Command line options used by C<MakeMaker-E<gt>new()>, and thus by
C<WriteMakefile()>.  The string is split on whitespace, and the result
is processed before any actual command line arguments are processed.

=item PERL_MM_USE_DEFAULT

If set to a true value then MakeMaker's prompt function will
always return the default without waiting for user input.

=item PERL_CORE

Same as the PERL_CORE parameter.  The parameter overrides this.

=back

=head1 SEE ALSO

L<Module::Build> is a pure-Perl alternative to MakeMaker which does
not rely on make or any other external utility.  It is easier to
extend to suit your needs.

L<Module::Install> is a wrapper around MakeMaker which adds features
not normally available.

L<ExtUtils::ModuleMaker> and L<Module::Starter> are both modules to
help you setup your distribution.

=head1 AUTHORS

Andy Dougherty C<doughera@lafayette.edu>, Andreas KE<ouml>nig
C<andreas.koenig@mind.de>, Tim Bunce C<timb@cpan.org>.  VMS
support by Charles Bailey C<bailey@newman.upenn.edu>.  OS/2 support
by Ilya Zakharevich C<ilya@math.ohio-state.edu>.

Currently maintained by Michael G Schwern C<schwern@pobox.com>

Send patches and ideas to C<makemaker@perl.org>.

Send bug reports via http://rt.cpan.org/.  Please send your
generated Makefile along with your report.

For more up-to-date information, see L<http://www.makemaker.org>.

=head1 LICENSE

This program is free software; you can redistribute it and/or 
modify it under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>


=cut
