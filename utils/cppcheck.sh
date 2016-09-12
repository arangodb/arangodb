#!/bin/bash
rm -f cppcheck.log cppcheck.tmp
touch cppcheck.tmp

cppcheck -j4 \
  --std=c++11 \
  --enable=style,warning \
  --force \
  --quiet \
  --platform=unix64 \
  --inline-suppr \
  --suppress="*:lib/V8/v8-json.cpp" \
  --suppress="*:lib/Zip/crypt.h" \
  --suppress="*:lib/Zip/iowin32.cpp" \
  --suppress="*:lib/Zip/unzip.cpp" \
  --suppress="*:lib/Zip/zip.cpp" \
  --suppress="*:Aql/grammar.cpp" \
  --suppress="*:Aql/tokens.cpp" \
  --suppress="*:Aql/tokens.ll" \
  arangod/ arangosh/ lib/ 2>> cppcheck.tmp

sort cppcheck.tmp | uniq > cppcheck.log

#lines below will be ignored
comm -23 cppcheck.log <(sort << EOF
[arangod/Aql/AqlValue.h:586]: (performance) Variable 'materialized' is assigned in constructor body. Consider performing initialization in initialization list.
[arangod/Aql/BasicBlocks.cpp:193]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/BasicBlocks.cpp:302]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/BasicBlocks.cpp:329]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/BasicBlocks.cpp:341]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/BasicBlocks.cpp:427]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/BasicBlocks.cpp:483]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/BasicBlocks.cpp:498]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/BasicBlocks.cpp:506]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/BasicBlocks.cpp:65]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/CalculationBlock.cpp:204]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:1041]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:1116]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:113]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:1173]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:1202]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:1301]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:1336]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:1397]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:1423]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:1450]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:147]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:162]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:177]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:206]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:311]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:382]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:403]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:474]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:513]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:529]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:547]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:566]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:581]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:604]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:631]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:696]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:749]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:764]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:785]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:85]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:876]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ClusterBlocks.cpp:924]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/EnumerateCollectionBlock.cpp:106]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/EnumerateCollectionBlock.cpp:115]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/EnumerateCollectionBlock.cpp:132]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/EnumerateCollectionBlock.cpp:221]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/EnumerateCollectionBlock.cpp:281]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/EnumerateCollectionBlock.cpp:74]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ExecutionPlan.cpp:1981]: (error) syntax error
[arangod/Aql/IndexBlock.cpp:204]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/IndexBlock.cpp:307]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/IndexBlock.cpp:412]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/IndexBlock.cpp:428]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/IndexBlock.cpp:526]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/IndexBlock.cpp:586]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ShortestPathBlock.cpp:445]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/ShortestPathBlock.cpp:632]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/SortBlock.cpp:65]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/SubqueryBlock.cpp:116]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/TraversalBlock.cpp:152]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/TraversalBlock.cpp:191]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/TraversalBlock.cpp:239]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/TraversalBlock.cpp:251]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/TraversalBlock.cpp:401]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Aql/TraversalBlock.cpp:445]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Cluster/ClusterInfo.cpp:891]: (style) Variable 'DBServers' is assigned a value that is never used.
[arangod/GeoIndex/GeoIndex.cpp:1057]: (style) The scope of the variable 'pot' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1122]: (style) The scope of the variable 'pot' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1190]: (style) The scope of the variable 'newslotct' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1191]: (style) The scope of the variable 'x' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1191]: (style) The scope of the variable 'y' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1441]: (style) The scope of the variable 'js' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1441]: (style) The scope of the variable 'pot2' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1442]: (style) The scope of the variable 'potx' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1443]: (style) The scope of the variable 'lv1' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1443]: (style) The scope of the variable 'lvx' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1455]: (style) The scope of the variable 'gs' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1455]: (style) The scope of the variable 'mings' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1667]: (style) The scope of the variable 'lev' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1667]: (style) The scope of the variable 'levb' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1667]: (style) The scope of the variable 'levp' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1679]: (style) The scope of the variable 'j' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1679]: (style) The scope of the variable 'js' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1680]: (style) The scope of the variable 'gs' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:1680]: (style) The scope of the variable 'mings' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:2170]: (style) The scope of the variable 'gc' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:2209]: (style) The scope of the variable 'pota' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:2209]: (style) The scope of the variable 'potb' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:2210]: (style) The scope of the variable 'lev' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:2210]: (style) The scope of the variable 'leva' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:306]: (style) The scope of the variable 'newpotct' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:307]: (style) The scope of the variable 'x' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:307]: (style) The scope of the variable 'y' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:363]: (style) The scope of the variable 'lat' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:363]: (style) The scope of the variable 'lon' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:363]: (style) The scope of the variable 'x' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:363]: (style) The scope of the variable 'y' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:363]: (style) The scope of the variable 'z' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:592]: (style) The scope of the variable 'nz' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:655]: (style) The scope of the variable 'snmd' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:655]: (style) The scope of the variable 'xx1' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:655]: (style) The scope of the variable 'yy1' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:655]: (style) The scope of the variable 'z1' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:845]: (style) The scope of the variable 'jj1' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:845]: (style) The scope of the variable 'jj2' can be reduced.
[arangod/GeoIndex/GeoIndex.cpp:937]: (style) The scope of the variable 'slot' can be reduced.
[arangod/Replication/InitialSyncer.cpp:1109]: (warning) Return value of function memcmp() is not used.
[arangod/Scheduler/ListenTask.cpp:212]: (style) Variable 'buf' is assigned a value that is never used.
[arangod/Scheduler/ListenTask.cpp:223]: (style) Variable 'buf' is assigned a value that is never used.
[arangod/Scheduler/ListenTask.cpp:238]: (style) Variable 'buf' is assigned a value that is never used.
[arangod/Scheduler/ListenTask.cpp:249]: (style) Variable 'buf' is assigned a value that is never used.
[arangod/Scheduler/SchedulerLibev.cpp:117]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:142]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:167]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:327]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:335]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:343]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:351]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:375]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:430]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:444]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:470]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:481]: (style) C-style pointer casting
[arangod/Scheduler/SchedulerLibev.cpp:73]: (style) C-style pointer casting
[arangod/V8Server/v8-collection.cpp:1302]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/V8Server/v8-collection.cpp:2673]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/V8Server/v8-collection.cpp:2694]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/V8Server/v8-collection.cpp:802]: (style) Variable 'isolate' is assigned a value that is never used.
[arangod/V8Server/v8-collection.cpp:881]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/V8Server/v8-query.cpp:403]: (warning) Return value of function memcmp() is not used.
[arangod/V8Server/v8-query.cpp:404]: (warning) Return value of function memcmp() is not used.
[arangod/V8Server/v8-query.cpp:437]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/V8Server/v8-query.cpp:447]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/V8Server/v8-query.cpp:457]: (style) Statements following return, break, continue, goto or throw will never be executed.
[arangod/Wal/LogfileManager.h:535]: (style) Unused variable: _activeTransactions
[arangod/Wal/LogfileManager.h:538]: (style) Unused variable: _failedTransactions
[lib/Basics/VelocyPackHelper.cpp:87]: (error) Analysis failed. If the code is valid then please report this failure.
[lib/Basics/process-utils.cpp:104]: (style) struct or union member 'process_state_s::rsslim' is never used.
[lib/Basics/process-utils.cpp:105]: (style) struct or union member 'process_state_s::startcode' is never used.
[lib/Basics/process-utils.cpp:106]: (style) struct or union member 'process_state_s::endcode' is never used.
[lib/Basics/process-utils.cpp:107]: (style) struct or union member 'process_state_s::startstack' is never used.
[lib/Basics/process-utils.cpp:108]: (style) struct or union member 'process_state_s::kstkesp' is never used.
[lib/Basics/process-utils.cpp:109]: (style) struct or union member 'process_state_s::signal' is never used.
[lib/Basics/process-utils.cpp:111]: (style) struct or union member 'process_state_s::blocked' is never used.
[lib/Basics/process-utils.cpp:112]: (style) struct or union member 'process_state_s::sigignore' is never used.
[lib/Basics/process-utils.cpp:113]: (style) struct or union member 'process_state_s::sigcatch' is never used.
[lib/Basics/process-utils.cpp:114]: (style) struct or union member 'process_state_s::wchan' is never used.
[lib/Basics/process-utils.cpp:116]: (style) struct or union member 'process_state_s::nswap' is never used.
[lib/Basics/process-utils.cpp:117]: (style) struct or union member 'process_state_s::cnswap' is never used.
[lib/Basics/process-utils.cpp:119]: (style) struct or union member 'process_state_s::exit_signal' is never used.
[lib/Basics/process-utils.cpp:120]: (style) struct or union member 'process_state_s::processor' is never used.
[lib/Basics/process-utils.cpp:122]: (style) struct or union member 'process_state_s::rt_priority' is never used.
[lib/Basics/process-utils.cpp:123]: (style) struct or union member 'process_state_s::policy' is never used.
[lib/Basics/process-utils.cpp:125]: (style) struct or union member 'process_state_s::delayacct_blkio_ticks' is never used.
[lib/Basics/process-utils.cpp:127]: (style) struct or union member 'process_state_s::guest_time' is never used.
[lib/Basics/process-utils.cpp:129]: (style) struct or union member 'process_state_s::cguest_time' is never used.
[lib/Basics/process-utils.cpp:702]: (portability) scanf without field width limits can crash with huge input data on some versions of libc.
[lib/Basics/process-utils.cpp:702]: (warning) scanf without field width limits can crash with huge input data.
[lib/Basics/tri-zip.cpp:49]: (style) The scope of the variable 'fout' can be reduced.
[lib/V8/v8-buffer.cpp:1065] -> [lib/V8/v8-buffer.cpp:1088]: (warning) Possible null pointer dereference: source - otherwise it is redundant to check it against null.
[lib/V8/v8-buffer.cpp:1080] -> [lib/V8/v8-buffer.cpp:1088]: (warning) Possible null pointer dereference: source - otherwise it is redundant to check it against null.
[lib/V8/v8-buffer.cpp:1084] -> [lib/V8/v8-buffer.cpp:1088]: (warning) Possible null pointer dereference: source - otherwise it is redundant to check it against null.
[lib/V8/v8-buffer.cpp:1306]: (style) The scope of the variable 'a' can be reduced.
[lib/V8/v8-buffer.cpp:1306]: (style) The scope of the variable 'b' can be reduced.
[lib/V8/v8-buffer.cpp:1306]: (style) The scope of the variable 'c' can be reduced.
[lib/V8/v8-buffer.cpp:1306]: (style) The scope of the variable 'd' can be reduced.
[lib/V8/v8-buffer.cpp:185]: (style) Variable 'isolate' is assigned a value that is never used.
[lib/V8/v8-buffer.cpp:475]: (style) The scope of the variable 't' can be reduced.
[lib/V8/v8-buffer.cpp:965]: (style) The scope of the variable 'c' can be reduced.
[lib/V8/v8-shell.cpp:55]: (style) Variable 'isolate' is assigned a value that is never used.
[lib/Zip/crypt.h:97]: (style) The scope of the variable 'c' can be reduced.
[lib/Zip/iowin32.cpp:103]: (style) Variable 'mode_fopen' is assigned a value that is never used.
[lib/Zip/iowin32.cpp:120]: (style) Variable 'mode_fopen' is assigned a value that is never used.
[lib/Zip/iowin32.cpp:137]: (style) Variable 'mode_fopen' is assigned a value that is never used.
[lib/Zip/iowin32.cpp:86]: (style) Variable 'mode_fopen' is assigned a value that is never used.
[lib/Zip/unzip.cpp:1036]: (style) Variable 'lSeek' is assigned a value that is never used.
[lib/Zip/unzip.cpp:1384]: (style) The scope of the variable 'source' can be reduced.
[lib/Zip/unzip.cpp:1555] -> [lib/Zip/unzip.cpp:1554]: (warning) Possible null pointer dereference: s - otherwise it is redundant to check it against null.
[lib/Zip/zip.cpp:1047] -> [lib/Zip/zip.cpp:1049]: (performance) Variable 'err' is reassigned a value before the old one has been used.
[lib/Zip/zip.cpp:1049] -> [lib/Zip/zip.cpp:1052]: (performance) Variable 'err' is reassigned a value before the old one has been used.
[lib/Zip/zip.cpp:1052] -> [lib/Zip/zip.cpp:1054]: (performance) Variable 'err' is reassigned a value before the old one has been used.
[lib/Zip/zip.cpp:1956]: (style) The scope of the variable 'header' can be reduced.
[lib/Zip/zip.cpp:1956]: (style) The scope of the variable 'header' can be reduced.
[lib/Zip/zip.cpp:1957]: (style) The scope of the variable 'dataSize' can be reduced.
[lib/Zip/zip.cpp:1957]: (style) The scope of the variable 'dataSize' can be reduced.
EOF
)

rm cppcheck.tmp
