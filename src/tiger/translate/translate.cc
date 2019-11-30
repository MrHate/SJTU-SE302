#include "tiger/translate/translate.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/temp.h"
#include "tiger/semant/semant.h"
#include "tiger/semant/types.h"
#include "tiger/util/util.h"

extern EM::ErrorMsg errormsg;

namespace {
static TY::TyList *make_formal_tylist(S::Table<TY::Ty> *tenv, A::FieldList *params) {
  if (params == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(params->head->typ);
  if (ty == nullptr) {
    errormsg.Error(params->head->pos, "undefined type %s",
                   params->head->typ->Name().c_str());
  }

  return new TY::TyList(ty->ActualTy(), make_formal_tylist(tenv, params->tail));
}

static TY::FieldList *make_fieldlist(S::Table<TY::Ty> *tenv, A::FieldList *fields) {
  if (fields == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(fields->head->typ);
  return new TY::FieldList(new TY::Field(fields->head->name, ty),
                           make_fieldlist(tenv, fields->tail));
}

}  // namespace

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

AccessList* Level::Formals(Level *level) {
	F::AccessList *p = frame->Formals()->tail;
	AccessList *ret = nullptr;
	
	while(p){
		Access *acc = new Access(level,p->head);
		ret = new AccessList(acc,ret);
		p = p->tail;
	}

	return ret;
}

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

  T::Exp *UnEx() const override { return exp; }

  T::Stm *UnNx() const override { return new T::ExpStm(exp); }

  Cx UnCx() const override {
		// (p110) 特殊对待CONST 0和CONST 1
		T::CjumpStm *cj_stm = new T::CjumpStm(
				T::NE_OP,
				exp,
				new T::ConstExp(0),
				nullptr,
				nullptr);
		TR::PatchList *trues = new TR::PatchList(&cj_stm->true_label, nullptr),
			*falses = new TR::PatchList(&cj_stm->false_label, nullptr);
		return TR::Cx(trues, falses, cj_stm);
	}
};

class NxExp : public Exp {
 public:
  T::Stm *stm;

  NxExp(T::Stm *stm) : Exp(NX), stm(stm) {}

  T::Exp *UnEx() const override { return new T::EseqExp(stm, new T::ConstExp(0)); }
	
  T::Stm *UnNx() const override { return stm; }

	// (p111) unCx应拒绝Tr_nx的Tr_exp
  Cx UnCx() const override { return Cx(nullptr,nullptr,nullptr); }
};

class CxExp : public Exp {
 public:
  Cx cx;

  CxExp(struct Cx cx) : Exp(CX), cx(cx) {}
  CxExp(PatchList *trues, PatchList *falses, T::Stm *stm)
      : Exp(CX), cx(trues, falses, stm) {}

  T::Exp *UnEx() const override {
		TEMP::Temp *r = TEMP::Temp::NewTemp();
		TEMP::Label *t = TEMP::NewLabel(),
			*f = TEMP::NewLabel();
		TR::do_patch(cx.trues,t);
		TR::do_patch(cx.falses,f);
		return new T::EseqExp(new T::MoveStm(new T::TempExp(r),new T::ConstExp(1)),
				new T::EseqExp(cx.stm,
					new T::EseqExp(new T::LabelStm(f),
						new T::EseqExp(new T::MoveStm(new T::TempExp(r), new T::ConstExp(0)),
							new T::EseqExp(new T::LabelStm(t),
								new T::TempExp(r))))));
	}

  T::Stm *UnNx() const override {
		TEMP::Label *done_label = TEMP::NewLabel();
		do_patch(cx.trues,done_label);
		do_patch(cx.falses,done_label);
		return new T::SeqStm(cx.stm, new T::LabelStm(done_label));
	}

