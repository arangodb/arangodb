add_subdirectory(Rule)

target_sources(arangoserver PRIVATE
  Feature.cpp
  Optimizer.cpp
  OptionsProvider.cpp
  Rules.cpp
  Utils.cpp)
