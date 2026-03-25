#include "newton-irPass-LLVMIR-quantDecider.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#include "config.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "newton-irPass-LLVMIR-quantization.h"

using namespace llvm;

namespace
{
struct CostBreakdown {
	double cfp;
	double cint;
	double cq;
	double cdq;
	double ccf;
	int    instructionCount;
	int    fpClusterInstructionCount;
};

struct TargetProfile {
	bool   hasFPU;
	double fpFactor;
	double intFactor;
	double qFactor;
	double dqFactor;
	double marginFactor;
	bool   confident;
};

struct ScheduleModel {
	TargetProfile		   profile;
	std::map<unsigned, double> opcodeCost;
	double			   mathCallCost;
	double			   unknownFpCost;
	bool			   loaded;
};

static ScheduleModel gScheduleModel = {{false, 1.0, 1.0, 1.0, 1.0, 0.05, false}, {}, 20.0, 1.0, false};

static std::string collectTargetFeatures(const Module & module);
static bool	   containsToken(const std::string & haystack, const std::string & token);

static std::string
trim(const std::string & input)
{
	const std::string whitespace = " \t\r\n";
	const auto	  begin	     = input.find_first_not_of(whitespace);
	if (begin == std::string::npos)
		return "";
	const auto end = input.find_last_not_of(whitespace);
	return input.substr(begin, end - begin + 1);
}

static bool
startsWith(const std::string & text, const std::string & prefix)
{
	return text.rfind(prefix, 0) == 0;
}

static std::string
stripQuotes(const std::string & value)
{
	std::string trimmed = trim(value);
	if (trimmed.size() >= 2 &&
	    ((trimmed.front() == '"' && trimmed.back() == '"') ||
	     (trimmed.front() == '\'' && trimmed.back() == '\'')))
	{
		return trimmed.substr(1, trimmed.size() - 2);
	}
	return trimmed;
}

static bool
parseBool(const std::string & value)
{
	std::string lowered = stripQuotes(value);
	std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
	return lowered == "true" || lowered == "1" || lowered == "yes";
}

static double
parseDouble(const std::string & value, double fallbackValue)
{
	std::stringstream stream(stripQuotes(value));
	double		  parsed = fallbackValue;
	stream >> parsed;
	return stream.fail() ? fallbackValue : parsed;
}

static unsigned
opcodeFromToken(const std::string & token)
{
	if (token == "FAdd")
		return Instruction::FAdd;
	if (token == "FSub")
		return Instruction::FSub;
	if (token == "FMul")
		return Instruction::FMul;
	if (token == "FDiv")
		return Instruction::FDiv;
	if (token == "FRem")
		return Instruction::FRem;
	if (token == "FNeg")
		return Instruction::FNeg;
	if (token == "FPToSI")
		return Instruction::FPToSI;
	if (token == "FPToUI")
		return Instruction::FPToUI;
	if (token == "SIToFP")
		return Instruction::SIToFP;
	if (token == "UIToFP")
		return Instruction::UIToFP;
	if (token == "Add")
		return Instruction::Add;
	if (token == "Sub")
		return Instruction::Sub;
	if (token == "And")
		return Instruction::And;
	if (token == "Or")
		return Instruction::Or;
	if (token == "Xor")
		return Instruction::Xor;
	if (token == "Shl")
		return Instruction::Shl;
	if (token == "AShr")
		return Instruction::AShr;
	if (token == "LShr")
		return Instruction::LShr;
	if (token == "Mul")
		return Instruction::Mul;
	if (token == "SDiv")
		return Instruction::SDiv;
	if (token == "UDiv")
		return Instruction::UDiv;
	if (token == "SRem")
		return Instruction::SRem;
	if (token == "URem")
		return Instruction::URem;
	return 0;
}

static std::string
resolveProfileKey(const Module & module)
{
	const char * envProfile = std::getenv("NEWTON_QUANT_PROFILE");
	if (envProfile && std::string(envProfile).size() > 0)
		return std::string(envProfile);

	std::string triple	= module.getTargetTriple();
	std::string features	= collectTargetFeatures(module);
	bool	    explicitFPU = containsToken(features, "+vfp") || containsToken(features, "+neon") ||
			   containsToken(features, "+fp-armv8") || containsToken(features, "+fp16") ||
			   containsToken(features, "+vfp4") || containsToken(features, "+fpv4-sp-d16");
	bool softFloat	= containsToken(features, "+soft-float") || containsToken(features, "+softfp");
	bool isAArch64	= containsToken(triple, "aarch64");
	bool isThumb	= containsToken(triple, "thumb");
	bool isMProfile = containsToken(triple, "armv6-m") || containsToken(triple, "armv6m") ||
			  containsToken(triple, "armv7-m") || containsToken(triple, "armv7m") ||
			  containsToken(triple, "armv7e-m") || containsToken(triple, "armv7em") ||
			  containsToken(triple, "armv8-m") || containsToken(triple, "armv8m") ||
			  isThumb;

	if (isAArch64)
		return "arm-a-profile";
	if (isMProfile && (!explicitFPU || softFloat))
		return "arm-m-profile-soft";
	if (isMProfile && explicitFPU)
		return "arm-m-profile-fpu";
	if (containsToken(triple, "x86_64") || containsToken(triple, "x86"))
		return "analysis-default";
	return "generic-default";
}

static bool
loadScheduleModelFromFile(const std::string & profileKey, const std::string & filePath,
			  ScheduleModel & model)
{
	std::ifstream input(filePath);
	if (!input.is_open())
		return false;

	std::string line;
	bool	    insideTargetBlock = false;
	while (std::getline(input, line))
	{
		std::string content = trim(line);
		if (content.empty() || startsWith(content, "//") || startsWith(content, "#"))
			continue;

		if (startsWith(content, "def NewtonScheduleModel<"))
		{
			size_t quoteStart = content.find('"');
			size_t quoteEnd	  = content.find('"', quoteStart + 1);
			if (quoteStart != std::string::npos && quoteEnd != std::string::npos)
			{
				std::string modelKey = content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
				insideTargetBlock    = (modelKey == profileKey);
			}
			continue;
		}

		if (!insideTargetBlock)
			continue;

		if (content == "}")
			break;

		if (startsWith(content, "let "))
		{
			size_t eqPos	    = content.find('=');
			size_t semicolonPos = content.rfind(';');
			if (eqPos == std::string::npos || semicolonPos == std::string::npos || semicolonPos <= eqPos)
				continue;

			std::string key	  = trim(content.substr(4, eqPos - 4));
			std::string value = trim(content.substr(eqPos + 1, semicolonPos - eqPos - 1));
			if (key == "HasFPU")
				model.profile.hasFPU = parseBool(value);
			else if (key == "FpFactor")
				model.profile.fpFactor = parseDouble(value, model.profile.fpFactor);
			else if (key == "IntFactor")
				model.profile.intFactor = parseDouble(value, model.profile.intFactor);
			else if (key == "QFactor")
				model.profile.qFactor = parseDouble(value, model.profile.qFactor);
			else if (key == "DQFactor")
				model.profile.dqFactor = parseDouble(value, model.profile.dqFactor);
			else if (key == "MarginFactor")
				model.profile.marginFactor = parseDouble(value, model.profile.marginFactor);
			else if (key == "Confident")
				model.profile.confident = parseBool(value);
			else if (key == "MathCallCost")
				model.mathCallCost = parseDouble(value, model.mathCallCost);
			else if (key == "UnknownFpCost")
				model.unknownFpCost = parseDouble(value, model.unknownFpCost);
			continue;
		}

		if (startsWith(content, "def COST_"))
		{
			size_t lt    = content.find('<');
			size_t comma = content.find(',');
			size_t gt    = content.find('>');
			if (lt == std::string::npos || comma == std::string::npos || gt == std::string::npos || comma <= lt)
				continue;

			std::string opToken = stripQuotes(content.substr(lt + 1, comma - lt - 1));
			double	    cost    = parseDouble(content.substr(comma + 1, gt - comma - 1), 0.0);
			unsigned    opcode  = opcodeFromToken(opToken);
			if (opcode != 0)
				model.opcodeCost[opcode] = cost;
		}
	}

	model.loaded = !model.opcodeCost.empty();
	return model.loaded;
}

static ScheduleModel
resolveScheduleModel(const Module & module)
{
	ScheduleModel model	 = {{false, 1.0, 1.0, 1.0, 1.0, 0.05, false}, {}, 20.0, 1.0, false};
	std::string   profileKey = resolveProfileKey(module);

	std::vector<std::string> candidateFiles;
	const char *		 envScheduleFile = std::getenv("NEWTON_QUANT_SCHEDULE_FILE");
	if (envScheduleFile && std::string(envScheduleFile).size() > 0)
		candidateFiles.push_back(std::string(envScheduleFile));
	candidateFiles.push_back("Schedule/DefaultSchedule.td");
	candidateFiles.push_back("./Schedule/DefaultSchedule.td");
	candidateFiles.push_back("src/newton/Schedule/DefaultSchedule.td");
	candidateFiles.push_back("ScheduleProfiles.td");
	candidateFiles.push_back("./ScheduleProfiles.td");
	candidateFiles.push_back("src/newton/ScheduleProfiles.td");

	for (const std::string & filePath : candidateFiles)
	{
		if (loadScheduleModelFromFile(profileKey, filePath, model))
			return model;
	}

	model.profile	    = {false, 1.4, 1.0, 1.2, 1.2, 0.06, false};
	model.unknownFpCost = 1.0;
	model.mathCallCost  = 20.0;
	model.loaded	    = false;
	return model;
}

static bool
isMathRuntimeCall(const CallBase & callBase)
{
	Function * callee = callBase.getCalledFunction();
	if (!callee || !callee->hasName())
		return false;

	StringRef name = callee->getName();
	return name == "log" || name == "logf" ||
	       name == "exp" || name == "expf" ||
	       name == "sqrt" || name == "sqrtf" ||
	       name == "sin" || name == "sinf" ||
	       name == "cos" || name == "cosf" ||
	       name == "log1p" || name == "log1pf" ||
	       name.startswith("llvm.sqrt") ||
	       name.startswith("llvm.log") ||
	       name.startswith("llvm.exp") ||
	       name.startswith("llvm.sin") ||
	       name.startswith("llvm.cos");
}

static bool
usesOrProducesFloatingPoint(const Instruction & instruction)
{
	if (instruction.getType()->isFloatingPointTy())
		return true;

	for (const Value * operand : instruction.operands())
	{
		if (operand->getType()->isFloatingPointTy())
			return true;
		if (operand->getType()->isPointerTy() && operand->getType()->getPointerElementType()->isFloatingPointTy())
			return true;
	}

	if (const auto * callBase = dyn_cast<CallBase>(&instruction))
	{
		if (isMathRuntimeCall(*callBase))
			return true;
		if (callBase->getType()->isFloatingPointTy())
			return true;
		for (const Value * arg : callBase->args())
		{
			if (arg->getType()->isFloatingPointTy())
				return true;
		}
	}

	return false;
}

static bool
isCoreFloatingPointOp(const Instruction & instruction)
{
	switch (instruction.getOpcode())
	{
		case Instruction::FAdd:
		case Instruction::FSub:
		case Instruction::FMul:
		case Instruction::FDiv:
		case Instruction::FRem:
		case Instruction::FNeg:
			return true;
		case Instruction::Call:
		{
			const auto * callBase = dyn_cast<CallBase>(&instruction);
			return callBase && isMathRuntimeCall(*callBase);
		}
		default:
			return false;
	}
}

static double
floatingPointOpCost(const Instruction & instruction)
{
	if (!isCoreFloatingPointOp(instruction) && instruction.getOpcode() != Instruction::Call)
		return 0.0;

	auto   costIt = gScheduleModel.opcodeCost.find(instruction.getOpcode());
	double cost   = (costIt != gScheduleModel.opcodeCost.end()) ? costIt->second : 0.0;
	if (cost > 0.0)
		return cost;

	if (instruction.getOpcode() == Instruction::Call)
		return gScheduleModel.mathCallCost;

	return 0.0;
}

static double
integerOpCost(const Instruction & instruction)
{
	switch (instruction.getOpcode())
	{
		case Instruction::Add:
		case Instruction::Sub:
		case Instruction::And:
		case Instruction::Or:
		case Instruction::Xor:
		case Instruction::Shl:
		case Instruction::AShr:
		case Instruction::LShr:
		case Instruction::Mul:
		case Instruction::SDiv:
		case Instruction::UDiv:
		case Instruction::SRem:
		case Instruction::URem:
			break;
		default:
			return 0.0;
	}

	auto costIt = gScheduleModel.opcodeCost.find(instruction.getOpcode());
	return (costIt != gScheduleModel.opcodeCost.end()) ? costIt->second : 0.0;
}

static double
quantizationBoundaryCost(const Instruction & instruction)
{
	if (instruction.getOpcode() != Instruction::FPToSI &&
	    instruction.getOpcode() != Instruction::FPToUI)
		return 0.0;

	auto costIt = gScheduleModel.opcodeCost.find(instruction.getOpcode());
	return (costIt != gScheduleModel.opcodeCost.end()) ? costIt->second : 0.0;
}

static double
dequantizationBoundaryCost(const Instruction & instruction)
{
	if (instruction.getOpcode() != Instruction::SIToFP &&
	    instruction.getOpcode() != Instruction::UIToFP)
		return 0.0;

	auto costIt = gScheduleModel.opcodeCost.find(instruction.getOpcode());
	return (costIt != gScheduleModel.opcodeCost.end()) ? costIt->second : 0.0;
}

static double
estimateBasicBlockWeight(const Function & function, const BasicBlock & block,
			 const std::map<const BasicBlock *, int> & blockOrder)
{
	double weight		= 1.0;
	int    predecessorCount = 0;
	for (const BasicBlock * ignored : predecessors(&block))
	{
		(void)ignored;
		predecessorCount++;
	}
	weight += 0.10 * predecessorCount;

	const Instruction * terminator = block.getTerminator();
	if (terminator)
	{
		if (terminator->getOpcode() == Instruction::Br)
		{
			const auto * branchInst = cast<BranchInst>(terminator);
			if (branchInst->isConditional())
				weight += 0.35;
		}
		else if (terminator->getOpcode() == Instruction::Switch)
		{
			const auto * switchInst = cast<SwitchInst>(terminator);
			weight += 0.05 * (switchInst->getNumCases() + 1);
		}
	}

	auto currentIt = blockOrder.find(&block);
	if (currentIt != blockOrder.end())
	{
		int currentIndex = currentIt->second;
		for (const BasicBlock * successor : successors(&block))
		{
			auto succIt = blockOrder.find(successor);
			if (succIt != blockOrder.end() && succIt->second <= currentIndex)
			{
				weight += 0.70;
			}
		}
	}

	if (function.getEntryBlock().getName().equals(block.getName()))
		weight += 0.05;

	return weight;
}

static CostBreakdown
estimateCostBreakdown(const Function & function)
{
	CostBreakdown breakdown = {0.0, 0.0, 0.0, 0.0, 0.0, 0, 0};

	std::map<const BasicBlock *, int> blockOrder;
	int				  index = 0;
	for (const BasicBlock & block : function)
	{
		blockOrder.emplace(&block, index++);
	}

	for (const BasicBlock & block : function)
	{
		double blockWeight = estimateBasicBlockWeight(function, block, blockOrder);
		double blockCfp	   = 0.0;
		double blockCint   = 0.0;
		double blockCq	   = 0.0;
		double blockCdq	   = 0.0;

		for (const Instruction & instruction : block)
		{
			if (isCoreFloatingPointOp(instruction))
			{
				blockCfp += floatingPointOpCost(instruction);
				breakdown.fpClusterInstructionCount++;
			}
			else if (usesOrProducesFloatingPoint(instruction))
			{
				blockCfp += gScheduleModel.unknownFpCost;
				breakdown.fpClusterInstructionCount++;
			}

			blockCint += integerOpCost(instruction);
			blockCq += quantizationBoundaryCost(instruction);
			blockCdq += dequantizationBoundaryCost(instruction);
			breakdown.instructionCount++;
		}

		double blockControlFlowCost = 0.0;
		if (const Instruction * terminator = block.getTerminator())
		{
			switch (terminator->getOpcode())
			{
				case Instruction::Br:
				{
					const auto * branchInst = cast<BranchInst>(terminator);
					blockControlFlowCost += branchInst->isConditional() ? 2.0 : 0.7;
					break;
				}
				case Instruction::Switch:
				{
					const auto * switchInst = cast<SwitchInst>(terminator);
					blockControlFlowCost += 3.0 + 0.25 * (switchInst->getNumCases() + 1);
					break;
				}
				default:
					break;
			}
		}

		breakdown.cfp += blockWeight * blockCfp;
		breakdown.cint += blockWeight * blockCint;
		breakdown.cq += blockWeight * blockCq;
		breakdown.cdq += blockWeight * blockCdq;
		breakdown.ccf += blockWeight * blockControlFlowCost;
	}

	return breakdown;
}

static std::string
collectTargetFeatures(const Module & module)
{
	std::string merged;
	for (const Function & function : module)
	{
		if (function.hasFnAttribute("target-features"))
		{
			if (!merged.empty())
				merged += ",";
			merged += function.getFnAttribute("target-features").getValueAsString().str();
		}
	}
	return merged;
}

static bool
containsToken(const std::string & haystack, const std::string & token)
{
	return haystack.find(token) != std::string::npos;
}

static TargetProfile
detectTargetProfile(const Module & module)
{
	gScheduleModel = resolveScheduleModel(module);
	return gScheduleModel.profile;
}

static bool
hasFloatingPointCluster(const Function & function)
{
	for (const BasicBlock & block : function)
	{
		for (const Instruction & instruction : block)
		{
			if (isCoreFloatingPointOp(instruction) || usesOrProducesFloatingPoint(instruction))
				return true;
		}
	}
	return false;
}

static int
countFloatingPointOperations(const Function & function)
{
	int operationCount = 0;
	for (const BasicBlock & block : function)
	{
		for (const Instruction & instruction : block)
		{
			if (isCoreFloatingPointOp(instruction))
				operationCount++;
		}
	}
	return operationCount;
}
}  // namespace