  Cx UnCx() const override {
		return cx;
	}
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

F::FragList *AllocFrag(F::Frag *frag){
	static F::FragList *frags = nullptr;
	F::FragList **p = &frags;
	while(*p) p = &((*p)->tail);
	*p = new F::FragList(frag, nullptr);
	return frags;
}

F::FragList *TranslateProgram(A::Exp *root) {
	T::Stm *stm = root->Translate(E::BaseVEnv(), E::BaseTEnv(), Outermost(), nullptr).exp->UnNx();
	F::Frag *frag = new F::ProcFrag(stm, nullptr);
  return AllocFrag(frag);
}

}  // namespace TR

namespace A {

TR::ExpAndTy SimpleVar::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
	E::VarEntry *ent = static_cast<E::VarEntry*>(venv->Look(sym));
	if(ent == nullptr){
		errormsg.Error(pos,"undefined variable %s",sym->Name().c_str());
		return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
	}

	// trace static link
	TR::Level *dst_lv = ent->access->level, *p_lv = level;
	T::Exp *fp = new T::TempExp(F::FP());
	while(p_lv != dst_lv){
		//fp = new T::MemExp(
		//    new T::BinopExp(
		//      T::PLUS_OP,
		//      fp,
		//      new T::ConstExp(0)));
		fp = p_lv->frame->Formals()->head->ToExp(fp);
		p_lv = p_lv->parent;
	}

	// calculate data location based on its frame pointer
	TR::Exp *exp = new TR::ExExp(ent->access->access->ToExp(fp));
  return TR::ExpAndTy(exp, ent->ty);
}

TR::ExpAndTy FieldVar::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
	// var should be translated into an T::MemExp pointing to the record body
	// the certain field of the body can be located by counting offset.
	TR::ExpAndTy var_expty = var->Translate(venv,tenv,level,label);
	if(var_expty.ty->ActualTy()->kind == TY::Ty::RECORD){
		TY::RecordTy *recty = static_cast<TY::RecordTy*>(var_expty.ty->ActualTy());
		TY::FieldList *p = recty->fields;
		int offset = 0;
		while(p){
			if(p->head->name == sym){
				T::Exp *e = new T::MemExp(
						new T::BinopExp(
							T::PLUS_OP,
							var_expty.exp->UnEx(),
							new T::ConstExp(offset)));
				return TR::ExpAndTy(new TR::ExExp(e),p->head->ty);
			}
			p = p->tail;
			offset += F::X64Frame::wordSize;
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
	// var should be translated into an T::MemExp pointing to the record body
	// the certain field of the body can be located by calculating offset by subscript.
	TR::ExpAndTy var_expty = var->Translate(venv,tenv,level,label);
	if(var_expty.ty->kind == TY::Ty::ARRAY){
		TY::ArrayTy *arrty = static_cast<TY::ArrayTy*>(var_expty.ty);
		//calculate exp to get offset
		TR::ExpAndTy sub_expty = subscript->Translate(venv,tenv,level,label);
		T::Exp *e = new T::MemExp(
				new T::BinopExp(
					T::PLUS_OP,
					var_expty.exp->UnEx(),
					sub_expty.exp->UnEx()));
		return TR::ExpAndTy(new TR::ExExp(e),arrty->ty);
	}
	errormsg.Error(pos,"array type required");
  return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
}

TR::ExpAndTy VarExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
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
  return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(i)), TY::IntTy::Instance());
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

	TEMP::Label *str_label = TEMP::NewLabel();
	//char str_head = (char)s.size();
	//F::StringFrag *str_frag = new F::StringFrag(str_label, str_head + s);
	F::StringFrag *str_frag = new F::StringFrag(str_label, s);
	TR::AllocFrag(str_frag);
	TR::Exp *ret_exp = new TR::ExExp(new T::NameExp(str_label));

  return TR::ExpAndTy(ret_exp, TY::StringTy::Instance());
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
			TY::Ty *a_arg = formals->head;

			// check args' type during semant processing
			TR::ExpAndTy p_expty = p->head->Translate(venv,tenv,level,label);
			if(!a_arg->IsSameType(p_expty.ty))
				errormsg.Error(pos,"para type mismatch");

			// translate args into tree explist
			//T::Exp *t_arg = a_arg->Translate(venv,tenv,level,label);
			T::Exp *t_arg = p_expty.exp->UnEx();
			ret_args = new T::ExpList(t_arg,ret_args);

			formals = formals->tail;
			p = p->tail;
		}
		if(p){
			errormsg.Error(pos,"too many params in function %s",func->Name().c_str());
		}

		// add static link as the first arg
		TR::Level *plv = level,
			*lv = static_cast<E::FunEntry*>(ent)->level;
		//T::Exp *sl = new T::NameExp(lv->frame->Name());
		T::Exp *sl = new T::TempExp(F::FP());
		while(lv != plv){
			//sl = new T::NameExp(new T::MemExp(sl));
			//sl = new T::MemExp(sl);
			sl = lv->frame->Formals()->head->ToExp(sl);
			lv = lv->parent;
		}
		ret_args = new T::ExpList(sl,ret_args);

		T::Exp *ret_exp = new T::CallExp(
				new T::NameExp(static_cast<E::FunEntry*>(ent)->label),
				ret_args);
		TY::Ty *ret_ty = static_cast<E::FunEntry*>(ent)->result;
		return TR::ExpAndTy(new TR::ExExp(ret_exp), ret_ty);
	}
	else{
		errormsg.Error(pos,"not a name of func");
		return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
	}
}

