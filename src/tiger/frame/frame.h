#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <string>

#include "tiger/codegen/assem.h"
#include "tiger/translate/tree.h"
#include "tiger/util/util.h"


namespace F {

class Access {
 public:
  enum Kind { INFRAME, INREG };

  Kind kind;

  Access(Kind kind) : kind(kind) {}

  // Hints: You may add interface like
  //        `virtual T::Exp* ToExp(T::Exp* framePtr) const = 0`

	virtual T::Exp* ToExp(T::Exp* framePtr) const = 0;
};

class AccessList {
 public:
  Access *head;
  AccessList *tail;

  AccessList(Access *head, AccessList *tail) : head(head), tail(tail) {}
};

class Frame {
  // Base class
 public:
	virtual Access* AllocLocal(bool escape) = 0;
	virtual AccessList* Formals() const = 0;

	virtual TEMP::Label* Name() const = 0;

	virtual T::Stm* ProcEntryExit1(T::Stm* stm) = 0;
	virtual AS::Proc* ProcEntryExit3(AS::InstrList* il) = 0;
};

class X64Frame : public Frame {
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
	
	X64Frame(TEMP::Label *name, U::BoolList *formals): name(name), size(0){
		// newFrame函数必须做两件事
		// 1. 在函数内如何看待参数(寄存器还是栈帧存储单元中)
		// 2. 实现"视角位移"的指令

		while(formals){
			Access* access = AllocLocal(formals->head);
			this->formals = new AccessList(access, this->formals);
			formals = formals->tail;
		}
	}

	Access* AllocLocal(bool escape);

	AccessList* Formals() const {
		return formals;
	}

	TEMP::Label* Name() const {
		return name;
	}

	T::Stm* ProcEntryExit1(T::Stm* stm);
	AS::Proc* ProcEntryExit3(AS::InstrList* il);
};

/*
 * Fragments
 */

class Frag {
 public:
  enum Kind { STRING, PROC };

  Kind kind;

  Frag(Kind kind) : kind(kind) {}
};

class StringFrag : public Frag {
 public:
  TEMP::Label *label;
  std::string str;

  StringFrag(TEMP::Label *label, std::string str)
      : Frag(STRING), label(label), str(str) {}
};

class ProcFrag : public Frag {
 public:
  T::Stm *body;
  Frame *frame;

  ProcFrag(T::Stm *body, Frame *frame) : Frag(PROC), body(body), frame(frame) {}
};

class FragList {
 public:
  Frag *head;
  FragList *tail;

  FragList(Frag *head, FragList *tail) : head(head), tail(tail) {}
};


// Global regs

TEMP::Temp* FP() ;
TEMP::Temp* RV() ;
TEMP::Temp* RSP() ;
TEMP::Temp* RAX() ;
TEMP::Temp* RDI() ;
TEMP::Temp* RSI() ;
TEMP::Temp* RDX() ;
TEMP::Temp* RCX() ;
TEMP::Temp* R8() ;
TEMP::Temp* R9() ;
TEMP::Temp* R10() ;
TEMP::Temp* R11() ;

//AS::Proc *F_procEntryExit3(Frame *frame, AS::InstrList *il){return nullptr;}

}  // namespace F

#endif
