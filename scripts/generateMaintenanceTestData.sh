#!/bin/bash
#
# Generate test data for maintenance unit tests
#

# collections for system database
echo "== Creating collection _system/bar =="
curl [::]:8530/_api/collection -sd \
     '{"name":"bar", "type": 3, "numberOfShards": 9, "replicationFactor": 2}' \
    | jq -c
curl [::]:8530/_api/index?collection=bar -sd \
     '{"fields":["gi"],"geoJson":false,"sparse":true,"type":"geo","unique":false}' \
    |jq -c
curl [::]:8530/_api/index?collection=bar -sd \
     '{"fields":["gij"],"geoJson":true,"sparse":true,"type":"geo","unique":false}'\
    |jq -c
curl [::]:8530/_api/index?collection=bar -sd \
     '{"deduplicate":false,"fields":["hi","_key"],"sparse":false,"type":"hash","unique":true}'|jq -c
curl [::]:8530/_api/index?collection=bar -sd \
     '{"deduplicate":false,"fields":["pi"],"sparse":true,"type":"persistent","unique":false}'|jq -c
curl [::]:8530/_api/index?collection=bar -sd \
     '{"fields":["fi"],"id":"2010132","minLength":3,"sparse":true,"type":"fulltext","unique":false}'|jq -c
curl [::]:8530/_api/index?collection=bar -sd \
     '{"deduplicate":true,"fields":["sli"],"sparse":false,"type":"skiplist","unique":false}'

echo "== Creating collection _system/baz =="
curl [::]:8530/_api/collection -sd \
     '{"name":"baz", "type": 2, "numberOfShards": 1, "replicationFactor": 2}' | jq -c
# create foo database
echo "== Creating database foo =="
curl [::]:8530/_api/database -sd '{"name":"foo"}' | jq -c 
echo "== Creating collection foo/foobar =="
curl [::]:8530/_db/foo/_api/collection -sd \
     '{"name":"foobar", "type": 3, "numberOfShards": 4, "replicationFactor": 3}' \
    | jq -c 
echo "== Creating collection foo/foobaz =="
curl [::]:8530/_db/foo/_api/collection -sd \
     '{"name":"foobaz", "type": 2, "numberOfShards": 6, "replicationFactor": 2}' \
    | jq -c



header="R\"=("
footer=")=\""

outfile=Plan.json
echo $header > $outfile
curl [::]:4001/_api/agency/read -sLd'[["/arango/Plan"]]'|jq .[0].arango.Plan >> $outfile
echo $footer >> $outfile

outfile=Current.json
echo $header > $outfile
curl [::]:4001/_api/agency/read -sLd'[["/arango/Current"]]'|jq .[0].arango.Current >> $outfile
echo $footer >> $outfile

outfile=Supervision.json
echo $header > $outfile
supervision=$(curl [::]:4001/_api/agency/read -sLd'[["/arango/Supervision"]]'|jq .[0].arango.Supervision)
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