TR::ExpAndTy OpExp::Translate(S::Table<E::EnvEntry> *venv,
                              S::Table<TY::Ty> *tenv, TR::Level *level,
                              TEMP::Label *label) const {
	TR::ExpAndTy left_expty = left->Translate(venv,tenv,level,label),
		right_expty = right->Translate(venv,tenv,level,label);

	if(!left_expty.ty->IsSameType(right_expty.ty)){
			errormsg.Error(left->pos,"same type required");
	}

	if(oper == A::PLUS_OP ||
			oper == A::MINUS_OP ||
			oper == A::TIMES_OP ||
			oper == A::DIVIDE_OP ||
			oper == A::LT_OP ||
			oper == A::GE_OP ||
			oper == A::GT_OP ||
			oper == A::GE_OP){
		if(!left_expty.ty->IsSameType(TY::IntTy::Instance())){
			errormsg.Error(left->pos,"integer required");
			return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
		}
		if(!right_expty.ty->IsSameType(TY::IntTy::Instance())){
			errormsg.Error(right->pos,"integer required");
			return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
		}
	}


	T::CjumpStm *stm = nullptr;
	TR::PatchList *trues = nullptr,
		*falses = nullptr;
	TR::Exp *ret_exp = nullptr;
	TY::Ty *ret_ty = TY::IntTy::Instance();

	switch(oper){
		case A::PLUS_OP:
			ret_exp = new TR::ExExp( new T::BinopExp( T::PLUS_OP,  left_expty.exp->UnEx(), right_expty.exp->UnEx()));
			break;
		case A::MINUS_OP:
			ret_exp = new TR::ExExp( new T::BinopExp( T::MINUS_OP, left_expty.exp->UnEx(), right_expty.exp->UnEx()));
			break;
		case A::TIMES_OP:
			ret_exp = new TR::ExExp( new T::BinopExp( T::MUL_OP, left_expty.exp->UnEx(), right_expty.exp->UnEx()));
			break;
		case A::DIVIDE_OP:
			ret_exp = new TR::ExExp( new T::BinopExp( T::DIV_OP, left_expty.exp->UnEx(), right_expty.exp->UnEx()));
			break;

		case A::LT_OP:
			stm = new T::CjumpStm(
					T::LT_OP,
					left_expty.exp->UnEx(),
					right_expty.exp->UnEx(),
					nullptr,
					nullptr);
			trues = new TR::PatchList( &stm->true_label, nullptr);
			falses = new TR::PatchList( &stm->false_label, nullptr);
			ret_exp = new TR::CxExp(trues,falses,stm);
			break;
		case A::LE_OP:
			stm = new T::CjumpStm(
					T::LE_OP,
					left_expty.exp->UnEx(),
					right_expty.exp->UnEx(),
					nullptr,
					nullptr);
			trues = new TR::PatchList( &stm->true_label, nullptr);
			falses = new TR::PatchList( &stm->false_label, nullptr);
			ret_exp = new TR::CxExp(trues,falses,stm);
			break;
		case A::GT_OP:
			stm = new T::CjumpStm(
					T::GT_OP,
					left_expty.exp->UnEx(),
					right_expty.exp->UnEx(),
					nullptr,
					nullptr);
			trues = new TR::PatchList( &stm->true_label, nullptr);
			falses = new TR::PatchList( &stm->false_label, nullptr);
			ret_exp = new TR::CxExp(trues,falses,stm);
			break;
		case A::GE_OP:
			stm = new T::CjumpStm(
					T::GE_OP,
					left_expty.exp->UnEx(),
					right_expty.exp->UnEx(),
					nullptr,
					nullptr);
			trues = new TR::PatchList( &stm->true_label, nullptr);
			falses = new TR::PatchList( &stm->false_label, nullptr);
			ret_exp = new TR::CxExp(trues,falses,stm);
			break;
		case A::EQ_OP:
			stm = new T::CjumpStm(
					T::EQ_OP,
					left_expty.exp->UnEx(),
					right_expty.exp->UnEx(),
					nullptr,
					nullptr);
			trues = new TR::PatchList( &stm->true_label, nullptr);
			falses = new TR::PatchList( &stm->false_label, nullptr);
			ret_exp = new TR::CxExp(trues,falses,stm);
			break;
		case A::NEQ_OP:
			stm = new T::CjumpStm(
					T::NE_OP,
					left_expty.exp->UnEx(),
					right_expty.exp->UnEx(),
					nullptr,
					nullptr);
			trues = new TR::PatchList( &stm->true_label, nullptr);
			falses = new TR::PatchList( &stm->false_label, nullptr);
			ret_exp = new TR::CxExp(trues,falses,stm);
			break;

		default:
			return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
	}

	return TR::ExpAndTy(ret_exp,ret_ty);

}

