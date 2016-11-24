#include <cstdio>
#include <cstdlib>

#include <frontend.h>
#include <transformation/irScheduler.h>
#include <lib/elfFile.h>
#include <lib/endianness.h>
#include <lib/tools.h>
#include <isa/vexISA.h>
#include <isa/mipsToVexISA.h>



	struct bytecodeHeader {
		unsigned int functionTableSize;
		unsigned int functionTablePtr;
		unsigned int symbolTableSize;
		unsigned int symbolTablePtr;
	};

	struct functionHeader {
		unsigned char nbrGlobalVariable;
		unsigned char dummy;
		unsigned short nbrBlock;
		unsigned int blockTablePtr;
	};

	struct blockHeader {
		unsigned char nbrInstr;
		unsigned char numberSucc;
		unsigned char numberGR;
		unsigned char numberGW;
		unsigned int placeOfSucc;
		unsigned int placeInstr;
		unsigned int placeG;
	};

	struct successor {
		unsigned char instrNumber;
		unsigned char state;
		unsigned short dest;
		unsigned int callDest;
	};

//#define __DEBUG


using namespace std;

const unsigned int debugLevel = 5;


#if !defined(__USE_AC) && !defined(__SIMULATION_ACCELERATE)
/* Global values */
char isOutsideNext = 0;
char droppedInstruction = 0;
unsigned int outsideNext;
unsigned int outsideNext_bytecode1;
unsigned int outsideNext_pred1_reg;
unsigned int outsideNext_pred1;
unsigned int outsideNext_pred2_reg;
unsigned int outsideNext_dest_reg;
unsigned int outsideNext_imm;

unsigned int outsideNext_isImm;
unsigned int outsideNext_isLongImm;

unsigned char outsideNext_pred1_ena;
unsigned char outsideNext_pred1_solved;
unsigned char outsideNext_pred2_ena;
unsigned char outsideNext_dest_ena;
unsigned char outsideNext_dest_alloc;






inline void writeShort(unsigned char* bytecode, int place, unsigned short value){
	unsigned short *bytecodeAsShort = (unsigned short *) bytecode;
	//bytecodeAsShort[place>>2] = value;

	//FIXME endianness
	bytecode[place+1] = (value >> 8) & 0xff;
	bytecode[place+0] = (value >> 0) & 0xff;

}

inline void writeSuccessor(unsigned char *bytecode, unsigned char srcInstr, unsigned char destInstr, int offsetCode){
	unsigned char* bytecodeAsChar = (unsigned char*) bytecode;
	int offset[4] = {3, 1, -1, -3};

	if (debugLevel >= 1)
		printf("Writing successor from %d to %d and data is %d\n", srcInstr, destInstr, 0);

	unsigned char wordWithNbDataSucc = bytecode[offsetCode + srcInstr*16 + 4]; //FIXME endianness
	unsigned char nbSucc = wordWithNbDataSucc & 0x7;
	unsigned char nbDSucc = (wordWithNbDataSucc >> 3) & 0x7;

	if (nbSucc == 7){
		printf("\n\n\n\t\t\t\tError, too many successors for %d, exiting... \n", srcInstr);
//		exit(-1);
	}
	if (nbSucc == 3){
		//TODO
	}

	unsigned int index = offsetCode + 16*srcInstr + 8 + 1 + (6 - nbSucc) + offset[(7-nbSucc) & 0x3]; //FIXME endianness

	bytecodeAsChar[index] = destInstr;
	nbSucc++;

	wordWithNbDataSucc = (wordWithNbDataSucc & 0xc0) + (nbDSucc << 3) + nbSucc;
	bytecode[offsetCode + srcInstr*16+4] = wordWithNbDataSucc;//FIXME endianness
}

inline unsigned int writeDataSuccessor(unsigned char *bytecode, unsigned char srcInstr, unsigned char destInstr, int offsetCode){

	unsigned char* bytecodeAsChar = (unsigned char*) bytecode;
	int offset[4] = {3, 1, -1, -3};

	if (debugLevel >= 1)
		printf("Writing successor from %d to %d and data is %d\n", srcInstr, destInstr, 1);

	unsigned int wordWithNbDataSucc = bytecode[offsetCode + srcInstr*16 + 4];//FIXME endianness
	unsigned char nbSucc = wordWithNbDataSucc & 0x7;
	unsigned char nbDSucc = (wordWithNbDataSucc >> 3) & 0x7;

	if (nbSucc == 7){
		printf("\n\n\n\t\t\t\tError, too many successors for %d, exiting... \n", srcInstr);
//		exit(-1);
	}

	unsigned int index = offsetCode + 16*srcInstr + 8 + 1 + (6-nbSucc) + offset[(7-nbSucc) & 0x3];//FIXME endianness

	bytecodeAsChar[index] = destInstr;
	nbSucc++;
	nbDSucc++;

	wordWithNbDataSucc = (wordWithNbDataSucc & 0xc0) + (nbDSucc << 3) + nbSucc;
	bytecode[offsetCode + srcInstr*16+4] = wordWithNbDataSucc;//FIXME endianness

	return nbSucc;
}


