#include "tiger/codegen/codegen.h"

namespace {

AS::InstrList *iList = nullptr, *last = nullptr;
std::string fs = "";

inline TEMP::TempList* TL(TEMP::Temp *t, TEMP::TempList *tail){
	return new TEMP::TempList(t, tail);
}

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
	//fprintf(stderr, "munchArgs...\n");
	int pushs = 0;
	if(reg && args){
		TEMP::TempList *p_reg = argregs;
		for(int i=0;i<6 && args;i++){
			std::string strReg = "%rdi";
			switch(i){
				case 0: strReg = "%rdi"; break;
				case 1: strReg = "%rsi"; break;
				case 2: strReg = "%rcx"; break;
				case 3: strReg = "%rdx"; break;
				case 4: strReg = "%r8"; break;
				case 5: strReg = "%r9"; break;
				default: assert(0);
			}
			emit(new AS::MoveInstr( "movq `s0," + strReg, nullptr, TL(munchExp(args->head), nullptr)));
			//emit(new AS::MoveInstr( "movq `s0, `d0", TL(p_reg->head, nullptr), TL(munchExp(args->head), nullptr)));
			args = args->tail;
			p_reg = p_reg->tail;
		}
	}else if(args){
		pushs = munchArgs(args->tail, false) + 1;
		//fprintf(stderr, "push args: %d\n", pushs);
		emit(new AS::OperInstr( "pushq `s0", nullptr, TL(munchExp(args->head), nullptr), new AS::Targets(nullptr)));
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
											"movq " + std::to_string(dynamic_cast<T::ConstExp*>(e1->left)->consti) + "(`s0), `d0",
											TL(r, nullptr),
											TL(munchExp(e1->right), nullptr),
											new AS::Targets(nullptr)));
							else if(e1->right->kind == T::Exp::CONST)
								emit(new AS::OperInstr(
											// movq imm(S), D
											"movq " + std::to_string(dynamic_cast<T::ConstExp*>(e1->right)->consti) + "(`s0), `d0",
											TL(r, nullptr),
											TL(munchExp(e1->left), nullptr),
											new AS::Targets(nullptr)));
							else {
								emit(new AS::MoveInstr(
											"movq `s0, %r14 # line 90",
											nullptr,
											TL(munchExp(e1->left), nullptr)));
								emit(new AS::OperInstr(
											"addq `s0, %r14 # line 94",
											nullptr,
											TL(munchExp(e1->right), nullptr),
											new AS::Targets(nullptr)));
								emit(new AS::MoveInstr(
											"movq (%r14),`d0 # line 99",
											TL(r, nullptr),
											nullptr));
							}
							return r;
						}
					case T::Exp::CONST:
						{
							emit(new AS::OperInstr( "movq " + std::to_string(dynamic_cast<T::ConstExp*>(e0->exp)->consti) + "(`r0), `d0", TL(r,nullptr), nullptr, new AS::Targets(nullptr)));
							return r;
						}
					default:
						{
							emit(new AS::OperInstr( "movq `s0, `d0 # line112", TL(r, nullptr), TL(munchExp(e0->exp), nullptr), new AS::Targets(nullptr)));
							return r;
						}
				}
			}
		case T::Exp::BINOP:
			{
				T::BinopExp *e0 = dynamic_cast<T::BinopExp*>(e);
				TEMP::Temp *r = TEMP::Temp::NewTemp();
				TEMP::Temp *lr = munchExp(e0->left),
					*rr = munchExp(e0->right);
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
						emit(new AS::MoveInstr("movq `s0,`d0", TL(F::RAX(), nullptr), TL(lr, nullptr)));
						emit(new AS::OperInstr("cqto", nullptr, nullptr, new AS::Targets(nullptr)));
						emit(new AS::OperInstr("idivq `s0", nullptr, TL(rr, nullptr), new AS::Targets(nullptr)));
						emit(new AS::MoveInstr("movq `s0,`d0", TL(r, nullptr), TL(F::RAX(), nullptr)));
						return r;
					default:
						assert(0);
				}
				emit(new AS::MoveInstr( "movq `s0,`d0", TL(F::RAX(), nullptr), TL(lr, nullptr)));
				emit(new AS::OperInstr( oper_str + " `s0,`d0", TL(F::RAX(), nullptr), TL(rr, nullptr), new AS::Targets(nullptr)));
				emit(new AS::MoveInstr( "movq `s0,`d0", TL(r,nullptr), TL(F::RAX(), nullptr)));
				//emit(new AS::MoveInstr(
				//      "movq `s0, `d0",
				//      new TEMP::TempList(r, nullptr),
				//      new TEMP::TempList(lr, nullptr)));
				//emit(new AS::OperInstr(
				//      // addq S, D
				//      oper_str + " `s0, `d0",
				//      new TEMP::TempList(r, nullptr),
				//      new TEMP::TempList(rr, nullptr),
				//      nullptr));
				return r;
			}
		case T::Exp::CONST:
			{
				T::ConstExp *e0 = dynamic_cast<T::ConstExp*>(e);
				TEMP::Temp *r = TEMP::Temp::NewTemp();
				emit(new AS::MoveInstr( "movq $" + std::to_string(e0->consti) + ", `d0", TL(r, nullptr), nullptr));
				return r;
			}
		case T::Exp::NAME:
			{
				T::NameExp *e0 = dynamic_cast<T::NameExp*>(e);
				TEMP::Temp *r = TEMP::Temp::NewTemp();
				assert(e0->name);
				//emit(new AS::MoveInstr( "movq $" + TEMP::LabelString(e0->name) + ", `d0", TL(r, nullptr), nullptr));
				std::string inst = "leaq " + e0->name->Name() + "(%rip),`d0";
				emit(new AS::OperInstr(inst, TL(r,nullptr), nullptr, new AS::Targets(nullptr)));
				return r;
			}
		case T::Exp::TEMP:
			{
				T::TempExp *e0 = dynamic_cast<T::TempExp*>(e);
				if (e0->temp == F::FP()) {
					TEMP::Temp *r = TEMP::Temp::NewTemp();
					std::string instr = "leaq " + fs + "(%rsp),`d0";
					emit(new AS::OperInstr(instr, TL(r, nullptr), nullptr, new AS::Targets(nullptr)));
					return r;
				}
				else return e0->temp;
			}
		case T::Exp::CALL:
			{
				T::CallExp *e0 = dynamic_cast<T::CallExp*>(e);
				assert(e0->fun->kind == T::Exp::NAME);
				T::NameExp *func_name = dynamic_cast<T::NameExp*>(e0->fun);
				TEMP::Temp *r = TEMP::Temp::NewTemp();
				//TEMP::Temp *r = munchExp(e0->fun);
				int pushs = munchArgs(e0->args, true);
				emit(new AS::OperInstr( "call " + func_name->name->Name() + "@PLT", nullptr, nullptr, new AS::Targets(nullptr)));
				if(pushs){
					//std::string inst = "addq $"; inst += pushs * 8; isnt += ", %%rsp";
					emit(new AS::OperInstr( "addq $" + std::to_string(pushs * 8) + ", %rsp", nullptr, nullptr, new AS::Targets(nullptr)));
				}
				emit(new AS::MoveInstr("movq %rax,`d0", TL(r, nullptr), nullptr));
				return r;
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
						assert(e1b->op == T::PLUS_OP);
						// case MOVE( MEM( BINOP( -, CONST() ) ), e2 )
						if(e1b->right->kind == T::Exp::CONST){
							emit(new AS::MoveInstr( "movq `s0, " + std::to_string(dynamic_cast<T::ConstExp*>(e1b->right)->consti) + "(`s1)", nullptr, TL(munchExp(e0->src), TL(munchExp(e1b->left), nullptr))));
						}
						// case MOVE( MEM( BINOP( CONST(), - ) ), e2 )
						else if(e1b->left->kind == T::Exp::CONST){
							emit(new AS::MoveInstr( "movq `s0, " + std::to_string(dynamic_cast<T::ConstExp*>(e1b->left)->consti) + "(`s1)", nullptr, TL(munchExp(e0->src), TL(munchExp(e1b->right), nullptr))));
						}
						else{
							emit(new AS::MoveInstr("movq `s0,(`s1)", nullptr, TL(munchExp(e0->src), TL(munchExp(e1->exp), nullptr))));
						}
									
					}

					// case MOVE( MEM(), MEM() )
					else if(e0->src->kind == T::Exp::MEM){
						emit(new AS::MoveInstr( "movq (`s0), (`s1)", nullptr, TL(munchExp(e0->src), TL(munchExp(e0->dst), nullptr))));
					}
					// case MOVE( MEM( CONST() ), e2 )
					else if(e1->exp->kind == T::Exp::CONST){
						emit(new AS::MoveInstr( "movq `s0, " + std::to_string(dynamic_cast<T::ConstExp*>(e1->exp)->consti) + "(`r0)", nullptr, TL(munchExp(e0->src), nullptr)));
					}
					// case MOVE( MEM(e1), e2)
					else {
						emit(new AS::MoveInstr( "movq `s0, (`s1)", nullptr, TL(munchExp(e0->src), TL(munchExp(e1), nullptr))));
					}
				}
				// case MOVE( TEMP(), e2)
				else if(e0->dst->kind == T::Exp::TEMP){
					emit(new AS::MoveInstr( "movq `s0, `d0", TL(dynamic_cast<T::TempExp*>(e0->dst)->temp, nullptr), TL(munchExp(e0->src), nullptr)));
				}
				else assert(0);
			}
			break;
		case T::Stm::LABEL:
			{
				T::LabelStm *e0 = dynamic_cast<T::LabelStm*>(s);
				//fprintf(stderr, "[label]%s\n",TEMP::LabelString(e0->label).c_str());
				emit(new AS::LabelInstr( TEMP::LabelString(e0->label), e0->label));
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
				emit(new AS::OperInstr( "cmp `s1, `s0", nullptr, TL(munchExp(e0->left), TL(munchExp(e0->right),nullptr)), new AS::Targets(nullptr)));
				emit(new AS::OperInstr( inst, nullptr, nullptr, new AS::Targets(new TEMP::LabelList(e0->true_label, nullptr))));
			}
			break;
		case T::Stm::JUMP:
			{
				T::JumpStm *e0 = dynamic_cast<T::JumpStm*>(s);
				emit(new AS::OperInstr( "jmp `j0", nullptr, nullptr, new AS::Targets(e0->jumps)));
			}
			break;
		default:
			assert(0);
	}
}


}

namespace CG {

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList) {
	fs = f->Name()->Name() + "_framesize";
	iList = last = new AS::InstrList(nullptr, nullptr);
	for(T::StmList *sl = stmList; sl; sl = sl->tail) munchStm(sl->head);
  return iList;
}

}  // namespace CG
