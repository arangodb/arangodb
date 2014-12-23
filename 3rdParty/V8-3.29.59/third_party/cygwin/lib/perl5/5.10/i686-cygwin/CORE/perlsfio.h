/*    perlsfio.h
 *
 *    Copyright (C) 1996, 1999, 2000, 2001, 2002, 2003, 2005, 2007,
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/* The next #ifdef should be redundant if Configure behaves ... */
#ifndef FILE
#define FILE FILE
#endif
#ifdef I_SFIO
#include <sfio.h>
#endif

/* sfio 2000 changed _stdopen to _stdfdopen */
#if SFIO_VERSION >= 20000101L
#define _stdopen _stdfdopen
#endif

extern Sfio_t*	_stdopen _ARG_((int, const char*));
extern int	_stdprintf _ARG_((const char*, ...));

#define PerlIO				Sfio_t
#define PerlIO_stderr()			sfstderr
#define PerlIO_stdout()			sfstdout
#define PerlIO_stdin()			sfstdin

#define PerlIO_isutf8(f)		0

#define PerlIO_printf			sfprintf
#define PerlIO_stdoutf			_stdprintf
#define PerlIO_vprintf(f,fmt,a)		sfvprintf(f,fmt,a)
#define PerlIO_read(f,buf,count)	sfread(f,buf,count)
#define PerlIO_write(f,buf,count)	sfwrite(f,buf,count)
#define PerlIO_open(path,mode)		sfopen(NULL,path,mode)
#define PerlIO_fdopen(fd,mode)		_stdopen(fd,mode)
#define PerlIO_reopen(path,mode,f)	sfopen(f,path,mode)
#define PerlIO_close(f)			sfclose(f)
#define PerlIO_puts(f,s)		sfputr(f,s,-1)
#define PerlIO_putc(f,c)		sfputc(f,c)
#define PerlIO_ungetc(f,c)		sfungetc(f,c)
#define PerlIO_sprintf			sfsprintf
#define PerlIO_getc(f)			sfgetc(f)
#define PerlIO_eof(f)			sfeof(f)
#define PerlIO_error(f)			sferror(f)
#define PerlIO_fileno(f)		sffileno(f)
#define PerlIO_clearerr(f)		sfclrerr(f)
#define PerlIO_flush(f)			sfsync(f)
#define PerlIO_tell(f)			sftell(f)
#define PerlIO_seek(f,o,w)		sfseek(f,o,w)
#define PerlIO_rewind(f)		(void) sfseek((f),0L,0)
#define PerlIO_tmpfile()		sftmp(0)
#define PerlIO_exportFILE(f,fl)		Perl_croak(aTHX_ "Export to FILE * unimplemented")
#define PerlIO_releaseFILE(p,f)		Perl_croak(aTHX_ "Release of FILE * unimplemented")

#define PerlIO_setlinebuf(f)		sfset(f,SF_LINE,1)

/* Now our interface to equivalent of Configure's FILE_xxx macros */

#define PerlIO_has_cntptr(f)		1
#define PerlIO_get_ptr(f)		((f)->next)
#define PerlIO_get_cnt(f)		((f)->endr - (f)->next)
#define PerlIO_canset_cnt(f)		1
#define PerlIO_fast_gets(f)		1
#define PerlIO_set_ptrcnt(f,p,c)	STMT_START {(f)->next = (unsigned char *)(p); assert(PerlIO_get_cnt(f) == (c));} STMT_END
#define PerlIO_set_cnt(f,c)		STMT_START {(f)->next = (f)->endr - (c);} STMT_END

#define PerlIO_has_base(f)		1
#define PerlIO_get_base(f)		((f)->data)
#define PerlIO_get_bufsiz(f)		((f)->endr - (f)->data)

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
