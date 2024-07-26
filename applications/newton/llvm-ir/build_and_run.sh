#!/bin/bash

# Step 1: Generate LLVM IR file
echo "Step 1: Generate LLVM IR file"
clang -g -O0 -Xclang -disable-O0-optnone -S -emit-llvm -Wall -Wextra -o /home/xyf/CoSense/applications/newton/llvm-ir/floating_point_operations.ll /home/xyf/CoSense/applications/newton/llvm-ir/c-files/floating_point_operations.c

# Step 2: Use newton for optimization and quantization
echo "Step 2: Use newton for optimization and quantization"
cd /home/xyf/CoSense/src/newton && ./newton-linux-EN --llvm-ir=/home/xyf/CoSense/applications/newton/llvm-ir/floating_point_operations.ll --llvm-ir-liveness-check --llvm-ir-auto-quantization /home/xyf/CoSense/applications/newton/sensors/test.nt

# Step 3: Convert generated bytecode file to LLVM IR file
echo "Step 3: Convert generated bytecode file to LLVM IR file"
llvm-dis /home/xyf/CoSense/applications/newton/llvm-ir/floating_point_operations_output.bc -o /home/xyf/CoSense/applications/newton/llvm-ir/floating_point_operations_output.ll

# Step 4: Optimize the generated LLVM IR file
echo "Step 4: Optimize the generated LLVM IR file"
opt /home/xyf/CoSense/applications/newton/llvm-ir/floating_point_operations_output.ll --simplifycfg --instsimplify -O3 -Os -S -o /home/xyf/CoSense/applications/newton/llvm-ir/out.llperformace

# Step 5: Compile the optimized LLVM IR file to bitcode
echo "Step 5: Compile the optimized LLVM IR file to bitcode"
llvm-as /home/xyf/CoSense/applications/newton/llvm-ir/out.llperformace -o /home/xyf/CoSense/applications/newton/llvm-ir/out.bc

# Step 6: Compile the bitcode file to assembly
echo "Step 6: Compile the bitcode file to assembly"
llc /home/xyf/CoSense/applications/newton/llvm-ir/out.bc -o /home/xyf/CoSense/applications/newton/llvm-ir/out.s

# Step 7: Compile the assembly file to object file
echo "Step 7: Compile the assembly file to object file"
clang -c /home/xyf/CoSense/applications/newton/llvm-ir/out.s -o /home/xyf/CoSense/applications/newton/llvm-ir/out.o

# Step 8: Package the object file into a static library
echo "Step 8: Package the object file into a static library"
ar -rc /home/xyf/CoSense/applications/newton/llvm-ir/libout.a /home/xyf/CoSense/applications/newton/llvm-ir/out.o

# Step 9: Compile the test file and link with the static library
echo "Step 9: Compile the test file and link with the static library"
clang /home/xyf/CoSense/applications/newton/llvm-ir/c-files/test_floating_point_operations.c -no-pie -L/home/xyf/CoSense/applications/newton/llvm-ir -lout -O3 -Os -g -fno-builtin -o /home/xyf/CoSense/applications/newton/llvm-ir/main_out -lm

# Step 10: Run the test executable
echo "Step 10: Run the test executable"
/home/xyf/CoSense/applications/newton/llvm-ir/main_out
