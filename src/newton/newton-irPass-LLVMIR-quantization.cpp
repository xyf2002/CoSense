/*
	Authored 2022. Pei Mu.
	All rights reserved.
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:
	*	Redistributions of source code must retain the above
		copyright notice, this list of conditions and the following
		disclaimer.
	*	Redistributions in binary form must reproduce the above
		copyright notice, this list of conditions and the following
		disclaimer in the documentation and/or other materials
		provided with the distribution.
	*	Neither the name of the author nor the names of its
		contributors may be used to endorse or promote products
		derived from this software without specific prior written
		permission.
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include "newton-irPass-LLVMIR-quantization.h"

using namespace llvm;

#define FRAC_Q 16
#define FRAC_BASE (1 << FRAC_Q)
#define BIT_WIDTH 32

extern "C"

{
// Set the quantized type for a given value
void
setQuantizedType(Value * inValue, Type * quantizedType)
{
	auto	 valueType = inValue->getType();
	unsigned pointerAddr;
	bool	 isPointer = false;
	if (valueType != nullptr)
	{
		if (valueType->isPointerTy())
		{
			isPointer = true;
			// If the value is a pointer, get the address space and element type
			pointerAddr = valueType->getPointerAddressSpace();
			valueType   = valueType->getPointerElementType();
		}
		if (valueType->isDoubleTy() || valueType->isFloatTy() || valueType->isArrayTy())
		{
			// If the value is a pointer, get the address space and element type
			if (isPointer)
			{
				inValue->mutateType(quantizedType->getPointerTo(pointerAddr));
			}
			else
			{
				// Otherwise, directly set the type to the quantized type
				inValue->mutateType(quantizedType);
			}
		}
	}
}

// Quantize constants within an instruction
void
quantizeConstant(Instruction * inInstruction, Type * quantizedType)
{
	for (size_t idx = 0; idx < inInstruction->getNumOperands(); idx++)
	{
		Value * inValue = inInstruction->getOperand(idx);

		// Skip if the operand is not a floating-point constant
		if (!isa<llvm::ConstantFP>(inValue))
		{
			continue;
		}

		ConstantFP * constFp  = llvm::dyn_cast<llvm::ConstantFP>(inValue);
		Value *	     newValue = nullptr;
		if (inValue->getType()->isFloatTy())
		{
			// Convert float constant to fixed-point
			float constValue = constFp->getValueAPF().convertToFloat();
			constValue *= FRAC_BASE;
			newValue = ConstantInt::get(quantizedType, round(constValue), true);
		}
		else if (inValue->getType()->isDoubleTy())
		{
			// Convert double constant to fixed-point
			double constValue = constFp->getValueAPF().convertToDouble();
			constValue *= FRAC_BASE;
			newValue = ConstantInt::get(quantizedType, round(constValue), true);
		}
		else
		{
			// Assert if the type is unknown
			assert(false && "unknown floating type");
		}

		inInstruction->replaceUsesOfWith(inValue, newValue);
	}
}

// Simplify constants in the instruction
void
simplifyConstant(Instruction * inInstruction, Type * quantizedType)
{
	auto checkDecimal = [](float decimalNum) {
		int digits = 0;
		/*
		 * Since the max value of `int16` is 32767,
		 * we maximum multiply with 1,000 to make sure it won't exceed max_int16
		 * */

		// Ensure the decimal number can be represented within the fixed-point range
		while (fabs(round(decimalNum) - decimalNum) > 0.001 && digits < 4)
		{
			decimalNum *= 10;
			digits++;
		}
		return decimalNum;
	};

	auto compensateFP = [inInstruction, quantizedType](float quantizedNum, float decimalNum) {
		/*
		 * 3333.3 / 3.3333 = 1000
		 * ===>
		 * Example 1:
		 * a * 3.3333 ~= a * (3333 / 1000) ~= (int)a * 3333 / 1000
		 *
		 * Example 2:
		 * a / 3.3333 ~= a / (3333 / 1000) ~= (int)a * 1000 / 3333
		 *
		 * Example 3:
		 * 3.3333 / a ~= (3333 / 1000) / a ~= 3333 / (int)a / 1000
		 * */

		// Calculate the compensation factor for fixed-point arithmetic
		float compensateNum = quantizedNum / decimalNum;

		Value *	 constOperand, *nonConstOperand;
		unsigned constIdx, nonConstIdx;
		if (isa<llvm::Constant>(inInstruction->getOperand(0)))
		{
			constIdx	= 0;
			nonConstIdx	= 1;
			constOperand	= inInstruction->getOperand(0);
			nonConstOperand = inInstruction->getOperand(1);
		}
		else
		{
			constIdx	= 1;
			nonConstIdx	= 0;
			constOperand	= inInstruction->getOperand(1);
			nonConstOperand = inInstruction->getOperand(0);
		}

		auto quantizeNumValue = ConstantInt::get(quantizedType, round(quantizedNum), true);

		if (compensateNum == 1)
		{
			// If the compensation factor is 1, directly set the quantized value
			inInstruction->setOperand(constIdx, quantizeNumValue);
		}
		else
		{
			// Otherwise, apply compensation to the fixed-point arithmetic
			auto compensateNumValue = ConstantInt::get(quantizedType, round(compensateNum), true);

			IRBuilder<>   Builder(inInstruction);
			Instruction * insertPoint = inInstruction->getNextNode();
			Builder.SetInsertPoint(insertPoint);
			Value * newFisrtInst  = nullptr;
			Value * newSecondInst = nullptr;
			auto	instOpCode    = inInstruction->getOpcode();
			if (instOpCode == Instruction::FMul)
			{
				// Replace floating-point multiplication with fixed-point equivalent
				newFisrtInst  = Builder.CreateMul(nonConstOperand, quantizeNumValue);
				newSecondInst = Builder.CreateSDiv(newFisrtInst, compensateNumValue);
			}
			else if (instOpCode == Instruction::FDiv && isa<llvm::Constant>(inInstruction->getOperand(1)))
			{
				// Replace floating-point division with fixed-point equivalent (divisor is constant)
				newFisrtInst  = Builder.CreateMul(nonConstOperand, compensateNumValue);
				newSecondInst = Builder.CreateSDiv(newFisrtInst, quantizeNumValue);
			}
			else if (instOpCode == Instruction::FDiv && isa<llvm::Constant>(inInstruction->getOperand(0)))
			{
				newFisrtInst  = Builder.CreateSDiv(quantizeNumValue, nonConstOperand);
				newSecondInst = Builder.CreateSDiv(newFisrtInst, compensateNumValue);
			}

			inInstruction->replaceAllUsesWith(newSecondInst);
			inInstruction->removeFromParent();
		}
	};

	for (size_t idx = 0; idx < inInstruction->getNumOperands(); idx++)
	{
		Value * inValue = inInstruction->getOperand(idx);

		if (!isa<llvm::ConstantFP>(inValue))
		{
			continue;
		}

		ConstantFP * constFp  = llvm::dyn_cast<llvm::ConstantFP>(inValue);
		Value *	     newValue = nullptr;
		if (inValue->getType()->isFloatTy())
		{
			float constValue = constFp->getValueAPF().convertToFloat();
			compensateFP(checkDecimal(constValue), constValue);
		}
		else if (inValue->getType()->isDoubleTy())
		{
			double constValue = constFp->getValueAPF().convertToDouble();
			compensateFP(checkDecimal(constValue), constValue);
		}
		else
		{
			assert(false && "unknown floating type");
		}
	}
}