int irGenerator(unsigned char* code, unsigned int *size, unsigned int addressStart,
		unsigned char* bytecode, unsigned int *placeCode,
		short* blocksBoundaries, short* proceduresBoundaries){


	/****** Metadata on procedures & blocks******/
	int procedureNumber = 0;
	int procedureEnds[250];
	int procedureStarts[250];
	procedureStarts[0] = 0;

	int basicBlockNumber = 0;
	int totalBasicBlockNumber = 0;


	for (int oneInstructionIndex = 0; oneInstructionIndex<*size; oneInstructionIndex++){
		if (proceduresBoundaries[oneInstructionIndex] == 1){
			if (procedureNumber != 0){
				procedureEnds[procedureNumber-1] = oneInstructionIndex;
			}
			proceduresBoundaries[oneInstructionIndex] = (procedureNumber<<2) + 1;
			procedureStarts[procedureNumber] = oneInstructionIndex;
			procedureNumber++;

			basicBlockNumber = 0;
		}
		if (blocksBoundaries[oneInstructionIndex] & 0x1 == 1){
			blocksBoundaries[oneInstructionIndex] += (basicBlockNumber << 2);
			basicBlockNumber++;
			totalBasicBlockNumber++;
		}

	}
	procedureEnds[procedureNumber-1] = *size;


	/*We start generating the bytecode*/
	bytecodeHeader *oneBytecodeHeader = (bytecodeHeader*) &(bytecode[0]);
	oneBytecodeHeader->functionTableSize = procedureNumber;
	oneBytecodeHeader->functionTablePtr = 16; // = FIX_INT(12) TODO : change for nios

	//TODO : fill information on symbol table when ready
	struct functionHeader *funtionHeaders = (struct functionHeader *) &(bytecode[16]);



	//Declaration of placeInBlockTable which will keep track of the place where we write
	int placeInBlockTable = 16 + (procedureNumber<<3);
	*placeCode = placeInBlockTable + (totalBasicBlockNumber << 4);


	unsigned char numberSucc;
	unsigned char numberGR;
	unsigned char numberGW;
	for (unsigned int oneProcedure = 0; oneProcedure<procedureNumber; oneProcedure++){

		//**************************************************************************
		//Procedure header is placed at 16 + oneProcedure * 8
		//Its size is 8 bytes...
		// 1 byte for the number of global variables
		// 1 blank byte
		// 2 bytes for the number of basic block
		// 4 bytes pointing to the block table. The block table should start at 16 + procedureNumber*8


		struct blockHeader *blockHeaders = (struct blockHeader *) &(bytecode[placeInBlockTable]);


		funtionHeaders[oneProcedure].blockTablePtr = placeInBlockTable;
		//**************************************************************************


		int procedureEnd = procedureEnds[oneProcedure];
		int start = procedureStarts[oneProcedure];

		int oneInstructionIndex = start;

		/* Data for global variables */
		//int globalVariables[32] = {256,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,257,258,511};
		int globalVariables[32][33] = {256,259,260,261,262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,286,257,258,511};
		for (int i=1; i<32; i++)
			for (int j=0; j<33; j++)
				globalVariables[i][j] = globalVariables[0][j];

		int globalVariableCounter = 287;
		unsigned int reg1_mul = 0, reg2_mul = 0, imm_mul = 0, is_imm_mul = 0;


		while (oneInstructionIndex < procedureEnd){

			unsigned long long currentRegistresUsageWord = 0;


			/* Basic Block metadata */
			int numberSuccessors = 0;
			int successor1;
			int successor2;
			unsigned char indexInCurrentBlock = 0;

			/* Datastructures for dag construction*/
			int registers[64] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

			/* Generated code */
			unsigned char numbersSuccessor[256];
			unsigned char numbersDataSuccessor[256];
			unsigned char successors[256][30];

			unsigned char numbersPredecessor[256];
			int predecessors[256][8];

			/* Datastructure for RAW dependencies on global registers */
			int lastWriterOnGlobal[32] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
			int lastReaderOnGlobalCounter[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
			int lastReaderOnGlobalPlaceToWrite[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
			int lastReaderOnGlobal[32][4];

			/* Datastructure for control dependencies on memories */
			int lastWriterOnMemory = -1;
			int lastReaderOnMemoryCounter = 0;
			int lastReaderOnMemoryPlaceToWrite = 0;
			int lastReaderOnMemory[4];

			int isCallBlock = 0;
			basicBlockNumber = blocksBoundaries[oneInstructionIndex]>>2;

			char haveJump = 0;
			char jumpID = 0;

			short reglo, reghi = 0;

			do {

				char insertMove_ena = 0;
				short insertMove_src;

				droppedInstruction = 0;

				int wasOutside = 0;

				unsigned int oneInstruction = 0;
				if (isOutsideNext){
					wasOutside = 1;
					isOutsideNext = 0;
				}
				else{
					oneInstruction = (code[4*oneInstructionIndex] << 24)
							+ (code[4*oneInstructionIndex+1] << 16)
							+ (code[4*oneInstructionIndex+2] << 8)
							+ (code[4*oneInstructionIndex+3]);
				}

				if (debugLevel >= 1)
					printf("Source instruction is %x\n", oneInstruction);


				char op = (oneInstruction >> 26);

				char funct = (oneInstruction & 0x3f);
				int shamt = ((oneInstruction >> 6) & 0x1f);
				char rd = ((oneInstruction >> 11) & 0x1f);
				char rt = ((oneInstruction >> 16) & 0x1f);
				char rs = ((oneInstruction >> 21) & 0x1f);
				int regimm = rt;
				int address = (oneInstruction & 0xffff);

				int tgtadr = (oneInstruction & 0x3ffffff);

				int correctedTgtadr = tgtadr - (addressStart>>2);


				numbersSuccessor[indexInCurrentBlock] = 0;
				numbersPredecessor[indexInCurrentBlock] = 0;

				/* Local value for numberDependencies */
				char numberDependencies = 0;

				char pred1_ena = 0, pred2_ena = 0, dest_ena = 0;
				short pred1_reg = rs, pred2_reg = rt, dest_reg;



				//We determine whether to use pred1
				if (wasOutside){
					pred1_ena = outsideNext_pred1_ena;
					pred1_reg = outsideNext_pred1_reg;
				}
				else if (op == R && ((funct == SLL && rd != 0) || funct == SRA || funct == SRL)){
					pred1_ena = 1;
					pred1_reg = rt;
				}

				else if (!(op == J || op == JAL ) &&  !(op == 0 && funct == 0 && rd == 0)){
					pred1_ena = 1;
				}

				//We determine whether to use pred2
				if (wasOutside){
					pred2_ena = outsideNext_pred2_ena;
					pred2_reg = outsideNext_pred2_reg;
				}
				else if (op == R && (funct == MFHI || funct == MFLO)){
					pred2_ena = 0;
				}
				else if (op == BEQ || op == BNE || op == BLEZ || op == BGTZ || op == SB || op == SH || op == SW ||
						(op == R && !(funct == SLL || funct == SRL || funct == SRA || funct == 0)))
					pred2_ena = 1;

				//We determine whether to use dest
				if (wasOutside){
					dest_ena = outsideNext_dest_ena;
					dest_reg = outsideNext_dest_reg;
				}
				else if (op == R && (funct == MULT || funct == MULTU)){
					dest_ena = 0;
				}
				else if (op == R){

					if (oneInstruction == 0){
						droppedInstruction = 1;
					}
					else if (funct != JALR && funct != JR){
						dest_ena = 1;
						dest_reg = rd;
					}

				}
				else if (! (op == SB || op == SH || op == SW || op == J || op == JAL || op == BEQ || op == BNE || op == BLEZ || op == BGTZ )){
					dest_ena = 1;
					dest_reg = rt;
				}


				//******************************************
				//We find predecessors and correct its value

				int temp_pred1 = registers[pred1_reg];
				if ((wasOutside && outsideNext_pred1_solved))
					temp_pred1 = outsideNext_pred1;
				if (op == 0 && funct == MFHI)
					temp_pred1 = reghi;
				if (op == 0 && funct == MFLO)
					temp_pred1 = reglo;


				int pred1;
				if (!pred1_ena & temp_pred1 == -1) //If value comes from global register
					temp_pred1 = globalVariables[basicBlockNumber & 0x1f][pred1_reg];


				if (pred1_ena){

					//To gather succ information on different path
					char succ_ena = 0;
					char succ_src;
					char succ_isData = 0;

					if (temp_pred1 == -1){ //If value comes from global register
						if (globalVariables[basicBlockNumber & 0x1f][pred1_reg] == -1){ //If value is not assigned yet, we allocate the value from globalVariableCounter
							temp_pred1 = globalVariableCounter;
							globalVariables[basicBlockNumber & 0x1f][pred1_reg] = globalVariableCounter++;
						}
						else{ //Otherwise we use the already allocated value
							temp_pred1 = globalVariables[basicBlockNumber & 0x1f][pred1_reg];
						}

						//** We also mark this node as a reader of the global value. Should this value be modified
						//** in the block, we will add dependencies
						if (lastReaderOnGlobalCounter[pred1_reg] < 3){
							lastReaderOnGlobalCounter[pred1_reg]++;
							//** We handle successors: if the value in a global register has been written in the same
							//** basic block, we add a dependency from the writer node to this instruction.
							if (lastWriterOnGlobal[pred1_reg] != -1){
								succ_ena = 1;
								succ_src = lastWriterOnGlobal[pred1_reg];
								numberDependencies++;
							}
						}
						else{
							int readerToEvince = lastReaderOnGlobal[pred1_reg][lastReaderOnGlobalPlaceToWrite[pred1_reg]];
							succ_ena = 1;
							succ_src = readerToEvince;
							numberDependencies++;
						}
						lastReaderOnGlobal[pred1_reg][lastReaderOnGlobalPlaceToWrite[pred1_reg]] = indexInCurrentBlock;
						lastReaderOnGlobalPlaceToWrite[pred1_reg] = (lastReaderOnGlobalPlaceToWrite[pred1_reg] + 1) % 3;

					}
					else{ //We add a successor to the predecessor
						succ_ena = 1;
						succ_src = temp_pred1;
						succ_isData = 1;
						numberDependencies++;
					}

					pred1 = temp_pred1;
					if (succ_ena)
						if (succ_isData){
							char nbSucc = writeDataSuccessor(bytecode, succ_src, indexInCurrentBlock, *placeCode);
							if (nbSucc == 7){
								insertMove_ena = 1;
								insertMove_src = pred1;
								dest_reg = pred1_reg;
								dest_ena = 1;
							}
						}
						else
							writeSuccessor(bytecode, succ_src, indexInCurrentBlock, *placeCode);





					predecessors[indexInCurrentBlock][numbersPredecessor[indexInCurrentBlock]++] = pred1;

				}


				int temp_pred2 = registers[pred2_reg];
				int pred2;
				if (!pred2_ena & temp_pred2 == -1) //If value comes from global register
					temp_pred2 = globalVariables[basicBlockNumber & 0x1f][pred2_reg];

				if (pred2_ena){

					//To gather succ information on different path
					char succ_ena = 0;
					char succ_src;
					char succ_isData = 0;

					if (temp_pred2 == -1){ //If value comes from global register
						if (globalVariables[basicBlockNumber & 0x1f][pred2_reg] == -1){
							temp_pred2 = globalVariableCounter;
							globalVariables[basicBlockNumber & 0x1f][pred2_reg] = globalVariableCounter++;
						}
						else
							temp_pred2 = globalVariables[basicBlockNumber & 0x1f][pred2_reg];

						//If the global register has been used in the current block, we add a control dependency
						if (lastReaderOnGlobalCounter[pred2_reg] < 3){
							lastReaderOnGlobalCounter[pred2_reg]++;
							if (lastWriterOnGlobal[pred2_reg] != -1){
								succ_ena = 1;
								succ_src = lastWriterOnGlobal[pred2_reg];
								numberDependencies++;
							}
						}
						else{
							int readerToEvince = lastReaderOnGlobal[pred2_reg][lastReaderOnGlobalPlaceToWrite[pred2_reg]];
							succ_ena = 1;
							succ_src = readerToEvince;
							numberDependencies++;
						}
						lastReaderOnGlobal[pred2_reg][lastReaderOnGlobalPlaceToWrite[pred2_reg]] = indexInCurrentBlock;
						lastReaderOnGlobalPlaceToWrite[pred2_reg] = (lastReaderOnGlobalPlaceToWrite[pred2_reg] + 1) % 3;
					}
					else{ //We add a successor to the predecessor
						succ_ena = 1;
						succ_src = temp_pred2;
						succ_isData = 1;
						numberDependencies++;

					}

					pred2 = temp_pred2;

					if (succ_ena)
						if (succ_isData){
							char nbSucc = writeDataSuccessor(bytecode, succ_src, indexInCurrentBlock, *placeCode);
							if (nbSucc == 7){
								insertMove_ena = 1;
								insertMove_src = pred2;
								dest_reg = pred2_reg;
								dest_ena = 1;
							}
						}
						else
							writeSuccessor(bytecode, succ_src, indexInCurrentBlock, *placeCode);

					predecessors[indexInCurrentBlock][numbersPredecessor[indexInCurrentBlock]++] = pred2;
				}


				//******************************************
				//We set the destination

				int temp_destination = indexInCurrentBlock;
				int destination;
				if (!dest_ena & temp_destination == -1) //If value comes from global register
					temp_destination = globalVariables[basicBlockNumber & 0x1f][dest_reg];


				char alloc = 1;

				if (dest_ena) {
					if (globalVariables[basicBlockNumber & 0x1f][dest_reg] == -1 || outsideNext_dest_alloc || insertMove_ena){
						registers[dest_reg] = indexInCurrentBlock;

						//We mark the value as a write which is potentially not read
						currentRegistresUsageWord |= 1<<dest_reg;
					}
					else{
						alloc = 0;
						temp_destination = globalVariables[basicBlockNumber & 0x1f][dest_reg];
						lastWriterOnGlobal[dest_reg] = indexInCurrentBlock;
						for (int oneReader = 0; oneReader < lastReaderOnGlobalCounter[dest_reg]; oneReader++)
							if (lastReaderOnGlobal[dest_reg][oneReader] != indexInCurrentBlock){
								writeSuccessor(bytecode, lastReaderOnGlobal[dest_reg][oneReader], indexInCurrentBlock, *placeCode);
								numberDependencies++;
							}
						lastReaderOnGlobalCounter[dest_reg] = 0;

					}
				}
				destination = temp_destination;

			//	printf("DEBUG : for instr %d : op = %x funct = %x pred1_reg = %d pred1_ena = %d pred2_reg = %d, pred2_ena = %d dest_reg = %d dest_ena = %d\n", indexInCurrentBlock, op, funct, pred1_reg, pred1_ena, pred2_reg, pred2_ena, dest_reg,dest_ena);

				/***************************************************************/
				/*  We generate the instruction  */
				unsigned int bytecode1=0, bytecode2=0, bytecode3=0, bytecode4=0;

				if (insertMove_ena){
					bytecode1 += 0x8<<28;
					bytecode1 += alloc << 27;
					bytecode1 += 0x41 << 19;

					bytecode1 += insertMove_src;

					bytecode2 += destination << 14;
					bytecode2 += numberDependencies << 6;

					isOutsideNext = wasOutside;
					wasOutside = 1;
				}
				else if (wasOutside){
					bytecode1 = outsideNext_bytecode1;
					if (outsideNext_isImm){
						bytecode1 += (outsideNext_imm & 0x7ff);
						bytecode2 += pred1 << 23;
						bytecode2 += destination << 14;
					}
					else if (outsideNext_isLongImm){
						bytecode1 += ((outsideNext_imm>>9) & 0x3ff);

						bytecode2 += (outsideNext_imm & 0x1ff) << 23;

						if (pred1_ena)
							bytecode2 += pred1 << 14;
						else
							bytecode2 += destination << 14;

					}
					else{
						bytecode1 += pred1;

						bytecode2 += pred2 << 23;
						bytecode2 += destination << 14;
					}

					bytecode2 += numberDependencies << 6;


				}
				else if (op == R){
					//Instrucion is R-Type
					if (funct == SLL || funct == SRL || funct == SRA){

						bytecode1 += (0x8<<28); //stage=2 type = 0
						bytecode1 += alloc << 27;
						bytecode1 += functBinding[funct] << 19;
						bytecode1 += 2<<17; // i=1 br=0
						bytecode1 += (shamt & 0x7ff);

						bytecode2 += pred1 << 23;
						bytecode2 += destination << 14;
						bytecode2 += numberDependencies<<6;
					}
					else if (funct == MFHI || funct == MFLO){


						bytecode1 += 0x8<<28;
						bytecode1 += alloc << 27;
						bytecode1 += 0x41 << 19;

						bytecode1 += pred1;

						bytecode2 += destination << 14;
						bytecode2 += numberDependencies << 6;

					}
					else if(funct == JALR){
						numberSuccessors = 1;
						successor1 = basicBlockNumber + 1;

						bytecode1 += (0x2<<28); //stage=0 type = 2
						bytecode1 += 0;
						bytecode1 += functBinding[funct] << 19;

						bytecode2 += pred1 << 14;
						bytecode2 += numberDependencies<<6;

						haveJump = 1;
						jumpID = indexInCurrentBlock;
					}
					else if (funct == JR){
						if (pred1_reg == 31){
							numberSuccessors = 0;
							successor1 = basicBlockNumber + 1;

							bytecode1 += (0x2<<28); //stage=0 type = 2
							bytecode1 += 0;
							bytecode1 += functBinding[funct] << 19;

							bytecode2 += pred1 << 14;
							bytecode2 += numberDependencies<<6;

							haveJump = 1;
							jumpID = indexInCurrentBlock;
						}
						else{
							printf("Funct not handled %x (JR)\n", funct);
							exit(-1);
						}
					}
					else if (funct == MULT || funct == MULTU){

						bytecode1 += 0xc << 28;
						bytecode1 += 0x1 << 27;
						bytecode1 += functBinding[MFLO] << 19;
						bytecode1 += pred1;

						bytecode2 += pred2 << 23;

						bytecode2 += indexInCurrentBlock << 14;
						bytecode2 += numberDependencies<< 6;


						reglo = indexInCurrentBlock;
						reghi = indexInCurrentBlock+1;

						outsideNext_bytecode1 += 0xc << 28; //stage=2 type = 0
						outsideNext_bytecode1 += 1 << 27;
						outsideNext_bytecode1 += functBinding[MFHI]<<19;


						outsideNext_isImm = 0;
						outsideNext_isLongImm = 0;
						outsideNext_imm = 0;
						outsideNext_pred1_ena = 1;
						outsideNext_pred1_solved = 0;
						outsideNext_pred1_reg = pred1_reg;
						outsideNext_pred2_ena = 1;
						outsideNext_pred2_reg = pred2_reg;
						outsideNext_dest_ena = 0;
						outsideNext_dest_reg = indexInCurrentBlock+1;
						outsideNext_dest_alloc = 1;

						isOutsideNext = 1;

					}
					else {

						if (functBinding[funct] == -1 || functBinding[funct] == -2){
							printf("Funct not handled %x in %x\n", funct, oneInstruction);
							exit(-1);
						}

						bytecode1 += (0x8<<28); //stage=2 type = 0
						bytecode1 += alloc << 27;
						bytecode1 += functBinding[funct] << 19;
						bytecode1 += 0; // i=0 br=0
						bytecode1 += pred1;

						bytecode2 += pred2 << 23;
						bytecode2 += destination << 14;
						bytecode2 += numberDependencies<<6;
					}
				}
				else if (op == LUI){

					//Instruction is fisrt translated as a movi to the destination register
					bytecode1 += (0xa<<28); //stage=0 type = 2
					bytecode1 += alloc << 27;
					bytecode1 += 0x58 << 19; //opcode of movi
					bytecode1 += 2<<17; // i=1 br=0
					bytecode1 += ((address>>9) & 0x3ff);

					bytecode2 += (address & 0x1ff) << 23;
					bytecode2 += destination<<14;
					bytecode2 += numberDependencies<<6;



					//Then next instruction is shl, from destination register to destination register, of 16bits
					outsideNext_bytecode1 += (0x8<<28); //stage=2 type = 0
					outsideNext_bytecode1 += alloc << 27;
					outsideNext_bytecode1 += 0x4f << 19;
					outsideNext_bytecode1 += 2<<17; // i=1 br=0

					outsideNext_isImm = 1;
					outsideNext_isLongImm = 0;
					outsideNext_imm = 16;
					outsideNext_pred1_ena = 1;
					outsideNext_pred1_solved = 0;

					outsideNext_pred1_reg = dest_reg;
					outsideNext_pred2_ena = 0;
					outsideNext_dest_ena = 1;
					outsideNext_dest_reg = dest_reg;
					outsideNext_dest_alloc = 0;

					isOutsideNext = 1;

				}
				else if (op == SB || op == SH || op == SW){
					/****************************/
					/* We update lastWriterOnMemory and add required dependencies to keep memory coherence */

					for (int oneReader = 0; oneReader < lastReaderOnMemoryCounter; oneReader++){
						writeSuccessor(bytecode, lastReaderOnMemory[oneReader], indexInCurrentBlock, *placeCode);
						numberDependencies++;
					}

					lastReaderOnMemoryCounter = 0;
					lastWriterOnMemory = indexInCurrentBlock;

					bytecode1 += (0x10<<26); //stage=1, type=0=alloc=allocbr
					bytecode1 += opcodeBinding[op] << 19;
					bytecode1 += 2<<17; // i=1 br=0
					bytecode1 += (address & 0x7ff);

					bytecode2 += pred1 << 23;
					bytecode2 += pred2 << 14;
					bytecode2 += numberDependencies<<6;

				}
				else if (op == LB || op == LH || op == LW || op == LBU || op == LHU){
					/****************************/
					/* We update lastReaderOneMemory and add required dependencies to keep memory coherence */
					if (lastReaderOnMemoryCounter < 4){
						lastReaderOnMemoryCounter++;
						if (lastWriterOnMemory != -1){
							writeSuccessor(bytecode, lastWriterOnMemory, indexInCurrentBlock, *placeCode);
							numberDependencies++;
						}
					}
					else{
						int readerToEvince = lastReaderOnMemory[lastReaderOnMemoryPlaceToWrite];
						writeSuccessor(bytecode, readerToEvince, indexInCurrentBlock, *placeCode);
						numberDependencies++;
					}
					lastReaderOnMemory[lastReaderOnMemoryPlaceToWrite] = indexInCurrentBlock;
					lastReaderOnMemoryPlaceToWrite = (lastReaderOnMemoryPlaceToWrite + 1) & 0x3;

					lastReaderOnMemoryCounter = 0;
					lastWriterOnMemory = indexInCurrentBlock;

					bytecode1 += (0x4<<28); //stage=1 type = 0
					bytecode1 += alloc << 27;
					bytecode1 += opcodeBinding[op] << 19;
					bytecode1 += 2<<17; // i=1 br=0
					bytecode1 += (address & 0x7ff);

					bytecode2 += pred1 << 23;
					bytecode2 += destination << 14;
					bytecode2 += numberDependencies<<6;

				}
				else if (op == J){
					numberSuccessors = 1;
					successor1 = blocksBoundaries[correctedTgtadr];

					bytecode1 += (0x2<<28); //stage=0 type = 2
					bytecode1 += 0;
					bytecode1 += opcodeBinding[op] << 19;
					bytecode1 += 2<<17; // i=1 br=0

					bytecode2 += numberDependencies<<6;

					haveJump = 1;
					jumpID = indexInCurrentBlock;
				}
				else if (op == BEQ || op == BNE){
					numberSuccessors = 2;
					if (address >= 32768)
						address = address - 65536;
					successor1 = blocksBoundaries[(address+oneInstructionIndex) + 1];
					successor2 = blocksBoundaries[oneInstructionIndex + 2];

					bytecode1 += (0x8<<28); //stage=2
					bytecode1 += 1 << 27;
					bytecode1 += opcodeBinding[op] << 19;//opcode (here the binding will be done with cmpeq/cmpne
					bytecode1 += 0<<17; // i=1 br=0
					bytecode1 += pred1;

					bytecode2 += pred2 << 23;
					bytecode2 += oneInstructionIndex << 14;
					bytecode2 += numberDependencies<<6;

					isOutsideNext = 1;
					outsideNext_bytecode1 = (0x2<<28) + (0x25<<19) + (2<<17);
					outsideNext_isLongImm = 1;
					outsideNext_imm = 0;
					outsideNext_pred1_ena = 1;
					outsideNext_pred1_solved = 1;

					outsideNext_pred2_ena = 0;
					outsideNext_dest_ena = 0;

					outsideNext_pred1_reg = 32;
					outsideNext_pred1 = indexInCurrentBlock; //TODO marche pas !!!
					outsideNext_dest_alloc = 0;

					haveJump = 1;
					jumpID = indexInCurrentBlock +1;


				}
				else if (op == BLEZ || op == BGTZ || op == REGIMM){
					numberSuccessors = 2;
					if (address >= 32768)
						address = address - 65536;
					successor1 = blocksBoundaries[(address+oneInstructionIndex) + 1];
					successor2 = blocksBoundaries[oneInstructionIndex + 2];

					bytecode1 += (0x8<<28); //stage=2
					bytecode1 += 1 << 27;
					bytecode1 += ((op==REGIMM) ? regimmBindings[rt] : opcodeBinding[op]) << 19;//opcode (here the binding will be done with cmpeq/cmpne
					bytecode1 += 2<<17; // i=1 br=0
					bytecode1 += 0;

					bytecode2 += pred1 << 23;
					bytecode2 += oneInstructionIndex << 14;
					bytecode2 += numberDependencies<<6;

					isOutsideNext = 1;
					outsideNext_bytecode1 = (0x2<<28) + (0x25<<19) + (2<<17);
					outsideNext_isLongImm = 1;
					outsideNext_imm = address;
					outsideNext_pred1_ena = 1;
					outsideNext_pred2_ena = 1;
					outsideNext_dest_ena = 1;
					outsideNext_pred1_solved = 1;
					outsideNext_dest_alloc = 0;
					outsideNext_pred1_reg = 32;
					outsideNext_pred1 = indexInCurrentBlock; //TODO marche pas !!!

					haveJump = 1;
					jumpID = indexInCurrentBlock+1;

				}
				else if (op == JAL){
					isCallBlock = 1;
					numberSuccessors = 1;
					//successor1 = basicBlockNumber + 1;
					successor1 = proceduresBoundaries[correctedTgtadr];

					bytecode1 += (0x2<<28); //stage=0 type = 2
					bytecode1 += 0;
					bytecode1 += opcodeBinding[op] << 19;
					bytecode1 += 2<<17; // i=1 br=0

					bytecode2 += numberDependencies<<6;

					haveJump = 1;
					jumpID = indexInCurrentBlock;
				}
				else{ //For all other instructions

					//These instruction should not be immediate
					if (opcodeBinding[op] == -1 || opcodeBinding[op] == -2){
						printf("Opcode not handled at %x :  %x (%x)\n",(oneInstructionIndex>>2) + addressStart, op, oneInstruction);
						exit(-1);
					}

					bytecode1 += (0x8<<28); //stage=2 (mult are handled elsewhere as they need to be modified) type = 0
					bytecode1 += alloc << 27;
					bytecode1 += opcodeBinding[op] << 19;
					bytecode1 += 2<<17; // i=1 br=0
					bytecode1 += (address & 0x7ff);

					bytecode2 += pred1 << 23;
					bytecode2 += destination << 14;
					bytecode2 += numberDependencies<<6;

				}

				writeInt(bytecode, *placeCode + 16*indexInCurrentBlock, bytecode1);
				writeInt(bytecode, *placeCode + 16*indexInCurrentBlock+4, bytecode2);
				writeInt(bytecode, *placeCode + 16*indexInCurrentBlock+8, bytecode3);
				writeInt(bytecode, *placeCode + 16*indexInCurrentBlock+12, bytecode4);

				if (debugLevel >= 1)
					printf("Generated bytecode is %x %x %x %x\n", bytecode1, bytecode2, bytecode3, bytecode4);


				if (!droppedInstruction)
					indexInCurrentBlock++;

				if (wasOutside)
					isOutsideNext = 0;
				else
					oneInstructionIndex++;

			}
			while ((blocksBoundaries[oneInstructionIndex] & 0x1) == 0 && oneInstructionIndex < procedureEnd);


			//*****************************************************************
			//  Second loop to add dependencies to jump instruction

			if (haveJump)
				for (int oneInstructionFromBlock = 0; oneInstructionFromBlock < indexInCurrentBlock; oneInstructionFromBlock++){
					int offset[4] = {3, 1, -1, -3};

					unsigned char wordWithNbDataSucc = ((unsigned int *)bytecode)[*placeCode/4 + 4*oneInstructionFromBlock + 1]; //FIXME endianness
					unsigned char nbSucc = wordWithNbDataSucc & 0x7;

					if (nbSucc == 0){

						unsigned int index = *placeCode + oneInstructionFromBlock + 16*oneInstructionFromBlock + 8 + 1 + (6 - nbSucc) + offset[(7-nbSucc) & 0x3]; //FIXME endianness

						bytecode[index] = jumpID;
						nbSucc++;

						wordWithNbDataSucc = (wordWithNbDataSucc & 0xf8) + nbSucc;
						bytecode[*placeCode/4 + 4*oneInstructionFromBlock + 1] = wordWithNbDataSucc;//FIXME endianness
					}
				}



			successor1 >>= 2;
			successor2 >>= 2;

			//*****************************************************************
			/* We write information on current block in the bytecode */



			char typeOfFirstSucc = 0;
			if (numberSuccessors == 0 && !haveJump){
				typeOfFirstSucc = 254;
				numberSuccessors++;
				successor1 = basicBlockNumber + 1;
			}

			//First we write the block header

			blockHeaders[basicBlockNumber].nbrInstr = indexInCurrentBlock;
			blockHeaders[basicBlockNumber].numberSucc = numberSuccessors;
			blockHeaders[basicBlockNumber].placeInstr = *placeCode;
			blockHeaders[basicBlockNumber].placeOfSucc = *placeCode + 16*indexInCurrentBlock;

			int placeSucc = *placeCode + 16*indexInCurrentBlock;


			if (isCallBlock){
				bytecode[placeSucc] = indexInCurrentBlock-1; //The place of the call
				bytecode[placeSucc + 1] = 255;
				bytecode[placeSucc + 4] = (successor1>>24) & 0xff;
				bytecode[placeSucc + 5] = (successor1>>16) & 0xff;
				bytecode[placeSucc + 6] = (successor1>>8) & 0xff;
				bytecode[placeSucc + 7] = (successor1>>0) & 0xff;

				placeSucc += 8;

			}
			else{
				if (numberSuccessors >= 1){
					bytecode[placeSucc] = jumpID; //The place of the jump
					bytecode[placeSucc + 1] = typeOfFirstSucc;
					bytecode[placeSucc + 2] = (successor1>>8) & 0xff;
					bytecode[placeSucc + 3] = (successor1>>0) & 0xff;
				}
				if (numberSuccessors >= 2){
					bytecode[placeSucc +8] = jumpID; //The place of the jump
					bytecode[placeSucc + 9] = 0;
					bytecode[placeSucc + 10] = (successor2>>8) & 0xff;
					bytecode[placeSucc + 11] = (successor2>>0) & 0xff;
				}

			}

			//We update place of code for the next BB
			*placeCode = *placeCode +(16*indexInCurrentBlock) + numberSuccessors*8;


			//*****************************************************************
#ifdef __DEBUG
			printf("BasicBlock ends before %x with %d successors : %d and %d\n", 4*oneInstructionIndex, numberSuccessors, successor1, successor2);
#endif

			basicBlockNumber++;
			start = oneInstructionIndex;
			placeInBlockTable = placeInBlockTable + sizeof(struct blockHeader);

		}

		/* We complete information on current procedure */
		funtionHeaders[oneProcedure].nbrBlock = basicBlockNumber;

#ifdef __DEBUG
		printf("*******************************************************\n");
#endif



	}
}
#endif

#ifdef __USE_AC
typedef ac_int<5,false> acu5;
typedef ac_int<32,false> acu32;
typedef ac_int<64,false> acu64;
typedef ac_int<6,false> acu6;
typedef ac_int<9,false> acu9;
typedef ac_int<16,true> acs16;
typedef ac_int<16,true> acu16;

typedef ac_int<26,true> acs26;
typedef ac_int<1,false> acu1;

/* Global values */
acu1 isOutsideNext = 0;
acu1 droppedInstruction = 0;

ac_int<128, false> outsideNext_bytecode;
acu9 outsideNext_pred1_reg;
acu9 outsideNext_pred1;
acu9 outsideNext_pred2_reg;
ac_int<10, true> outsideNext_dest_reg;
acs16 outsideNext_imm;

acu1 outsideNext_isImm;
acu1 outsideNext_isLongImm;
acu1 outsideNext_pred1_ena;
acu1 outsideNext_pred1_solved;

acu1 outsideNext_pred2_ena;
acu1 outsideNext_dest_ena;
acu1 outsideNext_dest_alloc;


ac_int<8, false> writeSucc_lastAddr = 255;
ac_int<128, false> writeSucc_lastValue;


inline unsigned int writeSuccessor_ac(ac_int<128, false> bytecode[1024], ac_int<8, false> srcInstr, ac_int<8, false> destInstr, ac_int<1,false> isData){

	ac_int<128, false> oneBytecodeInstruction = (writeSucc_lastAddr == srcInstr) ? writeSucc_lastValue : bytecode[srcInstr];
	ac_int<3, false> nbSucc = oneBytecodeInstruction.slc<3>(64);
	ac_int<3, false> nbDSucc = oneBytecodeInstruction.slc<3>(67);

	ac_int<8, false> offset = nbSucc;
	offset = offset << 3;
	oneBytecodeInstruction.set_slc(offset, destInstr);

	nbSucc++;
	if (isData)
		nbDSucc++;



	oneBytecodeInstruction.set_slc(64, nbSucc);
	oneBytecodeInstruction.set_slc(67, nbDSucc);

	if (srcInstr != destInstr){
		bytecode[srcInstr] = oneBytecodeInstruction;
		writeSucc_lastAddr = srcInstr;
		writeSucc_lastValue = oneBytecodeInstruction;
	}
	return nbSucc;
}

ac_int<128, false> assembleRBytecodeInstruction(ac_int<2, false> stageCode, ac_int<1, false> isAlloc,
		ac_int<7, false> opcode, ac_int<9, false> regA, ac_int<9, false> regB, ac_int<9, false> regDest,
		ac_int<8, false> nbDep){

	ac_int<128, false> result = 0;
	//Node: Type is zero: no need to write it for real. Same for isImm

	result.set_slc(96+30, stageCode);
	result.set_slc(96+27, isAlloc);
	result.set_slc(96+19, opcode);
	result.set_slc(96+0, regA);

	result.set_slc(64+23, regB);
	result.set_slc(64+14, regDest);
	result.set_slc(64+6, nbDep);

	return result;
}

ac_int<128, false> assembleRiBytecodeInstruction(ac_int<2, false> stageCode, ac_int<1, false> isAlloc,
		ac_int<7, false> opcode, ac_int<9, false> regA, ac_int<13, false> imm13,
		ac_int<9, false> regDest, ac_int<8, false> nbDep){

	ac_int<128, false> result = 0;
	ac_int<1, false> isImm = 1;

	//Node: Type is zero: no need to write it for real.

	result.set_slc(96+30, stageCode);
	result.set_slc(96+27, isAlloc);
	result.set_slc(96+19, opcode);
	result.set_slc(96+18, isImm);
	result.set_slc(96+0, imm13);

	result.set_slc(64+23, regA);
	result.set_slc(64+14, regDest);
	result.set_slc(64+6, nbDep);

	return result;
}

ac_int<128, false> assembleIBytecodeInstruction(ac_int<2, false> stageCode, ac_int<1, false> isAlloc,
		ac_int<7, false> opcode, ac_int<9, false> reg, ac_int<19, true> imm19, ac_int<8, false> nbDep){

	ac_int<128, false> result = 0;
	ac_int<2, false> typeCode = 2;
	ac_int<1, false> isImm = 1;

	result.set_slc(96+30, stageCode);
	result.set_slc(96+28, typeCode);
	result.set_slc(96+27, isAlloc);
	result.set_slc(96+19, opcode);
	result.set_slc(96+18, isImm);

	result.set_slc(64+23, imm19);
	result.set_slc(64+14, reg);
	result.set_slc(64+6, nbDep);

	return result;
}

unsigned int DBTFrontend_loop(ac_int<128, false> srcBinaries[1024], unsigned int blockSize,
		ac_int<128, false> bytecode_code[1024], int globalVariables[64],
		ac_int<64, false> registersUsage[1], int globalVariableCounter){

	ac_int<1, false> const0 = 0;
	ac_int<1, false> const1 = 1;
	//**************************************************************************
	//Procedure header is placed at 16 + oneProcedure * 8
	//Its size is 8 bytes...
	// 1 byte for the number of global variables
	// 1 blank byte
	// 2 bytes for the number of basic block
	// 4 bytes pointing to the block table. The block table should start at 16 + procedureNumber*8


		/* Basic Block metadata */
		int numberSuccessors = 0;
		ac_int<32, false> successor1, successor2;
		unsigned char indexInCurrentBlock = 0;
		ac_int<9, true> registers[64];

		/* Datastructures for dag construction*/
		for (int i=0; i<64; i++)
			registers[i] = -1;

		ac_int<64, false> currentRegistresUsageWord = 0;

		/* Generated code */
		unsigned char numbersSuccessor[256];
		unsigned char numbersDataSuccessor[256];
		unsigned char successors[256][30];

		unsigned char numbersPredecessor[256];
		int predecessors[256][8];

		/* Datastructure for RAW dependencies on global registers */
		int lastWriterOnGlobal[64] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
		ac_int<2, false> lastReaderOnGlobalCounter[64] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		ac_int<2, false> lastReaderOnGlobalPlaceToWrite[64] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		int lastReaderOnGlobal[64][4];

		/* Datastructure for control dependencies on memories */
		int lastWriterOnMemory = -1;
		ac_int<2, false> lastReaderOnMemoryCounter = 0;
		ac_int<2, false> lastReaderOnMemoryPlaceToWrite = 0;
		int lastReaderOnMemory[4];

		int isCallBlock = 0;
		ac_int<1, false> haveJump = 0;
		ac_int<8, false> jumpID = 0;
		ac_int<8, false> indexInSourceBinaries = 0;
		ac_int<128, false> previousVLIWSyllabus = 0;

		do {

			/********************************************************************
			 * First step is to fetch the instruction to translate
			 *
			 * For this, we load a complete 128-bits instruction word from the VLIW
			 * and consider each instruction not empty. If there is more than one, the
			 * second one is kept for next iteration
			 ********************************************************************/
			//FIXME handle the mov insertion
			ac_int<1, false> insertMove_ena = 0;
			ac_int<9, true> insertMove_src;


			ac_int<128, false> oneVLIWSyllabus = srcBinaries[indexInSourceBinaries];
			ac_int<32, false> oneInstruction = 0;
			if (previousVLIWSyllabus.slc<32>(0) != 0){
				//We fetch the instruction from the previous syllabus
				oneInstruction = previousVLIWSyllabus.slc<32>(0);
				previousVLIWSyllabus = 0;
			}
			else{
				//Otherwise we use the new syllabus and fetch the non-null instruction from the three first words
				ac_int<32, false> opcode1 = oneVLIWSyllabus.slc<7>(32);
				ac_int<32, false> opcode2 = oneVLIWSyllabus.slc<7>(64);

				oneInstruction = (opcode1 != 0) ? oneVLIWSyllabus.slc<32>(32) :
								(opcode2 != 0) ? oneVLIWSyllabus.slc<32>(64) :
								oneVLIWSyllabus.slc<32>(96);

				//We then store the new syllabus to use it if necessary at next cycle, and increment the index
				previousVLIWSyllabus = oneVLIWSyllabus;
				indexInSourceBinaries++;
			}

			/********************************************************************
			 * Second step is to identify correct operand according to opcode
			 *
			 * For this we will decode the instruction and consider the opcode to
			 * select registers to read and to write. Instructions are separated into
			 * eight groups:
			 * -> Memory loads
			 * -> Memory writes
			 * -> Direct Branch
			 * -> Indirect/Conditional Branch (which needs to access a register)
			 * -> Movi (which only writes a register)
			 * -> Arith1 (arithmetical instruction with only one operand: sign extention or not)
			 * -> Arith2 (arithmetical instruction with two register operands)
			 * -> ArithImm (arithmetical instruction with one register operand and one immediate value
			 ********************************************************************/

			ac_int<7, false> opcode = oneInstruction.slc<7>(0);
			ac_int<13, true> imm13 = oneInstruction.slc<13>(7);
			ac_int<13, true> imm19 = oneInstruction.slc<19>(7);
			ac_int<6, false> reg14 = oneInstruction.slc<6>(14);
			ac_int<6, false> reg20 = oneInstruction.slc<6>(20);
			ac_int<6, false> reg26 = oneInstruction.slc<6>(26);


			ac_int<1, false> isIType = (opcode.slc<3>(4) == 2);

			ac_int<1, false> isLoadType = opcode == VEX_LDB | opcode == VEX_LDBU | opcode == VEX_LDH
					| opcode == VEX_LDHU | opcode == VEX_LDW;
			ac_int<1, false> isStoreType = opcode == VEX_STB | opcode == VEX_STH | opcode == VEX_STW;
			ac_int<1, false> isBranchWithNoReg = opcode == VEX_GOTO | opcode == VEX_CALL | opcode == VEX_RETURN
					| opcode == VEX_STOP;
			ac_int<1, false> isBranchWithReg = opcode == VEX_IGOTO | opcode == VEX_ICALL | opcode == VEX_BR
					| opcode == VEX_BRF;
			ac_int<1, false> isMovi = opcode == VEX_MOVI;
			ac_int<1, false> isArith1 = opcode == VEX_NOT | opcode == VEX_SXTH | opcode == VEX_SXTB
					| opcode == VEX_ZXTH | opcode == VEX_ZXTB;
			ac_int<1, false> isArith2 = (opcode.slc<3>(4) == 4 | opcode.slc<3>(4) == 5) & !isArith1;
			ac_int<1, false> isArithImm = opcode.slc<3>(4) == 6 | opcode.slc<3>(4) == 7;
			ac_int<1, false> isMultType = opcode.slc<3>(4) == 0;


			acu1 pred1_ena = 0, pred2_ena = 0, dest_ena = 0;
			acu6 pred1_reg = reg26, pred2_reg = reg20, dest_reg;

			//Solving accessed register 1
			if (!isBranchWithNoReg && !isMovi)
				pred1_ena = 1;

			//Solving accessed register 2
			if (isStoreType || isArith2)
				pred2_ena = 1;

			//Solving written register
			if (isArithImm || isArith1 || isLoadType){
				dest_ena = 1;
				dest_reg = reg20;
			}
			else if (isMovi){
				dest_ena = 1;
				dest_reg = reg26;
			}
			else if (isArith2){
				dest_ena = 1;
				dest_reg = reg14;
			}



			//FIXME check these three lines
			numbersSuccessor[indexInCurrentBlock] = 0;
			numbersPredecessor[indexInCurrentBlock] = 0;

			/* Local value for numberDependencies */
			ac_int<8, false> numberDependencies = 0;

			/********************************************************************
			 * Third step is to solve/build dependencies in the current block
			 *
			 * This is the delicate step of the generation
			 * TODO detailled description of the process
			 ********************************************************************/


			ac_int<1, false> pred1_succ_ena = 0;
			ac_int<8, false> pred1_succ_src;
			ac_int<1, false> pred1_succ_isData = 0;

			ac_int<10, true> temp_pred1 = registers[pred1_reg];


			//We perform memory accesses for pred1
			ac_int<9, false> pred1;
			ac_int<1, false> pred1_global = 0;
			ac_int<9, false> pred1_global_address = pred1_reg;
			ac_int<10, true> pred1_global_access = globalVariables[pred1_reg];
			ac_int<2, false> lastReaderOnGlobalCounter_access_pred1 = lastReaderOnGlobalCounter[pred1_reg];
			ac_int<2, false> lastReaderOnGlobalPlaceToWrite_access_pred1 = lastReaderOnGlobalPlaceToWrite[pred1_reg];
			ac_int<2, false> lastReaderOnGlobalPlaceToWrite_access_pred1_old = lastReaderOnGlobalPlaceToWrite_access_pred1;
			ac_int<10, true> lastWriterOnGlobal_access_pred1 = lastWriterOnGlobal[pred1_reg];
			ac_int<10, true> lastReaderOnGlobal_value_pred1 = lastReaderOnGlobal[pred1_reg][lastReaderOnGlobalPlaceToWrite_access_pred1];
			ac_int<9, false> pred1_global_value = 0;

			//We perform memory accesses for pred2
			ac_int<1, false> pred2_global = 0;
			ac_int<9, false> pred2_global_address = pred2_reg;
			ac_int<10, true> pred2_global_access = globalVariables[pred2_reg];
			ac_int<2, false> lastReaderOnGlobalCounter_access_pred2 = lastReaderOnGlobalCounter[pred2_reg];
			ac_int<2, false> lastReaderOnGlobalPlaceToWrite_access_pred2 = lastReaderOnGlobalPlaceToWrite[pred2_reg];
			ac_int<2, false> lastReaderOnGlobalPlaceToWrite_access_pred2_old = lastReaderOnGlobalPlaceToWrite_access_pred2;
			ac_int<10, true> lastWriterOnGlobal_access_pred2 = lastWriterOnGlobal[pred2_reg];
			ac_int<10, true> lastReaderOnGlobal_value_pred2 = lastReaderOnGlobal[pred2_reg][lastReaderOnGlobalPlaceToWrite_access_pred2];
			ac_int<9, false> pred2_global_value = 0;

			//We perform memory accesses for dest
			ac_int<9, false> dest_global_address = dest_reg;
			ac_int<10, true> dest_global_access = globalVariables[dest_reg];
			ac_int<2, false> lastReaderOnGlobalCounter_access_dest = lastReaderOnGlobalCounter[dest_reg];
			ac_int<10, true> lastWriterOnGlobal_access_dest = lastWriterOnGlobal[dest_reg];
			ac_int<10, true> lastReaderOnGlobal_value_dest_1 = lastReaderOnGlobal[dest_reg][0];
			ac_int<10, true> lastReaderOnGlobal_value_dest_2 = lastReaderOnGlobal[dest_reg][1];
			ac_int<10, true> lastReaderOnGlobal_value_dest_3 = lastReaderOnGlobal[dest_reg][2];



			if (pred1_ena){

				pred1_global = (temp_pred1 < 0);

				if (pred1_global){ //If value comes from global register

					if (pred1_global_access < 0){ //If value is not assigned yet, we allocate the value from globalVariableCounter

						temp_pred1 = globalVariableCounter;
						pred1_global_access = globalVariableCounter++;
					}
					else{ //Otherwise we use the already allocated value

						temp_pred1 = pred1_global_access;
					}

					pred1_global_value = temp_pred1;

					//** We also mark this node as a reader of the global value. Should this value be modified
					//** in the block, we will add dependencies


					if (lastReaderOnGlobalCounter_access_pred1 < 3){

						lastReaderOnGlobalCounter_access_pred1++;
						//** We handle successors: if the value in a global register has been written in the same
						//** basic block, we add a dependency from the writer node to this instruction.
						if (lastWriterOnGlobal_access_pred1 >= 0){
							pred1_succ_ena = 1;
							pred1_succ_src = lastWriterOnGlobal_access_pred1;
							numberDependencies++;
						}
					}
					else{

						int readerToEvince = lastReaderOnGlobal_value_pred1;
						pred1_succ_ena = 1;
						pred1_succ_src = readerToEvince;
						numberDependencies++;
					}
					lastReaderOnGlobal_value_pred1 = indexInCurrentBlock;

					lastReaderOnGlobalPlaceToWrite_access_pred1++;
					if (lastReaderOnGlobalPlaceToWrite_access_pred1 == 3)
						lastReaderOnGlobalPlaceToWrite_access_pred1 = 0;

				}
				else{
					//We are facing a simple data dependency
					pred1_succ_ena = 1;
					pred1_succ_src = temp_pred1;
					pred1_succ_isData = 1;
					numberDependencies++;
				}

				pred1 = temp_pred1;



			}



			/************** Pred 2 ****************/
			ac_int<10, true> temp_pred2 = registers[pred2_reg];
			ac_int<9, false> pred2;


			acu1 pred2_succ_ena = 0;
			ac_int<8, false> pred2_succ_src;
			acu1 pred2_succ_isData = 0;



			pred2_global_access = (pred2_global_address == pred1_global_address) ? pred1_global_access : pred2_global_access;


			if (pred2_ena){

				//To gather succ information on different path
				pred2_global = (temp_pred2 < 0);

				if (pred2_global){ //If value comes from global register
					if (pred2_global_access < 0){
						temp_pred2 = globalVariableCounter;
						pred2_global_access = globalVariableCounter++;
					}
					else
						temp_pred2 = pred2_global_access;


					pred2_global_value = temp_pred2;

					//If the global register has been used in the current block, we add a control dependency

					if (!(pred1_global && pred1_global_address == pred2_global_address)){
						if (lastReaderOnGlobalCounter_access_pred2 < 3){
							lastReaderOnGlobalCounter_access_pred2++;
							if (lastWriterOnGlobal_access_pred2 >= 0){
								pred2_succ_ena = 1;
								pred2_succ_src = lastWriterOnGlobal_access_pred2;
								numberDependencies++;
							}
						}
						else{
							int readerToEvince = lastReaderOnGlobal_value_pred2;
							pred2_succ_ena = 1;
							pred2_succ_src = readerToEvince;
							numberDependencies++;
						}
						lastReaderOnGlobal_value_pred2 = indexInCurrentBlock;
						lastReaderOnGlobalPlaceToWrite_access_pred2++;
						if (lastReaderOnGlobalPlaceToWrite_access_pred2 == 3)
							lastReaderOnGlobalPlaceToWrite_access_pred2 = 0;

						lastReaderOnGlobalCounter_access_pred2 = lastReaderOnGlobalCounter_access_pred2;
						lastReaderOnGlobalPlaceToWrite_access_pred2 = lastReaderOnGlobalPlaceToWrite_access_pred2;
					}
				}
				else{
					//We are facing a simple data dependency
					printf("Reaching non global for dest 2\n");
					pred2_succ_ena = 1;
					pred2_succ_src = temp_pred2;
					pred2_succ_isData = 1;
					numberDependencies++;
				}

				pred2 = temp_pred2;



				predecessors[indexInCurrentBlock][numbersPredecessor[indexInCurrentBlock]++] = pred2;
			}


			if (isLoadType){
				/****************************/
				/* We update lastReaderOneMemory and add required dependencies to keep memory coherence */

				acu16 succ_src;
				if (lastReaderOnMemoryCounter < 3){
					lastReaderOnMemoryCounter++;
					if (lastWriterOnMemory != -1){
						succ_src = lastWriterOnMemory;
						numberDependencies++;
					}
				}
				else{
					int readerToEvince = lastReaderOnMemory[lastReaderOnMemoryPlaceToWrite];
					succ_src = readerToEvince;
					numberDependencies++;
				}

				pred2_succ_ena = 1;
				pred2_succ_isData = 0;
				pred2_succ_src = succ_src;

				lastReaderOnMemory[lastReaderOnMemoryPlaceToWrite] = indexInCurrentBlock;
				lastReaderOnMemoryPlaceToWrite = (lastReaderOnMemoryPlaceToWrite + 1);
				if (lastReaderOnMemoryPlaceToWrite == 3)
					lastReaderOnMemoryPlaceToWrite = 0;

			}


			//******************************************
			//We set the destination

			acu1 global_succ_ena_1 = 0;
			acu1 global_succ_ena_2 = 0;
			acu1 global_succ_ena_3 = 0;
			acu1 global_succ_ena_4 = 0;

			ac_int<8, false> global_succ_src_1;
			ac_int<8, false> global_succ_src_2;
			ac_int<8, false> global_succ_src_3;
			ac_int<8, false> global_succ_src_4;


			ac_int<10, true> temp_destination = indexInCurrentBlock;
			ac_int<9, false> destination;



			acu1 alloc = 1;



			//When reading global name, we make sure the global variable has not been named by pred1 or pred2
			dest_global_access = (dest_global_address == pred1_global_address) ? pred1_global_access : dest_global_access;
			dest_global_access = (dest_global_address == pred2_global_address) ? pred2_global_access : dest_global_access;



			if (dest_ena) {
				if (dest_global_access < 0 || insertMove_ena){
					registers[dest_reg] = indexInCurrentBlock;

					//We mark the value as a write which is potentially not read
					currentRegistresUsageWord[dest_reg] = 1;
				}
				else{
					alloc = 0;
					temp_destination = dest_global_access;
					lastWriterOnGlobal_access_dest = indexInCurrentBlock;

					if (lastReaderOnGlobalCounter_access_dest >= 1 && lastReaderOnGlobal_value_dest_1 != indexInCurrentBlock){
						global_succ_ena_1 = 1;
						global_succ_src_1 = lastReaderOnGlobal_value_dest_1;
						numberDependencies++;
					}
					if (lastReaderOnGlobalCounter_access_dest >= 2 && lastReaderOnGlobal_value_dest_2 != indexInCurrentBlock){
						global_succ_ena_2 = 1;
						global_succ_src_2 = lastReaderOnGlobal_value_dest_2;
						numberDependencies++;
					}
					if (lastReaderOnGlobalCounter_access_dest >= 3 && lastReaderOnGlobal_value_dest_3 != indexInCurrentBlock){
						global_succ_ena_3 = 1;
						global_succ_src_3 = lastReaderOnGlobal_value_dest_3;
						numberDependencies++;
					}

					lastReaderOnGlobalCounter_access_dest = 0;

				}
			}
			destination = temp_destination;

			if (isStoreType){

				if (lastReaderOnMemoryCounter >= 1){
					global_succ_ena_1 = 1;
					global_succ_src_1 = lastReaderOnMemory[0];
					numberDependencies++;
				}
				if (lastReaderOnMemoryCounter >= 2){
					global_succ_ena_2 = 1;
					global_succ_src_2 = lastReaderOnMemory[1];
					numberDependencies++;
				}
				if (lastReaderOnMemoryCounter >= 3){
					global_succ_ena_3 = 1;
					global_succ_src_3 = lastReaderOnMemory[2];
					numberDependencies++;
				}

				lastReaderOnMemoryCounter = 0;
				lastWriterOnMemory = indexInCurrentBlock;

			}


			//We write back values in corresponding memories for pred1
			lastReaderOnGlobal[pred1_reg][lastReaderOnGlobalPlaceToWrite_access_pred1_old] = lastReaderOnGlobal_value_pred1;
			lastReaderOnGlobalCounter[pred1_reg] = lastReaderOnGlobalCounter_access_pred1;
			lastReaderOnGlobalPlaceToWrite[pred1_reg] = lastReaderOnGlobalPlaceToWrite_access_pred1;
			globalVariables[pred1_reg] = pred1_global_access;

			//We write back values in corresponding memories for pred2
			lastReaderOnGlobal[pred2_reg][lastReaderOnGlobalPlaceToWrite_access_pred2_old] = lastReaderOnGlobal_value_pred2;
			lastReaderOnGlobalCounter[pred2_reg] = lastReaderOnGlobalCounter_access_pred2;
			lastReaderOnGlobalPlaceToWrite[pred2_reg] = lastReaderOnGlobalPlaceToWrite_access_pred2;
			globalVariables[pred2_reg] = pred2_global_access;

			//We write back values in corresponding memories for dest
			lastWriterOnGlobal[dest_reg] = lastWriterOnGlobal_access_dest;
			lastReaderOnGlobalCounter[dest_reg] = lastReaderOnGlobalCounter_access_dest;


			//We add dependencies
			if (pred1_succ_ena){

				ac_int<3, false> nbSucc_pred1 = writeSuccessor_ac(bytecode_code, pred1_succ_src, indexInCurrentBlock, pred1_succ_isData);
				if (pred1_succ_isData & (nbSucc_pred1 == 7)){
						insertMove_ena = 1;
						insertMove_src = pred1;
						alloc = 1;
						destination = indexInCurrentBlock;
						dest_ena = 1;
				}
			}

			if (pred2_succ_ena){
				ac_int<3, false> nbSucc_pred2 = writeSuccessor_ac(bytecode_code, pred2_succ_src, indexInCurrentBlock, pred2_succ_isData);
				if (pred2_succ_isData & (nbSucc_pred2 == 7)){
						insertMove_ena = 1;
						insertMove_src = pred2;
						alloc = 1;
						destination = indexInCurrentBlock;
						dest_ena = 1;
				}
			}


			if (global_succ_ena_1){
				writeSuccessor_ac(bytecode_code, global_succ_src_1, indexInCurrentBlock, const0);
			}

			if (global_succ_ena_2){
				writeSuccessor_ac(bytecode_code, global_succ_src_2, indexInCurrentBlock, const0);
			}

			if (global_succ_ena_3){
				writeSuccessor_ac(bytecode_code, global_succ_src_3, indexInCurrentBlock, const0);
			}



			/********************************************************************
			 * Last step is to generate the bytecode instruction
			 *
			 * TODO detailled description of the process
			 ********************************************************************/

			ac_int<128, false> oneBytecode = 0;

			if (insertMove_ena){
				//TODO

				printf("Implementation do not support mov insertion yet...\n Exiting...\n");
				exit(-1);

				ac_int<4, false> const8 = 0x8;

				ac_int<7, false> const41 = 0x41;
				oneBytecode.set_slc(96+28, const8);
				oneBytecode.set_slc(96+27, alloc);
				oneBytecode.set_slc(96+19, const41);

				oneBytecode.set_slc(96, insertMove_src);

				oneBytecode.set_slc(64+14, destination);
				oneBytecode.set_slc(64+6, numberDependencies);

			}
			else if (isBranchWithNoReg || isBranchWithReg){

				if (opcode == VEX_GOTO){
					numberSuccessors = 1;
					successor1 = imm19;
				}
				else if (opcode == VEX_BR || opcode == VEX_BRF){
					numberSuccessors = 2;
					successor1 = (imm13 + indexInSourceBinaries - 1) + 1; //FIXME
					successor2 = indexInSourceBinaries - 1 + 1;
				}
				else if (opcode == VEX_CALL){
					isCallBlock = 1;
					numberSuccessors = 1;
					successor1 = imm19;
				}

				oneBytecode = assembleIBytecodeInstruction(0, 0, opcode, pred1, imm19, numberDependencies);

				haveJump = 1;
				jumpID = indexInCurrentBlock;

			}
			else if (isMovi){
				oneBytecode = assembleIBytecodeInstruction(2, alloc, opcode, destination, imm19, numberDependencies);
			}
			else if (isStoreType){
				oneBytecode = assembleRiBytecodeInstruction(1, 0, opcode, pred1, imm13, pred2, numberDependencies);
			}
			else if (isLoadType){
				oneBytecode = assembleRiBytecodeInstruction(1, alloc, opcode, pred1, imm13, destination, numberDependencies);
			}
			else if (isMultType){
				oneBytecode = assembleRBytecodeInstruction(3, alloc, opcode, pred1, pred2, destination, numberDependencies);
			}
			else if (isArith1 || isArithImm){
				oneBytecode = assembleRiBytecodeInstruction(0, alloc, opcode, pred1, imm13, destination, numberDependencies);
			}
			else if (isArith2){
				oneBytecode = assembleRBytecodeInstruction(2, alloc, opcode, pred1, pred2, destination, numberDependencies);
			}
			else {
				printf("This case should never happen...\n");
			}


			//We place the instruction in memory
			bytecode_code[indexInCurrentBlock] = oneBytecode;


			if (!insertMove_ena)
				indexInCurrentBlock++;
		}
		while (indexInSourceBinaries<=blockSize && indexInCurrentBlock<255);

		if (haveJump){
			ac_int<128, false> jumpBytecodeWord = bytecode_code[jumpID];
			ac_int<8, false> numberDependencies = jumpBytecodeWord.slc<8>(64+6);

			for (int oneInstructionFromBlock = 0; oneInstructionFromBlock < indexInCurrentBlock; oneInstructionFromBlock++){

				ac_int<128, false> bytecodeWord = bytecode_code[oneInstructionFromBlock];
				ac_int<3, false> nbSucc = bytecodeWord.slc<3>(64);

				if (nbSucc == 0 && oneInstructionFromBlock != jumpID){

					bytecodeWord.set_slc(0, jumpID);
					nbSucc++;
					numberDependencies++;
					bytecodeWord.set_slc(64, nbSucc);
					bytecode_code[oneInstructionFromBlock] = bytecodeWord;
				}
			}
			jumpBytecodeWord.set_slc(64+6, numberDependencies);
			bytecode_code[jumpID] = jumpBytecodeWord;
		}

	return indexInCurrentBlock;


}

int globalVariables[64] = {256,257,258,259,260,261,262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,278,
		279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,295,296,297,298,299,300,301,302,303,304,305,306,
		307,308,309,310,311,312,313,314,315,316,317,318,319
};

#endif

#ifndef __SIMULATION_ACCELERATE
	unsigned int returnedValuesBytecodeForSim[1024];
#endif




#if defined(__USE_AC) || defined(__NIOS)

int irGenerator(unsigned char* code, unsigned int *size, unsigned int addressStart,
		unsigned char* bytecode, unsigned int *placeCode,
		short* blocksBoundaries, short* proceduresBoundaries, int* insertions){


	int globalVariableCounter = 288;
	unsigned long long registersUsage[256];

#ifndef __NIOS

	//If we are not running on the hardware platform, we emulate its functionment and create/copy data un ac_int memories
	ac_int<128, false> localBytecode_code[1024];
	ac_int<128, false> localBinaries[4096];

	int blockSize = 0;
	int blockStart = 181;
	int indexInSourceBinaries = 167;
	for (int oneBinary = blockStart; oneBinary<432; oneBinary++){
		ac_int<128, false> syllabus = 0;

		char isInsert = 0;
		for (int oneInsertion = 1; oneInsertion<=insertions[0]; oneInsertion++)
			if (insertions[oneInsertion] == oneBinary){
				isInsert = 1;
				break;
			}

		if (!isInsert)
			indexInSourceBinaries++;

		ac_int<8, false> byte = code[oneBinary*16 + 0];
		syllabus.set_slc(96, byte);
		byte = code[oneBinary*16 + 1];
		syllabus.set_slc(96+8, byte);
		byte = code[oneBinary*16 + 2];
		syllabus.set_slc(96+16, byte);
		byte = code[oneBinary*16 + 3];
		syllabus.set_slc(96+24, byte);


		byte = code[oneBinary*16 + 4];
		syllabus.set_slc(64, byte);
		byte = code[oneBinary*16 + 5];
		syllabus.set_slc(64+8, byte);
		byte = code[oneBinary*16 + 6];
		syllabus.set_slc(64+16, byte);
		byte = code[oneBinary*16 + 7];
		syllabus.set_slc(64+24, byte);
		byte = code[oneBinary*16 + 8];
		syllabus.set_slc(32, byte);
		byte = code[oneBinary*16 + 9];
		syllabus.set_slc(40, byte);
		byte = code[oneBinary*16 + 10];
		syllabus.set_slc(48, byte);
		byte = code[oneBinary*16 + 11];
		syllabus.set_slc(56, byte);
		byte = code[oneBinary*16 + 12];
		syllabus.set_slc(0, byte);
		byte = code[oneBinary*16 + 13];
		syllabus.set_slc(8, byte);
		byte = code[oneBinary*16 + 14];
		syllabus.set_slc(16, byte);
		byte = code[oneBinary*16 + 15];
		syllabus.set_slc(24, byte);

		localBinaries[oneBinary - blockStart] = syllabus;
		blockSize = oneBinary - blockStart;
	}
	acu64 local_registersUsage[1];

	blockSize = DBTFrontend_loop(localBinaries, blockSize, localBytecode_code, globalVariables, local_registersUsage, globalVariableCounter);
	printf("blockSize %d\n", blockSize);



	ac_int<6, 0> placeOfRegisters[512] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,
			40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63};
	ac_int<6, 0> freeRegisters[64] = {36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63};
	ac_int<32, false> placeOfInstr[256];

	ac_int<32, false> localBytecode_scheduling[1024];
	for (int i = 0; i<blockSize;i++){
		localBytecode_scheduling[i*4] = localBytecode_code[i].slc<32>(96);
		localBytecode_scheduling[i*4+1] = localBytecode_code[i].slc<32>(64);
		localBytecode_scheduling[i*4+2] = localBytecode_code[i].slc<32>(32);
		localBytecode_scheduling[i*4+3] = localBytecode_code[i].slc<32>(0);
	}
	ac_int<32, false> localCode_scheduling[1024];

	int binaSize = scheduling(1,blockSize, localBytecode_scheduling,localCode_scheduling, placeOfRegisters, 32, freeRegisters, 4, 0xadb4, placeOfInstr);
	binaSize = binaSize & 0xffff;

	printf("Block is scheduled in %d cycles\n", binaSize);
	printf("Modifying binaries from %d to %d\n", blockStart, blockStart+blockSize);

	for (int i = 0; i<=binaSize*4; i++){
		code[blockStart*16+i*4] = localCode_scheduling[i].slc<8>(0);
		code[blockStart*16+i*4+1] = localCode_scheduling[i].slc<8>(8);
		code[blockStart*16+i*4+2] = localCode_scheduling[i].slc<8>(16);
		code[blockStart*16+i*4+3] = localCode_scheduling[i].slc<8>(24);
	}
	for (int i = (binaSize+1)*4; i<blockSize*4; i++){
		code[blockStart*16+i*4] = 0;
		code[blockStart*16+i*4+1] = 0;
		code[blockStart*16+i*4+2] = 0;
		code[blockStart*16+i*4+3] = 0;
	}
	#endif

}


#endif


