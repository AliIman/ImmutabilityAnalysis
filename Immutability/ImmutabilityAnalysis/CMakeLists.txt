add_library(LLVMImmutabilityAnalysis MODULE
  MemQuery.cpp
  ImmutabilityAnalysis.cpp
  FunctionAnalysis.cpp
  Database.cpp
)

target_link_libraries(LLVMImmutabilityAnalysis
  iberty
  pq
  LLVMSupport
)