// Create a fixed-point multiplication function
llvm::Function *
createFixMul(llvm::Function * inFunction, Type * quantizedType, std::vector<llvm::Function *> & functionsToInsert)
{
	/*
	 * check if this function is exist
	 * */
	std::string fixmulFuncName = "fixmul";
	auto	    irModule	   = inFunction->getParent();
	for (auto & function : *irModule)
	{
		if (function.getName() == fixmulFuncName)
		{
			return &function;
		}
	}

	llvm::FunctionType * funcType = llvm::FunctionType::get(quantizedType, {quantizedType, quantizedType}, false);
	llvm::Function *     func     = llvm::Function::Create(funcType, llvm::Function::PrivateLinkage, fixmulFuncName, irModule);

	llvm::BasicBlock * entryBB = llvm::BasicBlock::Create(inFunction->getContext(), "entry", func);
	llvm::IRBuilder<>  builder(entryBB);
	builder.SetInsertPoint(entryBB);

	/*
	 * ((int64_t)x*y)>>FRAC_Q
	 *
	 * ===========>
	 *
	 * define private i32 @mulfix(i32 %0, i32 %1) {
	 * %3 = sext i32 %0 to i64
	 * %4 = sext i32 %1 to i64
	 * %5 = mul nsw i64 %3, %4
	 * %6 = ashr i64 %5, 8
	 * %7 = trunc i64 %6 to i32
	 * ret i32 %7
	 * }
	 * */

	// Implement fixed-point multiplication ((int64_t)x*y) >> FRAC_Q
	Type * higherQuantizedType;
	switch (BIT_WIDTH)
	{
		case 8:
			higherQuantizedType = Type::getInt16Ty(inFunction->getContext());
			break;
		case 16:
			higherQuantizedType = Type::getInt32Ty(inFunction->getContext());
			break;
		default:
			higherQuantizedType = Type::getInt64Ty(inFunction->getContext());
			break;
	}

	llvm::Function::arg_iterator arg1      = &*(func->arg_begin());
	llvm::Value *		     sext1     = builder.CreateSExt(arg1, higherQuantizedType);
	llvm::Function::arg_iterator arg2      = &*(++arg1);
	llvm::Value *		     sext2     = builder.CreateSExt(arg2, higherQuantizedType);
	llvm::Value *		     mulInst   = builder.CreateMul(sext1, sext2);
	llvm::Value *		     ashrInst  = builder.CreateAShr(mulInst, ConstantInt::get(higherQuantizedType, FRAC_Q));
	llvm::Value *		     truncInst = builder.CreateTrunc(ashrInst, quantizedType);
	builder.CreateRet(truncInst);

	functionsToInsert.emplace_back(func);

	return func;
}

