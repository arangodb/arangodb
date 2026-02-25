add_subdirectory(Rule)
add_subdirectory(Utils)

target_sources(arangoserver PRIVATE
  Feature.cpp
  Optimizer.cpp
  OptionsProvider.cpp
  Rules.cpp
  Utils.cpp)
