#include "tiger/semant/semant.h"
#include "tiger/errormsg/errormsg.h"


extern EM::ErrorMsg errormsg;

using VEnvType = S::Table<E::EnvEntry> *;
using TEnvType = S::Table<TY::Ty> *;

namespace {
static TY::TyList *make_formal_tylist(TEnvType tenv, A::FieldList *params) {
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

static TY::FieldList *make_fieldlist(TEnvType tenv, A::FieldList *fields) {
  if (fields == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(fields->head->typ);
  return new TY::FieldList(new TY::Field(fields->head->name, ty),
                           make_fieldlist(tenv, fields->tail));
}

}  // namespace

namespace A {

TY::Ty *SimpleVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"simplevar[sym]"+sym->Name());
#endif
	E::VarEntry *ent = static_cast<E::VarEntry*>(venv->Look(sym));
	if(ent == nullptr){
		errormsg.Error(pos,"undefined symbol %s",sym->Name().c_str());
		return TY::VoidTy::Instance();
	}

#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"simplevar[type]%s",ent->ty->PrintActualTy().c_str());
#endif
	return ent->ty;
}

TY::Ty *FieldVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
// Get fieldlist from the lvalue(var)
// then return the certain type
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"fieldvar[sym]"+sym->Name());
#endif
	TY::Ty *varty = var->SemAnalyze(venv,tenv,labelcount);
	if(varty->kind == TY::Ty::RECORD){
		TY::RecordTy *recty = static_cast<TY::RecordTy*>(varty);
		TY::FieldList *p = recty->fields;
		while(p){
			if(p->head->name == sym)return p->head->ty;
			p = p->tail;
		}
		errormsg.Error(pos,"no such a field %s",sym->Name().c_str());
	}
	else {
		errormsg.Error(pos,"not a field var");
	}

	return TY::VoidTy::Instance();
}

TY::Ty *SubscriptVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                                 int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"subscriptvar");
#endif
	TY::Ty *varty = var->SemAnalyze(venv,tenv,labelcount);
	//return var->SemAnalyze(venv,tenv,labelcount);
	if(varty->kind == TY::Ty::ARRAY){
		TY::ArrayTy *arrty = static_cast<TY::ArrayTy*>(varty);
#ifdef __DGY__DEBUG__
		errormsg.Error(pos,"[arraytype]%s",arrty->ty->PrintActualTy().c_str());
#endif
		return arrty->ty;
	}
	errormsg.Error(pos,"not a array var");

	return TY::VoidTy::Instance();
}

TY::Ty *VarExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"varexp");
#endif
	return var->SemAnalyze(venv,tenv,labelcount);
}

TY::Ty *NilExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"nilexp");
#endif
  return TY::NilTy::Instance();
}

TY::Ty *IntExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"intexp[i]%d",i);
#endif
  return TY::IntTy::Instance();
}

TY::Ty *StringExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"stringexp[s]%s",s.c_str());
#endif
  return TY::StringTy::Instance();
}

TY::Ty *CallExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                            int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"callexp[func]"+func->Name());
#endif
  E::EnvEntry *ent = venv->Look(func);
	if(ent->kind == E::EnvEntry::FUN){
		return static_cast<E::FunEntry*>(ent)->result;
	}
	else{
		errormsg.Error(pos,"not a name of func");
		return TY::VoidTy::Instance();
	}

}

TY::Ty *OpExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"opexp");
#endif
	TY::Ty *left_ty = left->SemAnalyze(venv,tenv,labelcount),
		*right_ty = right->SemAnalyze(venv,tenv,labelcount);

	if(!left_ty->IsSameType(right_ty)){
			errormsg.Error(left->pos,"same type required");
#ifdef __DGY__DEBUG__
			errormsg.Error(pos,"[left]%s[right]%s",left_ty->PrintActualTy().c_str(),right_ty->PrintActualTy().c_str());
#endif
			return TY::VoidTy::Instance();
	}

	switch(oper){
		case A::PLUS_OP:
		case A::MINUS_OP:
		case A::LT_OP:
		case A::LE_OP:
		case A::GT_OP:
		case A::GE_OP:
			if(!left_ty->IsSameType(TY::IntTy::Instance())){
				errormsg.Error(left->pos,"integer required");
#ifdef __DGY__DEBUG__
				errormsg.Error(pos,"[left]"+left_ty->PrintActualTy());
#endif
				return TY::VoidTy::Instance();
			}
			if(!right_ty->IsSameType(TY::IntTy::Instance())){
				errormsg.Error(right->pos,"integer required");
#ifdef __DGY__DEBUG__
				errormsg.Error(pos,"[right]"+right_ty->PrintActualTy());
#endif
				return TY::VoidTy::Instance();
			}
			return TY::IntTy::Instance();
			break;
		case A::EQ_OP:
		case A::NEQ_OP:
			return TY::IntTy::Instance();
		default:
			;
	}
  return TY::VoidTy::Instance();
}

