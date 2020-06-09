#include "Database.h"

#include <sstream>
#include <unordered_set>
#include <unordered_map>

#include <postgresql/libpq-fe.h>
#include <arpa/inet.h>

#include <clang/AST/CXXInheritance.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/ADT/StringMap.h>
#include <llvm/Support/Mutex.h>

using namespace llvm;
using namespace clang;
using namespace clang::tooling;

namespace {

PGconn *Connection;

class Params {
  std::vector<const char *> Values;
  std::list<uint32_t> BinaryValues; // Required for stable iterators
  std::vector<int> Lengths;
  std::vector<int> Formats;
public:
  void addText(const char *Text) {
    Values.push_back(Text);
    Lengths.push_back(0); // ignored for text, used for binary
    Formats.push_back(0); // 0 is text, 1 is binary
  }
  void addBinary(uint32_t Binary) {
    BinaryValues.push_back(htonl(Binary));
    auto &BinaryValue = BinaryValues.back();
    const char *Value = (const char *) &BinaryValue;

    Values.push_back(Value);
    Lengths.push_back(sizeof(BinaryValue));
    Formats.push_back(1); // 1 is binary
  }
  void addBool(bool B) {
    if (B)
      addText("true");
    else
      addText("false");
  }
  void clear() {
    Values.clear();
    BinaryValues.clear();
    Lengths.clear();
    Formats.clear();
  }

  const char * const * getValues() const {
    return Values.data();
  }
  const int * getLengths() const {
    return Lengths.data();
  }
  const int * getFormats() const {
    return Formats.data();
  }
  int getN() const {
    assert(Values.size() == Lengths.size());
    assert(Values.size() == Formats.size());
    return Values.size();
  }

  void dump() const {
    auto iBinary = BinaryValues.begin();
    for (size_t i = 0; i < getN(); ++i) {
      llvm::errs() << "    ." << i << " = ";
      if (Formats[i] == 0) {
        llvm::errs() << Values[i];
      }
      else {
        llvm::errs() << ntohl(*iBinary);
        ++iBinary;
      }
      llvm::errs() << '\n';
    }
  }
};

class Result {
protected:
  PGresult *PGResult;
public:
  Result(const char *Q, const Params &P) : PGResult(nullptr) {
    PGResult = PQexecParams(Connection, Q, P.getN(), nullptr,
                            P.getValues(), P.getLengths(), P.getFormats(), 1);
    assert(PGResult != nullptr);
  }
  ~Result() {
    PQclear(PGResult);
  }

  Result(const Result &) = delete;
  Result operator=(const Result &) = delete;
};

class TupleResult : public Result {
public:
  TupleResult(const char *Q, const Params &P) : Result(Q, P) {
    auto Status = PQresultStatus(PGResult);
    if (Status != PGRES_TUPLES_OK) {
      errs() << "TupleResult: " << PQresultErrorMessage(PGResult);
      errs() << "Query: " << Q << '\n';
      P.dump();
    }
    assert(Status == PGRES_TUPLES_OK && "TupleResult failed");
  }

  TupleResult(const TupleResult &) = delete;
  TupleResult operator=(const TupleResult &) = delete;

