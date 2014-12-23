# NOTE: Derived from ../../lib/DynaLoader.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package DynaLoader;

#line 320 "../../lib/DynaLoader.pm (autosplit into ../../lib/auto/DynaLoader/dl_expandspec.al)"
sub dl_expandspec {
    my($spec) = @_;
    # Optional function invoked if DynaLoader.pm sets $do_expand.
    # Most systems do not require or use this function.
    # Some systems may implement it in the dl_*.xs file in which case
    # this autoload version will not be called but is harmless.

    # This function is designed to deal with systems which treat some
    # 'filenames' in a special way. For example VMS 'Logical Names'
    # (something like unix environment variables - but different).
    # This function should recognise such names and expand them into
    # full file paths.
    # Must return undef if $spec is invalid or file does not exist.

    my $file = $spec; # default output to input

    
	return undef unless -f $file;
    
    print STDERR "dl_expandspec($spec) => $file\n" if $dl_debug;
    $file;
}

# end of DynaLoader::dl_expandspec
1;
