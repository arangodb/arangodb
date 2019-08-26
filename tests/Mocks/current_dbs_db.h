// How to regenerate: Take an empty cluster, create a new database "db"
// and dump the agency: This is the object under /arango/Current/Databases/db
// The JSON has been reformatted with jq, then par and then spaces have
// been removed. The idea is to have relatively few lines and bytes.
R"=(
{"PRMR-ab4cdcec-bae6-4998-af25-a93c0b4a3ada":{"name":"db",
"errorNum":0,"id":"2010605","error":false,"errorMessage":
""},"PRMR-8cb161bd-aa92-4d02-a0c3-9e48096e18a0":{"name":"db",
"errorNum":0,"id":"10414","error":false,"errorMessage":""},
"PRMR-f80f6cd4-e2e6-4fbd-b297-87d1064bf15c":{"name":"db","errorNum":0,"id":
"4010530","error":false,"errorMessage":""}}
)="
