target_sources(arango_vocbase PRIVATE
  Collections.cpp
  Databases.cpp
  Indexes.cpp
  Queries.cpp
  Ttl.cpp
  Upgrade.cpp
  UpgradeTasks.cpp
  Version.cpp)

if (USE_V8) 
  target_sources(arango_vocbase PRIVATE
    AqlUserFunctions.cpp
    Tasks.cpp
    Transactions.cpp)
endif()
