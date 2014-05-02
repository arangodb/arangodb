AC_ARG_ENABLE(
	[python],
	[AC_HELP_STRING([--enable-python=@<:no/yes@:>@],
			[Enable support for experimental python bindings. @<:@default=no@:>@])],
	[], [
		enable_python=no
		ax_python_header=no
	]
)
AS_IF([test "x$enable_python" = "xyes"], [
	AX_PYTHON
])
AM_CONDITIONAL([ENABLE_PYTHON], [test "x$ax_python_header" != "xno"])
