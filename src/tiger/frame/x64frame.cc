#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"

#include <string>

#define REGDEC { static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }

namespace F {

class InFrameAccess : public Access {
 public:
  int offset;

  InFrameAccess(int offset) : Access(INFRAME), offset(offset) {}

	T::Exp* ToExp(T::Exp* framePtr) const {
		int off = offset;
		return new T::MemExp(
				new T::BinopExp(
					T::PLUS_OP,
					framePtr,
					new T::ConstExp(off)));
	}
};

class InRegAccess : public Access {
 public:
  TEMP::Temp* reg;

  InRegAccess(TEMP::Temp* reg) : Access(INREG), reg(reg) {}

	T::Exp* ToExp(T::Exp* framePtr) const {
		return new T::TempExp(reg);
	}
};

// X64Frame Implementation
X64Frame::X64Frame(TEMP::Label *name, U::BoolList *formals): 
	name(name), 
	formals(nullptr), 
	viewShift(nullptr),
	size(0){
		// newFrame函数必须做两件事
		// 1. 在函数内如何看待参数(寄存器还是栈帧存储单元中)
		// 2. 实现"视角位移"的指令

		remainCalleeSaves = CalleeSaves();

		int parameterPos = 0;
		AccessList *last = nullptr;
		while(formals){
			Access* access = AllocLocal(formals->head);
			if(this->formals){
				last->tail = new AccessList(access, nullptr);
				last = last->tail;
			}
			else{
				last = this->formals = new AccessList(access, nullptr);
			}
			//this->formals = new AccessList(access, this->formals);

			switch(parameterPos){
				case 0:
					{
						T::Stm *stm = new T::MoveStm(
								access->ToExp(new T::TempExp(FP())),
								new T::TempExp(RDI()));
						AppendViewShift(stm);
					}
					break;

				case 1:
					{
						T::Stm *stm = new T::MoveStm(
								access->ToExp(new T::TempExp(FP())),
								new T::TempExp(RSI()));
						AppendViewShift(stm);
					}
					break;

				case 2:
					{
						T::Stm *stm = new T::MoveStm(
								access->ToExp(new T::TempExp(FP())),
								new T::TempExp(RCX()));
						AppendViewShift(stm);
					}
					break;

				case 3:
					{
						T::Stm *stm = new T::MoveStm(
								access->ToExp(new T::TempExp(FP())),
								new T::TempExp(RDX()));
						AppendViewShift(stm);
					}
					break;

				case 4:
					{
						T::Stm *stm = new T::MoveStm(
								access->ToExp(new T::TempExp(FP())),
								new T::TempExp(R8()));
						AppendViewShift(stm);
					}
					break;

				case 5:
					{
						T::Stm *stm = new T::MoveStm(
								access->ToExp(new T::TempExp(FP())),
								new T::TempExp(R9()));
						AppendViewShift(stm);
					}
					break;

				default:
					assert(0);
			}
			formals = formals->tail;
			++ parameterPos;
		}
}


Access* X64Frame::AllocLocal(bool escape){
	if(escape){
		size += wordSize;
		return new InFrameAccess(-size);
	}
	else{
		if(remainCalleeSaves != nullptr){
			Access *acc = new InRegAccess(remainCalleeSaves->head);
			remainCalleeSaves = remainCalleeSaves->tail;
			return acc;
		}
		return new InRegAccess(TEMP::Temp::NewTemp());
	}
}

TEMP::TempList *X64Frame::returnSink = nullptr;

T::Stm* X64Frame::ProcEntryExit1(T::Stm* stm){
	 // (4) 将逃逸参数包括静态链保存至栈帧的指令，以及将非逃逸参数传送到新的临时寄存器的指令。
	 // (5) 保存在此函数内用到的被调用者保护的寄存器，包括返回地址寄存器的存储指令。
	 // (7) 将返回值传送至专用于返回结果的寄存器的指令。

	return new T::SeqStm(viewShift, stm);
}

AS::InstrList* X64Frame::ProcEntryExit2(AS::InstrList* body){
	if (returnSink == nullptr){
		returnSink = new TEMP::TempList(RV(), nullptr);
	}
	return AS::InstrList::Splice(body, new AS::InstrList(new AS::OperInstr("",nullptr,returnSink,nullptr), nullptr));
}

AS::Proc* X64Frame::ProcEntryExit3(AS::InstrList* il){
	// (1) 特定汇编语言需要的一个声明函数开始的伪指令
	// not needed in x86 here
	
	// (3) 调整栈指针的一条命令
	std::string frame_size = ".set " + TEMP::LabelString(name) + "_framesize,",
		prolog = frame_size + std::to_string(size) + "\n";
	prolog += TEMP::LabelString(name) + ":\n";
	prolog += "subq $" + std::to_string(size) + ",%rsp\n";

	// (9) 恢复栈指针的指令
	std::string epilog = "addq $" + std::to_string(size) + ",%rsp\n";
	epilog += "ret\n\n";

	// (11) 汇编语言需要的声明一个函数结束的伪指令
	// not needed in x86 here

	return new AS::Proc(prolog, il, epilog);
}

void X64Frame::AppendViewShift(T::Stm *stm){
	if(viewShift == nullptr){
		viewShift = new T::SeqStm(stm, nullptr);
		return;
	}
	T::SeqStm *last = dynamic_cast<T::SeqStm*>(viewShift);
	while(last->right)last = dynamic_cast<T::SeqStm*>(last->right);
	last->right = new T::SeqStm(stm, nullptr);
}

TEMP::Map* X64Frame::RegAlloc(AS::InstrList* il){
	TEMP::Map *regMap = TEMP::Map::Empty();
	regMap->Enter(RSP(), new std::string("%rsp"));
	regMap->Enter(RAX(), new std::string("%rax"));
	regMap->Enter(RBX(), new std::string("%rbx"));
	regMap->Enter(RCX(), new std::string("%rcx"));
	regMap->Enter(RDX(), new std::string("%rdx"));
	regMap->Enter(RDI(), new std::string("%rdi"));
	regMap->Enter(RSI(), new std::string("%rsi"));
	regMap->Enter(RBP(), new std::string("%rbp"));
	regMap->Enter(R8(),  new std::string("%r8"));
	regMap->Enter(R9(),  new std::string("%r9"));
	regMap->Enter(R10(), new std::string("%r10"));
	regMap->Enter(R11(), new std::string("%r11"));
	regMap->Enter(R12(), new std::string("%r12"));
	regMap->Enter(R13(), new std::string("%r13"));
	regMap->Enter(R14(), new std::string("%r14"));
	regMap->Enter(R15(), new std::string("%r15"));
	return regMap;
}

// Global Registers

TEMP::Temp* FP() REGDEC
TEMP::Temp* RV() { return RAX();}

TEMP::Temp* RSP() REGDEC
TEMP::Temp* RAX() REGDEC
TEMP::Temp* RBX() REGDEC
TEMP::Temp* RCX() REGDEC
TEMP::Temp* RDX() REGDEC
TEMP::Temp* RDI() REGDEC
TEMP::Temp* RSI() REGDEC
TEMP::Temp* RBP() REGDEC
TEMP::Temp* R8()  REGDEC
TEMP::Temp* R9()  REGDEC
TEMP::Temp* R10() REGDEC
TEMP::Temp* R11() REGDEC
TEMP::Temp* R12() REGDEC
TEMP::Temp* R13() REGDEC
TEMP::Temp* R14() REGDEC
TEMP::Temp* R15() REGDEC

TEMP::TempList* ArgRegs(){
	static TEMP::TempList *_regs = nullptr;
	if(_regs == nullptr){
		_regs = new TEMP::TempList(
				RDI(), new TEMP::TempList(
					RSI(), new TEMP::TempList(
						RCX(), new TEMP::TempList(
							RDX(), new TEMP::TempList(
								R8(), new TEMP::TempList(
									R9(), nullptr))))));
	}
	return _regs;
}

TEMP::TempList* CallerSaves(){
	static TEMP::TempList *_regs = nullptr;
	if(_regs == nullptr){
		_regs = new TEMP::TempList(
				F::RBX(), new TEMP::TempList(
					F::R10(), new TEMP::TempList(
						F::R11(), ArgRegs())));
	}
	return _regs;
}

TEMP::TempList* CalleeSaves(){
	static TEMP::TempList *_regs = nullptr;
	if(_regs == nullptr){
		_regs = new TEMP::TempList(
					F::R12(), new TEMP::TempList(
						F::R13(), new TEMP::TempList(
							F::R14(), new TEMP::TempList(
								F::R15(), nullptr))));
	}
	return _regs;
}

TEMP::TempList* HardRegs(){
	static TEMP::TempList *_regs = nullptr;
	if(_regs == nullptr){
		_regs = new TEMP::TempList(
				F::RAX(), new TEMP::TempList(
					F::RBX(), new TEMP::TempList(
						F::RCX(), new TEMP::TempList(
							F::RDX(), new TEMP::TempList(
								F::RDI(), new TEMP::TempList(
									F::RSI(), new TEMP::TempList(
										F::R8(),  new TEMP::TempList(
											F::R9(),  new TEMP::TempList(
												F::R10(), new TEMP::TempList(
													F::R11(), new TEMP::TempList(
														F::R12(), new TEMP::TempList(
															F::R13(), new TEMP::TempList(
																F::R14(), new TEMP::TempList(
																	F::R15(), nullptr))))))))))))));
	}
	return _regs;
}

}  // namespace F
