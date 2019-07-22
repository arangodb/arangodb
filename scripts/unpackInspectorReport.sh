#!/usr/bin/env bash

filename=arango-inspector.json
outdir=arango-inspector

if [[ $# > 1 ]]; then
    echo "**Error** - usage: unpackInspectorReport [filename]"
    exit 1
elif [[ $# == 1 ]]; then
    filename=$1
fi

if [ -f $filename ]; then

    # check json validity
    if jq -e . >/dev/null 2>&1 <<<"$json_string"; then
        mkdir arango-inspector
        if [[ $? -ne 0 ]]; then #target directory exists
            echo "**Error** - failed to create directory structure"
            exit 1
        fi

        #dump agency
        echo -n "  writing agency dump ... "
        cat $filename | jq .agency | tee $outdir/agency.json > /dev/null
        echo done
        
        #dump
        echo -n "  writing agency analysis ... "
        cat $filename | jq .analysis | tee $outdir/agency-analysis.json > /dev/null
        echo done
        
        #servers
        echo "  writing servers ..."  
        for i in $(cat $filename | jq .servers | jq 'keys[]'); do
            name=$(echo $i|sed s/\"//g)
            mkdir $outdir/$name
            echo -n "    writing $i ... "
            for j in $(cat $filename | jq .servers[$i] | jq 'keys[]'); do
                what=$(echo $j|sed s/\"//g)
                cat $filename | jq -r .servers[$i][$j] | tee $outdir/$name/$what > /dev/null
            done     
            echo done
        done
        echo "  ... done "  
        
    else #invalid json
        echo "**Error** - failed to parse JSON, or got false/null"
    fi
else
    echo "**Error** - file $filename does not exit"
fi

echo "The report was unpacked successfully."
