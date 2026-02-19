add_subdirectory(Rule)

target_sources(arangoserver PRIVATE
  Feature.cpp
  OptionsProvider.cpp
  Utils.cpp)