TY::Ty *RecordExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"recordexp");
	errormsg.Error(pos,"[typ]"+typ->Name());
#endif
	A::EFieldList *fl = fields;
	TY::FieldList *tyfl = nullptr;
	while(fl){
		A::EField *p = fl->head;
		fl = fl->tail;
		tyfl = new TY::FieldList(new TY::Field(p->name,p->exp->SemAnalyze(venv,tenv,labelcount)),tyfl);
	}
  //return new TY::RecordTy(tyfl);
	return tenv->Look(typ);
}

TY::Ty *SeqExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"seqexp");
#endif
	A::ExpList *p = seq;
	TY::Ty *res = nullptr;
	while(p){
		res = p->head->SemAnalyze(venv,tenv,labelcount);
		p = p->tail;
	}
  return res;
}

TY::Ty *AssignExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"assignexp");
#endif
	E::VarEntry *ent;
	switch(var->kind){
		case A::Var::SIMPLE:
			ent = static_cast<E::VarEntry*>(venv->Look(static_cast<A::SimpleVar*>(var)->sym));
			if(ent == nullptr){
				//not found
			}
			if(ent->readonly){
				errormsg.Error(pos,"loop variable can't be assigned");
			}
			break;
		case A::Var::FIELD:
			break;
		case A::Var::SUBSCRIPT:
			break;
		default:
			;
	}

  return TY::VoidTy::Instance();
}

TY::Ty *IfExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"ifexp");
#endif
	TY::Ty *flag = test->SemAnalyze(venv,tenv,labelcount),
		*thenty = then->SemAnalyze(venv,tenv,labelcount);
	if(elsee){
		TY::Ty *elsety = elsee->SemAnalyze(venv,tenv,labelcount);
		if(!thenty->IsSameType(elsety)){
			errormsg.Error(pos,"then exp and else exp type mismatch");

#ifdef __DGY__DEBUG__
			errormsg.Error(pos,"[then]"+thenty->PrintActualTy()+"[else]"+elsety->PrintActualTy());
#endif
		}
		return thenty;
	}
	else {
		if(!thenty->IsSameType(TY::VoidTy::Instance())){
			errormsg.Error(pos,"if-then exp's body must produce no value");
		}
		return TY::VoidTy::Instance();
	}
}

TY::Ty *WhileExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"whileexp");
#endif
	TY::Ty *body_ty = body->SemAnalyze(venv,tenv,labelcount);

	if(!body_ty->IsSameType(TY::VoidTy::Instance()))
		errormsg.Error(body->pos,"while body must produce no value");
  return TY::VoidTy::Instance();
}

TY::Ty *ForExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"forexp");
#endif
	TY::Ty *lo_ty = lo->SemAnalyze(venv,tenv,labelcount),
		*hi_ty = hi->SemAnalyze(venv,tenv,labelcount);

	venv->BeginScope();
	venv->Enter(var,new E::VarEntry(TY::IntTy::Instance(),true));

	if(!lo_ty->IsSameType(TY::IntTy::Instance()))
		errormsg.Error(lo->pos,"for exp's range type is not integer");
	if(!hi_ty->IsSameType(TY::IntTy::Instance()))
		errormsg.Error(hi->pos,"for exp's range type is not integer");

	TY::Ty *body_ty = body->SemAnalyze(venv,tenv,labelcount);
	venv->EndScope();
	return body_ty;
}

TY::Ty *BreakExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"breakexp");
#endif
  return TY::VoidTy::Instance();
}

