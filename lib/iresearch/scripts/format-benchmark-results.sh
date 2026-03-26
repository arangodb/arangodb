#!/bin/bash -x
############################################################################
# Format performance result output
############################################################################

cd build
mkdir stats

for i in 1 5 10 15 20 25; do
    MAX_LINES=${i}000000
    OUT_SUFFIX="$(printf '%02dM' ${i})"

    # overall search/index logs
    for t in index search; do
        # overall time
        echo '<?xml version="1.0" encoding="UTF-8"?>' > stats/"$t.${OUT_SUFFIX}.time(s).xml"
        echo '<testsuite>' >> stats/"$t.${OUT_SUFFIX}.time(s).xml"

        cat <(cat bin/iresearch.stderr.${MAX_LINES}.$t.log.* | grep 'Elapsed (wall clock) time' | sed -e "s/^/IResearch $t /") \
            <(cat bin/lucene.stderr.${MAX_LINES}.$t.log.* | grep 'Elapsed (wall clock) time' | sed -e "s/^/Lucene $t /") \
        | perl -e '
            my $r="^(.*): ((([0-9]*):)?([0-9]*):)?([0-9]*.[0-9]*)\$";
            my %m;
            while(<STDIN>) {
                chomp;
                if (!/$r/) {
                    next;
                }
                ($empty, $name, $empty, $empty, $hour, $min, $sec) = split(/$r/);
                $m{$name}{"count"} += 1;
                $m{$name}{"time"} += ((($hour * 60) + $min) * 60) + $sec;
            }
            for my $name (sort(keys %m)) {
                printf(
                    "\t<testcase classname=\"\" name=\"%s\" time=\"%s\"/>\n",
                    $name, $m{$name}{"time"}/$m{$name}{"count"}
                )
            }
        ' >> stats/"$t.${OUT_SUFFIX}.time(s).xml"

        echo '</testsuite>' >> stats/"$t.${OUT_SUFFIX}.time(s).xml"

        # overall memmory
        echo '<?xml version="1.0" encoding="UTF-8"?>' > stats/"$t.${OUT_SUFFIX}.memory(KiB).xml"
        echo '<testsuite>' >> stats/"$t.${OUT_SUFFIX}.memory(KiB).xml"

        cat <(cat bin/iresearch.stderr.${MAX_LINES}.$t.log.* | grep 'Maximum resident set size' | sed -e "s/^/IResearch $t /") \
            <(cat bin/lucene.stderr.${MAX_LINES}.$t.log.* | grep 'Maximum resident set size' | sed -e "s/^/Lucene $t /") \
        | perl -e '
            my $r="^(.*): ([0-9]*)\$";
            my %m;
            while(<STDIN>) {
                chomp;
                if (!/$r/) {
                    next;
                }
                ($empty, $name, $bytes) = split(/$r/);
                $m{$name}{"count"} += 1;
                $m{$name}{"bytes"} += $bytes;
            }
            for my $name (sort(keys %m)) {
                printf(
                    "\t<testcase classname=\"\" name=\"%s\" time=\"%s\"/>\n",
                    $name, $m{$name}{"bytes"}/$m{$name}{"count"}
                )
            }
        ' >> stats/"$t.${OUT_SUFFIX}.memory(KiB).xml"
  
        echo '</testsuite>' >> stats/"$t.${OUT_SUFFIX}.memory(KiB).xml"
    done

    # individual query logs (call count)
    {
        echo '<?xml version="1.0" encoding="UTF-8"?>' > stats/"query.${OUT_SUFFIX}.calls(count).xml"
        echo '<testsuite>' >> stats/"query.${OUT_SUFFIX}.calls(count).xml"

        cat <(cat bin/iresearch.stdout.${MAX_LINES}.search.log.* | grep 'Query execution' | grep -v '<unknown>' | sed -e 's/^/IResearch /') \
            <(cat bin/lucene.stdlog.${MAX_LINES}.search.log.* | grep 'Query execution') \
        | perl -e '
            my $r="^(.*\\)) [^:]*:([^,]*), [^:]*: ([^ ]*) [^,]*, [^:]*: ([^ ]*) .*\$";
            my %m;
            while(<STDIN>) {
                chomp;
                if (!/$r/) {
                    next;
                }
                ($empty, $name, $calls, $time, $avg) = split(/$r/);
                $m{$name}{"count"} += 1;
                $m{$name}{"calls"} += $calls;
                $m{$name}{"time"} += $time;
                $m{$name}{"avg"} += $avg;
            }
            for my $name (sort(keys %m)) {
                printf(
                    "\t<testcase classname=\"\" name=\"calls %1\$s\" time=\"%2\$s\"/>\n",
                    $name, $m{$name}{"calls"}/$m{$name}{"count"}, $m{$name}{"time"}/$m{$name}{"count"}, $m{$name}{"avg"}/$m{$name}{"count"}
                );
            }
        ' >> stats/"query.${OUT_SUFFIX}.calls(count).xml"
  
        echo '</testsuite>' >> stats/"query.${OUT_SUFFIX}.calls(count).xml"
    }

    # individual query logs (total time)
    {
        echo '<?xml version="1.0" encoding="UTF-8"?>' > stats/"query.${OUT_SUFFIX}.total(sec).xml"
        echo '<testsuite>' >> stats/"query.${OUT_SUFFIX}.total(sec).xml"

        cat <(cat bin/iresearch.stdout.${MAX_LINES}.search.log.* | grep 'Query execution' | grep -v '<unknown>' | sed -e 's/^/IResearch /') \
            <(cat bin/lucene.stdlog.${MAX_LINES}.search.log.*) \
        | perl -e '
            my $r="^(.*\\)) [^:]*:([^,]*), [^:]*: ([^ ]*) [^,]*, [^:]*: ([^ ]*) .*\$";
            my %m;
            while(<STDIN>) {
                chomp;
                if (!/$r/) {
                    next;
                }
                ($empty, $name, $calls, $time, $avg) = split(/$r/);
                $m{$name}{"count"} += 1;
                $m{$name}{"calls"} += $calls;
                $m{$name}{"time"} += $time;
                $m{$name}{"avg"} += $avg;
            }
            for my $name (sort(keys %m)) {
                printf(
                    "\t<testcase classname=\"\" name=\"time %1\$s\" time=\"%3\$s\"/>\n",
                    $name, $m{$name}{"calls"}/$m{$name}{"count"}, $m{$name}{"time"}/$m{$name}{"count"}, $m{$name}{"avg"}/$m{$name}{"count"}
                );
            }
        ' >> stats/"query.${OUT_SUFFIX}.total(sec).xml"
  
        echo '</testsuite>' >> stats/"query.${OUT_SUFFIX}.total(sec).xml"
    }

    # individual query logs (average time)
    {
        echo '<?xml version="1.0" encoding="UTF-8"?>' > stats/"query.${OUT_SUFFIX}.average(sec).xml"
        echo '<testsuite>' >> stats/"query.${OUT_SUFFIX}.average(sec).xml"

        cat <(cat bin/iresearch.stdout.${MAX_LINES}.search.log.* | grep 'Query execution' | grep -v '<unknown>' | sed -e 's/^/IResearch /') \
            <(cat bin/lucene.stdlog.${MAX_LINES}.search.log.*) \
        | perl -e '
            my $r="^(.*\\)) [^:]*:([^,]*), [^:]*: ([^ ]*) [^,]*, [^:]*: ([^ ]*) .*\$";
            my %m;
            while(<STDIN>) {
                chomp;
                if (!/$r/) {
                    next;
                }
                ($empty, $name, $calls, $time, $avg) = split(/$r/);
                $m{$name}{"count"} += 1;
                $m{$name}{"calls"} += $calls;
                $m{$name}{"time"} += $time;
                $m{$name}{"avg"} += $avg;
            }
            for my $name (sort(keys %m)) {
                printf(
                    "\t<testcase classname=\"\" name=\"avg time %1\$s\" time=\"%4\$s\"/>\n",
                    $name, $m{$name}{"calls"}/$m{$name}{"count"}, $m{$name}{"time"}/$m{$name}{"count"}, $m{$name}{"avg"}/$m{$name}{"count"}
                );
            }
        ' >> stats/"query.${OUT_SUFFIX}.average(sec).xml"
  
        echo '</testsuite>' >> stats/"query.${OUT_SUFFIX}.average(sec).xml"
    }
done
