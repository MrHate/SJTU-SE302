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
					new T::ConstExp(offset)
					)
				);
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

T::Stm* X64Frame::ProcEntryExit1(T::Exp* exp){
	 // (4) 将逃逸参数包括静态链保存至栈帧的指令，以及将非逃逸参数传送到新的临时寄存器的指令。
	 // The stage above is moved to codegen when generating instructions for T::CallExp.

	 // (5) 保存在此函数内用到的被调用者保护的寄存器，包括返回地址寄存器的存储指令。
	 // This stage may be delayed to lab6.

	 // (7) 将返回值传送至专用于返回结果的寄存器的指令。
	T::Stm *with_rv = new T::MoveStm(new T::TempExp(F::RV()), exp);

	return new T::SeqStm(new T::LabelStm(name), with_rv);
}

AS::Proc* X64Frame::ProcEntryExit3(AS::InstrList* il){
	return new AS::Proc("", il, "");
}

TEMP::Map* X64Frame::RegAlloc(AS::InstrList* il){
	TEMP::Map *regMap = TEMP::Map::Empty();
	regMap->Enter(FP(), new std::string("%rsp"));
	regMap->Enter(RAX(), new std::string("%rax"));
	return regMap;
}

// Global Registers

TEMP::Temp* FP() { static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
TEMP::Temp* RV() { static TEMP::Temp *_t = nullptr; if(!_t) _t = TEMP::Temp::NewTemp(); return _t; }
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

}  // namespace F
