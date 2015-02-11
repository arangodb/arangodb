# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 759 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/load_imports.al)"
sub load_imports {
%EXPORT_TAGS = (

    assert_h =>	[qw(assert NDEBUG)],

    ctype_h =>	[qw(isalnum isalpha iscntrl isdigit isgraph islower
		isprint ispunct isspace isupper isxdigit tolower toupper)],

    dirent_h =>	[],

    errno_h =>	[qw(E2BIG EACCES EADDRINUSE EADDRNOTAVAIL EAFNOSUPPORT
		EAGAIN EALREADY EBADF EBUSY ECHILD ECONNABORTED
		ECONNREFUSED ECONNRESET EDEADLK EDESTADDRREQ EDOM EDQUOT
		EEXIST EFAULT EFBIG EHOSTDOWN EHOSTUNREACH EINPROGRESS
		EINTR EINVAL EIO EISCONN EISDIR ELOOP EMFILE EMLINK
		EMSGSIZE ENAMETOOLONG ENETDOWN ENETRESET ENETUNREACH
		ENFILE ENOBUFS ENODEV ENOENT ENOEXEC ENOLCK ENOMEM
		ENOPROTOOPT ENOSPC ENOSYS ENOTBLK ENOTCONN ENOTDIR
		ENOTEMPTY ENOTSOCK ENOTTY ENXIO EOPNOTSUPP EPERM
		EPFNOSUPPORT EPIPE EPROCLIM EPROTONOSUPPORT EPROTOTYPE
		ERANGE EREMOTE ERESTART EROFS ESHUTDOWN ESOCKTNOSUPPORT
		ESPIPE ESRCH ESTALE ETIMEDOUT ETOOMANYREFS ETXTBSY
		EUSERS EWOULDBLOCK EXDEV errno)],

    fcntl_h =>	[qw(FD_CLOEXEC F_DUPFD F_GETFD F_GETFL F_GETLK F_RDLCK
		F_SETFD F_SETFL F_SETLK F_SETLKW F_UNLCK F_WRLCK
		O_ACCMODE O_APPEND O_CREAT O_EXCL O_NOCTTY O_NONBLOCK
		O_RDONLY O_RDWR O_TRUNC O_WRONLY
		creat
		SEEK_CUR SEEK_END SEEK_SET
		S_IRGRP S_IROTH S_IRUSR S_IRWXG S_IRWXO S_IRWXU
		S_ISBLK S_ISCHR S_ISDIR S_ISFIFO S_ISGID S_ISREG S_ISUID
		S_IWGRP S_IWOTH S_IWUSR)],

    float_h =>	[qw(DBL_DIG DBL_EPSILON DBL_MANT_DIG
		DBL_MAX DBL_MAX_10_EXP DBL_MAX_EXP
		DBL_MIN DBL_MIN_10_EXP DBL_MIN_EXP
		FLT_DIG FLT_EPSILON FLT_MANT_DIG
		FLT_MAX FLT_MAX_10_EXP FLT_MAX_EXP
		FLT_MIN FLT_MIN_10_EXP FLT_MIN_EXP
		FLT_RADIX FLT_ROUNDS
		LDBL_DIG LDBL_EPSILON LDBL_MANT_DIG
		LDBL_MAX LDBL_MAX_10_EXP LDBL_MAX_EXP
		LDBL_MIN LDBL_MIN_10_EXP LDBL_MIN_EXP)],

    grp_h =>	[],

    limits_h =>	[qw( ARG_MAX CHAR_BIT CHAR_MAX CHAR_MIN CHILD_MAX
		INT_MAX INT_MIN LINK_MAX LONG_MAX LONG_MIN MAX_CANON
		MAX_INPUT MB_LEN_MAX NAME_MAX NGROUPS_MAX OPEN_MAX
		PATH_MAX PIPE_BUF SCHAR_MAX SCHAR_MIN SHRT_MAX SHRT_MIN
		SSIZE_MAX STREAM_MAX TZNAME_MAX UCHAR_MAX UINT_MAX
		ULONG_MAX USHRT_MAX _POSIX_ARG_MAX _POSIX_CHILD_MAX
		_POSIX_LINK_MAX _POSIX_MAX_CANON _POSIX_MAX_INPUT
		_POSIX_NAME_MAX _POSIX_NGROUPS_MAX _POSIX_OPEN_MAX
		_POSIX_PATH_MAX _POSIX_PIPE_BUF _POSIX_SSIZE_MAX
		_POSIX_STREAM_MAX _POSIX_TZNAME_MAX)],

    locale_h =>	[qw(LC_ALL LC_COLLATE LC_CTYPE LC_MESSAGES
		    LC_MONETARY LC_NUMERIC LC_TIME NULL
		    localeconv setlocale)],

    math_h =>	[qw(HUGE_VAL acos asin atan ceil cosh fabs floor fmod
		frexp ldexp log10 modf pow sinh tan tanh)],

    pwd_h =>	[],

    setjmp_h =>	[qw(longjmp setjmp siglongjmp sigsetjmp)],

    signal_h =>	[qw(SA_NOCLDSTOP SA_NOCLDWAIT SA_NODEFER SA_ONSTACK
		SA_RESETHAND SA_RESTART SA_SIGINFO SIGABRT SIGALRM
		SIGCHLD SIGCONT SIGFPE SIGHUP SIGILL SIGINT SIGKILL
		SIGPIPE %SIGRT SIGRTMIN SIGRTMAX SIGQUIT SIGSEGV SIGSTOP
		SIGTERM SIGTSTP SIGTTIN	SIGTTOU SIGUSR1 SIGUSR2
		SIG_BLOCK SIG_DFL SIG_ERR SIG_IGN SIG_SETMASK SIG_UNBLOCK
		raise sigaction signal sigpending sigprocmask sigsuspend)],

    stdarg_h =>	[],

    stddef_h =>	[qw(NULL offsetof)],

    stdio_h =>	[qw(BUFSIZ EOF FILENAME_MAX L_ctermid L_cuserid
		L_tmpname NULL SEEK_CUR SEEK_END SEEK_SET
		STREAM_MAX TMP_MAX stderr stdin stdout
		clearerr fclose fdopen feof ferror fflush fgetc fgetpos
		fgets fopen fprintf fputc fputs fread freopen
		fscanf fseek fsetpos ftell fwrite getchar gets
		perror putc putchar puts remove rewind
		scanf setbuf setvbuf sscanf tmpfile tmpnam
		ungetc vfprintf vprintf vsprintf)],

    stdlib_h =>	[qw(EXIT_FAILURE EXIT_SUCCESS MB_CUR_MAX NULL RAND_MAX
		abort atexit atof atoi atol bsearch calloc div
		free getenv labs ldiv malloc mblen mbstowcs mbtowc
		qsort realloc strtod strtol strtoul wcstombs wctomb)],

    string_h =>	[qw(NULL memchr memcmp memcpy memmove memset strcat
		strchr strcmp strcoll strcpy strcspn strerror strlen
		strncat strncmp strncpy strpbrk strrchr strspn strstr
		strtok strxfrm)],

    sys_stat_h => [qw(S_IRGRP S_IROTH S_IRUSR S_IRWXG S_IRWXO S_IRWXU
		S_ISBLK S_ISCHR S_ISDIR S_ISFIFO S_ISGID S_ISREG
		S_ISUID S_IWGRP S_IWOTH S_IWUSR S_IXGRP S_IXOTH S_IXUSR
		fstat mkfifo)],

    sys_times_h => [],

    sys_types_h => [],

    sys_utsname_h => [qw(uname)],

    sys_wait_h => [qw(WEXITSTATUS WIFEXITED WIFSIGNALED WIFSTOPPED
		WNOHANG WSTOPSIG WTERMSIG WUNTRACED)],

    termios_h => [qw( B0 B110 B1200 B134 B150 B1800 B19200 B200 B2400
		B300 B38400 B4800 B50 B600 B75 B9600 BRKINT CLOCAL
		CREAD CS5 CS6 CS7 CS8 CSIZE CSTOPB ECHO ECHOE ECHOK
		ECHONL HUPCL ICANON ICRNL IEXTEN IGNBRK IGNCR IGNPAR
		INLCR INPCK ISIG ISTRIP IXOFF IXON NCCS NOFLSH OPOST
		PARENB PARMRK PARODD TCIFLUSH TCIOFF TCIOFLUSH TCION
		TCOFLUSH TCOOFF TCOON TCSADRAIN TCSAFLUSH TCSANOW
		TOSTOP VEOF VEOL VERASE VINTR VKILL VMIN VQUIT VSTART
		VSTOP VSUSP VTIME
		cfgetispeed cfgetospeed cfsetispeed cfsetospeed tcdrain
		tcflow tcflush tcgetattr tcsendbreak tcsetattr )],

    time_h =>	[qw(CLK_TCK CLOCKS_PER_SEC NULL asctime clock ctime
		difftime mktime strftime tzset tzname)],

    unistd_h =>	[qw(F_OK NULL R_OK SEEK_CUR SEEK_END SEEK_SET
		STDERR_FILENO STDIN_FILENO STDOUT_FILENO W_OK X_OK
		_PC_CHOWN_RESTRICTED _PC_LINK_MAX _PC_MAX_CANON
		_PC_MAX_INPUT _PC_NAME_MAX _PC_NO_TRUNC _PC_PATH_MAX
		_PC_PIPE_BUF _PC_VDISABLE _POSIX_CHOWN_RESTRICTED
		_POSIX_JOB_CONTROL _POSIX_NO_TRUNC _POSIX_SAVED_IDS
		_POSIX_VDISABLE _POSIX_VERSION _SC_ARG_MAX
		_SC_CHILD_MAX _SC_CLK_TCK _SC_JOB_CONTROL
		_SC_NGROUPS_MAX _SC_OPEN_MAX _SC_PAGESIZE _SC_SAVED_IDS
		_SC_STREAM_MAX _SC_TZNAME_MAX _SC_VERSION
		_exit access ctermid cuserid
		dup2 dup execl execle execlp execv execve execvp
		fpathconf fsync getcwd getegid geteuid getgid getgroups
		getpid getuid isatty lseek pathconf pause setgid setpgid
		setsid setuid sysconf tcgetpgrp tcsetpgrp ttyname)],

    utime_h =>	[],

);

# Exporter::export_tags();
{
  # De-duplicate the export list: 
  my %export;
  @export{map {@$_} values %EXPORT_TAGS} = ();
  # Doing the de-dup with a temporary hash has the advantage that the SVs in
  # @EXPORT are actually shared hash key sacalars, which will save some memory.
  push @EXPORT, keys %export;
}

@EXPORT_OK = qw(
		abs
		alarm
		atan2
		chdir
		chmod
		chown
		close
		closedir
		cos
		exit
		exp
		fcntl
		fileno
		fork
		getc
		getgrgid
		getgrnam
		getlogin
		getpgrp
		getppid
		getpwnam
		getpwuid
		gmtime
		isatty
		kill
		lchown
		link
		localtime
		log
		mkdir
		nice
		open
		opendir
		pipe
		printf
		rand
		read
		readdir
		rename
		rewinddir
		rmdir
		sin
		sleep
		sprintf
		sqrt
		srand
		stat
		system
		time
		times
		umask
		unlink
		utime
		wait
		waitpid
		write
);

require Exporter;
}

# end of POSIX::SigAction::load_imports
1;
