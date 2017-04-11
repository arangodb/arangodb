

@brief maximum number of V8 contexts for executing JavaScript actions
`--javascript.v8-contexts number`

Specifies the maximum *number* of V8 contexts that are created for executing
JavaScript code. More contexts allow executing more JavaScript actions in
parallel, provided that there are also enough threads available. Please
note that each V8 context will use a substantial amount of memory and
requires periodic CPU processing time for garbage collection.

