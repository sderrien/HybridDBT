/**************************************************************************
 * 
                       PipeLined Four Issues Processor  CUSTOM MOV
 * 
 **************************************************************************/



/*	This is a four ways pipelined (5 stages) prossecor simulator
*		All the ways can perform common operations
*		But some ways can perform further operations :
*		->	Way 1 can perform Branch operations too
*		->	Way 2 can perform Memory access operations too
* 		->	Way 4 can perform Multiplication operations too
*		
*		The different stages are :
*		-> Fetch (F) 		: Access Instruction registers and store current instruction
*		-> Decode (DC)		: Decode the instruction and select the needed operands for the next stage (including accessing to registers)
*		-> Execute (EX) 	: Do the calculating part
*		-> Memory (M)		: Access memory if needed (Only for Way 2) 
*		-> Write Back (WB) 	: Update registers value if needed
*
*		For any questions, please contact yo.uguen@gmail.com
*/


#ifndef __VLIW

#ifdef __CATAPULT
#include <ac_int.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <simulator/vexSimulator.h>

#include <lib/ac_int.h> // This is a proprietary library (Mentor Graphics - Use term agreements included in the header)
#include <map>


using namespace std;
#endif

#define MAX_NUMBER_OF_INSTRUCTIONS 65536
#define REG_NUMBER 64
#define BRANCH_UNIT_NUMBER 8
#define DATA_SIZE 65536
#define SIZE_INSTRUCTION 128
#define SIZE_MEM_ADDR 16


#define __VERBOSE
//Declaration of different structs

struct FtoDC {
	ac_int<32, false> instruction; //Instruction to execute
};

struct DCtoEx {
	ac_int<32, true> dataa; //First data from register file
	ac_int<32, true> datab; //Second data, from register file or immediate value
	ac_int<32, true> datac; //Third data used only for store instruction and corresponding to rb
	ac_int<6, false> dest;  //Register to be written
	ac_int<7, false> opCode;//OpCode of the instruction
	ac_int<32, true> memValue; //Second data, from register file or immediate value

};

struct ExtoMem {
	ac_int<32, true> result;	//Result of the EX stage
	ac_int<32, true> datac;		//Data to be stored in memory (if needed)
	ac_int<6, false> dest;		//Register to be written at WB stage
	ac_int<1, false> WBena;		//Is a WB is needed ?
	ac_int<7, false> opCode;	//OpCode of the operation
	ac_int<32, true> memValue; //Second data, from register file or immediate value
};

struct MemtoWB {
	ac_int<32, true> result;	//Result to be written back
	ac_int<6, false> dest;		//Register to be written at WB stage
	ac_int<1, false> WBena;		//Is a WB is needed ?
};

// 32 bits registers


#ifdef __CATAPULT
ac_int<32, true> REG[64];
ac_int<32, false> PC, NEXT_PC, cycle=0;

#else
// 1 bit registers
ac_int<1, false> BREG[BRANCH_UNIT_NUMBER];
// Link register

std::map<unsigned int, ac_int<32, false>> memory;
ac_int<128, false> RI[MAX_NUMBER_OF_INSTRUCTIONS];
#endif

// Cycle counter

typedef ac_int<32, false> acuint32;
typedef ac_int<8, false> acuint8;


#ifndef __CATAPULT
void VexSimulator::stb(unsigned int addr, ac_int<8, false> value){
	if (addr == 0x10009000){
		fprintf(stdout,"%c",(char) value & 0xff);
	}
	else
		memory[addr] = value;

//	fprintf(stderr, "memwrite %x %x\n", addr, value);

}

void VexSimulator::sth(unsigned int addr, ac_int<16, false> value){
	this->stb(addr+1, value.slc<8>(8));
	this->stb(addr+0, value.slc<8>(0));
}

void VexSimulator::stw(unsigned int addr, ac_int<32, false> value){
	this->stb(addr+3, value.slc<8>(24));
	this->stb(addr+2, value.slc<8>(16));
	this->stb(addr+1, value.slc<8>(8));
	this->stb(addr+0, value.slc<8>(0));
}

ac_int<8, false> VexSimulator::ldb(unsigned int addr){
	ac_int<8, false>  result = 0;
	if (addr == 0x10009000){
		result = getchar();
	}
	else if (memory.find(addr) != memory.end())
		result = memory[addr];
//	fprintf(stderr, "memread %x %x\n", addr, result);

	return result;
}

ac_int<16, false> VexSimulator::ldh(unsigned int addr){
	ac_int<16, false> result = 0;
	result.set_slc(8, ldb(addr+1));
	result.set_slc(0, ldb(addr));
	return result;
}

ac_int<32, false> VexSimulator::ldw(unsigned int addr){
	ac_int<32, false> result = 0;
	result.set_slc(24, ldb(addr+3));
	result.set_slc(16, ldb(addr+2));
	result.set_slc(8, ldb(addr+1));
	result.set_slc(0, ldb(addr+0));
	return result;
}
#endif

#ifdef __CATAPULT
	ac_int<32, false> ldw(unsigned int addr, ac_int<8, false> memory0[65536], ac_int<8, false> memory1[65536], ac_int<8,false> memory2[65536], ac_int<8, false> memory3[65536]){
		ac_int<32, false> result = 0;
		result.set_slc(0, memory0[addr>>2]);
		result.set_slc(8, memory1[addr>>2]);
		result.set_slc(16, memory2[addr>>2]);
		result.set_slc(24, memory3[addr>>2]);

		return result;
	}

#endif