TR::ExpAndTy RecordExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
	TY::Ty *ty = tenv->Look(typ);
	if(ty == nullptr){
		errormsg.Error(pos,"undefined type %s",typ->Name().c_str());
		return TR::ExpAndTy(nullptr, TY::VoidTy::Instance());
	}

	A::EFieldList *fl = fields;
	TY::FieldList *tyfl = nullptr;
	int record_size = 0;
	std::vector<TR::Exp*> record_exps;
	while(fl){
		A::EField *p = fl->head;
		fl = fl->tail;
		TR::ExpAndTy p_expty = p->exp->Translate(venv,tenv,level,label);
		record_exps.push_back(p_expty.exp);
		tyfl = new TY::FieldList(
				new TY::Field(
					p->name,
					p_expty.ty),
				tyfl);

		record_size += F::X64Frame::wordSize;
	}

	// alloc memory needed for record body by calling runtime function
	T::ExpList *runtime_args = new T::ExpList(new T::ConstExp(record_size),nullptr);
	TEMP::Temp *record_t = TEMP::Temp::NewTemp();
	T::Stm *alloc_stm = new T::MoveStm(
			new T::TempExp(record_t),
			new T::CallExp(
				new T::NameExp( TEMP::NamedLabel("allocRecord")),
				runtime_args));

	// init record body
	T::Stm *init_stm = nullptr;
	for(int i=0;i<record_exps.size();i++){
		T::Stm *mv_stm = new T::MoveStm(
				new T::MemExp(
					new T::BinopExp(
						T::PLUS_OP,
						new T::TempExp(record_t),
						new T::ConstExp(i * F::X64Frame::wordSize))),
				record_exps[i]->UnEx());
		init_stm = new T::SeqStm(mv_stm, init_stm);
	}

	// integrate allocation and initialization into eseqexp
	TR::Exp* ret_exp = new TR::ExExp(
			new T::EseqExp(
				new T::SeqStm( alloc_stm, init_stm),
				new T::TempExp(record_t)));

	return TR::ExpAndTy(ret_exp,ty);
}

