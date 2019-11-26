#include "tiger/translate/translate.h"

#include <cstdio>
#include <set>
#include <string>

#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/temp.h"
#include "tiger/semant/semant.h"
#include "tiger/semant/types.h"
#include "tiger/util/util.h"

namespace TR {

class Access {
 public:
  Level *level;
  F::Access *access;

  Access(Level *level, F::Access *access) : level(level), access(access) {}

  static Access *AllocLocal(Level *level, bool escape) {
		F::Access *acc = level->frame->AllocLocal(escape);
		return new Access(level,acc);
	}
};

class AccessList {
 public:
  Access *head;
  AccessList *tail;

  AccessList(Access *head, AccessList *tail) : head(head), tail(tail) {}
};

class Level {
 public:
  F::Frame *frame;
  Level *parent;

  Level(F::Frame *frame, Level *parent) : frame(frame), parent(parent) {}
  AccessList *Formals(Level *level) {
		F::AccessList *p = frame->Formals()->tail;
		AccessList *ret = nullptr;
		
		while(p){
			Access *acc = new Access(level,p->head);
			ret = new AccessList(acc,ret);
			p = p->tail;
		}

		return ret;
	}

  static Level *NewLevel(Level *parent, TEMP::Label *name, U::BoolList *formals){
		// Frame模块不应当知道静态链的信息, 静态链由Translate负责, Translate知道每个栈
		// 帧都含有一个静态链, 静态链由寄存器传递给函数并保存在栈帧中, 尽可能将静态链
		// 当作形参对待。
		return new Level(new F::x64Frame(name,new U::BoolList(true,formals)),parent);
	}
};

class PatchList {
 public:
  TEMP::Label **head;
  PatchList *tail;

  PatchList(TEMP::Label **head, PatchList *tail) : head(head), tail(tail) {}
};

class Cx {
 public:
  PatchList *trues;
  PatchList *falses;
  T::Stm *stm;

  Cx(PatchList *trues, PatchList *falses, T::Stm *stm)
      : trues(trues), falses(falses), stm(stm) {}
};

class Exp {
 public:
  enum Kind { EX, NX, CX };

  Kind kind;

  Exp(Kind kind) : kind(kind) {}

  virtual T::Exp *UnEx() const = 0;
  virtual T::Stm *UnNx() const = 0;
  virtual Cx UnCx() const = 0;
};

class ExpAndTy {
 public:
  TR::Exp *exp;
  TY::Ty *ty;

  ExpAndTy(TR::Exp *exp, TY::Ty *ty) : exp(exp), ty(ty) {}
};

class ExExp : public Exp {
 public:
  T::Exp *exp;

  ExExp(T::Exp *exp) : Exp(EX), exp(exp) {}

  T::Exp *UnEx() const override {}
  T::Stm *UnNx() const override {}
  Cx UnCx() const override {}
};

class NxExp : public Exp {
 public:
  T::Stm *stm;

  NxExp(T::Stm *stm) : Exp(NX), stm(stm) {}

  T::Exp *UnEx() const override {}
  T::Stm *UnNx() const override {}
  Cx UnCx() const override {}
};

class CxExp : public Exp {
 public:
  Cx cx;

  CxExp(struct Cx cx) : Exp(CX), cx(cx) {}
  CxExp(PatchList *trues, PatchList *falses, T::Stm *stm)
      : Exp(CX), cx(trues, falses, stm) {}

  T::Exp *UnEx() const override {}
  T::Stm *UnNx() const override {}
  Cx UnCx() const override {}
};

void do_patch(PatchList *tList, TEMP::Label *label) {
  for (; tList; tList = tList->tail) *(tList->head) = label;
}

PatchList *join_patch(PatchList *first, PatchList *second) {
  if (!first) return second;
  for (; first->tail; first = first->tail)
    ;
  first->tail = second;
  return first;
}

Level *Outermost() {
  static Level *lv = nullptr;
  if (lv != nullptr) return lv;

  lv = new Level(nullptr, nullptr);
  return lv;
}

F::FragList *TranslateProgram(A::Exp *root) {
  // TODO: Put your codes here (lab5).
  return nullptr;
}

}  // namespace TR

namespace A {

TR::ExpAndTy SimpleVar::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
	E::VarEntry *ent = static_cast<E::VarEntry*>(venv->Look(sym->Name()));
	if(ent == nullptr){
		errormsg.Error(pos,"undefined variable %s",sym->Name().c_str());
		return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
	}

	// trace static link
	TR::Level dst_lv = ent->access->level,
						p_lv = level;
	T::Exp *fp = F::FP();
	while(p_lv != dst_lv){
		fp = new T::MemExp(
				new T::BinopExp(
					T::PLUS_OP,
					fp,
					new T::ConstExp(0)
					)
				);
		p_lv = p_lv->parent;
	}

	// calculate data location based on its frame pointer
	TR::Exp exp = new TR::ExExp(ent->access->ToExp(fp));
  return TR::ExpAndTy(exp, ent->ty);
}

