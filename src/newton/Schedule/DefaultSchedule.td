//==- DefaultSchedule.td - Newton quantization fallback schedules -*- tablegen -*-==//

// This file is the rollback/default source for profile loading.

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

def NewtonScheduleModel<"analysis-default"> {
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

def NewtonScheduleModel<"arm-m-profile-soft"> {
  let HasFPU = false;
  let FpFactor = 2.30;
  let IntFactor = 0.95;
  let QFactor = 1.25;
  let DQFactor = 1.30;
  let MarginFactor = 0.02;
  let Confident = true;
  let MathCallCost = 20.0;
  let UnknownFpCost = 1.0;

  def COST_FAdd : OpCost<"FAdd", 4.0>;
  def COST_FSub : OpCost<"FSub", 4.0>;
  def COST_FNeg : OpCost<"FNeg", 3.0>;
  def COST_FMul : OpCost<"FMul", 5.0>;
  def COST_FDiv : OpCost<"FDiv", 14.0>;
  def COST_FRem : OpCost<"FRem", 14.0>;
  def COST_FPToSI : OpCost<"FPToSI", 6.0>;
  def COST_FPToUI : OpCost<"FPToUI", 6.0>;
  def COST_SIToFP : OpCost<"SIToFP", 6.0>;
  def COST_UIToFP : OpCost<"UIToFP", 6.0>;

  def COST_Add : OpCost<"Add", 1.0>;
  def COST_Sub : OpCost<"Sub", 1.0>;
  def COST_And : OpCost<"And", 1.0>;
  def COST_Or : OpCost<"Or", 1.0>;
  def COST_Xor : OpCost<"Xor", 1.0>;
  def COST_Shl : OpCost<"Shl", 1.0>;
  def COST_AShr : OpCost<"AShr", 1.0>;
  def COST_LShr : OpCost<"LShr", 1.0>;
  def COST_Mul : OpCost<"Mul", 2.0>;
  def COST_SDiv : OpCost<"SDiv", 8.0>;
  def COST_UDiv : OpCost<"UDiv", 8.0>;
  def COST_SRem : OpCost<"SRem", 8.0>;
  def COST_URem : OpCost<"URem", 8.0>;
}

def NewtonScheduleModel<"arm-m-profile-fpu"> {
  let HasFPU = true;
  let FpFactor = 1.05;
  let IntFactor = 0.95;
  let QFactor = 1.10;
  let DQFactor = 1.15;
  let MarginFactor = 0.08;
  let Confident = true;
  let MathCallCost = 20.0;
  let UnknownFpCost = 1.0;

  def COST_FAdd : OpCost<"FAdd", 4.0>;
  def COST_FSub : OpCost<"FSub", 4.0>;
  def COST_FNeg : OpCost<"FNeg", 3.0>;
  def COST_FMul : OpCost<"FMul", 5.0>;
  def COST_FDiv : OpCost<"FDiv", 14.0>;
  def COST_FRem : OpCost<"FRem", 14.0>;
  def COST_FPToSI : OpCost<"FPToSI", 6.0>;
  def COST_FPToUI : OpCost<"FPToUI", 6.0>;
  def COST_SIToFP : OpCost<"SIToFP", 6.0>;
  def COST_UIToFP : OpCost<"UIToFP", 6.0>;

  def COST_Add : OpCost<"Add", 1.0>;
  def COST_Sub : OpCost<"Sub", 1.0>;
  def COST_And : OpCost<"And", 1.0>;
  def COST_Or : OpCost<"Or", 1.0>;
  def COST_Xor : OpCost<"Xor", 1.0>;
  def COST_Shl : OpCost<"Shl", 1.0>;
  def COST_AShr : OpCost<"AShr", 1.0>;
  def COST_LShr : OpCost<"LShr", 1.0>;
  def COST_Mul : OpCost<"Mul", 2.0>;
  def COST_SDiv : OpCost<"SDiv", 8.0>;
  def COST_UDiv : OpCost<"UDiv", 8.0>;
  def COST_SRem : OpCost<"SRem", 8.0>;
  def COST_URem : OpCost<"URem", 8.0>;
}

def NewtonScheduleModel<"generic-default"> {
  let HasFPU = false;
  let FpFactor = 1.40;
  let IntFactor = 1.00;
  let QFactor = 1.10;
  let DQFactor = 1.15;
  let MarginFactor = 0.06;
  let Confident = false;
  let MathCallCost = 20.0;
  let UnknownFpCost = 1.0;

  def COST_FAdd : OpCost<"FAdd", 4.0>;
  def COST_FSub : OpCost<"FSub", 4.0>;
  def COST_FNeg : OpCost<"FNeg", 3.0>;
  def COST_FMul : OpCost<"FMul", 5.0>;
  def COST_FDiv : OpCost<"FDiv", 14.0>;
  def COST_FRem : OpCost<"FRem", 14.0>;
  def COST_FPToSI : OpCost<"FPToSI", 6.0>;
  def COST_FPToUI : OpCost<"FPToUI", 6.0>;
  def COST_SIToFP : OpCost<"SIToFP", 6.0>;
  def COST_UIToFP : OpCost<"UIToFP", 6.0>;

  def COST_Add : OpCost<"Add", 1.0>;
  def COST_Sub : OpCost<"Sub", 1.0>;
  def COST_And : OpCost<"And", 1.0>;
  def COST_Or : OpCost<"Or", 1.0>;
  def COST_Xor : OpCost<"Xor", 1.0>;
  def COST_Shl : OpCost<"Shl", 1.0>;
  def COST_AShr : OpCost<"AShr", 1.0>;
  def COST_LShr : OpCost<"LShr", 1.0>;
  def COST_Mul : OpCost<"Mul", 2.0>;
  def COST_SDiv : OpCost<"SDiv", 8.0>;
  def COST_UDiv : OpCost<"UDiv", 8.0>;
  def COST_SRem : OpCost<"SRem", 8.0>;
  def COST_URem : OpCost<"URem", 8.0>;
}