// Substitute hardcoded functions like multiplication with fixed-point versions
void
substituteHardcodeFunc(Instruction * inInstruction, Type * quantizedType, llvm::Function * func)
{
	IRBuilder<>   Builder(inInstruction);
	Instruction * insertPoint = inInstruction->getNextNode();
	Builder.SetInsertPoint(insertPoint);
	//    Value * newInst = nullptr;

	llvm::CallInst * callInst = Builder.CreateCall(func, {inInstruction->getOperand(0), inInstruction->getOperand(1)});
	//    InlineFunctionInfo inlineFuncInfo;
	//    llvm::InlineFunction(*callInst, inlineFuncInfo);

	inInstruction->replaceAllUsesWith(callInst);
	inInstruction->removeFromParent();
}

// Quantize simple floating-point instructions
CmpInst::Predicate
quantizePredict(CmpInst::Predicate predict)
{
	switch (predict)
	{
		// Ordered comparisons
		case FCmpInst::FCMP_OEQ:  // equal
		case FCmpInst::FCMP_UEQ:  // equal
			return ICmpInst::ICMP_EQ;
		case FCmpInst::FCMP_OGT:  // greater than
		case FCmpInst::FCMP_UGT:  // greater than
			return ICmpInst::ICMP_SGT;
		case FCmpInst::FCMP_OGE:  // greater than or equal
		case FCmpInst::FCMP_UGE:  // greater than or equal
			return ICmpInst::ICMP_SGE;
		case FCmpInst::FCMP_OLT:  // less than
		case FCmpInst::FCMP_ULT:  // less than
			return ICmpInst::ICMP_SLT;
		case FCmpInst::FCMP_OLE:  // less than or equal
		case FCmpInst::FCMP_ULE:  // less than or equal
			return ICmpInst::ICMP_SLE;
		case FCmpInst::FCMP_ONE:  // not equal
		case FCmpInst::FCMP_UNE:  // not equal
			return ICmpInst::ICMP_NE;

		case FCmpInst::FCMP_ORD:    // ordered (no NaNs)
		case FCmpInst::FCMP_UNO:    // unordered (at least one NaN)
		case FCmpInst::FCMP_TRUE:   // always true
		case FCmpInst::FCMP_FALSE:  // always false
		default:
			llvm_unreachable("Unknown or unsupported predicate in quantizePredict");
	}
}
void
quantizeSimpleFPInstruction(Instruction * inInstruction, Type * quantizedType)
{
	IRBuilder<>   Builder(inInstruction);
	Instruction * insertPoint = inInstruction->getNextNode();
	Builder.SetInsertPoint(insertPoint);
	Value * newInst = nullptr;
	switch (inInstruction->getOpcode())
	{
		case Instruction::FAdd:
		{
			// Replace floating-point addition with integer addition
			newInst = Builder.CreateAdd(inInstruction->getOperand(0), inInstruction->getOperand(1));
			break;
		}
		case Instruction::FSub:
		{
			// Replace floating-point subtraction with integer subtraction
			newInst = Builder.CreateSub(inInstruction->getOperand(0), inInstruction->getOperand(1));
			break;
		}
		case Instruction::FRem:
		{
			// Replace floating-point remainder with integer remainder
			newInst = Builder.CreateSRem(inInstruction->getOperand(0), inInstruction->getOperand(1));
			break;
		}
		case Instruction::FCmp:
		{
			// Replace floating-point comparison with integer comparison
			FCmpInst * fcmp_inst = dyn_cast<FCmpInst>(inInstruction);
			newInst		     = Builder.CreateICmp(quantizePredict(fcmp_inst->getPredicate()),
								  fcmp_inst->getOperand(0), fcmp_inst->getOperand(1));
			break;
		}
		/*
		 * Change fneg(a) to `0-a`.
		 * */
		case Instruction::FNeg:
		{
			// Replace floating-point negation with integer negation
			auto constZero = ConstantInt::get(quantizedType, 0, true);
			newInst	       = Builder.CreateSub(constZero, inInstruction->getOperand(0));
			break;
		}

		// TODO add div
		case Instruction::FDiv:
		{
			// Replace floating-point division with integer division
			simplifyConstant(inInstruction, quantizedType);
			break;
		}

		default:
			break;
	}

	inInstruction->replaceAllUsesWith(newInst);
	inInstruction->removeFromParent();
}

