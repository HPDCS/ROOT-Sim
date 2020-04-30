#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/IntrinsicInst.h"

// TODO: this list should be automatically generated
const char *rootsim_exposed_functions[] = {
	"SetState",
	"FindReceiver",
	"GetVisit",
	"SetValueTopology",
	"SetVisit",
	"FindReceiverToward",
	"CountVisits",
	"Expent",
	"GetReceiver",
	"Poisson",
	"TrackNeighbourInfo",
	"GetPastVisit",
	"GetValueTopology",
	"Gamma",
	"RemoveVisit",
	"CountPastVisits",
	"ComputeMinTour",
	"ScheduleNewLeaveEvent",
	"AddVisit",
	"RegionsCount",
	"Random",
	"KillAgent",
	"DataAgent",
	"Normal",
	"Zipf",
	"DirectionsCount",
	"RandomRangeNonUniform",
	"CountAgents",
	"SpawnAgent",
	"NeighboursCount",
	"IterAgents",
	"GetNeighbourInfo",
	"SetTopology",
	"EnqueueVisit",
	"RandomRange",
	"CapabilityAvailable",
	nullptr
};

using namespace llvm;

namespace {
class ROOTSimCC: public ModulePass {
public:
	ROOTSimCC() : ModulePass(ID)
	{
		unsigned i = 0;
		while(rootsim_exposed_functions[i]) {
			++i;
		}

		rootsim_functions = new StringRef*[i + 1];
		rootsim_functions[i] = nullptr;

		while(i--){
			rootsim_functions[i] =
				new StringRef(rootsim_exposed_functions[i]);
		}
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const
	{
		AU.addRequired<TargetLibraryInfoWrapperPass>();
	}

	bool runOnModule(Module &M)
	{
		errs() << "Instrumenting module " << raw_ostream::CYAN <<
			M.getName() << raw_ostream::RESET << "\n";

		this->M = &M;

		Type *MemtraceArgs[] = {
			PointerType::getUnqual(Type::getVoidTy(M.getContext())),
			IntegerType::get(M.getContext(), sizeof(size_t) * CHAR_BIT)
		};

		std::vector <Function *> functions;
		for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {

			if (isSystemSide(&*F))
				continue;
#if LOG_LEVEL <= LOG_DEBUG
			errs() << "Found function " << F->getName()  << "\n";
#endif
			functions.push_back(&*F);
		}

		FC = M.getOrInsertFunction(
			"__write_mem",
			FunctionType::get(
				Type::getVoidTy(M.getContext()),
				MemtraceArgs,
				false
			)
		);

		ValueToValueMapTy VMap;
		for (std::vector<Function *>::iterator F = functions.begin(),
			E = functions.end(); F != E; ++F) {
			VMap[*F] = CloneStubInstr(*F);
		}

		for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
			if (	VMap.count(&*F) == 0 ||
				F->isDeclaration() ||
				isSystemSide(&*F))
				continue;
			Function *Cloned = &cast<Function>(*VMap[&*F]);
#if LOG_LEVEL <= LOG_DEBUG
			errs() << "Instrumenting " << Cloned->getName() << "\n";
#endif
			ClonedCodeInfo CloneInfo;
			CloneFunctionIntoInstr(Cloned, &*F, VMap, &CloneInfo);
			for (inst_iterator I = inst_begin(Cloned),
					E = inst_end(Cloned); I != E; ++I){
				InstrumentInstruction(&*I);
			}
		}

		errs() << "Found " << tot_instr << " memory writing IR instructions\n";
		errs() << raw_ostream::GREEN << "Instrumented\n" << raw_ostream::RESET;
		errs() << "\t " << memset_instr << " memset-like IR instructions\n";
		errs() << "\t " << memcpy_instr << " memcopy-like IR instructions\n";
		errs() << "\t " << store_instr << " store IR instructions\n";
		errs() << raw_ostream::GREEN << "Ignored\n" << raw_ostream::RESET;
		errs() << "\t " << atomic_instr << " atomic IR instructions\n";
		errs() << "\t " << call_instr << " call IR instructions\n";
		errs() << "\t " << unknown_instr << " unknown IR instructions\n";
		return functions.size() != 0;
	}

private:
	static char ID;
	Module *M = nullptr;
	FunctionCallee FC = nullptr;
	StringRef **rootsim_functions;

