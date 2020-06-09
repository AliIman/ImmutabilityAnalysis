add_library(LLVMImmutabilityAnalysis MODULE
  MemQuery.cpp
  ImmutabilityAnalysis.cpp

)

target_link_libraries(LLVMImmutabilityAnalysis
  iberty
  pq
  LLVMSupport
)
