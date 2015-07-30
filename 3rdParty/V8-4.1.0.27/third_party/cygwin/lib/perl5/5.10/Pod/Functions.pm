package Pod::Functions;
use strict;

=head1 NAME

Pod::Functions - Group Perl's functions a la perlfunc.pod

=head1 SYNOPSIS

    use Pod::Functions;
    
    my @misc_ops = @{ $Kinds{ 'Misc' } };
    my $misc_dsc = $Type_Description{ 'Misc' };

or

    perl /path/to/lib/Pod/Functions.pm

This will print a grouped list of Perl's functions, like the 
L<perlfunc/"Perl Functions by Category"> section.

=head1 DESCRIPTION

It exports the following variables:

=over 4

=item %Kinds

This holds a hash-of-lists. Each list contains the functions in the category
the key denotes.

=item %Type

In this hash each key represents a function and the value is the category.
The category can be a comma separated list.

=item %Flavor

In this hash each key represents a function and the value is a short 
description of that function.

=item %Type_Description

In this hash each key represents a category of functions and the value is 
a short description of that category.

=item @Type_Order

This list of categories is used to produce the same order as the
L<perlfunc/"Perl Functions by Category"> section.

=back

=head1 CHANGES

1.02 20020813 <abe@ztreet.demon.nl>
    de-typo in the SYNOPSIS section (thanks Mike Castle for noticing)

1.01 20011229 <abe@ztreet.demon.nl>
    fixed some bugs that slipped in after 5.6.1
    added the pod
    finished making it strict safe

1.00 ??
    first numbered version

=cut

our $VERSION = '1.03';

require Exporter;

our @ISA = qw(Exporter);
our @EXPORT = qw(%Kinds %Type %Flavor %Type_Description @Type_Order);

our(%Kinds, %Type, %Flavor);

our %Type_Description = (
    'ARRAY'	=> 'Functions for real @ARRAYs',
    'Binary'	=> 'Functions for fixed length data or records',
    'File'	=> 'Functions for filehandles, files, or directories',
    'Flow'	=> 'Keywords related to control flow of your perl program',
    'HASH'	=> 'Functions for real %HASHes',
    'I/O'	=> 'Input and output functions',
    'LIST'	=> 'Functions for list data',
    'Math'	=> 'Numeric functions',
    'Misc'	=> 'Miscellaneous functions',
    'Modules'	=> 'Keywords related to perl modules',
    'Network'	=> 'Fetching network info',
    'Objects'	=> 'Keywords related to classes and object-orientedness',
    'Process'	=> 'Functions for processes and process groups',
    'Regexp'	=> 'Regular expressions and pattern matching',
    'Socket'	=> 'Low-level socket functions',
    'String'	=> 'Functions for SCALARs or strings',
    'SysV'	=> 'System V interprocess communication functions',
    'Time'	=> 'Time-related functions',
    'User'	=> 'Fetching user and group info',
    'Namespace'	=> 'Keywords altering or affecting scoping of identifiers',
);

our @Type_Order = qw{
    String
    Regexp
    Math
    ARRAY
    LIST
    HASH
    I/O
    Binary
    File
    Flow
    Namespace
    Misc
    Process
    Modules
    Objects
    Socket
    SysV
    User
    Network
    Time
};

while (<DATA>) {
    chomp;
    s/#.*//;
    next unless $_;
    my($name, $type, $text) = split " ", $_, 3;
    $Type{$name} = $type;
    $Flavor{$name} = $text;
    for my $t ( split /[,\s]+/, $type ) {
        push @{$Kinds{$t}}, $name;
    }
}

close DATA;

my( $typedesc, $list );
unless (caller) { 
    foreach my $type ( @Type_Order ) {
	$list = join(", ", sort @{$Kinds{$type}});
	$typedesc = $Type_Description{$type} . ":";
	write;
    } 
}

format = 

^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    $typedesc 
~~ ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    $typedesc 
 ~~  ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	$list
.

1;

