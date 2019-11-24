#include "tiger/frame/frame.h"

#include <string>

namespace F {

class X64Frame : public Frame {
  // TODO: Put your codes here (lab6).
	
	// (p98) Frame contains the members below:
	// 1. the locations of all the formals,
	// 2. instructions required to implement the "view shift",
	// 3. the number of locals allocated so far,
	// 4. the label at which the function's machine code is begin.

	// It is ok to alloc infinite regs in lab5

 public:

	TEMP::Label name;
	F::AccessList formals;
	int localnum;
	
	X64Frame(TEMP::Label name, U::BoolList formals): name(name), localnum(0){
		while(formals){
			F::Access* access = allocLocal(formals->head);
			this.formals = new F::AccessList(access,this.formals);
			formals = formals->tail;
		}
	}

	F::Access* allocLocal(bool escape){
		if(escape){
			return new F::InFrameAccess(--localnum * 4);
		}
		else{
			return new F::InRegAccess(new TEMP::Temp::NewTemp());
		}
	}
};

class InFrameAccess : public Access {
 public:
  int offset;

  InFrameAccess(int offset) : Access(INFRAME), offset(offset) {}
};

class InRegAccess : public Access {
 public:
  TEMP::Temp* reg;

  InRegAccess(TEMP::Temp* reg) : Access(INREG), reg(reg) {}
};

}  // namespace F