TY::Ty *LetExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"letexp-declaration");
#endif
	A::DecList *p = decs;	
	while(p != nullptr){
		p->head->SemAnalyze(venv,tenv,labelcount);
		p = p->tail;
	}
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"letexp-expsequence");
#endif
  return body->SemAnalyze(venv,tenv,labelcount);
}

TY::Ty *ArrayExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"arrayexp");
	errormsg.Error(pos,"[typ]"+typ->Name());
#endif
	TY::Ty *ty = tenv->Look(typ);
	return ty;
}

TY::Ty *VoidExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                            int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"voidexp");
#endif
  return TY::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"funcdec");
#endif
	//process declaration
	A::FunDecList *p = functions;
	while(p){
		A::FunDec *func = p->head;
		p = p->tail;

		TY::Ty *rety = TY::VoidTy::Instance();
		if(func->result){
			rety = tenv->Look(func->result);
		}
		TY::TyList *formals = make_formal_tylist(tenv,func->params);
		venv->Enter(func->name,new E::FunEntry(formals,rety));
	}

	//process implementation
	p = functions;
	while(p){
		A::FunDec *func = p->head;
		p = p->tail;

#ifdef __DGY__DEBUG__
		errormsg.Error(pos,"func[name]"+func->name->Name());
#endif
		venv->BeginScope();
		A::FieldList *pf = func->params;
		while(pf){
			venv->Enter(pf->head->name,new E::VarEntry(tenv->Look(pf->head->typ)));
			pf = pf->tail;
		}
		TY::Ty *boty = func->body->SemAnalyze(venv,tenv,labelcount);
		venv->EndScope();

		TY::Ty *rety = TY::VoidTy::Instance();
		if(func->result){
			rety = tenv->Look(func->result);
		}
		if(!rety->IsSameType(boty))
			errormsg.Error(func->pos,"func return type differs from body");
	
	}
}

void VarDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"vardec");
	errormsg.Error(pos,"[var]"+var->Name());
	if(typ)errormsg.Error(pos,"[typ]"+typ->Name());
#endif
	TY::Ty *ty = init->SemAnalyze(venv,tenv,labelcount);
	if(typ)if(!ty->IsSameType(tenv->Look(typ))){
		errormsg.Error(pos,"not same");
	}
	if(ty == nullptr) errormsg.Error(pos,"1no such type");
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"vardec[var]%s[fact]%s",var->Name().c_str(),ty->PrintActualTy().c_str());
#endif
	venv->Enter(var,new E::VarEntry(ty,false));
}

void TypeDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"typedec");
#endif
	A::NameAndTyList *p = types;
	while(p){
		A::NameAndTy *nt = p->head;
#ifdef __DGY__DEBUG__
		errormsg.Error(pos,"[name]"+nt->name->Name());
#endif
		TY::Ty *ty = nt->ty->SemAnalyze(tenv);
		if(ty != nullptr){
#ifdef __DGY__DEBUG__
			errormsg.Error(pos,"Enter ty[name]"+nt->name->Name());
#endif
			tenv->Enter(nt->name,ty);
		}
		p = p->tail;
	}
}

TY::Ty *NameTy::SemAnalyze(TEnvType tenv) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"NameTy[name]"+name->Name());
#endif
	TY::Ty *ty = tenv->Look(name);
	if(ty == nullptr){
		errormsg.Error(pos,"2no such type");
		return nullptr;
	}
	else{
		return new TY::NameTy(name,ty);
	}
}

TY::Ty *RecordTy::SemAnalyze(TEnvType tenv) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"RecordTy");
#endif
  return new TY::RecordTy(make_fieldlist(tenv,record));
}

TY::Ty *ArrayTy::SemAnalyze(TEnvType tenv) const {
#ifdef __DGY__DEBUG__
	errormsg.Error(pos,"ArrayTy[name]"+array->Name());
#endif
	TY::Ty *ty = tenv->Look(array);
	if(ty == nullptr){
		errormsg.Error(pos,"4no such type");
		return nullptr;
	}
	else{
		return new TY::ArrayTy(ty);
	}
}

}  // namespace A

namespace SEM {
void SemAnalyze(A::Exp *root) {
  if (root) root->SemAnalyze(E::BaseVEnv(), E::BaseTEnv(), 0);
}

}  // namespace SEM