TR::ExpAndTy SeqExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
	A::ExpList *p = seq;
	T::Stm *seq_stms = nullptr;

	while(p->tail){
		TR::ExpAndTy p_expty = p->head->Translate(venv,tenv,level,label);
		//ret_ty = p_expty.ty;

		T::Stm **seq_leaf = &seq_stms;
		while(*seq_leaf)
			seq_leaf = &(static_cast<T::SeqStm*>(*seq_leaf)->right);
		*seq_leaf = new T::SeqStm(p_expty.exp->UnNx(), nullptr);

		p = p->tail;
	}

	// process the last exp
	TR::ExpAndTy p_expty = p->head->Translate(venv,tenv,level,label);
	TY::Ty *ret_ty = p_expty.ty;
	TR::Exp *ret_exp = new TR::ExExp(
			new T::EseqExp(
				seq_stms,
				p_expty.exp->UnEx()));
	
  return TR::ExpAndTy(ret_exp, ret_ty);
}

TR::ExpAndTy AssignExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
	TR::ExpAndTy var_expty = var->Translate(venv,tenv,level,label),
		exp_expty = exp->Translate(venv,tenv,level,label);

	if(!var_expty.ty->IsSameType(exp_expty.ty))
		errormsg.Error(pos,"unmatched assign exp");

	// check if being loop var
	if(var->kind == A::Var::SIMPLE){
		E::VarEntry *ent = static_cast<E::VarEntry*>(venv->Look(static_cast<A::SimpleVar*>(var)->sym));
		if(ent == nullptr){
			//not found
		}
		if(ent->readonly){
			errormsg.Error(pos,"loop variable can't be assigned");
		}
	}

	TR::Exp *ret_exp = new TR::NxExp(
			new T::MoveStm(
				var_expty.exp->UnEx(),
				exp_expty.exp->UnEx()));
  return TR::ExpAndTy(ret_exp, TY::VoidTy::Instance());
}

TR::ExpAndTy IfExp::Translate(S::Table<E::EnvEntry> *venv,
                              S::Table<TY::Ty> *tenv, TR::Level *level,
                              TEMP::Label *label) const {
	TR::ExpAndTy flag_expty = test->Translate(venv,tenv,level,label),
		then_expty = then->Translate(venv,tenv,level,label);

	TY::Ty *ret_ty = TY::VoidTy::Instance();
	TR::Cx e1 = flag_expty.exp->UnCx();
	T::Exp *e3 = nullptr;

	if(!flag_expty.ty->IsSameType(TY::IntTy::Instance()))
		errormsg.Error(pos,"integer required");
	if(elsee){
		TR::ExpAndTy else_expty = elsee->Translate(venv,tenv,level,label);
		e3 = else_expty.exp->UnEx();
		if(!then_expty.ty->IsSameType(else_expty.ty)){
			errormsg.Error(pos,"then exp and else exp type mismatch");
		}
		ret_ty = then_expty.ty;
	}
	else {
		if(!then_expty.ty->IsSameType(TY::VoidTy::Instance())){
			errormsg.Error(pos,"if-then exp's body must produce no value");
		}
		ret_ty = TY::VoidTy::Instance();
	}

	TR::Exp *ret_exp = nullptr;

	// construct if exp sequence
	if(e3 != nullptr){
		TEMP::Label *then_label = TEMP::NewLabel(),
			*else_label = TEMP::NewLabel(),
			*end_label = TEMP::NewLabel();
		TEMP::Temp *if_result = TEMP::Temp::NewTemp();

		//*(e1.falses->head) = else_label;
		TR::do_patch(e1.trues, then_label);
		TR::do_patch(e1.falses,else_label);

		T::Stm *else_seq = new T::SeqStm(
				new T::SeqStm(
					new T::LabelStm(else_label),
					new T::MoveStm(
						new T::TempExp(if_result),
						e3)),
				new T::LabelStm(end_label));
		T::Stm *then_seq = new T::SeqStm(
				new T::LabelStm(then_label),
				new T::SeqStm(
					new T::MoveStm(
						new T::TempExp(if_result),
						then_expty.exp->UnEx()),
					new T::JumpStm(
						new T::NameExp(end_label),
						new TEMP::LabelList(end_label,nullptr))));
		ret_exp = new TR::ExExp(
				new T::EseqExp(
					new T::SeqStm(
						e1.stm,
						new T::SeqStm( then_seq, else_seq)),
					new T::TempExp(if_result)));
	}
	else{
		TEMP::Label *then_label = TEMP::NewLabel(),
			*end_label = TEMP::NewLabel();

		//*(e1.falses->head) = end_label;
		TR::do_patch(e1.trues, then_label);
		TR::do_patch(e1.falses, end_label);

		T::Stm *else_seq = new T::LabelStm(end_label);
		T::Stm *then_seq = new T::SeqStm(
				new T::LabelStm(then_label),
				new T::SeqStm(
					then_expty.exp->UnNx(),
					new T::JumpStm(
						new T::NameExp(end_label),
						new TEMP::LabelList(end_label,nullptr))));
		ret_exp = new TR::NxExp(
				new T::SeqStm(
					e1.stm,
					new T::SeqStm(
						then_seq,
						else_seq)));
	}
	
  return TR::ExpAndTy(ret_exp, ret_ty);
}

