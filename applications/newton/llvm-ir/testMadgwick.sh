#!/bin/bash

# Get the current user's home directory
USER_HOME=$HOME

FILE_PATH="$USER_HOME/CoSense/applications/newton/llvm-ir/MadgwickAHRS_opt.ll"

 #Step 1: Generate LLVM IR file
echo "Step 1: Generate LLVM IR file"
clang -g0 -O0 -Xclang -disable-O0-optnone -S -emit-llvm -Wall -Wextra -o $USER_HOME/CoSense/applications/newton/llvm-ir/MadgwickAHRS.ll $USER_HOME/CoSense/applications/newton/llvm-ir/c-files/MadgwickAHRS.c

#clang -g -O0 -Xclang -disable-O0-optnone -fno-math-errno  -S -emit-llvm -Wall -Wextra -o $USER_HOME/CoSense/applications/newton/llvm-ir/MadgwickAHRS.ll $USER_HOME/CoSense/applications/newton/llvm-ir/c-files/MadgwickAHRS.c

#clang -O3 -ffast-math -S -emit-llvm -Wall -Wextra -o $HOME/CoSense/applications/newton/llvm-ir/MadgwickAHRS.ll
#$HOME/CoSense/applications/newton/llvm-ir/c-files/MadgwickAHRS.c

# Step 2: Use newton for optimization and quantization
echo "Step 2: Use newton for optimization and quantization"
cd $USER_HOME/CoSense/src/newton && ./newton-linux-EN --llvm-ir=$USER_HOME/CoSense/applications/newton/llvm-ir/MadgwickAHRS.ll --llvm-ir-liveness-check --llvm-ir-auto-quantization $USER_HOME/CoSense/applications/newton/sensors/test.nt
#
# Step 3: Convert generated bytecode file to LLVM IR file
echo "Step 3: Convert generated bytecode file to LLVM IR file"
#llvm-dis $USER_HOME/CoSense/applications/newton/llvm-ir/MadgwickAHRS_opt.bc -o $USER_HOME/CoSense/applications/newton/llvm-ir/MadgwickAHRS_opt.ll
cd $USER_HOME/CoSense/applications/newton/llvm-ir/&&
./replace.sh $USER_HOME/CoSense/applications/newton/llvm-ir/MadgwickAHRS_opt.ll

python3 replace.py


# Step 4: Optimize the generated LLVM IR file
echo "Step 4: Optimize the generated LLVM IR file"
opt $USER_HOME/CoSense/applications/newton/llvm-ir/MadgwickAHRS_opt.ll --simplifycfg --instsimplify  -O3 -Os -S -o $USER_HOME/CoSense/applications/newton/llvm-ir/out.llperformace

# Step 5: Compile the optimized LLVM IR file to bitcode
echo "Step 5: Compile the optimized LLVM IR file to bitcode"
llvm-as $USER_HOME/CoSense/applications/newton/llvm-ir/out.llperformace -o $USER_HOME/CoSense/applications/newton/llvm-ir/out.bc

# Step 6: Compile the bitcode file to assembly
echo "Step 6: Compile the bitcode file to assembly"
llc $USER_HOME/CoSense/applications/newton/llvm-ir/out.bc -o $USER_HOME/CoSense/applications/newton/llvm-ir/out.s

# Step 7: Compile the assembly file to object file
echo "Step 7: Compile the assembly file to object file"
clang -c $USER_HOME/CoSense/applications/newton/llvm-ir/out.s -o $USER_HOME/CoSense/applications/newton/llvm-ir/out.o

# Step 8: Package the object file into a static library
echo "Step 8: Package the object file into a static library"
ar -rc $USER_HOME/CoSense/applications/newton/llvm-ir/libout.a $USER_HOME/CoSense/applications/newton/llvm-ir/out.o

# Step 9: Compile the test file and link with the static library
echo "Step 9: Compile the test file and link with the static library"
clang $USER_HOME/CoSense/applications/newton/llvm-ir/c-files/test_MadgwickAHRS.c -no-pie -L$USER_HOME/CoSense/applications/newton/llvm-ir -lout -O3 -Os -g -fno-builtin -o $USER_HOME/CoSense/applications/newton/llvm-ir/main_out -lm

# Step 10: Run the test executable
echo "Step 10: Run the test executable"
/home/xyf/CoSense/applications/newton/llvm-ir/main_out
