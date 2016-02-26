package POSIX;
use strict;
use warnings;

our(@ISA, %EXPORT_TAGS, @EXPORT_OK, @EXPORT, $AUTOLOAD, %SIGRT) = ();

our $VERSION = "1.16";

use AutoLoader;

use XSLoader ();

use Fcntl qw(FD_CLOEXEC F_DUPFD F_GETFD F_GETFL F_GETLK F_RDLCK F_SETFD
	     F_SETFL F_SETLK F_SETLKW F_UNLCK F_WRLCK O_ACCMODE O_APPEND
	     O_CREAT O_EXCL O_NOCTTY O_NONBLOCK O_RDONLY O_RDWR O_TRUNC
	     O_WRONLY SEEK_CUR SEEK_END SEEK_SET
	     S_ISBLK S_ISCHR S_ISDIR S_ISFIFO S_ISREG
	     S_IRGRP S_IROTH S_IRUSR S_IRWXG S_IRWXO S_IRWXU S_ISGID S_ISUID
	     S_IWGRP S_IWOTH S_IWUSR S_IXGRP S_IXOTH S_IXUSR);

# Grandfather old foo_h form to new :foo_h form
my $loaded;

sub import {
    load_imports() unless $loaded++;
    my $this = shift;
    my @list = map { m/^\w+_h$/ ? ":$_" : $_ } @_;
    local $Exporter::ExportLevel = 1;
    Exporter::import($this,@list);
}

sub croak { require Carp;  goto &Carp::croak }
# declare usage to assist AutoLoad
sub usage;

XSLoader::load 'POSIX', $VERSION;

sub AUTOLOAD {
    no strict;
    no warnings 'uninitialized';
    if ($AUTOLOAD =~ /::(_?[a-z])/) {
	# require AutoLoader;
	$AutoLoader::AUTOLOAD = $AUTOLOAD;
	goto &AutoLoader::AUTOLOAD
    }
    local $! = 0;
    my $constname = $AUTOLOAD;
    $constname =~ s/.*:://;
    my ($error, $val) = constant($constname);
    croak $error if $error;
    *$AUTOLOAD = sub { $val };

    goto &$AUTOLOAD;
}

package POSIX::SigAction;

use AutoLoader 'AUTOLOAD';

package POSIX::SigRt;

use AutoLoader 'AUTOLOAD';

use Tie::Hash;

use vars qw($SIGACTION_FLAGS $_SIGRTMIN $_SIGRTMAX $_sigrtn @ISA);
@POSIX::SigRt::ISA = qw(Tie::StdHash);

$SIGACTION_FLAGS = 0;

tie %POSIX::SIGRT, 'POSIX::SigRt';

sub DESTROY {};

package POSIX;

1;
__END__

sub usage {
    my ($mess) = @_;
    croak "Usage: POSIX::$mess";
}

sub redef {
    my ($mess) = @_;
    croak "Use method $mess instead";
}

sub unimpl {
    my ($mess) = @_;
    $mess =~ s/xxx//;
    croak "Unimplemented: POSIX::$mess";
}

sub assert {
    usage "assert(expr)" if @_ != 1;
    if (!$_[0]) {
	croak "Assertion failed";
    }
}

sub tolower {
    usage "tolower(string)" if @_ != 1;
    lc($_[0]);
}

sub toupper {
    usage "toupper(string)" if @_ != 1;
    uc($_[0]);
}

sub closedir {
    usage "closedir(dirhandle)" if @_ != 1;
    CORE::closedir($_[0]);
}

sub opendir {
    usage "opendir(directory)" if @_ != 1;
    my $dirhandle;
    CORE::opendir($dirhandle, $_[0])
	? $dirhandle
	: undef;
}

sub readdir {
    usage "readdir(dirhandle)" if @_ != 1;
    CORE::readdir($_[0]);
}

sub rewinddir {
    usage "rewinddir(dirhandle)" if @_ != 1;
    CORE::rewinddir($_[0]);
}