TR::ExpAndTy WhileExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {

	TEMP::Label *test_label = TEMP::NewLabel(),
		*body_label = TEMP::NewLabel(),
		*done_label = TEMP::NewLabel();
	TR::ExpAndTy test_expty = test->Translate(venv,tenv,level,label),
		body_expty = body->Translate(venv,tenv,level,done_label);

	if(!body_expty.ty->IsSameType(TY::VoidTy::Instance()))
		errormsg.Error(body->pos,"while body must produce no value");

	T::Stm *test_stm = new T::CjumpStm(
			T::EQ_OP,
			test_expty.exp->UnEx(),
			new T::ConstExp(0),
			done_label,
			body_label);
	TR::Exp *ret_exp = new TR::NxExp(
			new T::SeqStm(
				new T::SeqStm(
					new T::LabelStm(test_label),
					test_stm),
				new T::SeqStm(
					new T::SeqStm(
						new T::LabelStm(body_label),
						body_expty.exp->UnNx()),
					new T::LabelStm(done_label))));

  return TR::ExpAndTy(ret_exp, TY::VoidTy::Instance());
}

TR::ExpAndTy ForExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
	//TR::ExpAndTy lo_expty = lo->Translate(venv,tenv,level,label),
	//  hi_expty = hi->Translate(venv,tenv,level,label);

	//venv->BeginScope();
	//venv->Enter(var,new E::VarEntry(TY::IntTy::Instance(),true));

	//if(!lo_expty.ty->IsSameType(TY::IntTy::Instance()))
	//  errormsg.Error(lo->pos,"for exp's range type is not integer");
	//if(!hi_expty.ty->IsSameType(TY::IntTy::Instance()))
	//  errormsg.Error(hi->pos,"for exp's range type is not integer");

	//TR::ExpAndTy body_expty = body->Translate(venv,tenv,level,label);
	//venv->EndScope();

  //return TR::ExpAndTy(nullptr, body_expty.ty);

	// An potential error may occur when limit equals to maxint
	S::Symbol *test_limit = S::Symbol::UniqueSymbol("limit");
	A::LetExp *let_exp = new A::LetExp( pos,
			new A::DecList(
				new A::VarDec(pos, var, nullptr, lo),
				new A::DecList(
					new A::VarDec(pos, test_limit, nullptr, hi),
					nullptr)),
			new A::WhileExp( pos,
				new A::OpExp( pos,
					A::LE_OP,
					new A::VarExp(pos, new A::SimpleVar(pos, var)),
					new A::VarExp(pos, new A::SimpleVar(pos, test_limit))),
				new A::SeqExp( pos,
					new A::ExpList(
						body,
						new A::ExpList(
							new A::AssignExp(pos,
								new A::SimpleVar(pos, var),
								new A::OpExp(pos,
									A::PLUS_OP,
									new A::VarExp(pos, new A::SimpleVar(pos, var)),
									new A::IntExp(pos, 1))),
							nullptr)))));
	return let_exp->Translate(venv,tenv,level,label);

}

TR::ExpAndTy BreakExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
	TR::Exp *ret_exp = new TR::NxExp(
			new T::JumpStm(
				new T::NameExp(label),
				new TEMP::LabelList(label,nullptr)));
  return TR::ExpAndTy(ret_exp, TY::VoidTy::Instance());
}

