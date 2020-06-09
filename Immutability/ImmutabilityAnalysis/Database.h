#ifndef CLANG_IMMUTABILITY_CHECK_DATABASE_H
#define CLANG_IMMUTABILITY_CHECK_DATABASE_H

#include <clang/AST/DeclCXX.h>
#include <clang/Tooling/CompilationDatabase.h>

namespace llvm {
namespace immutability {
namespace database {

struct MethodEntry {
  unsigned ID;
  std::string Name;
  std::string MangledName;
};

struct Entry {
  unsigned ID;
  std::string Name;
  std::vector<MethodEntry> Methods;
};

extern unsigned CurRecordDeclID;

void setup();
std::vector<Entry> getPublicMethods(unsigned PackageID);
void addIssue(StringRef MangeledName, std::string Description);
void finish();

}
}
}

#endif