sub errno {
    usage "errno()" if @_ != 0;
    $! + 0;
}

sub creat {
    usage "creat(filename, mode)" if @_ != 2;
    &open($_[0], &O_WRONLY | &O_CREAT | &O_TRUNC, $_[1]);
}

sub fcntl {
    usage "fcntl(filehandle, cmd, arg)" if @_ != 3;
    CORE::fcntl($_[0], $_[1], $_[2]);
}

sub getgrgid {
    usage "getgrgid(gid)" if @_ != 1;
    CORE::getgrgid($_[0]);
}

sub getgrnam {
    usage "getgrnam(name)" if @_ != 1;
    CORE::getgrnam($_[0]);
}

sub atan2 {
    usage "atan2(x,y)" if @_ != 2;
    CORE::atan2($_[0], $_[1]);
}

sub cos {
    usage "cos(x)" if @_ != 1;
    CORE::cos($_[0]);
}

sub exp {
    usage "exp(x)" if @_ != 1;
    CORE::exp($_[0]);
}

sub fabs {
    usage "fabs(x)" if @_ != 1;
    CORE::abs($_[0]);
}

sub log {
    usage "log(x)" if @_ != 1;
    CORE::log($_[0]);
}

sub pow {
    usage "pow(x,exponent)" if @_ != 2;
    $_[0] ** $_[1];
}

sub sin {
    usage "sin(x)" if @_ != 1;
    CORE::sin($_[0]);
}

sub sqrt {
    usage "sqrt(x)" if @_ != 1;
    CORE::sqrt($_[0]);
}

sub getpwnam {
    usage "getpwnam(name)" if @_ != 1;
    CORE::getpwnam($_[0]);
}

sub getpwuid {
    usage "getpwuid(uid)" if @_ != 1;
    CORE::getpwuid($_[0]);
}

sub longjmp {
    unimpl "longjmp() is C-specific: use die instead";
}

sub setjmp {
    unimpl "setjmp() is C-specific: use eval {} instead";
}

sub siglongjmp {
    unimpl "siglongjmp() is C-specific: use die instead";
}

sub sigsetjmp {
    unimpl "sigsetjmp() is C-specific: use eval {} instead";
}

sub kill {
    usage "kill(pid, sig)" if @_ != 2;
    CORE::kill $_[1], $_[0];
}

sub raise {
    usage "raise(sig)" if @_ != 1;
    CORE::kill $_[0], $$;	# Is this good enough?
}

sub offsetof {
    unimpl "offsetof() is C-specific, stopped";
}

sub clearerr {
    redef "IO::Handle::clearerr()";
}

sub fclose {
    redef "IO::Handle::close()";
}

sub fdopen {
    redef "IO::Handle::new_from_fd()";
}

sub feof {
    redef "IO::Handle::eof()";
}

sub fgetc {
    redef "IO::Handle::getc()";
}

sub fgets {
    redef "IO::Handle::gets()";
}

sub fileno {
    redef "IO::Handle::fileno()";
}

sub fopen {
    redef "IO::File::open()";
}

sub fprintf {
    unimpl "fprintf() is C-specific--use printf instead";
}

sub fputc {
    unimpl "fputc() is C-specific--use print instead";
}

sub fputs {
    unimpl "fputs() is C-specific--use print instead";
}

sub fread {
    unimpl "fread() is C-specific--use read instead";
}

sub freopen {
    unimpl "freopen() is C-specific--use open instead";
}

sub fscanf {
    unimpl "fscanf() is C-specific--use <> and regular expressions instead";
}

sub fseek {
    redef "IO::Seekable::seek()";
}

sub fsync {
    redef "IO::Handle::sync()";
}

sub ferror {
    redef "IO::Handle::error()";
}

sub fflush {
    redef "IO::Handle::flush()";
}

sub fgetpos {
    redef "IO::Seekable::getpos()";
}

sub fsetpos {
    redef "IO::Seekable::setpos()";
}

sub ftell {
    redef "IO::Seekable::tell()";
}

