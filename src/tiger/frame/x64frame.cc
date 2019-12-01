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

Access* X64Frame::AllocLocal(bool escape){
	if(escape){
		size += wordSize;
		return new InFrameAccess(-size);
	}
	else{
		return new InRegAccess(TEMP::Temp::NewTemp());
	}
}

T::Stm* X64Frame::ProcEntryExit1(T::Stm* stm){
	return stm;
}

AS::Proc* X64Frame::ProcEntryExit3(AS::InstrList* il){
	return nullptr;
}

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
