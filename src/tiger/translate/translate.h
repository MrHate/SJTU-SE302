#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

#include "tiger/absyn/absyn.h"
#include "tiger/frame/frame.h"

/* Forward Declarations */
namespace A {
class Exp;
}  // namespace A

namespace TR {

class Exp;
class ExpAndTy;

class Access;
class AccessList;

class Level {
 public:
  F::Frame *frame;
  Level *parent;

  AccessList *Formals(Level *level);

	Level(F::Frame *frame, Level *parent) : frame(frame), parent(parent) {}
  static Level *NewLevel(Level *parent, TEMP::Label *name, U::BoolList *formals){
		// Frame模块不应当知道静态链的信息, 静态链由Translate负责, Translate知道每个栈
		// 帧都含有一个静态链, 静态链由寄存器传递给函数并保存在栈帧中, 尽可能将静态链
		// 当作形参对待。
		return new Level(new F::X64Frame(name,new U::BoolList(true,formals)),parent);
	}


};

Level* Outermost();

class PatchList;
void do_patch(PatchList *tList, TEMP::Label *label);
PatchList *join_patch(PatchList *first, PatchList *second);

F::FragList* TranslateProgram(A::Exp*);

}  // namespace TR

#endif
