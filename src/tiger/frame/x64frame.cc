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

	TEMP::Label *name;
	AccessList *formals;
	int size;

	static const int wordSize = 4;
	
	X64Frame(TEMP::Label name, U::BoolList formals): name(name), size(0){
		// newFrame函数必须做两件事
		// 1. 在函数内如何看待参数(寄存器还是栈帧存储单元中)
		// 2. 实现"视角位移"的指令

		while(formals){
			Access* access = AllocLocal(formals->head);
			this.formals = new AccessList(access,this.formals);
			formals = formals->tail;
		}
	}

	Access* AllocLocal(bool escape){
		if(escape){
			size += wordSize;
			return new InFrameAccess(-size);
		}
		else{
			return new InRegAccess(new TEMP::Temp::NewTemp());
		}
	}

	AccessList* Formals() const {
		return formals;
	}

	TEMP::Label* Name() const {
		return name;
	}

};

class InFrameAccess : public Access {
 public:
  int offset;

  InFrameAccess(int offset) : Access(INFRAME), offset(offset) {}

	T::Exp* ToExp(T::Exp* framePtr) const {
		return new T::MemExp(
				new T::BinopExp(
					T::BINOP,
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
		return T::TempExp(reg);
	}
};

TEMP::Temp* FP(){
	return nullptr;
}

}  // namespace F
