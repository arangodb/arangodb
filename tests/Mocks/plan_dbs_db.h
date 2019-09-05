// How to regenerate: Take an empty cluster, create a new database "db"
// and dump the agency: This is the object under /arango/Plan/Databases/db
// The JSON has been reformatted with jq, then par and then spaces have
// been removed. The idea is to have relatively few lines and bytes.
R"=(
{"options":{},"name":"db","id":"10068"}
)="
