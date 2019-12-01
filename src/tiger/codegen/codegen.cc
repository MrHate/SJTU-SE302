#include "tiger/codegen/codegen.h"

namespace {

AS::InstrList *iList = nullptr, last = nullptr;

void emit(AS::Instr* inst){
	if(last != nullptr){
		last = last->tail = new AS::InstrList(inst, nullptr);
	}
	else{
		last = iList = new AS::InstrList(inst, nullptr);
	}
}

TEMP::Temp* munchExp(T::Exp* e){
	switch(e->kind){
		case T::Exp::MEM:
			{
				T::MemExp *e0 = static_cast<T::MemExp*>(e);
				TEMP::Temp *r = TEMP::Temp::NewTemp();
				switch(e0->exp->kind){
					case T::Exp::BINOP:
						{
							T::BinopExp *e1 = static<T::BinopExp*>(e0->exp);
							if(e1->left->kind == T::Exp::CONST)
								emit(new AS::OperInstr(
											"movq " + static_cast<T::ConstExp*>(e1->left)->consti + "(`s0) `d0",
											new TEMP::TempList(r, nullptr),
											new TEMP::TempList(munchExp(e1->right), nullptr),
											nullptr));
							else if(e1->right->kind == T::Exp::CONST)
								emit(new AS::OperInstr(
											"movq " + static_cast<T::ConstExp*>(e1->right)->consti + "(`s0) `d0",
											new TEMP::TempList(r, nullptr),
											new TEMP::TempList(munchExp(e1->left), nullptr),
											nullptr));
							else assert(0);
							return r;
						}
					case T::Exp::CONST:
						{
							emit(new AS::OperInstr(
										"movq " + static_cast<T::ConstExp*>(e0->exp)->consti + "(`r0) `d0",
										new TEMP::TempList(r,nullptr),
										nullptr,
										nullptr));
							return r;
						}
					default:
						{
							emit(new AS::OperInstr(
										"movq (`s0) `d0",
										new TEMP::TempList(r, nullptr),
										new TEMP::TempList(munchExp(e0->exp), nullptr),
										nullptr));
							return r;
						}
				}
			}
		case T::Exp::BINOP:
			{
				T::BinopExp *e0 = static_cast<T::BinopExp*>(e);
				TEMP::Temp *r = TEMP::Temp::NewTemp();
				std::string oper_str = "";
				switch(e0->op){
					case T::PLUS_OP:
						oper_str = "addq";
						break;
					case T::MINUS_OP:
						oper_str = "subq";
						break;
					case T::MUL_OP:
						oper_str = "imulq";
						break;
					case T::DIV_OP:
						oper_str = "idivq";
						break;
					default:
						assert(0);
				}
				TEMP::Temp *lr = munchExp(e0->left),
					&rr = munchExp(e0->right);
				emit(new AS::MoveInstr(
							"movq `s0 `d0",
							new TEMP::TempList(r, nullptr),
							new TEMP::TempList(lr, nullptr)));
				emit(new AS::OperInstr(
							"addq `s0 `d0",
							new TEMP::TempList(r, nullptr),
							new TEMP::TempList(rr, nullptr),
							nullptr));
				return r;
			}
		case T::Exp::CONST:
			{
				T::ConstExp *e0 = static_cast<T::ConstExp*>(e);
				TEMP::Temp *r = TEMP::Temp::NewTemp();
				emit(new AS::MoveInstr(
							"movq $" + e0->consti + " `d0",
							new TEMP::TempList(r, nullptr),
							nullptr,
							nullptr));
				return r;
			}
		case T::Exp::NAME:
			{
				T::NameExp *e0 = static_cast<T::NameExp*>(e);
				TEMP::Temp *r = TEMP::Temp::NewTemp();
				emit(new AS::MoveInstr(
							"movq $" + TEMP::LabelString(e0->name) + " `d0",
							new TEMP::TempList(r, nullptr),
							nullptr,
							nullptr));
				return r;
			}
		case T::Exp::TEMP:
			return static_cast<T::TempExp*>(e)->temp;
		case T::Exp::CALL:
			return nullptr;
		case T::Exp::ESEQ:
			assert(0);
	}

	// control flow should not be here
	assert(0);
	return nullptr;
}

void munchStm(T::Stm* s){

}


}

namespace CG {

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList) {
	AS::InstrList *list;
	for(T::StmList *sl = stmList; sl; sl = sl->tail) munchStm(sl->head);
	list = iList; 
	iList = last = nullptr; 
  return list;
}

}  // namespace CG