sub fwrite {
    unimpl "fwrite() is C-specific--use print instead";
}

sub getc {
    usage "getc(handle)" if @_ != 1;
    CORE::getc($_[0]);
}

sub getchar {
    usage "getchar()" if @_ != 0;
    CORE::getc(STDIN);
}

sub gets {
    usage "gets()" if @_ != 0;
    scalar <STDIN>;
}

sub perror {
    print STDERR "@_: " if @_;
    print STDERR $!,"\n";
}

sub printf {
    usage "printf(pattern, args...)" if @_ < 1;
    CORE::printf STDOUT @_;
}

sub putc {
    unimpl "putc() is C-specific--use print instead";
}

sub putchar {
    unimpl "putchar() is C-specific--use print instead";
}

sub puts {
    unimpl "puts() is C-specific--use print instead";
}

sub remove {
    usage "remove(filename)" if @_ != 1;
    (-d $_[0]) ? CORE::rmdir($_[0]) : CORE::unlink($_[0]);
}

sub rename {
    usage "rename(oldfilename, newfilename)" if @_ != 2;
    CORE::rename($_[0], $_[1]);
}

sub rewind {
    usage "rewind(filehandle)" if @_ != 1;
    CORE::seek($_[0],0,0);
}

sub scanf {
    unimpl "scanf() is C-specific--use <> and regular expressions instead";
}

sub sprintf {
    usage "sprintf(pattern,args)" if @_ == 0;
    CORE::sprintf(shift,@_);
}

sub sscanf {
    unimpl "sscanf() is C-specific--use regular expressions instead";
}

sub tmpfile {
    redef "IO::File::new_tmpfile()";
}

sub ungetc {
    redef "IO::Handle::ungetc()";
}

sub vfprintf {
    unimpl "vfprintf() is C-specific";
}

sub vprintf {
    unimpl "vprintf() is C-specific";
}

sub vsprintf {
    unimpl "vsprintf() is C-specific";
}

sub abs {
    usage "abs(x)" if @_ != 1;
    CORE::abs($_[0]);
}

sub atexit {
    unimpl "atexit() is C-specific: use END {} instead";
}

sub atof {
    unimpl "atof() is C-specific, stopped";
}

sub atoi {
    unimpl "atoi() is C-specific, stopped";
}

sub atol {
    unimpl "atol() is C-specific, stopped";
}

sub bsearch {
    unimpl "bsearch() not supplied";
}

sub calloc {
    unimpl "calloc() is C-specific, stopped";
}

sub div {
    unimpl "div() is C-specific, use /, % and int instead";
}

sub exit {
    usage "exit(status)" if @_ != 1;
    CORE::exit($_[0]);
}

sub free {
    unimpl "free() is C-specific, stopped";
}

sub getenv {
    usage "getenv(name)" if @_ != 1;
    $ENV{$_[0]};
}

sub labs {
    unimpl "labs() is C-specific, use abs instead";
}

sub ldiv {
    unimpl "ldiv() is C-specific, use /, % and int instead";
}

sub malloc {
    unimpl "malloc() is C-specific, stopped";
}

sub qsort {
    unimpl "qsort() is C-specific, use sort instead";
}

sub rand {
    unimpl "rand() is non-portable, use Perl's rand instead";
}

sub realloc {
    unimpl "realloc() is C-specific, stopped";
}

sub srand {
    unimpl "srand()";
}

sub system {
    usage "system(command)" if @_ != 1;
    CORE::system($_[0]);
}

sub memchr {
    unimpl "memchr() is C-specific, use index() instead";
}

sub memcmp {
    unimpl "memcmp() is C-specific, use eq instead";
}

sub memcpy {
    unimpl "memcpy() is C-specific, use = instead";
}

sub memmove {
    unimpl "memmove() is C-specific, use = instead";
}

sub memset {
    unimpl "memset() is C-specific, use x instead";
}

sub strcat {
    unimpl "strcat() is C-specific, use .= instead";
}

sub strchr {
    unimpl "strchr() is C-specific, use index() instead";
}

