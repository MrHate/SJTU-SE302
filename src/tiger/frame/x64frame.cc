#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"

#include <string>

namespace F {

class InFrameAccess : public Access {
 public:
  int offset;

  InFrameAccess(int offset) : Access(INFRAME), offset(offset) {}

	T::Exp* ToExp(T::Exp* framePtr) const {
		return new T::MemExp(
				new T::BinopExp(
					T::PLUS_OP,
					framePtr,
					new T::ConstExp(offset)));
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

T::Stm* X64Frame::ProcEntryExit1(T::Exp* exp){
	 // (4) 将逃逸参数包括静态链保存至栈帧的指令，以及将非逃逸参数传送到新的临时寄存器的指令。
	 // The stage above is moved to codegen when generating instructions for T::CallExp.

	 // (5) 保存在此函数内用到的被调用者保护的寄存器，包括返回地址寄存器的存储指令。
	 // This stage may be delayed to lab6.

	 // (7) 将返回值传送至专用于返回结果的寄存器的指令。
	T::Stm *with_rv = new T::MoveStm(new T::TempExp(F::RV()), exp);

	return with_rv;
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

TEMP::Map* X64Frame::RegAlloc(AS::InstrList* il){
	TEMP::Map *regMap = TEMP::Map::Empty();
	regMap->Enter(FP(), new std::string("%rsp"));
	regMap->Enter(RAX(), new std::string("%rax"));
	regMap->Enter(RV(), new std::string("%rdi"));
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
