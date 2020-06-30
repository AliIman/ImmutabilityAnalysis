add_library(LLVMImmutabilityAnalysis MODULE
  ClassQuery.cpp
  MemQuery.cpp
  ImmutabilityAnalysis.cpp
  FunctionAnalysis.cpp
  Database.cpp
)

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(LLVMImmutabilityAnalysis
  iberty
  pq
  ${llvm_libs}
)