  int getNumTuples() {
    return PQntuples(PGResult);
  }
  uint32_t getID() {
    assert(getNumTuples() == 1);
    int FieldIndex = PQfnumber(PGResult, "id");
    char *Value = PQgetvalue(PGResult, 0, FieldIndex);
    uint32_t ID = ntohl(*((uint32_t *) Value));
    return ID;
  }
  uint32_t getID(const char *FieldName) {
    assert(getNumTuples() == 1);
    int FieldIndex = PQfnumber(PGResult, FieldName);
    char *Value = PQgetvalue(PGResult, 0, FieldIndex);
    uint32_t ID = ntohl(*((uint32_t *) Value));
    return ID;
  }
  const char *getValue(const char *FieldName) {
    assert(getNumTuples() == 1);
    int FieldIndex = PQfnumber(PGResult, FieldName);
    char *Value = PQgetvalue(PGResult, 0, FieldIndex);
    return Value;
  }
  uint32_t getBinary() {
    assert(getNumTuples() == 1);
    assert(PQnfields(PGResult) == 1);
    char *Value = PQgetvalue(PGResult, 0, 0);
    uint32_t Binary = ntohl(*((uint32_t *) Value));
    return Binary;
  }
  std::vector<uint32_t> getIDs() {
    std::vector<uint32_t> IDs;
    int FieldIndex = PQfnumber(PGResult, "id");
    for (int i = 0; i < getNumTuples(); ++i) {
      char *Value = PQgetvalue(PGResult, i, FieldIndex);
      uint32_t ID = ntohl(*((uint32_t *) Value));
      IDs.push_back(ID);
    }
    return IDs;
  }
  std::vector<uint32_t> getIDs(const char *FieldName) {
    std::vector<uint32_t> IDs;
    int FieldIndex = PQfnumber(PGResult, FieldName);
    for (int i = 0; i < getNumTuples(); ++i) {
      char *Value = PQgetvalue(PGResult, i, FieldIndex);
      uint32_t ID = ntohl(*((uint32_t *) Value));
      IDs.push_back(ID);
    }
    return IDs;
  }
};

class CommandResult : public Result {
public:
  CommandResult(const char *Q, const Params &P) : Result(Q, P) {
    if (PQresultStatus(PGResult) != PGRES_COMMAND_OK) {
      errs() << "CommandResult: " << PQresultErrorMessage(PGResult);
    }
    assert(PQresultStatus(PGResult) == PGRES_COMMAND_OK);
  }

  CommandResult(const CommandResult &) = delete;
  CommandResult operator=(const CommandResult &) = delete;
};



// uint32_t getCompileCommandID(const CompileCommand &CC) {
//   llvm_unreachable("TODO: Compile Command");

//   uint32_t DirectoryID = getFileDescriptorID(CC.Directory);
//   uint32_t FileID = getFileDescriptorID(CC.Filename);
//   assert(!(DirectoryID == 0 || FileID == 0));

//   bool First = true;
//   std::stringstream SS;
//   SS << '{';
//   for (const auto &Arg : CC.CommandLine) {
//     if (First) {
//       First = false;
//     }
//     else {
//       SS << ", ";
//     }
//     SS << '"' << Arg << '"';
//   }
//   SS << '}';
//   auto CommandLine = SS.str();

//   Params P;
//   P.addBinary(PackageID);
//   P.addBinary(DirectoryID);
//   P.addBinary(FileID);
//   P.addText(CommandLine.c_str());
//   Result = PQexecParams(Connection,
//     "INSERT INTO cpp_doc_compile_command"
//     "(package_id, directory_id, file_id, command_line)"
//     " VALUES ($1, $2, $3, $4) ON CONFLICT DO NOTHING",
//     P.getN(), nullptr, P.getValues(), P.getLengths(), P.getFormats(), 1);
//   assert(PQresultStatus(Result) == PGRES_COMMAND_OK);

//   PQclear(Result);

//   Result = PQexecParams(Connection,
//     "SELECT id FROM cpp_doc_compile_command"
//     " WHERE package_id = $1 AND directory_id = $2"
//     " AND file_id = $3 AND command_line = $4",
//     P.getN(), nullptr, P.getValues(), P.getLengths(), P.getFormats(), 1);
//   assert(PQresultStatus(Result) == PGRES_TUPLES_OK);
//   assert(PQntuples(Result) == 1);

//   int FieldIndex = PQfnumber(Result, "id");
//   char *Value = PQgetvalue(Result, 0, FieldIndex);
//   uint32_t CompileCommandID = ntohl(*((uint32_t *) Value));

//   PQclear(Result);
//   P.clear();

//   return CompileCommandID;
// }

}

namespace llvm {
namespace immutability {
namespace database {

unsigned CurRecordDeclID;

void setup() {
  Connection = PQconnectdb("dbname = cpp_doc");
  assert(PQstatus(Connection) == CONNECTION_OK);
}


