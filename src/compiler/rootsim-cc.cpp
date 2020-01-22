#include "llvm/Pass.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/Function.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include <iostream>
#include <vector>

using namespace llvm;
namespace {

    struct ROOTSimCC : public ModulePass {
        static char ID;

        ROOTSimCC() : ModulePass(ID) {}


        bool runOnModule(Module &M) {

            std::vector < Function * > functions;
            std::map<const StringRef, Function *> clonedFunctions;

            for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
                if (!F->isDeclaration()) {
                    //errs() << " ********** " << F->getName() << " ********** " << "\n";
                    functions.push_back(&*F);
                }
            }

            for (std::vector<Function *>::iterator F = functions.begin(), E = functions.end(); F != E; ++F) {
                Function *clonedFunction = clone_llvm_function(*F, &M);
                clonedFunctions[(*F)->getName()] = clonedFunction;
                //TODO: identify functions by name and number and type of arguments
            }

            for (std::pair<const StringRef, Function *> theGuy : clonedFunctions) {
                errs() << " ********** " << theGuy.second->getName() << " ********** " << "\n";
                for (Function::iterator BI = theGuy.second->begin(), BE = theGuy.second->end(); BI != BE; ++BI) {
                    for (BasicBlock::iterator BB = BI->begin(), BBE = BI->end(); BB != BBE; ++BB) {
                        if (CallInst * inst = dyn_cast<CallInst>(&(*BB))) {

                            Function *func = inst->getCalledFunction();
                            if (!func) { //this means it is an indirect call (ASM)
                                continue;
                            }

                            auto funIterator = clonedFunctions.find(inst->getCalledFunction()->getName());

                            if (funIterator == clonedFunctions.end()) {
                                continue;
                            }

                            Function *fun = funIterator->second;

                            //errs() << "\t" <<" ********** " << fun->getName() << " ********** " << "\n";
                            inst->setCalledFunction(fun);
                        }
                    }
                }
            }


            errs() << "\n ======================  Entered Module " << "[ " << M.getName() << " ]" << "====================== \n";
            
            //run over every file in folder
            for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {

                if(F->getName().find("cloned") == std::string::npos) {
                    continue;
                }

                errs() << " \n\t******************* Function name:  " << "[ " << F->getName() << " ]" << " ********************\n\n";


                //loop over each function within the file
                for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
                    //loop over each instruction inside function
                    for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI) {

                        if(!BI->mayWriteToMemory()) {
                            continue;
                        }

                        if (StoreInst *inst = dyn_cast<StoreInst>(&(*BI))) {
                            //Retrieve parse version of data related to the inst
                            Value *address_of_store = inst->getPointerOperand();

                            PointerType *pointerType = cast<PointerType>(address_of_store->getType());
                            //Returns the maximum number of bytes that may be overwritten by storing the specified type.
                            DataLayout *dataLayout = new DataLayout(&M);
                            uint64_t storeSize = dataLayout->getTypeStoreSize(pointerType->getPointerElementType());

                            insertCallBefore(inst, &M, address_of_store, cast<PointerType>(address_of_store->getType()), storeSize);

                            errs() << " [STORE]";
                        } else if (isa<MemSetInst>(&(*BI))) {
                            MemSetInst *inst = dyn_cast<MemSetInst>(&(*BI));

                            insertCallBefore(inst, &M, inst->getRawDest(), cast<PointerType>(inst->getRawDest()->getType()), (uint64_t) inst->getLength());

                            errs() << "[MEMSET]";
                        } else if (isa<MemCpyInst>(&(*BI))) {
                            MemCpyInst *inst = dyn_cast<MemCpyInst>(&(*BI));

                            insertCallBefore(inst, &M, inst->getRawDest(), cast<PointerType>(inst->getRawDest()->getType()), (uint64_t) inst->getLength());

                            errs() << "[MEMCOPY]";
                        } else {
                            if (isa<CallInst>(&(*BI))) continue;
                            errs() << "[UNRECOGNIZED!1!]";
                        }
                        errs() << "\n";
                    }
                }
            }
            return true;
        }

        llvm::Function *clone_llvm_function(llvm::Function *toClone, Module *M) {
            ValueToValueMapTy VMap;
            Function *NewF = Function::Create(toClone->getFunctionType(),
                                              toClone->getLinkage(),
                                              "cloned_" + toClone->getName(),
                                              M);
            ClonedCodeInfo info;
            Function::arg_iterator DestI = NewF->arg_begin();
            for (Function::const_arg_iterator I = toClone->arg_begin(), E = toClone->arg_end(); I != E; ++I) {
                DestI->setName(I->getName());    // Copy the name over...
                VMap[&*I] = &*DestI++;        // Add mapping to VMap
            }

            // Necessary in case the function is self referential
            VMap[&*toClone] = NewF;

            SmallVector < ReturnInst * , 8 > Returns;
            llvm::CloneFunctionInto(NewF, toClone, VMap, true, Returns, "", NULL, NULL, NULL);

            return NewF;
        }

        void insertCallBefore(Instruction *theInstruction, Module *M, Value *arg1, PointerType *arg2, uint64_t arg3) {
            IRBuilder<> builder(M->getContext());
            FunctionCallee memtraceFunction = M->getOrInsertFunction("memtrace",
                                                      Type::getVoidTy(M->getContext()));

            ArrayRef<Value*> args = {
                    builder.CreatePointerCast(arg1, arg2),
                    builder.getInt64(arg3)
                            };

            (builder.CreateCall(memtraceFunction.getCallee(), args))->insertBefore(theInstruction);
        }
    };
}


// Pass info
char ROOTSimCC::ID = 0; // LLVM ignores the actual value: it referes to the pointer.
//static RegisterPass<ROOTSimCC> X("rootsim-cc", "ROOT-Sim CC pass", false, false);

// Pass loading stuff
// To use, run: clang -Xclang -load -Xclang <your-pass>.so <other-args> ...

// This function is of type PassManagerBuilder::ExtensionFn
static void loadPass(const PassManagerBuilder &Builder, llvm::legacy::PassManagerBase &PM) {
    PM.add(new ROOTSimCC());
}

// These constructors add our pass to a list of global extensions.
static RegisterStandardPasses clangtoolLoader_Ox(PassManagerBuilder::EP_OptimizerLast, loadPass);
static RegisterStandardPasses clangtoolLoader_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, loadPass);
