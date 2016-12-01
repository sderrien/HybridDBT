/*
 * irISA.h
 *
 *  Created on: 24 nov. 2016
 *      Author: simon
 */

#ifndef INCLUDES_ISA_IRISA_H_
#define INCLUDES_ISA_IRISA_H_

#include <lib/ac_int.h>
#include <types.h>


/********************************************************************
 * Declaration of a data structure to represent the control flow of the binaries analyzed.
 * ******************************************************************
 *
 * The idea here is to offer a light representation of the control flow which can be built dynamically
 * and allow the DBT process to keep track of what has been done and what still need to be done.
 *
 * This representation will have to be extended during the development of the DBT tool in order to
 * track more advances information (like for example if a procedure was moved to a different location
 * or optimized with some specifics parameters...)
 *
 *******************************************************************/
class IRBlock;


/* IRProcedure is meant to represent a procedure in the binaries.
 * Its current implementation only store its start/end address and a pointer to the basic blocks*/
class IRProcedure
{
public:
	unsigned int vliwStartAddress;	//Address of the first instruction in the procedure
	unsigned int vliwEndAddress;   	//End address is the address of the first instruction not in the procedure
	IRBlock *blocks;				//A pointer to an array of blocks
	int nbBlock;

	unsigned int procedureState;	//A value to store its state (optimized/translated or other things like that)

	IRProcedure(int startAddress, int endAddress, IRBlock* blocks, int nbBlock);

};

/* IRBlock represent a block in the binaries.
 * The data structure only store start/end address as well as a pointer to some instructions if the IR was stored.*/
class IRBlock
{
public:
	unsigned int vliwStartAddress;	//Address of the first instruction in the block
	unsigned int vliwEndAddress;   	//End address is the address of the first instruction not in the block
	uint128 *instructions;			//A pointer to an array of uint128 describint the instructions

	unsigned int blockState;		//A value to store its state (optimized/translated or other things like that)
	unsigned short successor1;
	unsigned short successor2;

	IRBlock(int startAddress, int endAddress);

};

/********************************************************************
 * Declaration functions to assemble uint128 instruction for IR
 * ******************************************************************
 *
 * To represent blocks, the IR uses a set of uint128 instruction which describe the data-flow graph inside the block.
 * Above are defined a set of procedure which encode instruction following our encoding.
 *
 * TODO: also define instruction to decode them...
 *
 *******************************************************************/

ac_int<128, false> assembleRBytecodeInstruction(ac_int<2, false> stageCode, ac_int<1, false> isAlloc,
		ac_int<7, false> opcode, ac_int<9, false> regA, ac_int<9, false> regB, ac_int<9, false> regDest,
		ac_int<8, false> nbDep);

ac_int<128, false> assembleRiBytecodeInstruction(ac_int<2, false> stageCode, ac_int<1, false> isAlloc,
		ac_int<7, false> opcode, ac_int<9, false> regA, ac_int<13, false> imm13,
		ac_int<9, false> regDest, ac_int<8, false> nbDep);

ac_int<128, false> assembleIBytecodeInstruction(ac_int<2, false> stageCode, ac_int<1, false> isAlloc,
		ac_int<7, false> opcode, ac_int<9, false> reg, ac_int<19, true> imm19, ac_int<8, false> nbDep);


/********************************************************************
 * Declaration of debug function
 * ******************************************************************
 *
 * These functions are used to print IR elements in order to debug the DBT process.
 *
 *******************************************************************/
void printBytecodeInstruction(int index, ac_int<128, false> instruction);
void printBytecodeInstruction(int index, uint32  instructionPart1, uint32  instructionPart2, uint32 instructionPart3, uint32 instructionPart4);



#endif /* INCLUDES_ISA_IRISA_H_ */