  /*
uint32_t getDeclID(const Decl *D) {
  if (isa<TranslationUnitDecl>(D)) {
    return RootDeclID;
  }

  if (auto RD = dyn_cast<RecordDecl>(D)) {
    if (RecordIDCache.count(RD)) {
      return RecordIDCache[RD];
    }
  }
  else if (auto NSD = dyn_cast<NamespaceDecl>(D)) {
    if (NamespaceIDCache.count(NSD)) {
      return NamespaceIDCache[NSD];
    }
  }
  else if (auto MD = dyn_cast<CXXMethodDecl>(D)) {
    if (MethodIDCache.count(MD)) {
      return MethodIDCache[MD];
    }
  }
  // Note: Method is a subclass of Function, so it needs to come after Method
  else if (auto FD = dyn_cast<FunctionDecl>(D)) {
    if (FunctionIDCache.count(FD)) {
      return FunctionIDCache[FD];
    }
  }

  const DeclContext *DC = D->getDeclContext();

  if (isa<LinkageSpecDecl>(D)) {
    // If this is a linkage spec, ignore it by using the containing decl context
    return getDeclID(cast<Decl>(DC));
  }

  uint32_t ParentID = getDeclID(cast<Decl>(DC));
  std::string Name;
  std::string Path;

  if (auto FD = dyn_cast<FunctionDecl>(D)) {
    Name = getSignature(FD->getCanonicalDecl(), false);
    Path = getSignature(FD->getCanonicalDecl(), true);
  }
  else {
    Name = cast<NamedDecl>(D)->getNameAsString();
    Path = cast<NamedDecl>(D)->getQualifiedNameAsString();
  }

  uint32_t DeclID;

  Params P;
  P.addBinary(PackageID);
  P.addBinary(ParentID);
  P.addText(Name.c_str());
  P.addText(Path.c_str());
  if (auto MD = dyn_cast<CXXMethodDecl>(D)) {
    uint32_t PresumedLocID = 0;
    if (MD->isDefined()) {
      // It can be pure and defined if it's a comment
      // assert (!MD->isPure() && "This should never happen");
      MD = cast<CXXMethodDecl>(MD->getDefinition());
      PresumedLocID = getPresumedLocID(MD);
    }
    else if (MD->isPure()) {
      assert (!MD->isDefined() && "This should never happen");
      PresumedLocID = getPresumedLocID(MD);
    }

    if (PresumedLocID != 0) {
      P.addBinary(PresumedLocID);
      TupleResult Select("SELECT get_decl($1, $2, $3, $4, $5)", P);
      DeclID = Select.getBinary();
    }
    else {
      TupleResult Select("SELECT get_decl($1, $2, $3, $4)", P);
      DeclID = Select.getBinary();
    }
  }
  else if (auto FD = dyn_cast<FieldDecl>(D)) {
    uint32_t PresumedLocID = 0;
    PresumedLocID = getPresumedLocID(FD);
    if (PresumedLocID != 0) {
      P.addBinary(PresumedLocID);
      TupleResult Select("SELECT get_decl($1, $2, $3, $4, $5)", P);
      DeclID = Select.getBinary();
    }
    else {
      TupleResult Select("SELECT get_decl($1, $2, $3, $4)", P);
      DeclID = Select.getBinary();
    }
  }
  else {
    TupleResult Select("SELECT get_decl($1, $2, $3, $4)", P);
    DeclID = Select.getBinary();
  }

  P.clear();
  P.addBinary(DeclID);
  if (auto RD = dyn_cast<RecordDecl>(D)) {
    if (auto CRD = dyn_cast<CXXRecordDecl>(RD)) {
      CRD = CRD->getCanonicalDecl();

      if (!CRD->hasDefinition()) {
        return DeclID;
      }

      P.addBool(CRD->isAbstract());
      P.addBool(CRD->hasAnyDependentBases());
      TupleResult Select("SELECT get_record_decl($1, $2, $3)", P);
      // Only cache the record if it has a defintion, otherwise we'll miss
      // information
      RecordIDCache[RD] = DeclID;
    }
  }
  else if (auto NSD = dyn_cast<NamespaceDecl>(D)) {
    TupleResult Select("SELECT get_namespace_decl($1)", P);
    NamespaceIDCache[NSD] = DeclID;
  }
  else if (auto FD = dyn_cast<FieldDecl>(D)) {
    P.addBool(FD->isMutable());
    P.addBinary(FD->getAccess());
    TupleResult Select("SELECT get_field_decl($1, $2, $3)", P);
    FieldIDCache[FD] = DeclID;
  }
  else if (auto MD = dyn_cast<CXXMethodDecl>(D)) {
    std::string MangledName = getMangledName(MD);
    P.addText(MangledName.c_str());
    P.addBool(MD->isConst());
    P.addBool(MD->isPure());
    P.addBinary(MD->getAccess());
    TupleResult Select("SELECT get_method_decl($1, $2, $3, $4, $5)", P);
    MethodIDCache[MD] = DeclID;
    // for (const CXXMethodDecl *SuperMethod : MD->overridden_methods()) {
    //     errs() << "Override: " << Signature << '\n';
    //   P.clear();
    //   uint32_t SuperMethodID = getDeclID(SuperMethod);
    //   P.addBinary(DeclID);
    //   P.addBinary(SuperMethodID);
    //   TupleResult Select("SELECT get_override($1, $2)", P);
    // }
  }
  // Note: Method is a subclass of Function, so it needs to come after Method
  else if (auto FD = dyn_cast<FunctionDecl>(D)) {
    TupleResult Select("SELECT get_function_decl($1)", P);
    FunctionIDCache[FD] = DeclID;
  }

  return DeclID;
}

uint32_t getPresumedLocID(const Decl *D) {
  PresumedLoc PLoc = sourceManager->getPresumedLoc(D->getLocation());
  uint32_t PresumedLocID = getPresumedLocIDPLoc(PLoc);
  return PresumedLocID;
}

void insertMethod(const CXXMethodDecl *MD) {
  getDeclID(MD);
}

void insertPublicMethod(const CXXRecordDecl *RD, const CXXMethodDecl *MD) {
  Params P;
  P.addBinary(getDeclID(RD));
  P.addBinary(getDeclID(MD));
  TupleResult Select("SELECT get_public_view($1, $2)", P);
}

void insertPublicField(const CXXRecordDecl *RD, const FieldDecl *FD) {
  Params P;
  P.addBinary(getDeclID(RD));
  P.addBinary(getDeclID(FD));
  TupleResult Select("SELECT get_public_view($1, $2)", P);
}

void insertMethodCheck(const CXXMethodDecl *MD, MethodResult Result) {
  uint32_t MethodDeclID = getDeclID(MD);

  Params P;
  P.addBinary(MethodDeclID);
  P.addBinary(static_cast<uint32_t>(Result.mutateResult));
  P.addBinary(static_cast<uint32_t>(Result.returnResult));

  TupleResult Select("SELECT get_clang_immutability_check_method($1, $2, $3)", P);
}

void insertMethodCheckNoAssumption(const CXXMethodDecl *MD, MethodResult Result) {
  uint32_t MethodDeclID = getDeclID(MD);

  Params P;
  P.addBinary(MethodDeclID);
  P.addBinary(static_cast<uint32_t>(Result.mutateResult));
  P.addBinary(static_cast<uint32_t>(Result.returnResult));

  TupleResult Select("SELECT get_clang_immutability_check_method_no_assumption($1, $2, $3)", P);
}

void insertFieldCheck(const FieldDecl *FD, bool isExplicit, bool isTransitive) {
    assert(FD);
  uint32_t FieldDeclID = getDeclID(FD);
  Params P;
  P.addBinary(FieldDeclID);
  if (isExplicit) {
    P.addText("true");
  }
  else {
    P.addText("false");
  }
  if (isTransitive) {
    P.addText("true");
  }
  else {
    P.addText("false");
  }
  TupleResult Select("SELECT get_clang_immutability_check_field($1, $2, $3)", P);
}
  */

namespace {
  unsigned LastPackageID;
  sys::SmartMutex<false> Mutex;

