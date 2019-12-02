#include "tiger/codegen/codegen.h"

namespace {

AS::InstrList *iList = nullptr, *last = nullptr;

TEMP::TempList *argregs = new TEMP::TempList(
		F::RDI(), new TEMP::TempList(
			F::RSI(), new TEMP::TempList(
				F::RDX(), new TEMP::TempList(
					F::RCX(), new TEMP::TempList(
						F::R8(), new TEMP::TempList(
							F::R9(), nullptr))))));
TEMP::TempList *calldefs = new TEMP::TempList(
		F::RAX(), new TEMP::TempList(
			F::R10(), new TEMP::TempList(
				F::R11(), argregs)));

TEMP::Temp* munchExp(T::Exp* e);

void emit(AS::Instr* inst){
	if(last != nullptr){
		last = last->tail = new AS::InstrList(inst, nullptr);
	}
	else{
		last = iList = new AS::InstrList(inst, nullptr);
	}
}

int munchArgs(T::ExpList* args, bool reg){
	int pushs = 0;
	if(reg && args){
		TEMP::TempList *p_reg = argregs;
		for(int i=0;i<6 && args;i++){
			emit(new AS::MoveInstr(
						"movq `s0, `d0",
						new TEMP::TempList(p_reg->head, nullptr),
						new TEMP::TempList(munchExp(args->head), nullptr)));
			args = args->tail;
			p_reg = p_reg->tail;
		}
	}else if(args){
		pushs = munchArgs(args->tail, false) + 1;
		emit(new AS::OperInstr(
					"pushq `s0",
					nullptr,
					new TEMP::TempList(munchExp(args->head), nullptr),
					nullptr));
	}
	return pushs;
}

TEMP::Temp* munchExp(T::Exp* e){
	switch(e->kind){
		case T::Exp::MEM:
			{
				T::MemExp *e0 = dynamic_cast<T::MemExp*>(e);
				TEMP::Temp *r = TEMP::Temp::NewTemp();
				switch(e0->exp->kind){
					case T::Exp::BINOP:
						{
							T::BinopExp *e1 = dynamic_cast<T::BinopExp*>(e0->exp);
							assert(e1->op == T::PLUS_OP);
							if(e1->left->kind == T::Exp::CONST)
								emit(new AS::OperInstr(
											// movq imm(S), D
											"movq " + std::to_string(dynamic_cast<T::ConstExp*>(e1->left)->consti) + "(`s0), `d0",
											new TEMP::TempList(r, nullptr),
											new TEMP::TempList(munchExp(e1->right), nullptr),
											nullptr));
							else if(e1->right->kind == T::Exp::CONST)
								emit(new AS::OperInstr(
											// movq imm(S), D
											"movq " + std::to_string(dynamic_cast<T::ConstExp*>(e1->right)->consti) + "(`s0), `d0",
											new TEMP::TempList(r, nullptr),
											new TEMP::TempList(munchExp(e1->left), nullptr),
											nullptr));
							else {
								TEMP::Temp *lr = munchExp(e1->left);
								emit(new AS::OperInstr(
											"addq `s0, `d0",
											new TEMP::TempList(lr, nullptr),
											new TEMP::TempList(munchExp(e1->right), nullptr),
											nullptr));
								emit(new AS::MoveInstr(
											"movq (`s0) `d0",
											new TEMP::TempList(r, nullptr),
											new TEMP::TempList(lr, nullptr)));
							}
							return r;
						}
					case T::Exp::CONST:
						{
							emit(new AS::OperInstr(
										// movq imm(S), D
										"movq " + std::to_string(dynamic_cast<T::ConstExp*>(e0->exp)->consti) + "(`r0), `d0",
										new TEMP::TempList(r,nullptr),
										nullptr,
										nullptr));
							return r;
						}
					default:
						{
							emit(new AS::OperInstr(
										"movq (`s0), `d0",
										new TEMP::TempList(r, nullptr),
										new TEMP::TempList(munchExp(e0->exp), nullptr),
										nullptr));
							return r;
						}
				}
			}
		case T::Exp::BINOP:
			{
				T::BinopExp *e0 = dynamic_cast<T::BinopExp*>(e);
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
					*rr = munchExp(e0->right);
				emit(new AS::MoveInstr(
							"movq `s0, `d0",
							new TEMP::TempList(r, nullptr),
							new TEMP::TempList(lr, nullptr)));
				emit(new AS::OperInstr(
							// addq S, D
							oper_str + " `s0, `d0",
							new TEMP::TempList(r, nullptr),
							new TEMP::TempList(rr, nullptr),
							nullptr));
				return r;
			}
		case T::Exp::CONST:
			{
				T::ConstExp *e0 = dynamic_cast<T::ConstExp*>(e);
				TEMP::Temp *r = TEMP::Temp::NewTemp();
				emit(new AS::MoveInstr(
							// movq $imm, D
							"movq $" + std::to_string(e0->consti) + ", `d0",
							new TEMP::TempList(r, nullptr),
							nullptr));
				return r;
			}
		case T::Exp::NAME:
			{
				T::NameExp *e0 = dynamic_cast<T::NameExp*>(e);
				TEMP::Temp *r = TEMP::Temp::NewTemp();
				assert(e0->name);
				emit(new AS::MoveInstr(
							// movq $imm, D
							"movq $" + TEMP::LabelString(e0->name) + ", `d0",
							new TEMP::TempList(r, nullptr),
							nullptr));
				return r;
			}
		case T::Exp::TEMP:
			return dynamic_cast<T::TempExp*>(e)->temp;
		case T::Exp::CALL:
			{
				T::CallExp *e0 = dynamic_cast<T::CallExp*>(e);
				assert(e0->fun->kind == T::Exp::NAME);
				T::NameExp *func_name = dynamic_cast<T::NameExp*>(e0->fun);
				TEMP::Temp *r = munchExp(e0->fun);
				int pushs = munchArgs(0, e0->args);
				emit(new AS::OperInstr(
							"call " + func_name->name->Name(),
							nullptr,
							nullptr,
							nullptr));
				if(pushs){
					//std::string inst = "addq $"; inst += pushs * 8; isnt += ", %%rsp";
					emit(new AS::OperInstr(
								"addq $" + std::to_string(pushs * 8) + ", %%rsp",
								nullptr,
								nullptr,
								nullptr));
				}
				return F::RV();
			}
		case T::Exp::ESEQ:
			assert(0);
	}

	// control flow should not be here
	assert(0);
	return nullptr;
}

void munchStm(T::Stm* s){
	switch(s->kind){
		case T::Stm::MOVE:
			{
				T::MoveStm *e0 = dynamic_cast<T::MoveStm*>(s);
				// case MOVE( MEM(
				if(e0->dst->kind == T::Exp::MEM){
					T::MemExp *e1 = dynamic_cast<T::MemExp*>(e0->dst);
					// case MOVE( MEM( BINOP(
					if(e1->exp->kind == T::Exp::BINOP){
						T::BinopExp *e1b = dynamic_cast<T::BinopExp*>(e1->exp);
						// case MOVE( MEM( BINOP( -, CONST() ) ), e2 )
						if(e1b->right->kind == T::Exp::CONST){
							emit(new AS::MoveInstr(
										"movq `s0, " + std::to_string(dynamic_cast<T::ConstExp*>(e1b->right)->consti) + "(`d0)",
										new TEMP::TempList(munchExp(e1b->left), nullptr),
										new TEMP::TempList(munchExp(e0->src), nullptr)));
						}
						// case MOVE( MEM( BINOP( CONST(), - ) ), e2 )
						else if(e1b->left->kind == T::Exp::CONST){
							emit(new AS::MoveInstr(
										"movq `s0, " + std::to_string(dynamic_cast<T::ConstExp*>(e1b->left)->consti) + "(`d0)",
										new TEMP::TempList(munchExp(e1b->right), nullptr),
										new TEMP::TempList(munchExp(e0->src), nullptr)));
						}
					}

					// case MOVE( MEM(), MEM() )
					else if(e0->src->kind == T::Exp::MEM){
						emit(new AS::MoveInstr(
									"movq (`s0), (`d0)",
									new TEMP::TempList(munchExp(e0->dst), nullptr),
									new TEMP::TempList(munchExp(e0->src), nullptr)));
					}
					// case MOVE( MEM( CONST() ), e2 )
					else if(e1->exp->kind == T::Exp::CONST){
						emit(new AS::MoveInstr(
									"moveq `s0, " + std::to_string(dynamic_cast<T::ConstExp*>(e1->exp)->consti) + "(`r0)",
									nullptr,
									new TEMP::TempList(munchExp(e0->src), nullptr)));
					}
					// case MOVE( MEM(e1), e2)
					else {
						emit(new AS::MoveInstr(
									"movq `s0, (`s1)",
									nullptr,
									new TEMP::TempList(munchExp(e0->src),
										new TEMP::TempList(munchExp(e1), nullptr))));
					}
				}
				// case MOVE( TEMP(), e2)
				else if(e0->dst->kind == T::Exp::TEMP){
					emit(new AS::MoveInstr(
								"movq `s0, `d0",
								new TEMP::TempList(dynamic_cast<T::TempExp*>(e0->dst)->temp, nullptr),
								new TEMP::TempList(munchExp(e0->src), nullptr)));
				}
				else assert(0);
			}
			break;
		case T::Stm::LABEL:
			{
				T::LabelStm *e0 = dynamic_cast<T::LabelStm*>(s);
				emit(new AS::LabelInstr(
							TEMP::LabelString(e0->label) + ":\n",
							e0->label));
			}
			break;
		case T::Stm::EXP:
			munchExp(dynamic_cast<T::ExpStm*>(s)->exp);
			break;
		case T::Stm::CJUMP:
			{
				T::CjumpStm *e0 = dynamic_cast<T::CjumpStm*>(s);
				std::string inst;
				switch(e0->op){
					case T::EQ_OP:
						inst = "je `j0";
						break;
					case T::NE_OP:
						inst = "jne `j0";
						break;
					case T::LT_OP:
						inst = "jl `j0";
						break;
					case T::LE_OP:
						inst = "jle `j0";
						break;
					case T::GT_OP:
						inst = "jg `j0";
						break;
					case T::GE_OP:
						inst = "jge `j0";
						break;
					default:
						assert(0);
				}
				emit(new AS::OperInstr(
							"cmp `s1, `s0",
							nullptr,
							new TEMP::TempList(munchExp(e0->left),
								new TEMP::TempList(munchExp(e0->right),nullptr)),
							nullptr));
				emit(new AS::OperInstr(
							inst,
							nullptr,
							nullptr,
							new AS::Targets(new TEMP::LabelList(e0->true_label, nullptr))));
			}
			break;
		case T::Stm::JUMP:
			{
				T::JumpStm *e0 = dynamic_cast<T::JumpStm*>(s);
				emit(new AS::OperInstr(
							"jmp `j0",
							nullptr,
							nullptr,
							new AS::Targets(e0->jumps)));
			}
			break;
		default:
			assert(0);
	}
}


}

namespace CG {

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList) {
	iList = last = new AS::InstrList(nullptr, nullptr);
	for(T::StmList *sl = stmList; sl; sl = sl->tail) munchStm(sl->head);
  return iList;
}

}  // namespace CG
