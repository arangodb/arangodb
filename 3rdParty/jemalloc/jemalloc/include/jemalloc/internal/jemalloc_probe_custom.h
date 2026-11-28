#ifndef JEMALLOC_INTERNAL_JEMALLOC_PROBE_CUSTOM_H
#define JEMALLOC_INTERNAL_JEMALLOC_PROBE_CUSTOM_H

/* clang-format off */

/*
 * This section is based on sys/sdt.h and
 * https://sourceware.org/systemtap/wiki/UserSpaceProbeImplementation
 */

/* Emit NOP for the probe. */
#if (defined(__x86_64__) || defined(__i386__) || defined(__aarch64__) || \
     defined(__arm__)) && defined(__linux__)
#define JE_SDT_NOP nop
#else
#error "Architecture not supported"
#endif

/* Assembly macros */
#define JE_SDT_S(x) #x

#define JE_SDT_ASM_1(x) JE_SDT_S(x) "\n"

#define JE_SDT_ASM_2(x, y)			\
	JE_SDT_S(x) "," JE_SDT_S(y) "\n"

#define JE_SDT_ASM_3(x, y, z)					\
	JE_SDT_S(x) "," JE_SDT_S(y) ","  JE_SDT_S(z) "\n"

#define JE_SDT_ASM_3(x, y, z)					\
	JE_SDT_S(x) "," JE_SDT_S(y) ","  JE_SDT_S(z) "\n"

#define JE_SDT_ASM_4(x, y, z, p)					\
	JE_SDT_S(x) "," JE_SDT_S(y) "," JE_SDT_S(z) "," JE_SDT_S(p) "\n"

#define JE_SDT_ASM_5(x, y, z, p, q)					\
	JE_SDT_S(x) "," JE_SDT_S(y) ","	JE_SDT_S(z) "," JE_SDT_S(p) ","	\
		JE_SDT_S(q) "\n"

/* Arg size */
#ifdef __LP64__
#define JE_SDT_ASM_ADDR            .8byte
#else
#define JE_SDT_ASM_ADDR            .4byte
#endif

#define JE_SDT_NOTE_NAME  "stapsdt"
#define JE_SDT_NOTE_TYPE  3

#define JE_SDT_SEMAPHORE_NONE(provider, name)			\
	JE_SDT_ASM_1(JE_SDT_ASM_ADDR 0) /* No Semaphore support */
#define JE_SDT_SEMAPHORE_OPERAND(provider, name)	\
	[__sdt_semaphore] "ip" (0) /* No Semaphore */

#define JE_SDT_ASM_STRING(x)     JE_SDT_ASM_1(.asciz JE_SDT_S(x))

#define JE_SDT_NOTE(provider, name, arg_template)			\
	JE_SDT_ASM_1(990: JE_SDT_NOP)					\
	JE_SDT_ASM_3(     .pushsection .note.stapsdt,"?","note")	\
	JE_SDT_ASM_1(     .balign 4)					\
	JE_SDT_ASM_3(     .4byte 992f-991f, 994f-993f, JE_SDT_NOTE_TYPE) \
	JE_SDT_ASM_1(991: .asciz JE_SDT_NOTE_NAME)			\
	JE_SDT_ASM_1(992: .balign 4)					\
	JE_SDT_ASM_1(993: JE_SDT_ASM_ADDR 990b)				\
	JE_SDT_ASM_1(     JE_SDT_ASM_ADDR _.stapsdt.base)		\
	JE_SDT_SEMAPHORE_NONE(provider, name)				\
	JE_SDT_ASM_STRING(provider)					\
	JE_SDT_ASM_STRING(name)						\
	JE_SDT_ASM_STRING(arg_template)					\
	JE_SDT_ASM_1(994: .balign 4)					\
	JE_SDT_ASM_1(     .popsection)

#define JE_SDT_BASE							\
	JE_SDT_ASM_1(     .ifndef _.stapsdt.base)			\
	JE_SDT_ASM_5(     .pushsection .stapsdt.base, "aG", "progbits",	\
		    .stapsdt.base,comdat)				\
	JE_SDT_ASM_1(     .weak _.stapsdt.base)				\
	JE_SDT_ASM_1(     .hidden _.stapsdt.base)			\
	JE_SDT_ASM_1(     _.stapsdt.base: .space 1)			\
	JE_SDT_ASM_2(     .size _.stapsdt.base, 1)			\
	JE_SDT_ASM_1(     .popsection)					\
	JE_SDT_ASM_1(     .endif)