TR::ExpAndTy LetExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
	A::DecList *p = decs;	
	TR::ExExp *ret_exp = new TR::ExExp(new T::EseqExp(nullptr, nullptr));
	T::Stm **ret_tail = &static_cast<T::EseqExp*>(ret_exp->exp)->stm;

	venv->BeginScope();
	tenv->BeginScope();
	while(p != nullptr){
		TR::Exp *p_exp = p->head->Translate(venv,tenv,level,label);
		*ret_tail = new T::SeqStm(p_exp->UnNx(), nullptr);
		ret_tail = &static_cast<T::SeqStm*>(*ret_tail)->right;
		p = p->tail;
	}
	TR::ExpAndTy body_expty = body->Translate(venv,tenv,level,label);
	static_cast<T::EseqExp*>(ret_exp->exp)->exp = body_expty.exp->UnEx();
	venv->EndScope();
	tenv->EndScope();

  return TR::ExpAndTy(ret_exp, body_expty.ty);
}

TR::ExpAndTy ArrayExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
	TY::Ty *ret_ty = tenv->Look(typ);
	TR::ExpAndTy init_expty = init->Translate(venv,tenv,level,label),
		size_expty = size->Translate(venv,tenv,level,label);

	if(ret_ty->ActualTy()->kind == TY::Ty::ARRAY){
		if(!static_cast<TY::ArrayTy*>(ret_ty->ActualTy())->ty->IsSameType(init_expty.ty))
			errormsg.Error(pos,"type mismatch");
	}
	else{

	}

	// alloc memory and init with runtime function
	T::ExpList *runtime_args = new T::ExpList(
			size_expty.exp->UnEx(),
			new T::ExpList(
				init_expty.exp->UnEx(),
				nullptr));
	TEMP::Temp *arr_t = TEMP::Temp::NewTemp();
	T::Stm *alloc_stm = new T::MoveStm(
			new T::TempExp(arr_t),
			new T::CallExp(
				new T::NameExp(TEMP::NamedLabel("initArray")),
				runtime_args));

	TR::Exp *ret_exp = new TR::ExExp(
			new T::EseqExp(
				alloc_stm,
				new T::TempExp(arr_t)));

  return TR::ExpAndTy(ret_exp, ret_ty);
}

TR::ExpAndTy VoidExp::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
}

TR::Exp *FunctionDec::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
	//process declaration
	A::FunDecList *p = functions;
	while(p){
		A::FunDec *func = p->head;
		p = p->tail;

		TY::Ty *rety = TY::VoidTy::Instance();
		if(func->result){
			rety = tenv->Look(func->result);
		}

		// check repeated func name
		if(venv->Look(func->name) != nullptr){
			errormsg.Error(pos,"two functions have the same name");
			continue;
		}

		// process formals
		TY::TyList *formals = make_formal_tylist(tenv,func->params);
		U::BoolList *escapes = nullptr;
		A::FieldList *p_formals = func->params;
		while(p_formals){
			escapes = new U::BoolList(p_formals->head->escape, escapes);
			p_formals = p_formals->tail;
		}

		E::FunEntry *func_ent = new E::FunEntry(
				// (2)
				TR::Level::NewLevel(level, TEMP::NamedLabel(func->name->Name()),escapes),
				label,
				formals,
				rety);
		venv->Enter(func->name, func_ent);
	}

	//process implementation
	p = functions;
	while(p){
		A::FunDec *func = p->head;
		p = p->tail;

		// Process single func
		venv->BeginScope();

		// Enter parameters
		A::FieldList *pf = func->params;
		while(pf){
			venv->Enter(pf->head->name,new E::VarEntry(tenv->Look(pf->head->typ)));
			pf = pf->tail;
		}

		E::FunEntry *func_ent = static_cast<E::FunEntry*>(venv->Look(func->name));

		// (6) Function body
		TR::ExpAndTy body_expty = func->body->Translate(venv, tenv, func_ent->level, func_ent->label);
		// (4) (5) (7) (8)
		T::Stm *func_body = func_ent->level->frame->ProcEntryExit1(body_expty.exp->UnNx());
		// (1) (3) (9) (11) locate in procEntryExit3
		TR::AllocFrag(new F::ProcFrag(func_body, func_ent->level->frame));

		venv->EndScope();

		TY::Ty *rety = TY::VoidTy::Instance();
		if(func->result){
			rety = tenv->Look(func->result);
			if(!rety->IsSameType(body_expty.ty))
				errormsg.Error(func->pos,"func return type differs from body");
		}
		else {
			if(!rety->IsSameType(body_expty.ty))
				errormsg.Error(func->pos,"procedure returns value");
		}
	}

  return new TR::ExExp(new T::ConstExp(0));
}

