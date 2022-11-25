add_library(optimizer2 STATIC Optimizer2.cpp PlanNodeTypes/PostFilter.h)

target_include_directories(optimizer2 PUBLIC .)