sub strcmp {
    unimpl "strcmp() is C-specific, use eq instead";
}

sub strcpy {
    unimpl "strcpy() is C-specific, use = instead";
}

sub strcspn {
    unimpl "strcspn() is C-specific, use regular expressions instead";
}

sub strerror {
    usage "strerror(errno)" if @_ != 1;
    local $! = $_[0];
    $! . "";
}

sub strlen {
    unimpl "strlen() is C-specific, use length instead";
}

sub strncat {
    unimpl "strncat() is C-specific, use .= instead";
}

sub strncmp {
    unimpl "strncmp() is C-specific, use eq instead";
}

sub strncpy {
    unimpl "strncpy() is C-specific, use = instead";
}

sub strpbrk {
    unimpl "strpbrk() is C-specific, stopped";
}

sub strrchr {
    unimpl "strrchr() is C-specific, use rindex() instead";
}

sub strspn {
    unimpl "strspn() is C-specific, stopped";
}

sub strstr {
    usage "strstr(big, little)" if @_ != 2;
    CORE::index($_[0], $_[1]);
}

sub strtok {
    unimpl "strtok() is C-specific, stopped";
}

sub chmod {
    usage "chmod(mode, filename)" if @_ != 2;
    CORE::chmod($_[0], $_[1]);
}

sub fstat {
    usage "fstat(fd)" if @_ != 1;
    local *TMP;
    CORE::open(TMP, "<&$_[0]");		# Gross.
    my @l = CORE::stat(TMP);
    CORE::close(TMP);
    @l;
}

sub mkdir {
    usage "mkdir(directoryname, mode)" if @_ != 2;
    CORE::mkdir($_[0], $_[1]);
}

sub stat {
    usage "stat(filename)" if @_ != 1;
    CORE::stat($_[0]);
}

sub umask {
    usage "umask(mask)" if @_ != 1;
    CORE::umask($_[0]);
}

sub wait {
    usage "wait()" if @_ != 0;
    CORE::wait();
}

sub waitpid {
    usage "waitpid(pid, options)" if @_ != 2;
    CORE::waitpid($_[0], $_[1]);
}

sub gmtime {
    usage "gmtime(time)" if @_ != 1;
    CORE::gmtime($_[0]);
}

sub localtime {
    usage "localtime(time)" if @_ != 1;
    CORE::localtime($_[0]);
}

sub time {
    usage "time()" if @_ != 0;
    CORE::time;
}

sub alarm {
    usage "alarm(seconds)" if @_ != 1;
    CORE::alarm($_[0]);
}

sub chdir {
    usage "chdir(directory)" if @_ != 1;
    CORE::chdir($_[0]);
}

sub chown {
    usage "chown(uid, gid, filename)" if @_ != 3;
    CORE::chown($_[0], $_[1], $_[2]);
}

sub execl {
    unimpl "execl() is C-specific, stopped";
}

sub execle {
    unimpl "execle() is C-specific, stopped";
}

sub execlp {
    unimpl "execlp() is C-specific, stopped";
}

sub execv {
    unimpl "execv() is C-specific, stopped";
}

sub execve {
    unimpl "execve() is C-specific, stopped";
}

sub execvp {
    unimpl "execvp() is C-specific, stopped";
}

sub fork {
    usage "fork()" if @_ != 0;
    CORE::fork;
}

sub getegid {
    usage "getegid()" if @_ != 0;
    $) + 0;
}

sub geteuid {
    usage "geteuid()" if @_ != 0;
    $> + 0;
}

sub getgid {
    usage "getgid()" if @_ != 0;
    $( + 0;
}

sub getgroups {
    usage "getgroups()" if @_ != 0;
    my %seen;
    grep(!$seen{$_}++, split(' ', $) ));
}

sub getlogin {
    usage "getlogin()" if @_ != 0;
    CORE::getlogin();
}

sub getpgrp {
    usage "getpgrp()" if @_ != 0;
    CORE::getpgrp;
}