  unsigned getMethod(StringRef MangledName) {
    Params P;
    std::string S = MangledName.str();
    P.addText(S.c_str());
    P.addBinary(LastPackageID);
    TupleResult R("SELECT cpp_doc_method_decl.decl_id FROM cpp_doc_method_decl INNER JOIN cpp_doc_decl ON (cpp_doc_method_decl.decl_id = cpp_doc_decl.id) WHERE (cpp_doc_method_decl.mangled_name = $1 AND cpp_doc_decl.package_id = $2)", P);
    return R.getID("decl_id");
  }


}

std::vector<unsigned> getRecordDeclIDs(unsigned PackageID) {
  LastPackageID = PackageID;

  Params P;
  P.addBinary(PackageID);
  TupleResult R("SELECT cpp_doc_decl.id, cpp_doc_decl.package_id, cpp_doc_decl.path, cpp_doc_decl.name, cpp_doc_decl.parent_id, cpp_doc_decl.presumed_loc_id FROM cpp_doc_decl INNER JOIN cpp_doc_record_decl ON (cpp_doc_decl.id = cpp_doc_record_decl.decl_id) WHERE (cpp_doc_decl.package_id = $1 AND cpp_doc_record_decl.decl_id IS NOT NULL) ORDER BY cpp_doc_decl.path ASC", P);
  return R.getIDs();
}

