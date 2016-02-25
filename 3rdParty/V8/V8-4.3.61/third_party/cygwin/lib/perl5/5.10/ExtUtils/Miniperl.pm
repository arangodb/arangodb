# This File keeps the contents of miniperlmain.c.
#
# It was generated automatically by minimod.PL from the contents
# of miniperlmain.c. Don't edit this file!
#
#       ANY CHANGES MADE HERE WILL BE LOST! 
#


package ExtUtils::Miniperl;
require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(&writemain);

$head= <<'EOF!HEAD';
/*    miniperlmain.c
 *
 *    Copyright (C) 1994, 1995, 1996, 1997, 1999, 2000, 2001, 2002, 2003,
 *    2004, 2005, 2006, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "The Road goes ever on and on, down from the door where it began."
 */

/* This file contains the main() function for the perl interpreter.
 * Note that miniperlmain.c contains main() for the 'miniperl' binary,
 * while perlmain.c contains main() for the 'perl' binary.
 *
 * Miniperl is like perl except that it does not support dynamic loading,
 * and in fact is used to build the dynamic modules needed for the 'real'
 * perl executable.
 */

#ifdef OEMVS
#ifdef MYMALLOC
/* sbrk is limited to first heap segment so make it big */
#pragma runopts(HEAP(8M,500K,ANYWHERE,KEEP,8K,4K) STACK(,,ANY,) ALL31(ON))
#else
#pragma runopts(HEAP(2M,500K,ANYWHERE,KEEP,8K,4K) STACK(,,ANY,) ALL31(ON))
#endif
#endif


#include "EXTERN.h"
#define PERL_IN_MINIPERLMAIN_C
#include "perl.h"

static void xs_init (pTHX);
static PerlInterpreter *my_perl;

#if defined (__MINT__) || defined (atarist)
/* The Atari operating system doesn't have a dynamic stack.  The
   stack size is determined from this value.  */
long _stksize = 64 * 1024;
#endif

#if defined(PERL_GLOBAL_STRUCT_PRIVATE)
/* The static struct perl_vars* may seem counterproductive since the
 * whole idea PERL_GLOBAL_STRUCT_PRIVATE was to avoid statics, but note
 * that this static is not in the shared perl library, the globals PL_Vars
 * and PL_VarsPtr will stay away. */
static struct perl_vars* my_plvarsp;
struct perl_vars* Perl_GetVarsPrivate(void) { return my_plvarsp; }
#endif

#ifdef NO_ENV_ARRAY_IN_MAIN
extern char **environ;
int
main(int argc, char **argv)
#else
int
main(int argc, char **argv, char **env)
#endif
{
    dVAR;
    int exitstatus;
#ifdef PERL_GLOBAL_STRUCT
    struct perl_vars *plvarsp = init_global_struct();
#  ifdef PERL_GLOBAL_STRUCT_PRIVATE
    my_vars = my_plvarsp = plvarsp;
#  endif
#endif /* PERL_GLOBAL_STRUCT */
    (void)env;
#ifndef PERL_USE_SAFE_PUTENV
    PL_use_safe_putenv = 0;
#endif /* PERL_USE_SAFE_PUTENV */

    /* if user wants control of gprof profiling off by default */
    /* noop unless Configure is given -Accflags=-DPERL_GPROF_CONTROL */
    PERL_GPROF_MONCONTROL(0);

#ifdef NO_ENV_ARRAY_IN_MAIN
    PERL_SYS_INIT3(&argc,&argv,&environ);
#else
    PERL_SYS_INIT3(&argc,&argv,&env);
#endif

#if defined(USE_ITHREADS)
    /* XXX Ideally, this should really be happening in perl_alloc() or
     * perl_construct() to keep libperl.a transparently fork()-safe.
     * It is currently done here only because Apache/mod_perl have
     * problems due to lack of a call to cancel pthread_atfork()
     * handlers when shared objects that contain the handlers may
     * be dlclose()d.  This forces applications that embed perl to
     * call PTHREAD_ATFORK() explicitly, but if and only if it hasn't
     * been called at least once before in the current process.
     * --GSAR 2001-07-20 */
    PTHREAD_ATFORK(Perl_atfork_lock,
                   Perl_atfork_unlock,
                   Perl_atfork_unlock);
#endif

    if (!PL_do_undump) {
	my_perl = perl_alloc();
	if (!my_perl)
	    exit(1);
	perl_construct(my_perl);
	PL_perl_destruct_level = 0;
    }
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    exitstatus = perl_parse(my_perl, xs_init, argc, argv, (char **)NULL);
    if (!exitstatus)
        perl_run(my_perl);

    exitstatus = perl_destruct(my_perl);

    perl_free(my_perl);

#if defined(USE_ENVIRON_ARRAY) && defined(PERL_TRACK_MEMPOOL) && !defined(NO_ENV_ARRAY_IN_MAIN)
    /*
     * The old environment may have been freed by perl_free()
     * when PERL_TRACK_MEMPOOL is defined, but without having
     * been restored by perl_destruct() before (this is only
     * done if destruct_level > 0).
     *
     * It is important to have a valid environment for atexit()
     * routines that are eventually called.
     */
    environ = env;
#endif

#ifdef PERL_GLOBAL_STRUCT
    free_global_struct(plvarsp);
#endif /* PERL_GLOBAL_STRUCT */

    PERL_SYS_TERM();

    exit(exitstatus);
    return exitstatus;
}