sub getpid {
    usage "getpid()" if @_ != 0;
    $$;
}

sub getppid {
    usage "getppid()" if @_ != 0;
    CORE::getppid;
}

sub getuid {
    usage "getuid()" if @_ != 0;
    $<;
}

sub isatty {
    usage "isatty(filehandle)" if @_ != 1;
    -t $_[0];
}

sub link {
    usage "link(oldfilename, newfilename)" if @_ != 2;
    CORE::link($_[0], $_[1]);
}

sub rmdir {
    usage "rmdir(directoryname)" if @_ != 1;
    CORE::rmdir($_[0]);
}

sub setbuf {
    redef "IO::Handle::setbuf()";
}

sub setvbuf {
    redef "IO::Handle::setvbuf()";
}

sub sleep {
    usage "sleep(seconds)" if @_ != 1;
    $_[0] - CORE::sleep($_[0]);
}

sub unlink {
    usage "unlink(filename)" if @_ != 1;
    CORE::unlink($_[0]);
}

sub utime {
    usage "utime(filename, atime, mtime)" if @_ != 3;
    CORE::utime($_[1], $_[2], $_[0]);
}

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

package POSIX::SigAction;

sub new { bless {HANDLER => $_[1], MASK => $_[2], FLAGS => $_[3] || 0, SAFE => 0}, $_[0] }
sub handler { $_[0]->{HANDLER} = $_[1] if @_ > 1; $_[0]->{HANDLER} };
sub mask    { $_[0]->{MASK}    = $_[1] if @_ > 1; $_[0]->{MASK} };
sub flags   { $_[0]->{FLAGS}   = $_[1] if @_ > 1; $_[0]->{FLAGS} };
sub safe    { $_[0]->{SAFE}    = $_[1] if @_ > 1; $_[0]->{SAFE} };

package POSIX::SigRt;


sub _init {
    $_SIGRTMIN = &POSIX::SIGRTMIN;
    $_SIGRTMAX = &POSIX::SIGRTMAX;
    $_sigrtn   = $_SIGRTMAX - $_SIGRTMIN;
}

sub _croak {
    &_init unless defined $_sigrtn;
    die "POSIX::SigRt not available" unless defined $_sigrtn && $_sigrtn > 0;
}

sub _getsig {
    &_croak;
    my $rtsig = $_[0];
    # Allow (SIGRT)?MIN( + n)?, a common idiom when doing these things in C.
    $rtsig = $_SIGRTMIN + ($1 || 0)
	if $rtsig =~ /^(?:(?:SIG)?RT)?MIN(\s*\+\s*(\d+))?$/;
    return $rtsig;
}

sub _exist {
    my $rtsig = _getsig($_[1]);
    my $ok    = $rtsig >= $_SIGRTMIN && $rtsig <= $_SIGRTMAX;
    ($rtsig, $ok);
}

sub _check {
    my ($rtsig, $ok) = &_exist;
    die "No POSIX::SigRt signal $_[1] (valid range SIGRTMIN..SIGRTMAX, or $_SIGRTMIN..$_SIGRTMAX)"
	unless $ok;
    return $rtsig;
}

sub new {
    my ($rtsig, $handler, $flags) = @_;
    my $sigset = POSIX::SigSet->new($rtsig);
    my $sigact = POSIX::SigAction->new($handler,
				       $sigset,
				       $flags);
    POSIX::sigaction($rtsig, $sigact);
}

sub EXISTS { &_exist }
sub FETCH  { my $rtsig = &_check;
	     my $oa = POSIX::SigAction->new();
	     POSIX::sigaction($rtsig, undef, $oa);
	     return $oa->{HANDLER} }
sub STORE  { my $rtsig = &_check; new($rtsig, $_[2], $SIGACTION_FLAGS) }
sub DELETE { delete $SIG{ &_check } }
sub CLEAR  { &_exist; delete @SIG{ &POSIX::SIGRTMIN .. &POSIX::SIGRTMAX } }
sub SCALAR { &_croak; $_sigrtn + 1 }