/*
 * Default constraint for probes arguments.
 * See https://gcc.gnu.org/onlinedocs/gcc/Constraints.html
 */
#ifndef JE_SDT_ARG_CONSTRAINT
#define JE_SDT_ARG_CONSTRAINT      "nor"
#endif

#define JE_SDT_ARGARRAY(x)  ((__builtin_classify_type(x) == 14) ||  \
			     (__builtin_classify_type(x) == 5))
#define JE_SDT_ARGSIZE(x)   (JE_SDT_ARGARRAY(x) ? sizeof(void*) : sizeof(x))

/*
 * Format of each probe argument as operand.  Size tagged with JE_SDT_Sn,
 * with "n" constraint.  Value is tagged with JE_SDT_An with configured
 * constraint.
 */
#define JE_SDT_ARG(n, x)						\
	[JE_SDT_S##n] "n"                ((size_t)JE_SDT_ARGSIZE(x)),	\
		[JE_SDT_A##n] JE_SDT_ARG_CONSTRAINT(x)

/* Templates to append arguments as operands. */
#define JE_SDT_OPERANDS_0()     [__sdt_dummy] "g" (0)
#define JE_SDT_OPERANDS_1(_1)      JE_SDT_ARG(1, _1)
#define JE_SDT_OPERANDS_2(_1, _2)  JE_SDT_OPERANDS_1(_1), JE_SDT_ARG(2, _2)
#define JE_SDT_OPERANDS_3(_1, _2, _3) JE_SDT_OPERANDS_2(_1, _2), JE_SDT_ARG(3, _3)
#define JE_SDT_OPERANDS_4(_1, _2, _3, _4)			\
	JE_SDT_OPERANDS_3(_1, _2, _3), JE_SDT_ARG(4, _4)
#define JE_SDT_OPERANDS_5(_1, _2, _3, _4, _5)			\
	JE_SDT_OPERANDS_4(_1, _2, _3, _4), JE_SDT_ARG(5, _5)
#define JE_SDT_OPERANDS_6(_1, _2, _3, _4, _5, _6)			\
	JE_SDT_OPERANDS_5(_1, _2, _3, _4, _5), JE_SDT_ARG(6, _6)
#define JE_SDT_OPERANDS_7(_1, _2, _3, _4, _5, _6, _7)		\
	JE_SDT_OPERANDS_6(_1, _2, _3, _4, _5, _6), JE_SDT_ARG(7, _7)

/* Templates to reference the arguments from operands. */
#define JE_SDT_ARGFMT(num)        %n[JE_SDT_S##num]@%[JE_SDT_A##num]
#define JE_SDT_ARG_TEMPLATE_0    /* No args */
#define JE_SDT_ARG_TEMPLATE_1    JE_SDT_ARGFMT(1)
#define JE_SDT_ARG_TEMPLATE_2    JE_SDT_ARG_TEMPLATE_1 JE_SDT_ARGFMT(2)
#define JE_SDT_ARG_TEMPLATE_3    JE_SDT_ARG_TEMPLATE_2 JE_SDT_ARGFMT(3)
#define JE_SDT_ARG_TEMPLATE_4    JE_SDT_ARG_TEMPLATE_3 JE_SDT_ARGFMT(4)
#define JE_SDT_ARG_TEMPLATE_5    JE_SDT_ARG_TEMPLATE_4 JE_SDT_ARGFMT(5)
#define JE_SDT_ARG_TEMPLATE_6    JE_SDT_ARG_TEMPLATE_5 JE_SDT_ARGFMT(6)
#define JE_SDT_ARG_TEMPLATE_7    JE_SDT_ARG_TEMPLATE_6 JE_SDT_ARGFMT(7)

#define JE_SDT_PROBE(							\
	provider, name, n, arglist)					\
	do {								\
		__asm__ __volatile__(					\
			JE_SDT_NOTE(provider, name,			\
				    JE_SDT_ARG_TEMPLATE_##n)		\
			:: JE_SDT_SEMAPHORE_OPERAND(provider, name),	\
			JE_SDT_OPERANDS_##n arglist);			\
		__asm__ __volatile__(JE_SDT_BASE);			\
	} while (0)

#define JE_USDT(name, N, ...)						\
  JE_SDT_PROBE(jemalloc, name, N, (__VA_ARGS__))


#endif /* JEMALLOC_INTERNAL_JEMALLOC_PROBE_CUSTOM_H */

/* clang-format on */
