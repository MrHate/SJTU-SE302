#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"

#include <string>

namespace F {

class InFrameAccess : public Access {
 public:
  int offset;

  InFrameAccess(int offset) : Access(INFRAME), offset(offset) {}

	T::Exp* ToExp(T::Exp* framePtr) const {
		int off = offset;
		if(off < -8) off += 8;
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
								new T::MemExp(
									new T::BinopExp(
										T::PLUS_OP,
										new T::TempExp(FP()),
										new T::ConstExp(-size))),
								new T::TempExp(RDI()));
						AppendViewShift(stm);
					}
					break;

				case 1:
					{
						T::Stm *stm = new T::MoveStm(
								new T::MemExp(
									new T::BinopExp(
										T::PLUS_OP,
										new T::TempExp(FP()),
										new T::ConstExp(-size))),
								new T::TempExp(RSI()));
						AppendViewShift(stm);
					}
					break;

				case 2:
					{
						T::Stm *stm = new T::MoveStm(
								new T::MemExp(
									new T::BinopExp(
										T::PLUS_OP,
										new T::TempExp(FP()),
										new T::ConstExp(-size))),
								new T::TempExp(RCX()));
						AppendViewShift(stm);
					}
					break;

				case 3:
					{
						T::Stm *stm = new T::MoveStm(
								new T::MemExp(
									new T::BinopExp(
										T::PLUS_OP,
										new T::TempExp(FP()),
										new T::ConstExp(-size))),
								new T::TempExp(RDX()));
						AppendViewShift(stm);
					}
					break;

				case 4:
					{
						T::Stm *stm = new T::MoveStm(
								new T::MemExp(
									new T::BinopExp(
										T::PLUS_OP,
										new T::TempExp(FP()),
										new T::ConstExp(-size))),
								new T::TempExp(R8()));
						AppendViewShift(stm);
					}
					break;

				case 5:
					{
						T::Stm *stm = new T::MoveStm(
								new T::MemExp(
									new T::BinopExp(
										T::PLUS_OP,
										new T::TempExp(FP()),
										new T::ConstExp(-size))),
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
	if(true){
		size += wordSize;
		return new InFrameAccess(-size);
	}
	else{
		return new InRegAccess(TEMP::Temp::NewTemp());
	}
}

TEMP::TempList *X64Frame::returnSink = nullptr;

T::Stm* X64Frame::ProcEntryExit1(T::Stm* stm){
	 // (4) 将逃逸参数包括静态链保存至栈帧的指令，以及将非逃逸参数传送到新的临时寄存器的指令。
	 // The stage above is moved to codegen when generating instructions for T::CallExp.

	 // (5) 保存在此函数内用到的被调用者保护的寄存器，包括返回地址寄存器的存储指令。
	 // This stage may be delayed to lab6.

	 // (7) 将返回值传送至专用于返回结果的寄存器的指令。

	return new T::SeqStm(viewShift, stm);
}

AS::InstrList* X64Frame::ProcEntryExit2(AS::InstrList* body){
	if (returnSink == nullptr){
		returnSink = new TEMP::TempList(RSP(), new TEMP::TempList(RAX(), nullptr));
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
	regMap->Enter(FP(), new std::string("%rsp"));
	regMap->Enter(RAX(), new std::string("%rax"));
	regMap->Enter(R12(), new std::string("%r10"));
	regMap->Enter(R13(), new std::string("%r11"));

	regMap->Enter(RDI(), new std::string("%rdi"));
	regMap->Enter(RSI(), new std::string("%rsi"));
	regMap->Enter(RCX(), new std::string("%rcx"));
	regMap->Enter(RDX(), new std::string("%rdx"));
	regMap->Enter(R8(),  new std::string("%r8"));
	regMap->Enter(R9(),  new std::string("%r9"));
	return regMap;
}

// Global Registers

TEMP::Temp* FP() { static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* RV() { return RAX();}

TEMP::Temp* RSP(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* RAX(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* RDI(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* RSI(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* RDX(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* RCX(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* R8() { static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* R9() { static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* R10(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* R11(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* R12(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* R13(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* R14(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* R15(){ static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }

}  // namespace F
