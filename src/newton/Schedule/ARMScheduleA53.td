//==- ARMScheduleA53.td - Newton quantization schedule profile -*- tablegen -*-==//

def NewtonScheduleModel<"arm-a-profile"> {
  let HasFPU = true;
  let FpFactor = 1.0;
  let IntFactor = 1.0;
  let QFactor = 0.2;
  let DQFactor = 0.2;
  let MarginFactor = 0.10;
  let Confident = true;
  let MathCallCost = 20.0;
  let UnknownFpCost = 1.0;

  def COST_FAdd : OpCost<"FAdd", 6.0>;
  def COST_FSub : OpCost<"FSub", 6.0>;
  def COST_FNeg : OpCost<"FNeg", 6.0>;
  def COST_FMul : OpCost<"FMul", 6.0>;
  def COST_FDiv : OpCost<"FDiv", 25.5>;
  def COST_FRem : OpCost<"FRem", 25.5>;
  def COST_FPToSI : OpCost<"FPToSI", 6.0>;
  def COST_FPToUI : OpCost<"FPToUI", 6.0>;
  def COST_SIToFP : OpCost<"SIToFP", 6.0>;
  def COST_UIToFP : OpCost<"UIToFP", 6.0>;

  def COST_Add : OpCost<"Add", 3.0>;
  def COST_Sub : OpCost<"Sub", 3.0>;
  def COST_And : OpCost<"And", 3.0>;
  def COST_Or : OpCost<"Or", 3.0>;
  def COST_Xor : OpCost<"Xor", 3.0>;
  def COST_Shl : OpCost<"Shl", 3.0>;
  def COST_AShr : OpCost<"AShr", 3.0>;
  def COST_LShr : OpCost<"LShr", 3.0>;
  def COST_Mul : OpCost<"Mul", 4.0>;
  def COST_SDiv : OpCost<"SDiv", 4.0>;
  def COST_UDiv : OpCost<"UDiv", 4.0>;
  def COST_SRem : OpCost<"SRem", 4.0>;
  def COST_URem : OpCost<"URem", 4.0>;
}
