# This subroutine returns true if its argument is tainted, false otherwise.

sub tainted {
    local($@);
    eval { kill 0 * $_[0] };
    $@ =~ /^Insecure/;
}

1;
