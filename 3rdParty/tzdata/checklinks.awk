# Check links in tz tables.

# Contributed by Paul Eggert.  This file is in the public domain.

BEGIN {
    # Special marker indicating that the name is defined as a Zone.
    # It is a newline so that it cannot match a valid name.
    # It is not null so that its slot does not appear unset.
    Zone = "\n"
}

/^Z/ {
    if (defined[$2]) {
	if (defined[$2] == Zone) {
	    printf "%s: Zone has duplicate definition\n", $2
	} else {
	    printf "%s: Link with same name as Zone\n", $2
	}
	status = 1
    }
    defined[$2] = Zone
}

/^L/ {
    if (defined[$3]) {
	if (defined[$3] == Zone) {
	    printf "%s: Link with same name as Zone\n", $3
	} else if (defined[$3] == $2) {
	    printf "%s: Link has duplicate definition\n", $3
	} else {
	    printf "%s: Link to both %s and %s\n", $3, defined[$3], $2
	}
	status = 1
    }
    if (backcheck && FILENAME != backcheck && $3 != "GMT") {
      printf "%s: Link should be in '%s'\n", $3, backcheck
      status = 1
    }
    if ($4 == "#=") {
      shortcut[$5] = $3
    }
    used[$2] = 1
    defined[$3] = $2
}

END {
    for (tz in used) {
	if (defined[tz] != Zone) {
	  if (!defined[tz]) {
	    printf "%s: Link to nowhere\n", tz
	    status = 1
	  } else if (DATAFORM != "vanguard") {
	    printf "%s: Link to link\n", tz
	    status = 1
	  }
	}
    }
    for (tz in shortcut) {
      if (defined[shortcut[tz]] != defined[tz]) {
	target = (!defined[tz] ? "absence" \
		  : defined[tz] == "\n" ? "zone" \
		  : defined[tz])
	printf "%s: target %s disagrees with %s's target %s\n", \
	  tz, target, shortcut[tz], defined[shortcut[tz]]
	status = 1
      }
    }

    exit status
}
