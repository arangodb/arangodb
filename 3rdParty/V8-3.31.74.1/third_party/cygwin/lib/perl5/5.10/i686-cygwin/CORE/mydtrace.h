/*    mydtrace.h
 *
 *    Copyright (C) 2008, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 *	Provides macros that wrap the various DTrace probes we use. We add
 *	an extra level of wrapping to encapsulate the _ENABLED tests.
 */

#if defined(USE_DTRACE) && defined(PERL_CORE)

#  include "perldtrace.h"

#  define ENTRY_PROBE(func, file, line) 	\
    if (PERL_SUB_ENTRY_ENABLED()) {		\
	PERL_SUB_ENTRY(func, file, line); 	\
    }

#  define RETURN_PROBE(func, file, line)	\
    if (PERL_SUB_RETURN_ENABLED()) {		\
	PERL_SUB_RETURN(func, file, line); 	\
    }

#else

/* NOPs */
#  define ENTRY_PROBE(func, file, line)
#  define RETURN_PROBE(func, file, line)

#endif

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
