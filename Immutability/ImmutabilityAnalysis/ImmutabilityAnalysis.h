#ifndef LLVM_ANALYSIS_IMMUTABILITY
#define LLVM_ANALYSIS_IMMUTABILITY

#include "Database.h"
#include "Query.h"
#include "FunctionUtil.h"
#include "Graph.h"

#include <llvm/IR/Dominators.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Mutex.h>
#include <llvm/Support/ThreadPool.h>

namespace llvm {
namespace immutability {

class ImmutabilityAnalysis : public ModulePass {
private:
  ThreadPool Pool;
  sys::SmartMutex<false> Mutex;
  SmallVector<std::shared_future<void>, 16> Futures;

  bool allFuturesInvalid() {
    for (unsigned i = 0; i < Futures.size(); ++i) {
      if (Futures[i].valid()) {
        return false;
      }
    }
    return true;
  }
  void checkFutures() {
    std::future_status Status;
    for (unsigned i = 0; i < Futures.size(); ++i) {
      if (Futures[i].valid()) {
        Status = Futures[i].wait_for(std::chrono::seconds(0));
        if (Status == std::future_status::ready) {
          Futures[i] = std::shared_future<void>();
        }
      }
    }
  }
  size_t getFutureAvailable() {
    while(true) {
      checkFutures();
      for (unsigned i = 0; i < Futures.size(); ++i) {
        if (!Futures[i].valid()) {
          return i;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
  bool incompleteMethodInitialStatesEmpty() {
    bool Ret;
    Mutex.lock();
    Ret = IncompleteMethodInitialStates.empty();
    Mutex.unlock();
    return Ret;
  }
public:
  static char ID;
  unsigned IterationNum;

  std::unique_ptr<Query> Q;

  const StructType *CurrentType;

  typedef ClassQuery::FunctionSet FunctionSet;
  typedef DenseMap<const Function *, std::vector<GraphPtr>> MethodStates;
  MethodStates IncompleteMethodInitialStates;
  MethodStates CompleteMethodInitialStates;

  ImmutabilityAnalysis() : ModulePass(ID) {
    Futures.resize(16);
    errs() << ":: ImmutabilityAnalysis - Constructor\n";
  }

  const StructType *getCurrentType() {
    return CurrentType;
  }

  void iteration(std::string &ClassName, const FunctionSet &Methods);

  bool runOnModule(Module &M) override;
/*   { */
/*       bool DEBUG = false; */
/*     const char *PackageIDEnv = getenv("IMMUTABILITY_PACKAGE_ID"); */
/*     assert(PackageIDEnv != nullptr); */
/*     unsigned PackageID; */
/*     bool Err = StringRef(PackageIDEnv).getAsInteger(10, PackageID); */
/*     assert(!Err); */

/*     Q = make_unique<Query>(getAnalysis<ClassQuery>(), */
/*                            getAnalysis<MemQuery>()); */

/*     database::setup(); */
/*     unsigned NumClasses = 0; */
/*     auto Entries = database::getPublicMethods(PackageID); */
/*     for (auto &Entry : Entries) { */
/*       database::CurRecordDeclID = Entry.ID; */

/*       SmallPtrSet<const Function *, 16> PublicConstMethods; */
/*       bool HasUnknownMethod = false; */
/*       for (database::MethodEntry &ME : Entry.Methods) { */
/*         const Function *F = M.getFunction(ME.MangledName); */
/*         /\* if (ME.MangledName != "_ZNK8Sequence7PolySNP7ThetaPiEv") continue; // TODO: DELETE *\/ */
/*         /\* if (ME.MangledName != "_ZNK8Sequence7PolySNP15StochasticVarPiEv") continue; // TODO: DELETE *\/ */
/*         /\* if (ME.MangledName != "_ZNK8Sequence7PolySNP9FuLiDStarEv" *\/ */
/*         /\*     && ME.MangledName != "_ZNK8Sequence7PolySNP7ThetaPiEv") continue; // TODO: DELETE *\/ */
/*         /\* if (ME.MangledName != "_ZNK8Sequence7PolySNP6HprimeERKb") continue; // TODO: DELETE *\/ */
/*         /\* if (ME.MangledName != "_ZNK12TestThisWeak4getXEv") continue; // TODO: DELETE *\/ */
/*         /\* if (ME.MangledName != "_ZNK8CacheBad8getValueEv" *\/ */
/*         /\*     && ME.MangledName != "_ZNK8CacheBad9unrelatedEv") continue; *\/ */
/*         /\* if (ME.MangledName != "_ZNK8CacheBad8getValueEv") continue; *\/ */
/*         /\* if (ME.MangledName != "_ZNK8Sequence7PolySNP6WallsQEv") continue; *\/ */
/*         /\* if (!(ME.MangledName == "_ZNK8Sequence7PolySNP6DandVHEv" *\/ */
/*         /\*       || ME.MangledName == "_ZNK8Sequence7PolySNP14DisequilibriumERKjRKd")) continue; *\/ */

/*         if (F) { */
/*           if (F->empty()) { */
/*             HasUnknownMethod = true; */
/*             break; */
/*           } */
/*           if (F->getName().contains("DebugString")) { */
/*             continue; */
/*           } */

/*           if (F->getName() == "_ZNK6Parser6Action15act_on_terminalEPN8Terminal8EmulatorE") { */
/*             continue; */
/*           } */
/* /\* */
/*           if (!F->getName().contains("Wall") */
/*               && !F->getName().contains("Walsdfasdls")) { */
/*             //continue; */
/*           } */
/* *\/ */
/* /\* */
/*           if (!F->getName().contains("ThetaPi") */
/*               && !F->getName().contains("StochasticVarPi") */
/*               && !F->getName().contains("SamplingVarPi")) { */
/*             continue; */
/*           } */
/* *\/ */
/*           PublicConstMethods.insert(F); */
/*         } */
/*       } */
/*       if (PublicConstMethods.empty()) */
/*         continue; */
/* /\* */
/*       if (PublicConstMethods.size() >= 32) */
/*         continue; */
/* *\/ */
/*       if (HasUnknownMethod) */
/*         continue; */

/* /\* */
/*       // fish */
/*       if (Entry.Name == "env_vars_snapshot_t") */
/*         continue; */
/*       if (Entry.Name == "pager_t") */
/*         continue; */
/*       if (Entry.Name == "parser_t") */
/*         continue; */
/*       if (Entry.Name == "complete_entry_opt") */
/*         continue; */
/*       if (Entry.Name == "env_universal_t") */
/*         continue; */
/*       // mosh */
/*       if (Entry.Name == "Base64Key") */
/*         continue; */
/*       if (Entry.Name == "UserStream") */
/*         continue; */
/*       if (Entry.Name == "Clear") */
/*         continue; */
/*       if (Entry.Name == "Collect") */
/*         continue; */
/*       if (Entry.Name == "CSI_Dispatch") */
/*         continue; */
/*       if (Entry.Name == "Esc_Dispatch") */
/*         continue; */
/*       if (Entry.Name == "Hook") */
/*         continue; */
/*       if (Entry.Name == "Put") */
/*         continue; */
/*       if (Entry.Name == "Unhook") */
/*         continue; */
/*       if (Entry.Name == "Execute") */
/*         continue; */
/*       if (Entry.Name == "OSC_Start") */
/*         continue; */
/*       if (Entry.Name == "OSC_Put") */
/*         continue; */
/*       if (Entry.Name == "OSC_End") */
/*         continue; */
/*       if (Entry.Name == "UserByte") */
/*         continue; */
/*       if (Entry.Name == "Resize") */
/*         continue; */
/*       if (Entry.Name == "Ignore") */
/*         continue; */
/*       if (Entry.Name == "Param") */
/*         continue; */
/*       if (Entry.Name == "Print") */
/*         continue; */
/*       if (Entry.Name == "Cell") */
/*         continue; */
/*       if (Entry.Name == "Complete") */
/*         continue; */
/*       if (Entry.Name == "Display") */
/*         continue; */
/*       if (Entry.Name == "NotificationEngine") */
/*         continue; */
/*       if (Entry.Name == "PredictionEngine") */
/*         continue; */
/*       // libsequence */
/*       if (Entry.Name == "Comeron95") */
/*         continue; */
/*       if (Entry.Name == "FST") */
/*         continue; */
/*       if (Entry.Name == "GranthamWeights2") */
/*         continue; */
/*       if (Entry.Name == "GranthamWeights3") */
/*         continue; */
/*       if (Entry.Name == "PolySIM") */
/*         continue; */
/*       if (Entry.Name == "PolySites") */
/*         continue; */
/*       // protobuf */
/*       if (Entry.Name == "Message") */
/*         continue; */
/*       if (Entry.Name == "Any") */
/*         continue; */
/*       if (Entry.Name == "Api") */
/*         continue; */
/*       if (Entry.Name == "BoolValue") */
/*         continue; */
/*       //if (Entry.Name == "BytesValue") */
/*       //  continue; */
/*       if (Entry.Name == "AccessInfo") */
/*         continue; */
/*       if (Entry.Name == "FieldIndexSorter") */
/*         continue; */
/*       if (Entry.Name == "RepeatedPtrFieldMessageAccessor" */
/*           || Entry.Name == "RepeatedPtrFieldStringAccessor") */
/*         continue; */
/* if(Entry.Name != "BytesValue") continue; */
/* *\/ */

/*       /\* if (Entry.Name != "PolySNP") *\/ */
/*       /\*  continue; *\/ */

/*       /\* */
/*       if (Entry.Name == "Kimura80") */
/*           DEBUG=true; */
/*       if (!DEBUG) */
/*        continue; */
/*       if (Entry.Name == "PolySIM") */
/*           continue; */
/*       *\/ */

/*       /\* if (Entry.Name != "PolySNP") *\/ */
/*       /\*  continue; *\/ */
/*       /\* if (Entry.Name != "GranthamWeights2") *\/ */
/*       /\*  continue; *\/ */
/*       /\* if (Entry.Name == "PolySIM") *\/ */
/*       /\*  continue; *\/ */
/*       if (Entry.Name != "Clear") */
/*        continue; */

/*       errs() << "\033[1;34m" << Entry.ID << "\033[0;34m " << Entry.Name */
/*              << " (" << PublicConstMethods.size() << " methods)\033[m\n"; */

/*       StructType *T = nullptr; */
/*       for (const Function *F : PublicConstMethods) { */
/*         const Argument *A = getThisArg(F); */
/*         auto StructTy = cast<StructType>(A->getType()->getPointerElementType()); */
/*         errs() << "   StructTy "; StructTy->print(errs()); errs() << '\n'; */
/*         if (!T) */
/*           T = StructTy; */
/*         else { */
/*           if (T != StructTy) { */
/*             SmallPtrSet<const StructType *, 16> SubStructs; */
/*             addSubStructs(SubStructs, StructTy); */
/*             if (SubStructs.count(T)) { */
/*               T = StructTy; */
/*             } */
/*             for (auto ST : SubStructs) { */
/*                 errs() << "   substruct "; ST->print(errs()); errs() << '\n'; */
/*             } */
/*           } */
/*         } */
/*       } */

/*       CurrentType = T; */
/*       IncompleteMethodInitialStates.clear(); */
/*       CompleteMethodInitialStates.clear(); */

/*       analyzeMethods(Entry.Name, PublicConstMethods); */
/*       ++NumClasses; */
/*     } */
/*     errs() << "Analyzed " << NumClasses << " classes\n"; */
/*     database::finish(); */
/*     return false; */

/*     /\* */
/*     auto &CQ = Q->C; */

/*     for (const StructType *T : CQ.getTypes()) { */
/*       // Bad Fish classes */
/*       if (T->getName() == "class.parse_node_tree_t.662" */
/*           || T->getName() == "class.job_t" */
/*           || T->getName() == "class.pager_t" */
/*           || T->getName() == "class.history_item_t.294" */
/*           || T->getName() == "class.env_vars_snapshot_t" */
/*           || T->getName() == "class.parser_t") { */
/*         continue; */
/*       } */

/*       // Bad Ninja classes */
/*       if (T->getName() == "struct.Node") { */
/*         continue; */
/*       } */

/*       // Bad Mosh classes */
/*       if (T->getName() == "class.Terminal::Display" */
/*           || T->getName() == "class.Overlay::ConditionalOverlayRow.3480" */
/*           || T->getName() == "class.Terminal::DrawState" */
/*           || T->getName() == "class.Terminal::Complete" */
/*           || T->getName() == "class.Terminal::Framebuffer" */
/*           || T->getName() == "class.Parser::Print" */
/*           || T->getName() == "class.Parser::Resize" */
/*           || T->getName() == "class.Overlay::PredictionEngine" */
/*           || T->getName() == "class.ClientBuffers::UserMessage" */
/*           || T->getName() == "class.Overlay::NotificationEngine" */
/*           || T->getName() == "class.TransportBuffers::Instruction" */
/*           || T->getName() == "class.Network::UserStream") { */
/*         continue; */
/*       } */

/*       // Bad PS3 classes */
/*       if (T->getName() == "struct.shader_code::builder::writer_t" */
/*           || T->getName() == "struct.rsx::fragment_program::decompiler<shader_code::glsl_language>::instruction_t" */
/*           || T->getName().startswith("struct.shader_code::clike_language_impl::expression") */
/*           || T->getName() == "struct.shader_code::clike_language_impl::expression_t.63") { */
/*         continue; */
/*       } */

/*       // Bad sequence classes */
/*       if (T->getName() == "class.Sequence::PolyTable" */
/*           || T->getName() == "class.Sequence::SimData.635" */
/*           || T->getName() == "class.Sequence::PolySIM" */
/*           || T->getName() == "class.Sequence::PolySNP" */
/*           || T->getName() == "class.Sequence::ClustalW" */
/*           || T->getName() == "class.Sequence::samrecord" */
/*           || T->getName() == "class.Sequence::FST" */
/*           || T->getName() == "class.Sequence::GranthamWeights3" */
/*           || T->getName() == "class.Sequence::RedundancyCom95.781" */
/*           || T->getName() == "class.Sequence::Comeron95") { */
/*         continue; */
/*       } */

/*       CurrentType = T; */
/*       IncompleteMethodInitialStates.clear(); */
/*       CompleteMethodInitialStates.clear(); */

/*       auto &PublicConstMethods = CQ.getPublicConstMethods(T); */
/*       if (PublicConstMethods.size() == 0) { */
/*         continue; */
/*       } */
/*       errs() << "\033[1;34mStruct %" << T->getName() << " (" */
/*              << "public const methods: " << PublicConstMethods.size() */
/*              << ")\033[0m\n"; */
/*       analyzeMethods(PublicConstMethods); */
/*     } */

/*     return false; */
/*     *\/ */
/*   } */

  // We don't modify the program, so we preserve all analyses.
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<ClassQuery>();
    AU.addRequired<MemQuery>();
    errs() << ":: getAnalysisUsage end\n";
  }

  void print(raw_ostream &O, const Module *M) const override {
  }
private:

  bool hasEquivalentInitialState(GraphPtr &State,
                                 std::vector<GraphPtr> &InitialStates);
  void handleFinalState(const FunctionSet &Methods,
                        GraphPtr FinalState);
  void analyzeMethods(std::string &ClassName, const FunctionSet &Methods);

  void runMethod(const GraphPtr &InitialState, std::string &ClassName, const FunctionSet &Methods, const Function *Method, const BasicBlockEdge *IgnoredEdge=nullptr);
  void checkArgument(const Function *Method, const GraphPtr &ResultState, const Argument *Arg);
  void checkReturn(const Function *Method, const GraphPtr &ResultState);
};

}
}

#endif