TR::ExpAndTy FieldVar::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
	TR::ExpAndTy *var_expty = var->Translate(venv,tenv,level,label);
	if(var_expty->ty->ActualTy()->kind == TY::Ty::RECORD){
		TY::RecordTy *recty = static_cast<TY::RecordTy*>(var_expty->ty->ActualTy());
		TY::FieldList *p = recty->fields;
		int offset = 0;
		while(p){
			if(p->head->name == sym){
				T::Exp *e = new T::MemExp(
						new T::BinopExp(
							T::PLUS_OP,
							var_expty->exp,
							new T::ConstExp(offset)
							)
						);
				return new TR::ExpAndTy(e,p->head->ty);
			}
			p = p->tail;
			offset += F::x64Frame::wordSize;
		}
		errormsg.Error(pos,"field %s doesn't exist",sym->Name().c_str());
	}
	else {
		errormsg.Error(pos,"not a record type");
	}

  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy SubscriptVar::Translate(S::Table<E::EnvEntry> *venv,
                                     S::Table<TY::Ty> *tenv, TR::Level *level,
                                     TEMP::Label *label) const {
	TR::ExpAndTy *var_expty = var->Translate(venv,tenv,level,label);
	if(var_expty->ty->kind == TY::Ty::ARRAY){
		TY::ArrayTy *arrty = static_cast<TY::ArrayTy*>(var_expty->ty);
		//calculate exp to get offset
		TR::ExpAndTy *sub_expty = subscript->Translate(venv,tenv,level,label);
		T::Exp *e = new T::MemExp(
				new T::BinopExp(
					T::PLUS_OP,
					var_expty->exp,
					sub_expty->exp
					)
				);
		return TR::ExpAndTy(e,arrty->ty);
	}
	errormsg.Error(pos,"array type required");
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy VarExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  //return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
	return var->Translate(venv,tenv,level,label);
}

TR::ExpAndTy NilExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  return TR::ExpAndTy(nullptr, TY::NilTy::Instance());
}

TR::ExpAndTy IntExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  return TR::ExpAndTy(new T::ConstExp(i), TY::IntTy::Instance());
}

TR::ExpAndTy StringExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
	// (p117)
	// Tiger字符串应当能够表示任意8位码
	// 实现: 使用一个字符串指针指向一个字, 此字中的整数给出字符串长度(字符个数)
	// 紧跟其后的是组成字符串的字符, 这样与机器相关的模块便能用一个标号的定义,
	// 一条创建一个字且字中包含一个表示长度的整数的汇编语言伪指令, 以及一条生成
	// 字符数据的伪指令来生成一个字符串。
	// 所有字符串操作都由系统提供的函数来完成，这些函数为字符串操作分配堆空间并返回指针，
	// 编译器只知道每个字符串指针正好是一个字长。

	struct string str = {
		s.size(),
		s.c_str()
	};

  return TR::ExpAndTy(nullptr, TY::StringTy::Instance());
}

TR::ExpAndTy CallExp::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  E::EnvEntry *ent = venv->Look(func);
	if(ent == nullptr){
		errormsg.Error(pos,"undefined function %s",func->Name().c_str());
		return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
	}

	T::ExpList *ret_args = nullptr;
	if(ent->kind == E::EnvEntry::FUN){
		// check actuals
		TY::TyList *formals = static_cast<E::FunEntry*>(ent)->formals;
		A::ExpList *p = args;
		while(p && formals){
			A::Exp *a_arg = formals->head;

			// check args' type during semant processing
			if(!a_arg->IsSameType(p->head->SemAnalyze(venv,tenv,labelcount)))
				errormsg.Error(pos,"para type mismatch");

			// translate args into tree explist
			T::Exp *t_arg = a_arg->Translate(venv,tenv,level,label);
			ret_args = new T::ExpList(ret_args,t_arg);

			formals = formals->tail;
			p = p->tail;
		}
		if(p){
			errormsg.Error(pos,"too many params in function %s",func->Name().c_str());
		}

		// add static link as the first arg
		TR::level *plv = level,
			*lv = ent->level->parent;
		T::Exp *sl = new T::NameExp(lv->frame->Name());
		while(lv != plv){
			sl = new T::NameExp(new T::MemExp(sl));
			lv = lv->parent;
		}
		ret_args = new T::ExpList(sl,ret_args);

		T::Exp *ret_exp = new T::CallExp(
				new T::NameExp(ent->label),
				ret_args);
		TY::Ty *ret_ty = static_cast<E::FunEntry*>(ent)->result;
		return TR::ExpAndTy(ret_exp, ret_ty);
	}
	else{
		errormsg.Error(pos,"not a name of func");
		return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
	}
}

TR::ExpAndTy OpExp::Translate(S::Table<E::EnvEntry> *venv,
                              S::Table<TY::Ty> *tenv, TR::Level *level,
                              TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy RecordExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy SeqExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy AssignExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy IfExp::Translate(S::Table<E::EnvEntry> *venv,
                              S::Table<TY::Ty> *tenv, TR::Level *level,
                              TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy WhileExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy ForExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy BreakExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy LetExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy ArrayExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy VoidExp::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::Exp *FunctionDec::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return nullptr;
}

TR::Exp *VarDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                           TR::Level *level, TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return nullptr;
}

TR::Exp *TypeDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                            TR::Level *level, TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  return nullptr;
}

TY::Ty *NameTy::Translate(S::Table<TY::Ty> *tenv) const {
  // TODO: Put your codes here (lab5).
  return TY::VoidTy::Instance();
}

TY::Ty *RecordTy::Translate(S::Table<TY::Ty> *tenv) const {
  // TODO: Put your codes here (lab5).
  return TY::VoidTy::Instance();
}

TY::Ty *ArrayTy::Translate(S::Table<TY::Ty> *tenv) const {
  // TODO: Put your codes here (lab5).
  return TY::VoidTy::Instance();
}

}  // namespace A