#ifdef __CATAPULT
	void doWB(struct MemtoWB memtoWB){
#else
void VexSimulator::doWB(struct MemtoWB memtoWB){
#endif
	if(memtoWB.WBena && memtoWB.dest != 0){

			REG[memtoWB.dest] = memtoWB.result;
	}
}
#ifdef __CATAPULT
void doMemNoMem(struct ExtoMem extoMem, struct MemtoWB *memtoWB){
#else
	void VexSimulator::doMemNoMem(struct ExtoMem extoMem, struct MemtoWB *memtoWB){
#endif

	memtoWB->WBena = extoMem.WBena;
	memtoWB->dest = extoMem.dest;
	memtoWB->result = extoMem.result;
}

#ifdef __CATAPULT
void doMem(struct ExtoMem extoMem, struct MemtoWB *memtoWB, ac_int<8, false> memory0[65536], ac_int<8, false> memory1[65536], ac_int<8,false> memory2[65536], ac_int<8, false> memory3[65536]){
#else
	void VexSimulator::doMem(struct ExtoMem extoMem, struct MemtoWB *memtoWB){
#endif

	memtoWB->WBena = extoMem.WBena;
	memtoWB->dest = extoMem.dest;

	//IMPORTANT NOTE: In this function the value of exToMem.address is the address of the memory access to perform
	// 					and it is addressing byte. In current implementation DATA is a word array so we need to
	//					divide the address by four to have actual place inside the array

	if (extoMem.opCode.slc<3>(4) == 1){
		//The instruction is a memory access
		ac_int<32, false> address = extoMem.result;


		if (extoMem.opCode.slc<4>(0) <= 5){
			//The operation is a load instruction

			ac_int<16, false> const0_16 = 0;
			ac_int<16, false> const1_16 = 0xffff;
			ac_int<24, false> const0_24 = 0;
			ac_int<24, false> const1_24 = 0xffffff;
			ac_int<16, true> signed16Value;
			ac_int<8, true> signed8Value;

			ac_int<16, false> unsignedHalf = 0;
			ac_int<16, true> signedHalf = 0;
			ac_int<16, false> unsignedByte = 0;
			ac_int<16, true> signedByte = 0;
			char offset = (address << 3).slc<5>(0);

			switch(extoMem.opCode){
			case VEX_LDW:
				//ldw
				memtoWB->result = extoMem.memValue;
				break;
			case VEX_LDHU:
				//ldhu
				unsignedHalf.set_slc(0, extoMem.memValue.slc<16>(offset));
				memtoWB->result = unsignedHalf;
				break;
			case VEX_LDH:
				//ldh
				signedHalf.set_slc(0, extoMem.memValue.slc<16>(offset));
				memtoWB->result = signedHalf;
				break;
			case VEX_LDBU:
				//ldbu
				unsignedByte.set_slc(0, extoMem.memValue.slc<8>(offset));
				memtoWB->result = unsignedByte;
				break;
			case VEX_LDB:
				//ldb
				signedByte.set_slc(0, extoMem.memValue.slc<8>(offset));
				memtoWB->result = signedByte;
				break;
			default:
			break;
			}


		}
		else {


			memtoWB->WBena = 0; //TODO : this shouldn't be necessary : WB shouldn't be enabled before

			//We are on a store instruction
			ac_int<1, false> byteEna0=0, byteEna1=0, byteEna2=0, byteEna3=0;
			ac_int<8, false> byte0, byte1, byte2, byte3;


			switch (extoMem.opCode){
			case 0x16:
				//STW
				byte0 = extoMem.datac.slc<8>(0);
				byte1 = extoMem.datac.slc<8>(8);
				byte2 = extoMem.datac.slc<8>(16);
				byte3 = extoMem.datac.slc<8>(24);

				byteEna0 = 1;
				byteEna1 = 1;
				byteEna2 = 1;
				byteEna3 = 1;
			break;
			case 0x17:
				//STH
				if (address.slc<2>(0) == 2){
					byte2 = extoMem.datac.slc<8>(0);
					byte3 = extoMem.datac.slc<8>(8);
					byteEna2 = 1;
					byteEna3 = 1;
				}
				else {
					byte0 = extoMem.datac.slc<8>(0);
					byte1 = extoMem.datac.slc<8>(8);
					byteEna0 = 1;
					byteEna1 = 1;
				}
			case 0x18:
				//STB
				if (address.slc<2>(0) == 0){
					byte0 = extoMem.datac.slc<8>(0);
					byteEna0 = 1;
				}
				else if (address.slc<2>(0) == 1){
					byte1 = extoMem.datac.slc<8>(0);
					byteEna1 = 1;
				}
				else if (address.slc<2>(0) == 2){
					byte2 = extoMem.datac.slc<8>(0);
					byteEna2 = 1;
				}
				else{
					byte3 = extoMem.datac.slc<8>(0);
					byteEna3 = 1;
				}
				break;
			default:
			break;
			}

			#ifdef __CATAPULT

			if (byteEna0)
				memory0[address>>2] = byte0;
			if (byteEna1)
				memory1[address>>2] = byte1;
			if (byteEna2)
				memory2[address>>2] = byte2;
			if (byteEna3)
				memory3[address>>2] = byte3;

			#else
			if (byteEna0)
				this->stb((address & 0xfffffffc), byte0);
			if (byteEna1)
				this->stb((address & 0xfffffffc)+1, byte1);
			if (byteEna2)
				this->stb((address & 0xfffffffc)+2, byte2);
			if (byteEna3)
				this->stb((address & 0xfffffffc)+3, byte3);

			#endif
		}
	}
	else
		memtoWB->result = extoMem.result;
}

#ifdef __CATAPULT
void doEx(struct DCtoEx dctoEx, struct ExtoMem *extoMem){
#else
void VexSimulator::doEx(struct DCtoEx dctoEx, struct ExtoMem *extoMem){
#endif
	ac_int<5, false> shiftAmount = dctoEx.datab.slc<5>(0);

	ac_int<32, true> addDataa = (dctoEx.opCode == VEX_SH1ADD) ? (dctoEx.dataa << 1) :
				(dctoEx.opCode == VEX_SH2ADD) ? (dctoEx.dataa << 2) :
				(dctoEx.opCode == VEX_SH3ADD) ? (dctoEx.dataa << 3) :
				(dctoEx.opCode == VEX_SH4ADD) ? (dctoEx.dataa << 4) :
				dctoEx.dataa ;

	ac_int<32, true> addDatab = (dctoEx.opCode == VEX_SH3bADD || dctoEx.opCode == VEX_SH3bADDi) ? (dctoEx.datab << 3) :
					dctoEx.datab ;


	ac_int<1, false> selectAdd = (dctoEx.opCode.slc<3>(4) == 0x1) //Memory instructions
			| (dctoEx.opCode == VEX_ADD) |  (dctoEx.opCode == VEX_SH1ADD) |  (dctoEx.opCode == VEX_SH2ADD)
			| (dctoEx.opCode == VEX_SH3ADD) |  (dctoEx.opCode == VEX_SH4ADD)
			| (dctoEx.opCode == VEX_ADDi) |  (dctoEx.opCode == VEX_SH1ADDi) |  (dctoEx.opCode == VEX_SH2ADDi)
			| (dctoEx.opCode == VEX_SH3ADDi) |  (dctoEx.opCode == VEX_SH4ADDi) | (dctoEx.opCode == VEX_SH3bADD) |  (dctoEx.opCode == VEX_SH3bADDi) ;

	ac_int<1, false> selectSub = (dctoEx.opCode == VEX_SUB)| (dctoEx.opCode == VEX_SUBi);
	ac_int<1, false> selectSll = (dctoEx.opCode == VEX_SLL)| (dctoEx.opCode == VEX_SLLi);
	ac_int<1, false> selectSrl = (dctoEx.opCode == VEX_SRL)| (dctoEx.opCode == VEX_SRLi);
	ac_int<1, false> selectSra = (dctoEx.opCode == VEX_SRA)| (dctoEx.opCode == VEX_SRAi);
	ac_int<1, false> selectAnd = (dctoEx.opCode == VEX_AND)| (dctoEx.opCode == VEX_ANDi);
	ac_int<1, false> selectOr = (dctoEx.opCode == VEX_OR)| (dctoEx.opCode == VEX_ORi);
	ac_int<1, false> selectNot = (dctoEx.opCode == VEX_NOT)| (dctoEx.opCode == VEX_NOTi);
	ac_int<1, false> selectXor = (dctoEx.opCode == VEX_XOR)| (dctoEx.opCode == VEX_XORi);
	ac_int<1, false> selectNor = (dctoEx.opCode == VEX_NOR)| (dctoEx.opCode == VEX_NORi);
	ac_int<1, false> selectCmp = (dctoEx.opCode == VEX_CMPLT)| (dctoEx.opCode == VEX_CMPLTi)
			| (dctoEx.opCode == VEX_CMPLTU)| (dctoEx.opCode == VEX_CMPLTUi)
			| (dctoEx.opCode == VEX_CMPNE)| (dctoEx.opCode == VEX_CMPNEi)
			| (dctoEx.opCode == VEX_CMPEQ)| (dctoEx.opCode == VEX_CMPEQi)
			| (dctoEx.opCode == VEX_CMPGE)| (dctoEx.opCode == VEX_CMPGEi)
			| (dctoEx.opCode == VEX_CMPGEU)| (dctoEx.opCode == VEX_CMPGEUi)
			| (dctoEx.opCode == VEX_CMPGT)| (dctoEx.opCode == VEX_CMPGTi)
			| (dctoEx.opCode == VEX_CMPGTU)| (dctoEx.opCode == VEX_CMPGTUi)
			| (dctoEx.opCode == VEX_CMPLE)| (dctoEx.opCode == VEX_CMPLEi)
			| (dctoEx.opCode == VEX_CMPLEU)| (dctoEx.opCode == VEX_CMPLEUi);



	ac_int<32, false> unsigned_dataa = dctoEx.dataa;
	ac_int<32, false> unsigned_datab = dctoEx.datab;

	ac_int<32, true> add_result = addDataa + addDatab;
	ac_int<32, true> sub_result = dctoEx.dataa - dctoEx.datab;
	ac_int<32, true> sl_result = dctoEx.dataa << shiftAmount;
	ac_int<32, true> srl_result = unsigned_dataa >> shiftAmount;
	ac_int<32, true> sra_result = dctoEx.dataa >> shiftAmount;

	ac_int<32, true> and_result = dctoEx.dataa & dctoEx.datab;
	ac_int<32, true> or_result = dctoEx.dataa | dctoEx.datab;
	ac_int<32, true> not_result = ~dctoEx.dataa;
	ac_int<32, true> xor_result = dctoEx.dataa ^ dctoEx.datab;
	ac_int<32, true> nor_result = ~(dctoEx.dataa | dctoEx.datab);

	ac_int<32, true> unsigned_sub_result = unsigned_dataa - unsigned_datab;

	ac_int<1, false> cmpResult_1 = ((dctoEx.opCode == VEX_CMPLT) | (dctoEx.opCode == VEX_CMPLTi)) ? sub_result < 0 :
			((dctoEx.opCode == VEX_CMPLTU)| (dctoEx.opCode == VEX_CMPLTUi)) ? unsigned_dataa < unsigned_datab :
			((dctoEx.opCode == VEX_CMPNE)| (dctoEx.opCode == VEX_CMPNEi)) ? sub_result != 0 :
			((dctoEx.opCode == VEX_CMPEQ)| (dctoEx.opCode == VEX_CMPEQi)) ? sub_result == 0 :
			((dctoEx.opCode == VEX_CMPGE)| (dctoEx.opCode == VEX_CMPGEi)) ? sub_result >= 0 :
			((dctoEx.opCode == VEX_CMPGEU)| (dctoEx.opCode == VEX_CMPGEUi)) ? unsigned_dataa >= unsigned_datab :
			((dctoEx.opCode == VEX_CMPGT)| (dctoEx.opCode == VEX_CMPGTi)) ? sub_result > 0 :
			((dctoEx.opCode == VEX_CMPGTU)| (dctoEx.opCode == VEX_CMPGTUi)) ? unsigned_sub_result > 0 :
			((dctoEx.opCode == VEX_CMPLE)| (dctoEx.opCode == VEX_CMPLEi)) ? sub_result <= 0 :
			sub_result <= 0;

	ac_int<32, true> cmpResult = cmpResult_1;

	extoMem->result = selectAdd ? add_result :
			selectSub ? sub_result :
			selectSll ? sl_result :
			selectSra ? sra_result :
			selectSrl ? srl_result :
			selectAnd ? and_result :
			selectOr ? or_result :
			selectNot ? not_result :
			selectXor ? xor_result :
			selectNor ? nor_result :
			selectCmp ? cmpResult :
			dctoEx.dataa;


	extoMem->WBena = !(dctoEx.opCode == VEX_NOP); //TODO
	extoMem->datac = dctoEx.datac;
	extoMem->dest = dctoEx.dest;
	extoMem->opCode = dctoEx.opCode;
	extoMem->memValue = dctoEx.memValue;
}

#ifdef __CATAPULT
void doExMult(struct DCtoEx dctoEx, struct ExtoMem *extoMem){
#else
void VexSimulator::doExMult(struct DCtoEx dctoEx, struct ExtoMem *extoMem){
#endif
	ac_int<5, false> shiftAmount = dctoEx.datab.slc<5>(0);

	ac_int<32, true> addDataa = (dctoEx.opCode == VEX_SH1ADD) ? (dctoEx.dataa << 1) :
				(dctoEx.opCode == VEX_SH2ADD) ? (dctoEx.dataa << 2) :
				(dctoEx.opCode == VEX_SH3ADD) ? (dctoEx.dataa << 3) :
				(dctoEx.opCode == VEX_SH4ADD) ? (dctoEx.dataa << 4) :
				dctoEx.dataa ;

	ac_int<32, true> addDatab = (dctoEx.opCode == VEX_SH3bADD || dctoEx.opCode == VEX_SH3bADDi) ? (dctoEx.datab << 3) :
					dctoEx.datab ;


	ac_int<1, false> selectAdd = (dctoEx.opCode.slc<3>(4) == 0x1) //Memory instructions
			| (dctoEx.opCode == VEX_ADD) |  (dctoEx.opCode == VEX_SH1ADD) |  (dctoEx.opCode == VEX_SH2ADD)
			| (dctoEx.opCode == VEX_SH3ADD) |  (dctoEx.opCode == VEX_SH4ADD)
			| (dctoEx.opCode == VEX_ADDi) |  (dctoEx.opCode == VEX_SH1ADDi) |  (dctoEx.opCode == VEX_SH2ADDi)
			| (dctoEx.opCode == VEX_SH3ADDi) |  (dctoEx.opCode == VEX_SH4ADDi) | (dctoEx.opCode == VEX_SH3bADD) |  (dctoEx.opCode == VEX_SH3bADDi)
			| (dctoEx.opCode == VEX_AUIPC);

	ac_int<1, false> selectSub = (dctoEx.opCode == VEX_SUB)| (dctoEx.opCode == VEX_SUBi);
	ac_int<1, false> selectSll = (dctoEx.opCode == VEX_SLL)| (dctoEx.opCode == VEX_SLLi);
	ac_int<1, false> selectSrl = (dctoEx.opCode == VEX_SRL)| (dctoEx.opCode == VEX_SRLi);
	ac_int<1, false> selectSra = (dctoEx.opCode == VEX_SRA)| (dctoEx.opCode == VEX_SRAi);
	ac_int<1, false> selectAnd = (dctoEx.opCode == VEX_AND)| (dctoEx.opCode == VEX_ANDi);
	ac_int<1, false> selectOr = (dctoEx.opCode == VEX_OR)| (dctoEx.opCode == VEX_ORi);
	ac_int<1, false> selectNot = (dctoEx.opCode == VEX_NOT)| (dctoEx.opCode == VEX_NOTi);
	ac_int<1, false> selectXor = (dctoEx.opCode == VEX_XOR)| (dctoEx.opCode == VEX_XORi);
	ac_int<1, false> selectNor = (dctoEx.opCode == VEX_NOR)| (dctoEx.opCode == VEX_NORi);
	ac_int<1, false> selectCmp = (dctoEx.opCode == VEX_CMPLT)| (dctoEx.opCode == VEX_CMPLTi)
			| (dctoEx.opCode == VEX_CMPLTU)| (dctoEx.opCode == VEX_CMPLTUi)
			| (dctoEx.opCode == VEX_CMPNE)| (dctoEx.opCode == VEX_CMPNEi)
			| (dctoEx.opCode == VEX_CMPEQ)| (dctoEx.opCode == VEX_CMPEQi)
			| (dctoEx.opCode == VEX_CMPGE)| (dctoEx.opCode == VEX_CMPGEi)
			| (dctoEx.opCode == VEX_CMPGEU)| (dctoEx.opCode == VEX_CMPGEUi)
			| (dctoEx.opCode == VEX_CMPGT)| (dctoEx.opCode == VEX_CMPGTi)
			| (dctoEx.opCode == VEX_CMPGTU)| (dctoEx.opCode == VEX_CMPGTUi)
			| (dctoEx.opCode == VEX_CMPLE)| (dctoEx.opCode == VEX_CMPLEi)
			| (dctoEx.opCode == VEX_CMPLEU)| (dctoEx.opCode == VEX_CMPLEUi);



	ac_int<32, false> unsigned_dataa = dctoEx.dataa;
	ac_int<32, false> unsigned_datab = dctoEx.datab;

	ac_int<32, true> add_result = addDataa + addDatab;
	ac_int<32, true> sub_result = dctoEx.dataa - dctoEx.datab;
	ac_int<32, true> sl_result = dctoEx.dataa << shiftAmount;
	ac_int<32, true> srl_result = unsigned_dataa >> shiftAmount;
	ac_int<32, true> sra_result = dctoEx.dataa >> shiftAmount;

	ac_int<32, true> and_result = dctoEx.dataa & dctoEx.datab;
	ac_int<32, true> or_result = dctoEx.dataa | dctoEx.datab;
	ac_int<32, true> not_result = ~dctoEx.dataa;
	ac_int<32, true> xor_result = dctoEx.dataa ^ dctoEx.datab;
	ac_int<32, true> nor_result = ~(dctoEx.dataa | dctoEx.datab);

	ac_int<64, true> mul_result = dctoEx.dataa * dctoEx.datab;
	ac_int<32, true> mullo_result = mul_result.slc<32>(0);
	ac_int<32, true> mulhi_result = mul_result.slc<32>(32);

	ac_int<64, false> mulu_result = unsigned_dataa * unsigned_datab;
	ac_int<32, true> mulhiu_result = mulu_result.slc<32>(32);

	ac_int<64, false> mulsu_result = dctoEx.dataa * unsigned_datab;
	ac_int<32, true> mulhisu_result = mulsu_result.slc<32>(32);

	ac_int<33, true> const0 = 0;

#ifdef __CATAPULT
	ac_int<32, true> divlo_result = 0; //Currently catapult version do not do division
	ac_int<32, true> divhi_result = 0;
#else
	ac_int<32, true> divlo_result = !dctoEx.datab ? const0 : dctoEx.dataa / dctoEx.datab;
	ac_int<32, true> divhi_result = !dctoEx.datab ? dctoEx.datab : dctoEx.dataa % dctoEx.datab;
#endif
	ac_int<32, true> unsigned_sub_result = unsigned_dataa - unsigned_datab;

	ac_int<1, false> cmpResult_1 = ((dctoEx.opCode == VEX_CMPLT) | (dctoEx.opCode == VEX_CMPLTi)) ? sub_result < 0 :
			((dctoEx.opCode == VEX_CMPLTU)| (dctoEx.opCode == VEX_CMPLTUi)) ? unsigned_sub_result < 0 :
			((dctoEx.opCode == VEX_CMPNE)| (dctoEx.opCode == VEX_CMPNEi)) ? sub_result != 0 :
			((dctoEx.opCode == VEX_CMPEQ)| (dctoEx.opCode == VEX_CMPEQi)) ? unsigned_sub_result == 0 :
			((dctoEx.opCode == VEX_CMPGE)| (dctoEx.opCode == VEX_CMPGEi)) ? sub_result >= 0 :
			((dctoEx.opCode == VEX_CMPGEU)| (dctoEx.opCode == VEX_CMPGEUi)) ? unsigned_sub_result >=0 :
			((dctoEx.opCode == VEX_CMPGT)| (dctoEx.opCode == VEX_CMPGTi)) ? sub_result > 0 :
			((dctoEx.opCode == VEX_CMPGTU)| (dctoEx.opCode == VEX_CMPGTUi)) ? unsigned_sub_result > 0 :
			((dctoEx.opCode == VEX_CMPLE)| (dctoEx.opCode == VEX_CMPLEi)) ? sub_result <= 0 :
			sub_result <= 0;

	ac_int<32, true> cmpResult = cmpResult_1;

	extoMem->result = selectAdd ? add_result :
			selectSub ? sub_result :
			selectSll ? sl_result :
			selectSra ? sra_result :
			selectSrl ? srl_result :
			selectAnd ? and_result :
			selectOr ? or_result :
			selectNot ? not_result :
			selectXor ? xor_result :
			selectNor ? nor_result :
			selectCmp ? cmpResult :
			(dctoEx.opCode == VEX_MPYLO) ? mullo_result :
			(dctoEx.opCode == VEX_MPYHI) ? mulhi_result :
			(dctoEx.opCode == VEX_MPYHISU) ? mulhisu_result :
			(dctoEx.opCode == VEX_MPYHIU) ? mulhiu_result :
			(dctoEx.opCode == VEX_DIVLO) ? divlo_result :
			(dctoEx.opCode == VEX_DIVHI) ? divhi_result :
			dctoEx.dataa;


	extoMem->WBena = !(dctoEx.opCode == VEX_NOP); //TODO
	extoMem->datac = dctoEx.datac;
	extoMem->dest = dctoEx.dest;
	extoMem->opCode = dctoEx.opCode;
}

#ifdef __CATAPULT
void doDC(struct FtoDC ftoDC, struct DCtoEx *dctoEx){
#else
void VexSimulator::doDC(struct FtoDC ftoDC, struct DCtoEx *dctoEx){
#endif

	ac_int<6, false> RA = ftoDC.instruction.slc<6>(26);
	ac_int<6, false> RB = ftoDC.instruction.slc<6>(20);
	ac_int<6, false> RC = ftoDC.instruction.slc<6>(14);
	ac_int<19, false> IMM19 = ftoDC.instruction.slc<19>(7);
	ac_int<13, false> IMM13 = ftoDC.instruction.slc<13>(7);
	ac_int<7, false> OP = ftoDC.instruction.slc<7>(0);
	ac_int<3, false> BEXT = ftoDC.instruction.slc<3>(8);
	ac_int<9, false> IMM9 = ftoDC.instruction.slc<9>(11);

	ac_int<1, false> isIType = (OP.slc<3>(4) == 2);
	ac_int<1, false> isImm = OP.slc<3>(4) == 1 || OP.slc<3>(4) == 6 || OP.slc<3>(4) == 7;

	ac_int<1, false> isUnsigned = (OP == VEX_MPYLLU) | (OP == VEX_MPYLHU) | (OP == VEX_MPYHHU) | (OP == VEX_MPYLU)
			| (OP == VEX_MPYHIU) | (OP == VEX_CMPLTU) | (OP == VEX_CMPLTUi) | (OP == VEX_CMPGEU) | (OP == VEX_CMPGEUi)
			| (OP == VEX_ZXTB)  | (OP == VEX_ZXTBi) | (OP == VEX_ZXTH) | (OP == VEX_ZXTHi) | (OP == VEX_CMPGTU)
			| (OP == VEX_CMPGTUi) | (OP == VEX_CMPLEU) | (OP == VEX_CMPLEUi);


	ac_int<23, false> const0_23 = 0;
	ac_int<23, false> const1_23 = 0x7fffff;
	ac_int<19, false> const0_19 = 0;
	ac_int<19, false> const1_19 = 0x7ffff;
	ac_int<13, false> const0_13 = 0;
	ac_int<13, false> const1_13 = 0x1fff;

	ac_int<6, false> secondRegAccess = RB;


	ac_int<32, true> regValueA = REG[RA];
	ac_int<32, true> regValueB = REG[secondRegAccess];

	dctoEx->opCode = OP;
	dctoEx->datac = regValueB; //For store instructions

	if (isIType){
		//The instruction is I type
		dctoEx->dest = RA;

		dctoEx->dataa.set_slc(0, IMM19);
		if (IMM19[18])
			dctoEx->dataa.set_slc(19, const1_13);
		else
			dctoEx->dataa.set_slc(19, const0_13);


		if (OP == VEX_AUIPC)
			dctoEx->dataa = dctoEx->dataa<<12;

		dctoEx->datab = PC-1; //This will be used for AUIPC

	}
	else{
		//The instruction is R type
		dctoEx->dataa = regValueA;

		if (isImm){
			dctoEx->dest = RB;
			dctoEx->datab.set_slc(0, IMM13);
			if (IMM13[12] && !isUnsigned)
				dctoEx->datab.set_slc(13, const1_19);
			else
				dctoEx->datab.set_slc(13, const0_19);
		}
		else{
			dctoEx->dest = RC;
			dctoEx->datab = regValueB;
		}
	}
}

#ifdef __CATAPULT
void doDCMem(struct FtoDC ftoDC, struct DCtoEx *dctoEx, ac_int<8, false> memory0[65536], ac_int<8, false> memory1[65536], ac_int<8,false> memory2[65536], ac_int<8, false> memory3[65536]){
#else
void VexSimulator::doDCMem(struct FtoDC ftoDC, struct DCtoEx *dctoEx){
#endif

	ac_int<6, false> RA = ftoDC.instruction.slc<6>(26);
	ac_int<6, false> RB = ftoDC.instruction.slc<6>(20);
	ac_int<6, false> RC = ftoDC.instruction.slc<6>(14);
	ac_int<19, false> IMM19 = ftoDC.instruction.slc<19>(7);
	ac_int<13, false> IMM13 = ftoDC.instruction.slc<13>(7);
	ac_int<13, true> IMM13_signed = ftoDC.instruction.slc<13>(7);

	ac_int<7, false> OP = ftoDC.instruction.slc<7>(0);
	ac_int<3, false> BEXT = ftoDC.instruction.slc<3>(8);
	ac_int<9, false> IMM9 = ftoDC.instruction.slc<9>(11);

	ac_int<1, false> isIType = (OP.slc<3>(4) == 2);
	ac_int<1, false> isImm = OP.slc<3>(4) == 1 || OP.slc<3>(4) == 6 || OP.slc<3>(4) == 7;

	ac_int<1, false> isUnsigned = (OP == VEX_MPYLLU) | (OP == VEX_MPYLHU) | (OP == VEX_MPYHHU) | (OP == VEX_MPYLU)
			| (OP == VEX_MPYHIU) | (OP == VEX_CMPLTU) | (OP == VEX_CMPLTUi) | (OP == VEX_CMPGEU) | (OP == VEX_CMPGEUi)
			| (OP == VEX_ZXTB)  | (OP == VEX_ZXTBi) | (OP == VEX_ZXTH) | (OP == VEX_ZXTHi) | (OP == VEX_CMPGTU)
			| (OP == VEX_CMPGTUi) | (OP == VEX_CMPLEU) | (OP == VEX_CMPLEUi);


	ac_int<23, false> const0_23 = 0;
	ac_int<23, false> const1_23 = 0x7fffff;
	ac_int<19, false> const0_19 = 0;
	ac_int<19, false> const1_19 = 0x7ffff;
	ac_int<13, false> const0_13 = 0;
	ac_int<13, false> const1_13 = 0x1fff;

	ac_int<6, false> secondRegAccess = RB;


	ac_int<32, true> regValueA = REG[RA];
	ac_int<32, true> regValueB = REG[secondRegAccess];

	dctoEx->opCode = OP;
	dctoEx->datac = regValueB; //For store instructions

	if (isIType){
		//The instruction is I type
		dctoEx->dest = RA;

		dctoEx->dataa.set_slc(0, IMM19);
		if (IMM19[18])
			dctoEx->dataa.set_slc(19, const1_13);
		else
			dctoEx->dataa.set_slc(19, const0_13);


		if (OP == VEX_AUIPC)
			dctoEx->dataa = dctoEx->dataa<<12;

		dctoEx->datab = PC-1; //This will be used for AUIPC

	}
	else{
		//The instruction is R type
		dctoEx->dataa = regValueA;

		if (isImm){
			dctoEx->dest = RB;
			dctoEx->datab.set_slc(0, IMM13);
			if (IMM13[12] && !isUnsigned)
				dctoEx->datab.set_slc(13, const1_19);
			else
				dctoEx->datab.set_slc(13, const0_19);
		}
		else{
			dctoEx->dest = RC;
			dctoEx->datab = regValueB;
		}
	}

	ac_int<32, false> address = IMM13_signed + regValueA;

	if (OP.slc<4>(4) == 1 && (address != 0x10009000 || OP.slc<4>(0) <= 5)){
		//If we are in a memory access, the access is initiated here
		#ifdef __CATAPULT
		dctoEx->memValue = ldw(address & 0xfffffffc, memory0, memory1, memory2, memory3);
		#else
		dctoEx->memValue = this->ldw(address & 0xfffffffc);
		#endif
	}

}

#ifdef __CATAPULT
void doDCBr(struct FtoDC ftoDC, struct DCtoEx *dctoEx){
#else
void VexSimulator::doDCBr(struct FtoDC ftoDC, struct DCtoEx *dctoEx){
#endif
	ac_int<6, false> RA = ftoDC.instruction.slc<6>(26);
	ac_int<6, false> RB = ftoDC.instruction.slc<6>(20);
	ac_int<6, false> RC = ftoDC.instruction.slc<6>(14);
	ac_int<19, true> IMM19 = ftoDC.instruction.slc<19>(7);
	ac_int<13, false> IMM13 = ftoDC.instruction.slc<13>(7);
	ac_int<7, false> OP = ftoDC.instruction.slc<7>(0);
	ac_int<3, false> BEXT = ftoDC.instruction.slc<3>(8);
	ac_int<9, false> IMM9 = ftoDC.instruction.slc<9>(11);

	ac_int<1, false> isIType = (OP.slc<3>(4) == 2);
	ac_int<1, false> isImm = OP.slc<3>(4) == 1 || OP.slc<3>(4) == 6 || OP.slc<3>(4) == 7;

	ac_int<1, false> isUnsigned = (OP == VEX_MPYLLU) | (OP == VEX_MPYLHU) | (OP == VEX_MPYHHU) | (OP == VEX_MPYLU)
			| (OP == VEX_MPYHIU) | (OP == VEX_CMPLTU) | (OP == VEX_CMPLTUi) | (OP == VEX_CMPGEU) | (OP == VEX_CMPGEUi)
			| (OP == VEX_ZXTB)  | (OP == VEX_ZXTBi) | (OP == VEX_ZXTH) | (OP == VEX_ZXTHi) | (OP == VEX_CMPGTU)
			| (OP == VEX_CMPGTUi) | (OP == VEX_CMPLEU) | (OP == VEX_CMPLEUi);


	ac_int<23, false> const0_23 = 0;
	ac_int<23, false> const1_23 = 0x7fffff;
	ac_int<19, false> const0_19 = 0;
	ac_int<19, false> const1_19 = 0x7ffff;
	ac_int<13, false> const0_13 = 0;
	ac_int<13, false> const1_13 = 0x1fff;

	ac_int<6, false> secondRegAccess = RB;


	ac_int<32, true> regValueA = REG[RA];
	ac_int<32, true> regValueB = REG[secondRegAccess];

	dctoEx->opCode = OP;
	dctoEx->datac = regValueB;

	if (isIType){
		//The instruction is I type
		dctoEx->dest = RA;
		dctoEx->dataa = IMM19;


		if (OP == VEX_AUIPC)
			dctoEx->dataa = dctoEx->dataa<<12;

		dctoEx->datab = PC-1; //This will be used for AUIPC
	}
	else{
		//The instruction is R type
		dctoEx->dataa = regValueA;

		if (isImm){
			dctoEx->dest = RB;
			dctoEx->datab.set_slc(0, IMM13);
			if (IMM13[12] && !isUnsigned)
				dctoEx->datab.set_slc(13, const1_19);
			else
				dctoEx->datab.set_slc(13, const0_19);
		}
		else{
			dctoEx->dest = RC;
			dctoEx->datab = regValueB;
		}
	}
	switch(OP){ // Select the right operation and place the right values into the right operands
			case VEX_GOTO:
				dctoEx->opCode = 0;
				NEXT_PC = IMM19;
				break;	// GOTO1

			case VEX_CALL:
				dctoEx->dataa = PC + 1;
				NEXT_PC = IMM19;
				break; // CALL

			case VEX_CALLR :
				dctoEx->dataa = PC + 1;
				NEXT_PC = regValueA + IMM19;
				break; // ICALL

			case VEX_GOTOR :
				dctoEx->opCode = 0;
				NEXT_PC = regValueA+IMM19;
				break; //IGOTO

			case VEX_BR :
				dctoEx->opCode = 0;
				if(regValueA)
					NEXT_PC = PC + dctoEx->dataa -1;
				break;	// BR

			case VEX_BRF :
				dctoEx->opCode = 0;
				if(!regValueA)
					NEXT_PC = PC + dctoEx->dataa -1;
				break;	// BRF

			case VEX_RETURN :
				dctoEx->opCode = 0;
				REG[1] = REG[1] + IMM19;
				NEXT_PC = REG[63];

				break; // RETURN
			default:
				break;
			}
}


#ifdef __CATAPULT
int run(int mainPc, ac_int<8, false> memory0[65536], ac_int<8, false> memory1[65536], ac_int<8,false> memory2[65536], ac_int<8, false> memory3[65536], ac_int<128, false> RI[65536]){
#else
int VexSimulator::run(int mainPc){
#endif
	// Initialise program counter
	PC = mainPc;
	REG[2] = 0x70000;

	/*********************************************************************
	 * Definition and initialization of all pipeline registers
	 *********************************************************************/
	struct MemtoWB memtoWB1;	struct MemtoWB memtoWB2;	struct MemtoWB memtoWB3;	struct MemtoWB memtoWB4;	struct MemtoWB memtoWB5; 	struct MemtoWB memtoWB6;	struct MemtoWB memtoWB7;	struct MemtoWB memtoWB8;
	memtoWB1.WBena = 0;	memtoWB2.WBena = 0;	memtoWB3.WBena = 0;	memtoWB4.WBena = 0;	memtoWB5.WBena = 0;	memtoWB6.WBena = 0;	memtoWB7.WBena = 0;	memtoWB8.WBena = 0;

	struct ExtoMem extoMem1;	struct ExtoMem extoMem2;	struct ExtoMem extoMem3;	struct ExtoMem extoMem4;	struct ExtoMem extoMem5;	struct ExtoMem extoMem6;	struct ExtoMem extoMem7;	struct ExtoMem extoMem8;
	extoMem1.WBena = 0;	extoMem2.WBena = 0;	extoMem3.WBena = 0;	extoMem4.WBena = 0;	extoMem5.WBena = 0;	extoMem6.WBena = 0;	extoMem7.WBena = 0;	extoMem8.WBena = 0;
	extoMem1.opCode = 0; extoMem2.opCode = 0; extoMem3.opCode = 0; extoMem4.opCode = 0; extoMem5.opCode = 0; extoMem6.opCode = 0; extoMem7.opCode = 0; extoMem8.opCode = 0;

	struct DCtoEx dctoEx1; struct DCtoEx dctoEx2;	struct DCtoEx dctoEx3;	struct DCtoEx dctoEx4;	struct DCtoEx dctoEx5;	struct DCtoEx dctoEx6;	struct DCtoEx dctoEx7;	struct DCtoEx dctoEx8;
	dctoEx1.opCode = 0;	dctoEx2.opCode = 0;	dctoEx3.opCode = 0;	dctoEx4.opCode = 0;	dctoEx5.opCode = 0;	dctoEx6.opCode = 0;	dctoEx7.opCode = 0;	dctoEx8.opCode = 0;

	struct FtoDC ftoDC1;	struct FtoDC ftoDC2;	struct FtoDC ftoDC3;	struct FtoDC ftoDC4;	struct FtoDC ftoDC5;	struct FtoDC ftoDC6;	struct FtoDC ftoDC7;	struct FtoDC ftoDC8;
	ftoDC1.instruction = 0;	ftoDC2.instruction = 0;	ftoDC3.instruction = 0;	ftoDC4.instruction = 0;	ftoDC5.instruction = 0;	ftoDC6.instruction = 0;	ftoDC7.instruction = 0;	ftoDC8.instruction = 0;



	int stop = 0;



	/*	Description of the VLIW pipeline.
	 * Above we defined :
	 * 	-> Four groups of function which performs a pipeline step :
	 * 		* doDC performs instruction decoding, register file access and intiate memory access (if needed)
	 * 		* doEx performs ALU operations and select the correct result
	 * 		* doMem performs the memory write and read the memory operation initiated at dc
	 * 		* doWB write the result back in the result
	 *
	 * 	-> data structures to store values from one step to the next
	 *
	 * 	The pipeline description is done as follow:
	 *
	 * 	To describe a five step pipeline, we call each pipeline step from the last to the first:
	 * 		WB, Mem, Ex, DC and Fetch
	 * 	Then, after five loop iterations, an instruction fetched at iteration i will be decoded at iteration i+1,
	 * 	executed at iteration i+2, performs memory access at i+3 and the result will be wrote back at i+4.
	 *
	 *
	 * 	In the following we describe a three step pipeline (with zero latency eg. an instruction can use the result of *
	 * 	instructions from previous cycle.  One issue is organized as a four step pipeline to perform multiplication.
	 *
	 * 	Pipeline way 1 : branch and common
	 * 	Pipeline way 2 : Memory and common
	 * 	Pipeline way 3 : Mult and common (one more step)
	 *  Pipeline way 4 : Common only
	 *
	 */

	while (PC < MAX_NUMBER_OF_INSTRUCTIONS){


		doWB(memtoWB3);


		///////////////////////////////////////////////////////
		//													 //
		//                         EX                        //
		//													 //
		///////////////////////////////////////////////////////


		doEx(dctoEx4, &extoMem4);
		doEx(dctoEx1, &extoMem1);
		doExMult(dctoEx3, &extoMem3);
		doEx(dctoEx2, &extoMem2);
//		doEx(dctoEx5, &extoMem5);
//		doExMem(dctoEx6, &extoMem6);
//		doEx(dctoEx7, &extoMem7);
//		doExMult(dctoEx8, &extoMem8);



		///////////////////////////////////////////////////////
		//													 //
		//                       M                           //
		//													 //
		///////////////////////////////////////////////////////




		// If the operation code is 0x2f then the processor stops
		if(stop == 1){
			return PC;
		}


		doMemNoMem(extoMem1, &memtoWB1);
		doMemNoMem(extoMem3, &memtoWB3);
		doMemNoMem(extoMem4, &memtoWB4);
//		doMemNoMem(extoMem5, &memtoWB5);
//		doMemNoMem(extoMem7, &memtoWB7);
//		doMemNoMem(extoMem8, &memtoWB8);

#ifdef __CATAPULT
		doMem(extoMem2, &memtoWB2, memory0, memory1, memory2, memory3);
#else
		doMem(extoMem2, &memtoWB2);
#endif

		//		doMem(extoMem6, &memtoWB6, DATA0, DATA1, DATA2, DATA3);



		///////////////////////////////////////////////////////
		//													 //
		//                       WB                          //
		//  												 //
		///////////////////////////////////////////////////////

		doWB(memtoWB1);
		doWB(memtoWB2);
		doWB(memtoWB4);
//		doWB(memtoWB5);
//		doWB(memtoWB6);
//		doWB(memtoWB7);
//		doWB(memtoWB8);

		///////////////////////////////////////////////////////
		//													 //
		//                       DC                          //
		//													 //
		///////////////////////////////////////////////////////

		NEXT_PC = PC+1;
		doDCBr(ftoDC1, &dctoEx1);

#ifdef __CATAPULT
		doDCMem(ftoDC2, &dctoEx2, memory0, memory1, memory2, memory3);
#else
		doDCMem(ftoDC2, &dctoEx2);
#endif
		doDC(ftoDC3, &dctoEx3);
		doDC(ftoDC4, &dctoEx4);
//		doDC(ftoDC5, &dctoEx5);
//		doDC(ftoDC6, &dctoEx6);
//		doDC(ftoDC7, &dctoEx7);
//		doDC(ftoDC8, &dctoEx8);

		ac_int<7, false> OP1 = ftoDC1.instruction.slc<7>(0);

		// If the operation code is 0x2f then the processor stops
		if(OP1 == 0x2F){
			stop = 1;

			#ifndef __CATAPULT
			fprintf(stderr,"Simulation finished in %d cycles \n", cycle);
			#endif
		}


		///////////////////////////////////////////////////////
		//                       F                           //
		///////////////////////////////////////////////////////


		// Retrieving new instruction
		ac_int<SIZE_INSTRUCTION, false> vliw = RI[PC];

		// Redirect instructions to thier own ways
		ftoDC1.instruction = vliw.slc<32>(96);
		ftoDC2.instruction = vliw.slc<32>(64);
		ftoDC3.instruction = vliw.slc<32>(32);
		ftoDC4.instruction = vliw.slc<32>(0);
//		ftoDC5.instruction = vliw.slc<32>(96);
//		ftoDC6.instruction = vliw.slc<32>(64);
//		ftoDC7.instruction = vliw.slc<32>(32);
//		ftoDC8.instruction = vliw.slc<32>(0);

		int pcValueForDebug = PC;
		// Next instruction
		PC = NEXT_PC;
		cycle++;

		// DISPLAY

#ifndef __CATAPULT
		if (debugLevel >= 1){
			std::cerr << std::to_string(cycle) + ";" + std::to_string(pcValueForDebug) + ";";
			std::cerr << printDecodedInstr(ftoDC1.instruction);
			std::cerr << " ";
			std::cerr << printDecodedInstr(ftoDC2.instruction);
			std::cerr << " ";
			std::cerr << printDecodedInstr(ftoDC3.instruction);
			std::cerr << " ";
			std::cerr << printDecodedInstr(ftoDC4.instruction);
			std::cerr << " ";

		}

		if (debugLevel >= 1){
			for (int i = 0; i<34; i++)
				fprintf(stderr, ";%x", (int) REG[i]);

			fprintf(stderr, "\n");
		}
#endif
	}

	return PC;
}

#ifndef __CATAPULT

void VexSimulator::initializeDataMemory(unsigned char* content, unsigned int size, unsigned int start){
	for (int i = 0; i<size; i++){
		ac_int<8, false> oneByte = content[i];
		stb(i+start, oneByte);
	}
}

void VexSimulator::initializeDataMemory(ac_int<32, false>* content, unsigned int size, unsigned int start){
	for (int i = 0; i<size; i+=4){
		stw(i+start, content[i>>2]);
	}
}

void VexSimulator::initializeCodeMemory(unsigned char* content, unsigned int size, unsigned int start){
	for (int i = 0; i<size/16; i++){
		ac_int<8, false> instr1a = content[16*i];
		ac_int<8, false> instr1b = content[16*i+1];
		ac_int<8, false> instr1c = content[16*i+2];
		ac_int<8, false> instr1d = content[16*i+3];

		ac_int<32, false> instr1 = 0;
		instr1.set_slc(24, instr1d);
		instr1.set_slc(16, instr1c);
		instr1.set_slc(8, instr1b);
		instr1.set_slc(0, instr1a);

		//*********** Instr2
		ac_int<8, false> instr2a = content[16*i+4];
		ac_int<8, false> instr2b = content[16*i+5];
		ac_int<8, false> instr2c = content[16*i+6];
		ac_int<8, false> instr2d = content[16*i+7];

		ac_int<32, false> instr2 = 0;
		instr2.set_slc(24, instr2d);
		instr2.set_slc(16, instr2c);
		instr2.set_slc(8, instr2b);
		instr2.set_slc(0, instr2a);

		ac_int<8, false> instr3a = content[16*i+8];
		ac_int<8, false> instr3b = content[16*i+9];
		ac_int<8, false> instr3c = content[16*i+10];
		ac_int<8, false> instr3d = content[16*i+11];

		ac_int<32, false> instr3 = 0;
		instr3.set_slc(24, instr3d);
		instr3.set_slc(16, instr3c);
		instr3.set_slc(8, instr3b);
		instr3.set_slc(0, instr3a);

		ac_int<8, false> instr4a = content[16*i+12];
		ac_int<8, false> instr4b = content[16*i+13];
		ac_int<8, false> instr4c = content[16*i+14];
		ac_int<8, false> instr4d = content[16*i+15];

		ac_int<32, false> instr4 = 0;
		instr4.set_slc(24, instr4d);
		instr4.set_slc(16, instr4c);
		instr4.set_slc(8, instr4b);
		instr4.set_slc(0, instr4a);

		RI[i+start].set_slc(96, instr1);
		RI[i+start].set_slc(64, instr2);
		RI[i+start].set_slc(32, instr3);
		RI[i+start].set_slc(0, instr4);

		if (this->debugLevel >= 1){
			fprintf(stderr, "objdump;%d; ", (int) i);
			printDecodedInstr(instr1); fprintf(stderr, " ");
			printDecodedInstr(instr2); fprintf(stderr, " ");
			printDecodedInstr(instr3); fprintf(stderr, " ");
			printDecodedInstr(instr4); fprintf(stderr, "\n");
		}

	}
}

void VexSimulator::initializeCodeMemory(ac_int<128, false>* content, unsigned int size, unsigned int start){
	for (int i = 0; i<size/16; i++){

		RI[i+start]= content[i];



		if (this->debugLevel >= 1){
			fprintf(stderr, "objdump;%d; ", (int) i);
			std::cerr << printDecodedInstr(RI[i+start].slc<32>(0)); fprintf(stderr, " ");
			std::cerr << printDecodedInstr(RI[i+start].slc<32>(32)); fprintf(stderr, " ");
			std::cerr << printDecodedInstr(RI[i+start].slc<32>(64)); fprintf(stderr, " ");
			std::cerr << printDecodedInstr(RI[i+start].slc<32>(96)); fprintf(stderr, "\n");
		}

	}
}

void VexSimulator::doStep(){

}
#endif
#endif