	unsigned tot_instr = 0;
	unsigned memset_instr = 0;
	unsigned memcpy_instr = 0;
	unsigned store_instr = 0;
	unsigned atomic_instr = 0;
	unsigned call_instr = 0;
	unsigned unknown_instr = 0;

	bool isSystemSide(Function *F)
	{
		unsigned i = 0;
		const StringRef Fname = F->getName();
		while (rootsim_functions[i]) {
			if(Fname.equals(*rootsim_functions[i]))
				return true;
			++i;
		}

		enum llvm::LibFunc LLF;
		return F->getIntrinsicID() || F->doesNotReturn() ||
			getAnalysis<TargetLibraryInfoWrapperPass>()
			.getTLI(F->getFunction()).getLibFunc(F->getFunction(), LLF);
	}

	void InstrumentInstruction(Instruction *TI)
	{
		if (!TI->mayWriteToMemory()) {
			return;
		}

		++tot_instr;
		Value *args[2];

		if (StoreInst *SI = dyn_cast<StoreInst>(TI)) {
			Value *V = SI->getPointerOperand();
			PointerType *pointerType = cast<PointerType>(V->getType());
			uint64_t storeSize = M->getDataLayout()
				.getTypeStoreSize(pointerType->getPointerElementType());
			args[0] = V;
			args[1] = ConstantInt::get(IntegerType::get(M->getContext(),
				sizeof(size_t) * CHAR_BIT), storeSize);
			++store_instr;
		} else if (MemSetInst *MSI = dyn_cast<MemSetInst>(TI)) {
			args[0] = MSI->getRawDest();
			args[1] = MSI->getLength();
			++memset_instr;
		} else if (MemCpyInst *MCI = dyn_cast<MemCpyInst>(TI)) {
			args[0] = MCI->getRawDest();
			args[1] = MCI->getLength();
			++memcpy_instr;
		} else {
			 if (isa<CallBase>(TI)) {
				++call_instr;

			} else if (TI->isAtomic()) {
#if LOG_LEVEL <= LOG_DEBUG
				errs() << "Encountered an atomic non-store instruction in function "
					<< TI->getParent()->getParent()->getName() << "\n";
#endif
				++atomic_instr;
			} else {
				errs() << "Encountered an unknown memory writing instruction in function "
					<< TI->getParent()->getParent()->getName() << "\n";
				++unknown_instr;
			}
			return;
		}

		CallInst::Create(FC, args, "", TI);
	}

	Function* CloneStubInstr(Function *F)
	{
		std::vector<Type*> ArgTypes;

		for (const Argument &I : F->args())
			ArgTypes.push_back(I.getType());

		FunctionType *FTy = FunctionType::get(
			F->getFunctionType()->getReturnType(),
			ArgTypes,
			F->getFunctionType()->isVarArg()
		);

		std::string NewFName = F->getName().str() + "_instr";
		Function *NewF = Function::Create(
			FTy,
			F->getLinkage(),
			F->getAddressSpace(),
			NewFName,
			F->getParent()
		);

		return NewF;
	}

	static void CloneFunctionIntoInstr(Function *NewF, Function *F,
		ValueToValueMapTy &VMap, ClonedCodeInfo *CodeInfo)
	{
		Function::arg_iterator DestI = NewF->arg_begin();
		for (const Argument &I : F->args()){
			DestI->setName(I.getName());
			VMap[&I] = &*DestI++;
		}

		SmallVector<ReturnInst*, 8> Returns;
		CloneFunctionInto(
			NewF,
			F,
			VMap,
			true,
			Returns,
			"_instr",
			CodeInfo
		);
	}
};
}

char ROOTSimCC::ID = 0;

static void loadPass(
	const PassManagerBuilder &Builder,
	llvm::legacy::PassManagerBase &PM
) {
	(void)Builder;
	PM.add(new ROOTSimCC());
}

static RegisterStandardPasses clangtoolLoader_Ox(
	PassManagerBuilder::EP_OptimizerLast,
	loadPass
);
static RegisterStandardPasses clangtoolLoader_O0(
	PassManagerBuilder::EP_EnabledOnOptLevel0,
	loadPass
);