// Adapt type casts to the quantized type
void
adaptTypeCast(llvm::Function & llvmIrFunction, Type * quantizedType)
{
	for (BasicBlock & llvmIrBasicBlock : llvmIrFunction)
	{
		for (BasicBlock::iterator itBB = llvmIrBasicBlock.begin(); itBB != llvmIrBasicBlock.end();)
		{
			Instruction * llvmIrInstruction = &*itBB++;
			switch (llvmIrInstruction->getOpcode())
			{
				case Instruction::FPToUI:
				{
					// TODO Convert floating-point to unsigned integer
					IRBuilder<> Builder(llvmIrInstruction);
					Value *	    newInst = Builder.CreateFPToUI(llvmIrInstruction->getOperand(0), quantizedType);
					llvmIrInstruction->replaceAllUsesWith(newInst);
					llvmIrInstruction->removeFromParent();
					break;
				}
				case Instruction::FPToSI:
				{
					// TODO  Convert floating-point to integer (unsigned/signed)
					IRBuilder<> Builder(llvmIrInstruction);
					Value *	    newInst = Builder.CreateFPToSI(llvmIrInstruction->getOperand(0), quantizedType);
					llvmIrInstruction->replaceAllUsesWith(newInst);
					llvmIrInstruction->removeFromParent();
					break;
				}
				case Instruction::SIToFP:
				{
					// TODO Convert signed integer to floating-point
					IRBuilder<> Builder(llvmIrInstruction);
					Value *	    newInst = Builder.CreateSIToFP(llvmIrInstruction->getOperand(0), llvmIrInstruction->getType());
					llvmIrInstruction->replaceAllUsesWith(newInst);
					llvmIrInstruction->removeFromParent();
					break;
				}
				case Instruction::UIToFP:
				{
					// Handle floating-point to integer and integer to floating-point conversions
					auto sourceOp = llvmIrInstruction->getOperand(0);
					if (sourceOp->getType() == llvmIrInstruction->getType())
					{
						llvmIrInstruction->replaceAllUsesWith(sourceOp);
						llvmIrInstruction->removeFromParent();
					}
				}
				break;
					//				case Instruction::ZExt:
					//				case Instruction::SExt:
					//				case Instruction::Trunc:
					/*
					 * since the src type changed, adapt the new instruction
					 * */
				case Instruction::FPExt:
				{
					// TODO Extend floating-point precision
					IRBuilder<> Builder(llvmIrInstruction);
					Value *	    newInst = Builder.CreateFPExt(llvmIrInstruction->getOperand(0), llvmIrInstruction->getType());
					llvmIrInstruction->replaceAllUsesWith(newInst);
					llvmIrInstruction->removeFromParent();
					break;
				}
				case Instruction::FPTrunc:

				{
					// Handle floating-point extensions and truncations
					IRBuilder<>   Builder(llvmIrInstruction);
					Instruction * insertPoint = llvmIrInstruction->getNextNode();
					Builder.SetInsertPoint(insertPoint);
					Value * newInst = nullptr;
					if (llvmIrInstruction->getOperand(0)->getType()->isIntegerTy())
					{
						newInst = Builder.CreateSIToFP(
						    llvmIrInstruction->getOperand(0), llvmIrInstruction->getType());
					}
					else
					{
						newInst = Builder.CreateFPCast(
						    llvmIrInstruction->getOperand(0), llvmIrInstruction->getType());
					}
					llvmIrInstruction->replaceAllUsesWith(newInst);
					llvmIrInstruction->removeFromParent();
					break;
				}
				case Instruction::BitCast:
				{
					// Handle bitcasts
					IRBuilder<>   Builder(llvmIrInstruction);
					Instruction * insertPoint = llvmIrInstruction->getNextNode();
					Builder.SetInsertPoint(insertPoint);
					Value * newInst = Builder.CreateBitCast(
					    llvmIrInstruction->getOperand(0), llvmIrInstruction->getType());
					llvmIrInstruction->replaceAllUsesWith(newInst);
					llvmIrInstruction->removeFromParent();
					break;
				}

				default:
					break;
			}
		}
	}
}