  std::vector<unsigned> getPublicMethodDeclIDs(unsigned RecordDeclID) {
    Params P;
    P.addBinary(RecordDeclID);
    TupleResult R(
                  "SELECT cpp_doc_public_view.id, cpp_doc_public_view.record_id, cpp_doc_public_view.decl_id FROM cpp_doc_public_view INNER JOIN cpp_doc_decl T4 ON (cpp_doc_public_view.decl_id = T4.id) INNER JOIN cpp_doc_method_decl ON (T4.id = cpp_doc_method_decl.decl_id) WHERE (cpp_doc_public_view.record_id = $1 AND cpp_doc_method_decl.decl_id IS NOT NULL AND cpp_doc_method_decl.is_const = True)"
                  , P);
    return R.getIDs("decl_id");
  }

  std::string getRecordName(unsigned RecordDeclID) {
    Params P;
    P.addBinary(RecordDeclID);
    TupleResult R("SELECT name from cpp_doc_decl WHERE id = $1", P);
    return std::string(R.getValue("name"));
  }

  std::string getMethodName(unsigned MethodDeclID) {
    Params P;
    P.addBinary(MethodDeclID);
    TupleResult R("SELECT name from cpp_doc_decl WHERE id = $1", P);
    return std::string(R.getValue("name"));
  }
  std::string getMangledName(unsigned MethodDeclID) {
    Params P;
    P.addBinary(MethodDeclID);
    TupleResult R("SELECT mangled_name from cpp_doc_method_decl WHERE decl_id = $1", P);
    return std::string(R.getValue("mangled_name"));
  }

std::vector<Entry> getPublicMethods(unsigned PackageID) {
  std::vector<Entry> Entries;
  std::vector<unsigned> RecordDeclIDs = getRecordDeclIDs(PackageID);
  for (unsigned RecordDeclID : RecordDeclIDs) {
    auto MethodDeclIDs = getPublicMethodDeclIDs(RecordDeclID);
    if (MethodDeclIDs.empty())
      continue;

    Entry E;
    E.ID = RecordDeclID;
    E.Name = getRecordName(RecordDeclID);
    for (auto MethodDeclID : MethodDeclIDs) {
      MethodEntry ME;
      ME.ID = MethodDeclID;
      ME.Name = getMethodName(MethodDeclID);
      ME.MangledName = getMangledName(MethodDeclID);
      E.Methods.push_back(ME);
    }

    Entries.push_back(E);
  }
  return Entries;
}

void addIssue(StringRef MangledName, std::string Description) {
  Mutex.lock();
  Params P;
  P.addBinary(CurRecordDeclID);
  unsigned CurMethodDeclID = getMethod(MangledName);
  P.addBinary(CurMethodDeclID);
  P.addText(Description.c_str());
  CommandResult R("INSERT INTO cpp_doc_llvm_immutability_issue (record_id, method_id, description) VALUES ($1, $2, $3) ON CONFLICT DO NOTHING", P);
  Mutex.unlock();
}

void finish() {
  PQfinish(Connection);
}

}
}
}
