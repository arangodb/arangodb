package Devel::SelfStubber;
use File::Spec;
require SelfLoader;
@ISA = qw(SelfLoader);
@EXPORT = 'AUTOLOAD';
$JUST_STUBS = 1;
$VERSION = 1.03;
sub Version {$VERSION}

# Use as
# perl -e 'use Devel::SelfStubber;Devel::SelfStubber->stub(MODULE_NAME,LIB)'
# (LIB defaults to '.') e.g.
# perl -e 'use Devel::SelfStubber;Devel::SelfStubber->stub('Math::BigInt')'
# would print out stubs needed if you added a __DATA__ before the subs.
# Setting $Devel::SelfStubber::JUST_STUBS to 0 will print out the whole
# module with the stubs entered just before the __DATA__

sub _add_to_cache {
    my($self,$fullname,$pack,$lines, $prototype) = @_;
    push(@DATA,@{$lines});
    if($fullname){push(@STUBS,"sub $fullname $prototype;\n")}; # stubs
    '1;';
}

sub _package_defined {
    my($self,$line) = @_;
    push(@DATA,$line);
}

sub stub {
    my($self,$module,$lib) = @_;
    my($line,$end_data,$fh,$mod_file,$found_selfloader);
    $lib ||= File::Spec->curdir();
    ($mod_file = $module) =~ s,::,/,g;
    $mod_file =~ tr|/|:| if $^O eq 'MacOS';
    
    $mod_file = File::Spec->catfile($lib, "$mod_file.pm");
    $fh = "${module}::DATA";
    my (@BEFORE_DATA, @AFTER_DATA, @AFTER_END);
    @DATA = @STUBS = ();

    open($fh,$mod_file) || die "Unable to open $mod_file";
    local $/ = "\n";
    while(defined ($line = <$fh>) and $line !~ m/^__DATA__/) {
	push(@BEFORE_DATA,$line);
	$line =~ /use\s+SelfLoader/ && $found_selfloader++;
    }
    (defined ($line) && $line =~ m/^__DATA__/)
      || die "$mod_file doesn't contain a __DATA__ token";
    $found_selfloader || 
	print 'die "\'use SelfLoader;\' statement NOT FOUND!!\n"',"\n";
    if ($JUST_STUBS) {
        $self->_load_stubs($module);
    } else {
        $self->_load_stubs($module, \@AFTER_END);
    }
    if ( fileno($fh) ) {
	$end_data = 1;
	while(defined($line = <$fh>)) {
	    push(@AFTER_DATA,$line);
	}
    }
    close($fh);
    unless ($JUST_STUBS) {
    	print @BEFORE_DATA;
    }
    print @STUBS;
    unless ($JUST_STUBS) {
    	print "1;\n__DATA__\n",@DATA;
    	if($end_data) { print "__END__ DATA\n",@AFTER_DATA; }
    	if(@AFTER_END) { print "__END__\n",@AFTER_END; }
    }
}

1;
__END__

=head1 NAME

Devel::SelfStubber - generate stubs for a SelfLoading module

=head1 SYNOPSIS

To generate just the stubs:

    use Devel::SelfStubber;
    Devel::SelfStubber->stub('MODULENAME','MY_LIB_DIR');

or to generate the whole module with stubs inserted correctly

    use Devel::SelfStubber;
    $Devel::SelfStubber::JUST_STUBS=0;
    Devel::SelfStubber->stub('MODULENAME','MY_LIB_DIR');

MODULENAME is the Perl module name, e.g. Devel::SelfStubber,
NOT 'Devel/SelfStubber' or 'Devel/SelfStubber.pm'.

MY_LIB_DIR defaults to '.' if not present.

=head1 DESCRIPTION

Devel::SelfStubber prints the stubs you need to put in the module
before the __DATA__ token (or you can get it to print the entire
module with stubs correctly placed). The stubs ensure that if
a method is called, it will get loaded. They are needed specifically
for inherited autoloaded methods.

This is best explained using the following example:

Assume four classes, A,B,C & D.

A is the root class, B is a subclass of A, C is a subclass of B,
and D is another subclass of A.

                        A
                       / \
                      B   D
                     /
                    C

If D calls an autoloaded method 'foo' which is defined in class A,
then the method is loaded into class A, then executed. If C then
calls method 'foo', and that method was reimplemented in class
B, but set to be autoloaded, then the lookup mechanism never gets to
the AUTOLOAD mechanism in B because it first finds the method
already loaded in A, and so erroneously uses that. If the method
foo had been stubbed in B, then the lookup mechanism would have
found the stub, and correctly loaded and used the sub from B.

So, for classes and subclasses to have inheritance correctly
work with autoloading, you need to ensure stubs are loaded.

The SelfLoader can load stubs automatically at module initialization
with the statement 'SelfLoader-E<gt>load_stubs()';, but you may wish to
avoid having the stub loading overhead associated with your
initialization (though note that the SelfLoader::load_stubs method
will be called sooner or later - at latest when the first sub
is being autoloaded). In this case, you can put the sub stubs
before the __DATA__ token. This can be done manually, but this
module allows automatic generation of the stubs.

By default it just prints the stubs, but you can set the
global $Devel::SelfStubber::JUST_STUBS to 0 and it will
print out the entire module with the stubs positioned correctly.

At the very least, this is useful to see what the SelfLoader
thinks are stubs - in order to ensure future versions of the
SelfStubber remain in step with the SelfLoader, the
SelfStubber actually uses the SelfLoader to determine which
stubs are needed.

=cut