// Main function to perform LLVM IR auto quantization
void
irPassLLVMIRAutoQuantization(State * N, llvm::Function & llvmIrFunction, std::vector<llvm::Function *> & functionsToInsert)
{
	flexprint(N->Fe, N->Fm, N->Fpinfo, "\tauto quantization.\n");

	Type * quantizedType;
	switch (BIT_WIDTH)
	{
		case 8:
			quantizedType = Type::getInt8Ty(llvmIrFunction.getContext());
			break;
		case 16:
			quantizedType = Type::getInt16Ty(llvmIrFunction.getContext());
			break;
		case 32:
			quantizedType = Type::getInt32Ty(llvmIrFunction.getContext());
			break;
		case 64:
			quantizedType = Type::getInt64Ty(llvmIrFunction.getContext());
			break;
		default:
			flexprint(N->Fe, N->Fm, N->Fperr, "\tunknown int type.\n");
			return;
	}

	/*
	 * change the type of this function if it's fp
	 * */

	if (llvmIrFunction.getReturnType()->isFloatTy() || llvmIrFunction.getReturnType()->isDoubleTy())
	{
		llvmIrFunction.mutateType(quantizedType);
	}

	/*
	 * generate hardcode function - fixmul and fixdiv
	 * */
	llvm::Function * fixmul = createFixMul(&llvmIrFunction, quantizedType, functionsToInsert);

	/*
	 * quantize the arguments type
	 * */
	for (int idx = 0; idx < llvmIrFunction.arg_size(); idx++)
	{
		auto paramOp = llvmIrFunction.getArg(idx);
		setQuantizedType(paramOp, quantizedType);
	}

	for (BasicBlock & llvmIrBasicBlock : llvmIrFunction)
	{
		for (BasicBlock::iterator itBB = llvmIrBasicBlock.begin(); itBB != llvmIrBasicBlock.end();)
		{
			Instruction * llvmIrInstruction = &*itBB++;
			switch (llvmIrInstruction->getOpcode())
			{
				case Instruction::Alloca:
					if (auto llvmIrAllocaInstruction = dyn_cast<AllocaInst>(llvmIrInstruction))
					{
						auto allocaType = llvmIrAllocaInstruction->getAllocatedType();
						auto newType	= quantizedType;
						if (allocaType->getTypeID() == Type::ArrayTyID)
						{
							newType	   = ArrayType::get(quantizedType,
										    allocaType->getArrayNumElements());
							allocaType = allocaType->getArrayElementType();
						}
						if (allocaType->isDoubleTy() || allocaType->isFloatTy())
						{
							llvmIrAllocaInstruction->setAllocatedType(newType);
						}
						setQuantizedType(llvmIrAllocaInstruction, newType);
					}
					break;
				case Instruction::Call:
					if (auto llvmIrCallInstruction = dyn_cast<CallInst>(llvmIrInstruction))
					{
						Function * calledFunction = llvmIrCallInstruction->getCalledFunction();
						if (calledFunction == nullptr || !calledFunction->hasName() || calledFunction->getName().empty())
							break;
						if (!calledFunction->getName().startswith("llvm.dbg.value") &&
						    !calledFunction->getName().startswith("llvm.dbg.declare") &&
						    !calledFunction->getName().startswith("llvm.dbg.label"))
						{
							if (calledFunction->isDeclaration())
							{
								IRBuilder<>   Builder(llvmIrCallInstruction);
								Instruction * insertPoint = llvmIrCallInstruction->getNextNode();
								Builder.SetInsertPoint(insertPoint);
								Value * newInst = nullptr;
								if (calledFunction->getName().str() == "sqrt")
								{
									/*
									 * if the arg's type is int, convert to fp,
									 * after the call node, convert to int and shl FRAC_Q/2
									 *
									 * int32_t res = (int32_t)sqrt(x)<<(FRAC_Q/2);
									 * if (FRAC_Q%2)
									 *   return res*1.414213562;
									 * else
									 *   return res;
									 *
									 * %25 = sitofp i32 %0 to double
									 * %26 = call double @sqrt(double %25) #3
									 * %27 = fptosi double %26 to i32
									 * %28 = shl i32 %27, 4
									 * */
									// Handle sqrt function by converting to fixed-point and adjusting
									auto operand = llvmIrCallInstruction->getOperand(0);

									if (operand->getType()->isIntegerTy())
									{
										Value * newOperand = Builder.CreateSIToFP(
										    operand, llvmIrCallInstruction->getType());
										llvmIrCallInstruction->setOperand(0, newOperand);
									}
									auto	cloneInst  = llvmIrCallInstruction->clone();
									Value * fptosiInst = Builder.CreateFPToSI(
									    cloneInst, quantizedType);
									Value * shlInst = Builder.CreateShl(fptosiInst, FRAC_Q / 2);
									Value * resInst = nullptr;
									/*
									 * if (FRAC_Q%2) then multiply with 1.414213562;
									 * */
									if (FRAC_Q % 2)
									{
										Value * lhsCompensateInst = Builder.CreateSIToFP(
										    shlInst, llvmIrCallInstruction->getType());
										auto	compensateNum = ConstantFP::get(llvmIrCallInstruction->getType(),
															1.414213562);
										Value * mulInst	      = Builder.CreateFMul(lhsCompensateInst, compensateNum);
										resInst		      = Builder.CreateFPToSI(mulInst, quantizedType);
									}
									else
									{
										resInst = shlInst;
									}
									llvmIrCallInstruction->replaceAllUsesWith(resInst);
									ReplaceInstWithInst(llvmIrCallInstruction, cloneInst);
								}
								else
								{
									/*
									 * for other lib functions, de-quantize the arguments and quantize the return value
									 * */
								}
							}
							else
							{
								/*
								 * for user-defined function, quantize the arguments
								 * */
								for (size_t idx = 0; idx < llvmIrCallInstruction->getNumOperands() - 1; idx++)
								{
									setQuantizedType(llvmIrCallInstruction->getOperand(idx), quantizedType);
								}
								quantizeConstant(llvmIrCallInstruction, quantizedType);
								/*
								 * then quantize the return type
								 * */
								setQuantizedType(llvmIrCallInstruction, quantizedType);
							}
						}
					}
					break;
				case Instruction::GetElementPtr:
					if (auto gepInst = dyn_cast<GetElementPtrInst>(llvmIrInstruction))
					{
						auto gepType	= gepInst->getType();
						auto sourceType = quantizedType;
						//                        bool isPointer = false;
						//                        unsigned pointerAddr = 0;
						//                        if (gepType->isPointerTy()) {
						//                            isPointer = true;
						//                            pointerAddr = gepType->getPointerAddressSpace();
						//                            valueType = gepType->getPointerElementType();
						//                        }
						if (gepInst->getSourceElementType()->getTypeID() == Type::ArrayTyID)
						{
							sourceType = ArrayType::get(quantizedType,
										    gepInst->getSourceElementType()->getArrayNumElements());
						}
						//                        if (isPointer) {
						//                            inValue->mutateType(quantizedType->getPointerTo(pointerAddr));
						//                        }
						//                        if (gepType->isDoubleTy() || gepType->isFloatTy()) {
						gepInst->setSourceElementType(sourceType);
						gepInst->setResultElementType(quantizedType);
						//                        }
					}
				case Instruction::Load:
				case Instruction::PHI:
				{
					setQuantizedType(llvmIrInstruction, quantizedType);
				}
				break;

				case Instruction::Store:
				{
					/*
					 * If either of the operands is constant, change it to a int value
					 * */
					setQuantizedType(llvmIrInstruction->getOperand(0), quantizedType);
					quantizeConstant(llvmIrInstruction, quantizedType);
				}
				break;

				/*
				 * For fmul/fdiv,
				 *
				 * if either one of the operands is a constant value, simplify it by multiplying with 10^n,
				 * then replace the instruction to mul/div;
				 *
				 * else substitute this instruction to a pre-implemented function: mulfix/divfix.
				 * */
				case Instruction::FMul:
				case Instruction::FDiv:
				{
					if (isa<llvm::Constant>(llvmIrInstruction->getOperand(0)) ||
					    isa<llvm::Constant>(llvmIrInstruction->getOperand(1)))
					{
						simplifyConstant(llvmIrInstruction, quantizedType);
					}
					else
					{
						substituteHardcodeFunc(llvmIrInstruction, quantizedType, fixmul);
					}
					break;
				}

				/*
				 * If either one of the operands is a constant value, quantize it,
				 * then replace the instruction to the int version.
				 * */
				case Instruction::FCmp:
				case Instruction::FAdd:
				case Instruction::FSub:
				case Instruction::FRem:
				{
					quantizeConstant(llvmIrInstruction, quantizedType);
				}
				case Instruction::FNeg:
				{
					quantizeSimpleFPInstruction(llvmIrInstruction, quantizedType);
					break;
				}

					//                case Instruction::Add:
					//                case Instruction::Sub:
					//                case Instruction::Mul:
					//                case Instruction::UDiv:
					//				case Instruction::SDiv:
					//                case Instruction::URem:
					//                case Instruction::SRem:
					//
					//				case Instruction::Shl:
					//				case Instruction::LShr:
					//				case Instruction::AShr:
					//				case Instruction::And:
					//				case Instruction::Or:
					//				case Instruction::Xor:
					//
					//                case Instruction::ICmp:

				case Instruction::FPToUI:
				case Instruction::FPToSI:
				case Instruction::SIToFP:
				case Instruction::UIToFP:
				case Instruction::ZExt:
				case Instruction::SExt:
				case Instruction::Trunc:
				case Instruction::FPExt:
				case Instruction::FPTrunc:
				case Instruction::BitCast:
					break;

				case Instruction::Ret:
				case Instruction::Switch:
				case Instruction::Br:
				case Instruction::Select:
				case Instruction::IndirectBr:
				case Instruction::Invoke:
				case Instruction::Resume:
				case Instruction::Unreachable:
				case Instruction::CleanupRet:
				case Instruction::CatchRet:
				case Instruction::CatchSwitch:
				case Instruction::CallBr:
				case Instruction::Fence:
				case Instruction::AtomicCmpXchg:
				case Instruction::AtomicRMW:
				case Instruction::PtrToInt:
				case Instruction::IntToPtr:
				case Instruction::AddrSpaceCast:
				case Instruction::CleanupPad:
				case Instruction::CatchPad:
				case Instruction::UserOp1:
				case Instruction::UserOp2:
				case Instruction::VAArg:
				case Instruction::ExtractElement:
				case Instruction::InsertElement:
				case Instruction::ShuffleVector:
				case Instruction::ExtractValue:
				case Instruction::InsertValue:
				case Instruction::LandingPad:
				case Instruction::Freeze:
					break;
				default:
					break;
			}
		}
	}

	adaptTypeCast(llvmIrFunction, quantizedType);

	return;
}
}
