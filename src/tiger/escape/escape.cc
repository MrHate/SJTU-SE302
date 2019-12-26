#include "tiger/escape/escape.h"

namespace {
	static S::Table<ESC::EscapeEntry>* _env = nullptr;
	void _findEscapeExp(A::Exp* exp, int depth);
	void _findEscapeVar(A::Var* var, int depth);
	void _findEscapeDec(A::Dec* dec, int depth);
}

namespace ESC {

class EscapeEntry {
 public:
  int depth;
  bool* escape;

  EscapeEntry(int depth, bool* escape) : depth(depth), escape(escape) {}
};

void FindEscape(A::Exp* exp) {
	delete _env;
	_env = new S::Table<EscapeEntry>();
	_findEscapeExp(exp,0);
}

}  // namespace ESC

namespace {

	void _findEscapeExp(A::Exp* exp, int depth){
		switch(exp->kind){
			case A::Exp::VAR:
				_findEscapeVar(dynamic_cast<A::VarExp*>(exp)->var, depth);
				break;
			case A::Exp::CALL:
				for(A::ExpList* arg = dynamic_cast<A::CallExp*>(exp)->args; arg; arg = arg->tail)
					_findEscapeExp(arg->head, depth);
				break;
			case A::Exp::OP:
				{
					A::OpExp *e = dynamic_cast<A::OpExp*>(exp);
					_findEscapeExp(e->left, depth);
					_findEscapeExp(e->right, depth);
				}
				break;
			case A::Exp::RECORD:
				for(A::EFieldList* p = dynamic_cast<A::RecordExp*>(exp)->fields; p; p = p->tail)
					_findEscapeExp(p->head->exp, depth);
				break;
			case A::Exp::SEQ:
				for(A::ExpList* e = dynamic_cast<A::SeqExp*>(exp)->seq; e; e = e->tail)
					_findEscapeExp(e->head, depth);
				break;
			case A::Exp::ASSIGN:
				{
					A::AssignExp *e = dynamic_cast<A::AssignExp*>(exp);
					_findEscapeVar(e->var, depth);
					_findEscapeExp(e->exp, depth);
				}
				break;
			case A::Exp::IF:
				{
					A::IfExp* e = dynamic_cast<A::IfExp*>(exp);
					_findEscapeExp(e->test, depth);
					_findEscapeExp(e->then, depth);
					if(e->elsee) _findEscapeExp(e->elsee, depth);
				}
				break;
			case A::Exp::WHILE:
				{
					A::WhileExp *e = dynamic_cast<A::WhileExp*>(exp);
					_findEscapeExp(e->test, depth);
					_findEscapeExp(e->body, depth);
				}
				break;
			case A::Exp::LET:
				{
					A::LetExp *e = dynamic_cast<A::LetExp*>(exp);
					_env->BeginScope();
					for(A::DecList *p = e->decs; p; p = p->tail)
						_findEscapeDec(p->head, depth);
					_findEscapeExp(e->body, depth);
					_env->EndScope();
				}
				break;
			case A::Exp::ARRAY:
				{
					A::ArrayExp *e = dynamic_cast<A::ArrayExp*>(exp);
					_findEscapeExp(e->size, depth);
					_findEscapeExp(e->init, depth);
				}
				break;
			case A::Exp::FOR:
				{
					A::ForExp *e = dynamic_cast<A::ForExp*>(exp);
					_findEscapeExp(e->lo, depth);
					_findEscapeExp(e->hi, depth);

					_env->BeginScope();
					ESC::EscapeEntry *ent = new ESC::EscapeEntry(depth, new bool(true));
					_env->Enter(e->var, ent);
					_findEscapeExp(e->body, depth);
					_env->EndScope();
				}
				break;

			case A::Exp::BREAK:
			case A::Exp::VOID:
			case A::Exp::NIL:
			case A::Exp::INT:
			case A::Exp::STRING:
				break;
			default: assert(0);
		}
	}

	void _findEscapeVar(A::Var* var, int depth){
		switch(var->kind){
			case A::Var::SIMPLE:
				{
					ESC::EscapeEntry *ent = _env->Look(dynamic_cast<A::SimpleVar*>(var)->sym);
					assert(ent);
					if(ent->depth < depth){
						var->Print(stderr, depth);
						fprintf(stderr, "find escape\n");
					 	*(ent->escape) = true;
					}
				}
				break;
			case A::Var::FIELD:
				_findEscapeVar(dynamic_cast<A::FieldVar*>(var)->var, depth);
				break;
			case A::Var::SUBSCRIPT:
				{
					A::SubscriptVar *v = dynamic_cast<A::SubscriptVar*>(var);
					_findEscapeVar(v->var, depth);
					_findEscapeExp(v->subscript, depth);
				}
				break;
			default: assert(0);
		}
	}

	void _findEscapeDec(A::Dec* dec, int depth){
		switch(dec->kind){
			case A::Dec::FUNCTION:
				for(A::FunDecList *functions = dynamic_cast<A::FunctionDec*>(dec)->functions; functions; functions = functions->tail){
					A::FunDec *func = functions->head;
					_env->BeginScope();
					for(A::FieldList *params = func->params; params; params = params->tail){
						ESC::EscapeEntry *ent = new ESC::EscapeEntry(depth + 1, &params->head->escape);
						params->head->escape = false;
						_env->Enter(params->head->name, ent);
					}
					_findEscapeExp(func->body, depth + 1);
					_env->EndScope();
				}
				break;
			case A::Dec::VAR:
				{
					A::VarDec *d = dynamic_cast<A::VarDec*>(dec);
					ESC::EscapeEntry *ent = new ESC::EscapeEntry(depth, &d->escape);
					d->escape = false;
					_env->Enter(d->var, ent);
				}
				break;
			case A::Dec::TYPE: break;
			default: assert(0);
		}
	}

}