__DATA__
-X	File	a file test (-r, -x, etc)
abs	Math	absolute value function
accept	Socket	accept an incoming socket connect
alarm	Process	schedule a SIGALRM 
atan2	Math	arctangent of Y/X in the range -PI to PI
bind	Socket	binds an address to a socket
binmode	I/O	prepare binary files for I/O
bless	Objects	create an object 
caller	Flow,Namespace	get context of the current subroutine call
chdir	File	change your current working directory
chmod	File	changes the permissions on a list of files
chomp	String 	remove a trailing record separator from a string
chop	String 	remove the last character from a string
chown	File	change the owership on a list of files
chr	String 	get character this number represents
chroot	File	make directory new root for path lookups
close	I/O	close file (or pipe or socket) handle
closedir	I/O	close directory handle
connect	Socket	connect to a remote socket
continue	Flow	optional trailing block in a while or foreach 
cos	Math	cosine function
crypt	String	one-way passwd-style encryption
dbmclose	Objects,I/O	breaks binding on a tied dbm file
dbmopen	Objects,I/O	create binding on a tied dbm file
defined	Misc	test whether a value, variable, or function is defined
delete	HASH	deletes a value from a hash
die	I/O,Flow	raise an exception or bail out
do	Flow,Modules	turn a BLOCK into a TERM
dump	Misc,Flow	create an immediate core dump
each	HASH	retrieve the next key/value pair from a hash
endgrent	User	be done using group file
endhostent	User	be done using hosts file
endnetent	User	be done using networks file
endprotoent	Network	be done using protocols file
endpwent	User	be done using passwd file
endservent	Network	be done using services file
eof	I/O	test a filehandle for its end
eval	Flow,Misc	catch exceptions or compile and run code
exec	Process	abandon this program to run another
exists	HASH	test whether a hash key is present
exit	Flow	terminate this program
exp	Math	raise I<e> to a power
fcntl	File	file control system call
fileno	I/O	return file descriptor from filehandle
flock	I/O	lock an entire file with an advisory lock
fork	Process	create a new process just like this one
format	I/O	declare a picture format with use by the write() function
formline	Misc	internal function used for formats
getc	I/O	get	the next character from the filehandle
getgrent	User	get next group record 
getgrgid	User	get group record given group user ID
getgrnam	User	get group record given group name
gethostbyaddr	Network	get host record given its address
gethostbyname	Network	get host record given name
gethostent	Network	get next hosts record 
getlogin	User	return who logged in at this tty
getnetbyaddr	Network	get network record given its address
getnetbyname	Network	get networks record given name
getnetent	Network	get next networks record 
getpeername	Socket	find the other end of a socket connection
getpgrp	Process	get process group
getppid	Process	get parent process ID
getpriority	Process	get current nice value
getprotobyname	Network	get protocol record given name
getprotobynumber	Network	get protocol record numeric protocol
getprotoent	Network	get next protocols record
getpwent	User	get next passwd record
getpwnam	User	get passwd record given user login name
getpwuid	User	get passwd record given user ID
getservbyname	Network	get services record given its name
getservbyport	Network	get services record given numeric port
getservent	Network	get next services record 
getsockname	Socket	retrieve the sockaddr for a given socket
getsockopt	Socket	get socket options on a given socket
glob	File		expand filenames using wildcards
gmtime	Time	convert UNIX time into record or string using Greenwich time
goto	Flow	create spaghetti code
grep	LIST	locate elements in a list test true against a given criterion
hex	Math,String	convert a string to a hexadecimal number
import	Modules,Namespace	patch a module's namespace into your own
index	String	find a substring within a string
int	Math	get the integer portion of a number
ioctl	File	system-dependent device control system call
join	LIST	join a list into a string using a separator
keys	HASH	retrieve list of indices from a hash
kill	Process	send a signal to a process or process group
last	Flow	exit a block prematurely
lc	String	return lower-case version of a string
lcfirst	String	return a string with just the next letter in lower case
length	String	return the number of bytes in a string
link	File	create a hard link in the filesytem
listen	Socket	register your socket as a server 
local	Misc,Namespace	create a temporary value for a global variable (dynamic scoping)
localtime	Time	convert UNIX time into record or string using local time
lock	Threads	get a thread lock on a variable, subroutine, or method
log	Math	retrieve the natural logarithm for a number
lstat	File	stat a symbolic link
m//	Regexp	match a string with a regular expression pattern
map	LIST	apply a change to a list to get back a new list with the changes
mkdir	File	create a directory
msgctl	SysV	SysV IPC message control operations
msgget	SysV	get SysV IPC message queue
msgrcv	SysV	receive a SysV IPC message from a message queue
msgsnd	SysV	send a SysV IPC message to a message queue
my	Misc,Namespace	declare and assign a local variable (lexical scoping)
next	Flow	iterate a block prematurely
no	Modules	unimport some module symbols or semantics at compile time
package	Modules,Objects,Namespace	declare a separate global namespace
prototype	Flow,Misc	get the prototype (if any) of a subroutine
oct	String,Math	convert a string to an octal number
open	File	open a file, pipe, or descriptor
opendir	File	open a directory
ord	String	find a character's numeric representation
our	Misc,Namespace	declare and assign a package variable (lexical scoping)
pack	Binary,String	convert a list into a binary representation
pipe	Process	open a pair of connected filehandles
pop	ARRAY	remove the last element from an array and return it
pos	Regexp	find or set the offset for the last/next m//g search
print	I/O	output a list to a filehandle
printf	I/O  	output a formatted list to a filehandle
push	ARRAY	append one or more elements to an array
q/STRING/	String	singly quote a string
qq/STRING/	String	doubly quote a string
quotemeta	Regexp	quote regular expression magic characters
qw/STRING/	LIST	quote a list of words
qx/STRING/	Process	backquote quote a string
qr/STRING/	Regexp	Compile pattern 
rand	Math	retrieve the next pseudorandom number 
read	I/O,Binary	fixed-length buffered input from a filehandle
readdir	I/O	get a directory from a directory handle
readline	I/O	fetch a record from a file
readlink	File	determine where a symbolic link is pointing
readpipe	Process	execute a system command and collect standard output
recv	Socket	receive a message over a Socket
redo	Flow	start this loop iteration over again
ref	Objects	find out the type of thing being referenced
rename	File	change a filename
require	Modules	load in external functions from a library at runtime
reset	Misc	clear all variables of a given name
return	Flow	get out of a function early
reverse	String,LIST	flip a string or a list
rewinddir	I/O	reset directory handle
rindex	String	right-to-left substring search
rmdir	File	remove a directory
s///	Regexp	replace a pattern with a string
scalar	Misc	force a scalar context
seek	I/O	reposition file pointer for random-access I/O
seekdir	I/O	reposition directory pointer 
select	I/O	reset default output or do I/O multiplexing
semctl	SysV	SysV semaphore control operations
semget	SysV	get set of SysV semaphores
semop	SysV	SysV semaphore operations
send	Socket	send a message over a socket
setgrent	User	prepare group file for use
sethostent	Network	prepare hosts file for use
setnetent	Network	prepare networks file for use
setpgrp	Process	set the process group of a process
setpriority	Process	set a process's nice value
setprotoent	Network	prepare protocols file for use
setpwent	User	prepare passwd file for use
setservent	Network	prepare services file for use
setsockopt	Socket	set some socket options
shift	ARRAY	remove the first element of an array, and return it
shmctl	SysV	SysV shared memory operations
shmget	SysV	get SysV shared memory segment identifier
shmread	SysV	read SysV shared memory 
shmwrite	SysV	write SysV shared memory 
shutdown	Socket	close down just half of a socket connection
sin	Math	return the sine of a number
sleep	Process	block for some number of seconds
socket	Socket	create a socket
socketpair	Socket	create a pair of sockets
sort	LIST	sort a list of values 
splice	ARRAY	add or remove elements anywhere in an array
split	Regexp	split up a string using a regexp delimiter
sprintf	String	formatted print into a string	
sqrt	Math	square root function
srand	Math	seed the random number generator
stat	File	get a file's status information
study	Regexp	optimize input data for repeated searches
sub	Flow	declare a subroutine, possibly anonymously
substr	String	get or alter a portion of a stirng
symlink	File	create a symbolic link to a file
syscall	I/O,Binary	execute an arbitrary system call
sysopen	File	open a file, pipe, or descriptor
sysread	I/O,Binary	fixed-length unbuffered input from a filehandle
sysseek	I/O,Binary	position I/O pointer on handle used with sysread and syswrite
system	Process	run a separate program 
syswrite	I/O,Binary	fixed-length unbuffered output to a filehandle
tell	I/O	get current seekpointer on a filehandle
telldir	I/O	get current seekpointer on a directory handle
tie	Objects	bind a variable to an object class 
tied	Objects	get a reference to the object underlying a tied variable
time	Time	return number of seconds since 1970
times	Process,Time	return elapsed time for self and child processes
tr///	String	transliterate a string
truncate	I/O	shorten a file
uc	String	return upper-case version of a string
ucfirst	String	return a string with just the next letter in upper case
umask	File	set file creation mode mask
undef	Misc	remove a variable or function definition
unlink	File	remove one link to a file
unpack	Binary,LIST	convert binary structure into normal perl variables
unshift	ARRAY	prepend more elements to the beginning of a list
untie	Objects	break a tie binding to a variable
use	Modules,Namespace	load a module and import its namespace
use 	Objects	load in a module at compile time
utime	File	set a file's last access and modify times
values	HASH	return a list of the values in a hash
vec	Binary	test or set particular bits in a string
wait	Process	wait for any child process to die
waitpid	Process	wait for  a particular child process to die
wantarray	Misc,Flow	get void vs scalar vs list context of current subroutine call
warn	I/O	print debugging info
write	I/O	print a picture record
y///	String	transliterate a string
