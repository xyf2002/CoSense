/*
 * Cortex-A53 Schedule Cost Table
 *
 * Latencies extracted from LLVM AArch64SchedA53.td
 * Reference: https://github.com/llvm/llvm-project/blob/main/llvm/lib/Target/AArch64/AArch64SchedA53.td
 *
 * All latencies in cycles for Cortex-A53 in-order dual-issue pipeline
 */

#ifndef ARM_SCHEDULE_A53_COSTS_H
#define ARM_SCHEDULE_A53_COSTS_H

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

namespace A53Costs
{

/*
 * FP Instruction Latencies (from AArch64SchedA53.td)
 *
 * WriteF          FADD, FSUB, FNEG          6 cycles
 * WriteFMul       FMUL                      6 cycles
 * A53WriteFDivSP  FDIV (SP)                18 cycles
 * A53WriteFDivDP  FDIV (DP)                33 cycles
 * WriteFCvt       FCVT (int<->float)        6 cycles
 */

struct FPCostTable {
	unsigned     opcode;
	double	     latency;
	const char * writeRes;
};

static const FPCostTable FP_COSTS[] = {
    {llvm::Instruction::FAdd, 6.0, "WriteF"},
    {llvm::Instruction::FSub, 6.0, "WriteF"},
    {llvm::Instruction::FNeg, 6.0, "WriteF"},
    {llvm::Instruction::FMul, 6.0, "WriteFMul"},
    {llvm::Instruction::FDiv, 25.5, "A53WriteFDivSP/DP"},
    {llvm::Instruction::FRem, 25.5, "A53WriteFDivSP/DP"},
    {llvm::Instruction::FPToSI, 6.0, "WriteFCvt"},
    {llvm::Instruction::FPToUI, 6.0, "WriteFCvt"},
    {llvm::Instruction::SIToFP, 6.0, "WriteFCvt"},
    {llvm::Instruction::UIToFP, 6.0, "WriteFCvt"},
};

/*
 * Integer Instruction Latencies (from AArch64SchedA53.td)
 *
 * WriteI          ADD, SUB, AND, ORR, XOR  3 cycles
 * WriteIM32/64    MUL                      4 cycles
 * WriteID32/64    SDIV, UDIV               4 cycles
 */

struct IntCostTable {
	unsigned     opcode;
	double	     latency;
	const char * writeRes;
};

static const IntCostTable INT_COSTS[] = {
    {llvm::Instruction::Add, 3.0, "WriteI"},
    {llvm::Instruction::Sub, 3.0, "WriteI"},
    {llvm::Instruction::And, 3.0, "WriteI"},
    {llvm::Instruction::Or, 3.0, "WriteI"},
    {llvm::Instruction::Xor, 3.0, "WriteI"},
    {llvm::Instruction::Shl, 3.0, "WriteI"},
    {llvm::Instruction::AShr, 3.0, "WriteI"},
    {llvm::Instruction::LShr, 3.0, "WriteI"},
    {llvm::Instruction::Mul, 4.0, "WriteIM32/64"},
    {llvm::Instruction::SDiv, 4.0, "WriteID32/64"},
    {llvm::Instruction::UDiv, 4.0, "WriteID32/64"},
    {llvm::Instruction::SRem, 4.0, "WriteID32/64"},
    {llvm::Instruction::URem, 4.0, "WriteID32/64"},
};

/*
 * Memory and Control Flow Latencies
 *
 * WriteLD/WriteST   Load/Store              4 cycles
 * WriteBr           Branch                  1 cycle
 */

static const double MEMORY_LATENCY    = 4.0;
static const double BRANCH_LATENCY    = 1.0;
static const double CALL_LATENCY      = 3.0;
static const double MATH_CALL_LATENCY = 20.0;

/*
 * Target Profile Parameters
 *
 * The decision formula is:
 *   shouldQuantize = effectiveCfp > (effectiveQuant + effectiveCfp * marginFactor)
 */

struct TargetProfileParams {
	double fpFactor;
	double intFactor;
	double qFactor;
	double dqFactor;
	double marginFactor;
	bool   hasFPU;
};

static const TargetProfileParams A53_PROFILE = {
    .fpFactor	  = 1.0,
    .intFactor	  = 1.0,
    .qFactor	  = 0.2,
    .dqFactor	  = 0.2,
    .marginFactor = 0.10,
    .hasFPU	  = true,
};

/*
 * Lookup Functions
 */

inline double
getFPOpCost(unsigned opcode)
{
	for (const auto & entry : FP_COSTS)
	{
		if (entry.opcode == opcode)
			return entry.latency;
	}
	return 0.0;
}

inline double
getIntOpCost(unsigned opcode)
{
	for (const auto & entry : INT_COSTS)
	{
		if (entry.opcode == opcode)
			return entry.latency;
	}
	return 0.0;
}

inline double
getMemoryCost()
{
	return MEMORY_LATENCY;
}
inline double
getBranchCost()
{
	return BRANCH_LATENCY;
}
inline double
getCallCost()
{
	return CALL_LATENCY;
}
inline double
getMathCallCost()
{
	return MATH_CALL_LATENCY;
}

}  // namespace A53Costs

#endif /* ARM_SCHEDULE_A53_COSTS_H */
