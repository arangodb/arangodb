#!/bin/bash
header="R\"=("
footer=")=\""

outfile=Plan.json
echo $header > $outfile
curl -s [::]:4001/_api/agency/read -d'[["/arango/Plan"]]'|jq .[0].arango.Plan >> $outfile
echo $footer >> $outfile

outfile=Current.json
echo $header > $outfile
curl -s [::]:4001/_api/agency/read -d'[["/arango/Current"]]'|jq .[0].arango.Current >> $outfile
echo $footer >> $outfile

outfile=Supervision.json
echo $header > $outfile
supervision=$(curl -s [::]:4001/_api/agency/read -d'[["/arango/Supervision"]]'|jq .[0].arango.Supervision)
echo $supervision | jq .>> $outfile
echo $footer >> $outfile

servers=$(echo $supervision | jq .Health| jq 'keys[]')
for i in $servers; do
    shortname=$(echo $supervision | jq -r .Health[$i].ShortName)
    endpoint=$(echo $supervision | jq -r .Health[$i].Endpoint)
    endpoint=$(echo $endpoint |sed s/tcp/http/g)
                          
    dbs=$(curl -s $endpoint/_api/database|jq -r .result|jq -r '.[]')

    tmpfile=$shortname.tmp
    outfile=$shortname.json
    echo "{" >> $tmpfile
    j=0
    for i in $dbs; do
        if [ $j -gt 0 ]; then
            echo -n "," >> $tmpfile
        fi
        echo -n "\"$i\" :" >> $tmpfile
        curl -s $endpoint/_db/$i/_admin/execute?returnAsJSON=true -d 'return require("@arangodb/cluster").getLocalInfo()'|jq .result >> $tmpfile
        (( j++ ))
    done
    echo "}" >> $tmpfile
    echo "R\"=(" > $outfile
    cat $tmpfile | jq . >> $outfile
    echo ")=\"" >> $outfile
    rm $tmpfile
done
