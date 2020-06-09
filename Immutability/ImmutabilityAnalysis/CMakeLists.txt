add_library(LLVMImmutabilityAnalysis MODULE
  ImmutabilityAnalysis.cpp

)

target_link_libraries(LLVMImmutabilityAnalysis
  iberty
  pq
  LLVMSupport
)