/* Register any extra external extensions */

EOF!HEAD
$tail=<<'EOF!TAIL';

static void
xs_init(pTHX)
{
    PERL_UNUSED_CONTEXT;
}

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
EOF!TAIL

sub writemain{
    my(@exts) = @_;

    my($pname);
    my($dl) = canon('/','DynaLoader');
    print $head;

    foreach $_ (@exts){
	my($pname) = canon('/', $_);
	my($mname, $cname);
	($mname = $pname) =~ s!/!::!g;
	($cname = $pname) =~ s!/!__!g;
        print "EXTERN_C void boot_${cname} (pTHX_ CV* cv);\n";
    }

    my ($tail1,$tail2) = ( $tail =~ /\A(.*\n)(\s*\}.*)\Z/s );
    print $tail1;

    print "\tconst char file[] = __FILE__;\n";
    print "\tdXSUB_SYS;\n" if $] > 5.002;

    foreach $_ (@exts){
	my($pname) = canon('/', $_);
	my($mname, $cname, $ccode);
	($mname = $pname) =~ s!/!::!g;
	($cname = $pname) =~ s!/!__!g;
	print "\t{\n";
	if ($pname eq $dl){
	    # Must NOT install 'DynaLoader::boot_DynaLoader' as 'bootstrap'!
	    # boot_DynaLoader is called directly in DynaLoader.pm
	    $ccode = "\t/* DynaLoader is a special case */\n
\tnewXS(\"${mname}::boot_${cname}\", boot_${cname}, file);\n";
	    print $ccode unless $SEEN{$ccode}++;
	} else {
	    $ccode = "\tnewXS(\"${mname}::bootstrap\", boot_${cname}, file);\n";
	    print $ccode unless $SEEN{$ccode}++;
	}
	print "\t}\n";
    }
    print $tail2;
}

sub canon{
    my($as, @ext) = @_;
	foreach(@ext){
	    # might be X::Y or lib/auto/X/Y/Y.a
		next if s!::!/!g;
	    s:^(lib|ext)/(auto/)?::;
	    s:/\w+\.\w+$::;
	}
	grep(s:/:$as:, @ext) if ($as ne '/');
	@ext;
}

1;
__END__

=head1 NAME

ExtUtils::Miniperl, writemain - write the C code for perlmain.c

=head1 SYNOPSIS

C<use ExtUtils::Miniperl;>

C<writemain(@directories);>

=head1 DESCRIPTION

This whole module is written when perl itself is built from a script
called minimod.PL. In case you want to patch it, please patch
minimod.PL in the perl distribution instead.

writemain() takes an argument list of directories containing archive
libraries that relate to perl modules and should be linked into a new
perl binary. It writes to STDOUT a corresponding perlmain.c file that
is a plain C file containing all the bootstrap code to make the
modules associated with the libraries available from within perl.

The typical usage is from within a Makefile generated by
ExtUtils::MakeMaker. So under normal circumstances you won't have to
deal with this module directly.

=head1 SEE ALSO

L<ExtUtils::MakeMaker>

=cut