TR::Exp *VarDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                           TR::Level *level, TEMP::Label *label) const {
	TR::ExpAndTy init_expty = init->Translate(venv,tenv,level,label);
	if(typ){
		if(!init_expty.ty->IsSameType(tenv->Look(typ))){
			errormsg.Error(pos,"type mismatch");
		}
	}else{
		if(init_expty.ty->IsSameType(TY::NilTy::Instance())){
			errormsg.Error(pos,"init should not be nil without type specified");
		}
	}
	if(init_expty.ty == nullptr) errormsg.Error(pos,"1no such type");
	venv->Enter(var,new E::VarEntry(init_expty.ty,false));

	TR::Access *acc = TR::Access::AllocLocal(level,true);

	// (p120)
	// 变量定义中, transDec返回一个包含赋初值的赋值表达式的Tr_exp
	TR::Exp *ret_exp = new TR::NxExp(
			new T::MoveStm(
				acc->access->ToExp(new T::TempExp(F::FP())),
				init_expty.exp->UnEx()));

  return ret_exp;
}

TR::Exp *TypeDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                            TR::Level *level, TEMP::Label *label) const {
	A::NameAndTyList *p = types;
	//enter all names
	while(p){
		A::NameAndTy *nt = p->head;
		p = p->tail;
		
		// check repeated type name
		A::NameAndTyList *np = p;
		while(np){
			if(np->head->name == nt->name){
				errormsg.Error(pos,"two types have the same name");
				break;
			}
			np = np->tail;
		}
		if(np == nullptr)
			tenv->Enter(nt->name,new TY::NameTy(nt->name,nullptr));
	}

	//Resolve true type
	p = types;
	while(p){
		A::NameAndTy *nt = p->head;
		TY::Ty *ty = nt->ty->Translate(tenv);
		if(ty != nullptr){
			TY::NameTy *nty = static_cast<TY::NameTy*>(tenv->Look(nt->name));
			nty->ty = ty;
			tenv->Set(nt->name,nty);
		}
		p = p->tail;
	}

	// Check illegal type cycle
	p = types;
	while(p){
		A::NameAndTy *nt = p->head;
		p = p->tail;

		TY::NameTy *nty = static_cast<TY::NameTy*>(tenv->Look(nt->name));
		if(nty->ty->ActualTy() == nullptr){
			errormsg.Error(pos,"illegal type cycle");
			nty->ty = TY::VoidTy::Instance();
		}
	}

  return new TR::ExExp(new T::ConstExp(0));
}

TY::Ty *NameTy::Translate(S::Table<TY::Ty> *tenv) const {
	TY::Ty *ty = tenv->Look(name);
	if(ty == nullptr){
		errormsg.Error(pos,"2no such type");
		return nullptr;
		return TY::VoidTy::Instance();
	}
	else{
		return new TY::NameTy(name,ty);
	}
}

TY::Ty *RecordTy::Translate(S::Table<TY::Ty> *tenv) const {
	A::FieldList *p = record;
	while(p){
		if(tenv->Look(p->head->typ) == nullptr){
			errormsg.Error(pos,"undefined type %s",p->head->typ->Name().c_str());
		}
		p = p->tail;
	}
  return new TY::RecordTy(make_fieldlist(tenv,record));
}

TY::Ty *ArrayTy::Translate(S::Table<TY::Ty> *tenv) const {
	TY::Ty *ty = tenv->Look(array);
	if(ty == nullptr){
		errormsg.Error(pos,"4no such type");
		return nullptr;
		return TY::VoidTy::Instance();
	}
	else{
		return new TY::ArrayTy(ty);
	}
}

}  // namespace A
