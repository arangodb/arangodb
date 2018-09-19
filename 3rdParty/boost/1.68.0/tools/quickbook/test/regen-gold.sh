#==============================================================================
#   Copyright (c) 2017-2018 Daniel James
#
#   Use, modification and distribution is subject to the Boost Software
#   License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt)
#==============================================================================

#!/usr/bin/env bash

cd $(dirname $0)

cd ..
b2 -q || exit 1
cd -

find . -name "*.gold" | while read gold_file
do
    flags="--debug"

    quickbook_file=$(echo $gold_file | sed 's/[.]gold/.quickbook/')
    html_file=$(echo $gold_file | sed 's/[.]gold/.gold-html/')

    if [[ $gold_file =~ .*xinclude/.*-alt\.gold ]]
    then
        quickbook_file=$(echo $quickbook_file | sed 's/-alt\.quickbook/.quickbook/')
        flags="$flags --xinclude-base xinclude/sub"
    elif [[ $gold_file =~ .*xinclude/.*\.gold ]]
    then
        flags="$flags --xinclude-base xinclude/../.."
    fi

    if [[ $gold_file =~ .*include/filename[-_]path.* ]]
    then
        flags="$flags -I include/sub"
    fi

    if [[ $gold_file =~ .*command_line_macro.* ]]
    then
        flags="$flags -D__macro__=*bold* -D__empty__"
    fi

    ../../../dist/bin/quickbook $quickbook_file $flags --output-file $gold_file ||
        echo "*** Error generating boostbook from $quickbook_file"
    ../../../dist/bin/quickbook $quickbook_file $flags --output-file $html_file --output-format onehtml ||
        echo "*** Error generating html from $quickbook_file"
done