extern "C" {

QuantDeciderResult
irPassLLVMIRQuantDecideFunction(void * N, Module & module, Function & llvmIrFunction, int maxPrecisionBits)
{
	QuantDeciderResult result = {false, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, false};

	if (llvmIrFunction.isDeclaration())
		return result;

	if (!hasFloatingPointCluster(llvmIrFunction))
		return result;

	TargetProfile profile = detectTargetProfile(module);
	result.targetHasFPU   = profile.hasFPU;

	CostBreakdown originalBreakdown	 = estimateCostBreakdown(llvmIrFunction);
	result.originalInstructionCount	 = originalBreakdown.instructionCount;
	result.fpClusterInstructionCount = originalBreakdown.fpClusterInstructionCount;

	std::unique_ptr<Module> clonedModule = CloneModule(module);
	if (!clonedModule)
		return result;

	Function * clonedFunction = clonedModule->getFunction(llvmIrFunction.getName());
	if (!clonedFunction)
		return result;

	std::vector<Function *> functionsToInsert;
	irPassLLVMIRAutoQuantization(reinterpret_cast<State *>(N), *clonedFunction, functionsToInsert, maxPrecisionBits);

	CostBreakdown quantizedBreakdown = estimateCostBreakdown(*clonedFunction);
	result.quantizedInstructionCount = quantizedBreakdown.instructionCount;

	result.cfpCost	       = profile.fpFactor * originalBreakdown.cfp;
	result.cintCost	       = profile.intFactor * quantizedBreakdown.cint;
	result.cqCost	       = profile.qFactor * quantizedBreakdown.cq;
	result.cdqCost	       = profile.dqFactor * quantizedBreakdown.cdq;
	result.controlFlowCost = 0.5 * (originalBreakdown.ccf + quantizedBreakdown.ccf);

	result.originalCost  = result.cfpCost + originalBreakdown.ccf;
	result.quantizedCost = result.cintCost + result.cqCost + result.cdqCost + quantizedBreakdown.ccf;

	double effectiveCfp   = result.cfpCost + result.controlFlowCost;
	double effectiveQuant = result.cintCost + result.cqCost + result.cdqCost + result.controlFlowCost;
	result.decisionMargin = effectiveCfp * profile.marginFactor;
	if (!profile.confident)
		result.decisionMargin = (std::max)(result.decisionMargin, 4.0);

	int    fpOps		= countFloatingPointOperations(llvmIrFunction);
	double speedupEstimate	= effectiveCfp / (std::max)(effectiveQuant, 1e-6);
	bool   hasEnoughFpWork	= fpOps >= 6;
	bool   hasEnoughSpeedup = speedupEstimate >= 1.20;
	bool   baseDecision	= hasEnoughFpWork && hasEnoughSpeedup &&
			    effectiveCfp > (effectiveQuant + result.decisionMargin);
	bool lowFpHighSpeedupOutlier = fpOps >= 15 && fpOps <= 30 && speedupEstimate > 2.5 &&
				       result.controlFlowCost < 30.0;
	bool mediumFpBorderline = fpOps >= 35 && fpOps <= 50 && speedupEstimate < 2.0 &&
				  result.controlFlowCost > 50.0;
	bool highControlFlowPenalty = fpOps >= 40 && result.controlFlowCost > 60.0 &&
				      result.cintCost < 100.0;
	result.shouldQuantize = baseDecision && !lowFpHighSpeedupOutlier &&
				!mediumFpBorderline && !highControlFlowPenalty;

	errs() << "[quant-decider] function=" << llvmIrFunction.getName()
	       << " targetHasFPU=" << (result.targetHasFPU ? "true" : "false")
	       << " Cfp=" << result.cfpCost
	       << " Cint=" << result.cintCost
	       << " Cq=" << result.cqCost
	       << " Cdq=" << result.cdqCost
	       << " Ccf=" << result.controlFlowCost
	       << " fpOps=" << fpOps
	       << " speedup=" << speedupEstimate
	       << " margin=" << result.decisionMargin
	       << " shouldQuantize=" << (result.shouldQuantize ? "true" : "false") << "\n";

	return result;
}

void
irPassLLVMIRQuantDecider(void * N, Module & module, int maxPrecisionBits,
			 std::map<std::string, QuantDeciderResult> & decisionMap)
{
	decisionMap.clear();

	for (Function & function : module)
	{
		if (function.isDeclaration())
			continue;

		QuantDeciderResult result = irPassLLVMIRQuantDecideFunction(N, module, function, maxPrecisionBits);
		decisionMap.emplace(function.getName().str(), result);
	}

	for (const auto & entry : decisionMap)
	{
		Function * function = module.getFunction(entry.first);
		if (!function)
			continue;

		int fpOperationCount = countFloatingPointOperations(*function);
		if (fpOperationCount > 0)
		{
			errs() << "Function: " << entry.first
			       << " - Floating-point operations: " << fpOperationCount << "\n";
		}
	}
}
}
