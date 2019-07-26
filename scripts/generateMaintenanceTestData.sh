#!/usr/bin/env bash
#
# Generate test data for maintenance unit tests
#

# check if ports are available 
lsof -i -P -n| awk '{print $9}' | grep -E "6568|11097|11098|11196|11197|1198" > /dev/null
if [ $? -ne 1 ]; then
    echo "One or more of ports 6568, 11097, 11098, 11196, 11197, 1198 are blocked for startLocalCluster"
    exit 1;
fi

# start cluster for it all
scripts/startLocalCluster.sh -d 3 -c 2 -b 2567

# collections for system database
echo "== Creating collection _system/bar =="
curl [::1]:11097/_api/collection -sd \
     '{"name":"bar", "type": 3, "numberOfShards": 9, "replicationFactor": 2}' \
    | jq -c
curl [::1]:11097/_api/index?collection=bar -sd \
     '{"fields":["gi"],"geoJson":false,"sparse":true,"type":"geo","unique":false}' \
    |jq -c
curl [::1]:11097/_api/index?collection=bar -sd \
     '{"fields":["gij"],"geoJson":true,"sparse":true,"type":"geo","unique":false}'\
    |jq -c
curl [::1]:11097/_api/index?collection=bar -sd \
     '{"deduplicate":false,"fields":["hi","_key"],"sparse":false,"type":"hash","unique":true}'|jq -c
curl [::1]:11097/_api/index?collection=bar -sd \
     '{"deduplicate":false,"fields":["pi"],"sparse":true,"type":"persistent","unique":false}'|jq -c
curl [::1]:11097/_api/index?collection=bar -sd \
     '{"fields":["fi"],"id":"2010132","minLength":3,"sparse":true,"type":"fulltext","unique":false}'|jq -c
curl [::1]:11097/_api/index?collection=bar -sd \
     '{"deduplicate":true,"fields":["sli"],"sparse":false,"type":"skiplist","unique":false}'

echo "== Creating collection _system/baz =="
curl [::1]:11097/_api/collection -sd \
     '{"name":"baz", "type": 2, "numberOfShards": 1, "replicationFactor": 2}' | jq -c
# create foo database
echo "== Creating database foo =="
curl [::1]:11097/_api/database -sd '{"name":"foo"}' | jq -c 
echo "== Creating collection foo/foobar =="
curl [::1]:11097/_db/foo/_api/collection -sd \
     '{"name":"foobar", "type": 3, "numberOfShards": 4, "replicationFactor": 3}' \
    | jq -c 
echo "== Creating collection foo/foobaz =="
curl [::1]:11097/_db/foo/_api/collection -sd \
     '{"name":"foobaz", "type": 2, "numberOfShards": 6, "replicationFactor": 2}' \
    | jq -c

header="R\"=("
footer=")=\""

outfile=Plan.json
echo $header > $outfile
curl [::1]:6568/_api/agency/read -sLd'[["/arango/Plan"]]'|jq .[0].arango.Plan >> $outfile
echo $footer >> $outfile

outfile=Current.json
echo $header > $outfile
curl [::1]:6568/_api/agency/read -sLd'[["/arango/Current"]]'|jq .[0].arango.Current >> $outfile
echo $footer >> $outfile

outfile=Supervision.json
echo $header > $outfile
supervision=$(curl [::1]:6568/_api/agency/read -sLd'[["/arango/Supervision"]]'|jq .[0].arango.Supervision)
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
    curl -s $endpoint/_admin/actions?details=true|jq .state >> $tmpfile
    echo "R\"=(" > $outfile
    cat $tmpfile | jq . >> $outfile
    echo ")=\"" >> $outfile
    rm $tmpfile
done

# shutdown the cluster
scripts/shutdownLocalCluster.sh -d 3 -c 2 -b 2567
