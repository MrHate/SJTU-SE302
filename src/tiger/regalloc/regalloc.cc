#include "tiger/regalloc/color.h"
#include "tiger/regalloc/regalloc.h"
#include "tiger/liveness/flowgraph.h"

#include <map>

namespace {
	std::map<TEMP::Temp*, int> temp2offset;
	TEMP::TempList *replaceRegs = new TEMP::TempList(F::R12(), new TEMP::TempList(F::R13(), nullptr));

	TEMP::TempList* HasTemp(TEMP::TempList *tl, TEMP::Temp *t){
		for(;tl;tl = tl->tail)if(tl->head == t)return tl;
		return nullptr;
	}

	TEMP::Temp* ReplaceTemp(TEMP::TempList *tl, TEMP::Temp *target, TEMP::Temp *t){
		TEMP::TempList *p = HasTemp(tl,target);
		assert(p);
		//if(target == F::RAX() || target == F::RDI() || target == F::RSI() || target == F::RCX() || target == F::RDX() || target == F::R8() || target == F::R9())return target;
		p->head = t;
		return t;
	}

	std::string Temp2imm(F::Frame *f, TEMP::Temp *t){
		if(!temp2offset.count(t)){
			F::Access *acc = f->AllocLocal(false);
			temp2offset[t] = dynamic_cast<F::X64Frame*>(f)->size;
		}
		std::string imm = "(" + TEMP::LabelString(f->Name()) + "_framesize-" + std::to_string(temp2offset[t]) + ")";
		return imm;
	}


	void EasyRewriteProgram(F::Frame *f, AS::InstrList *il){
		temp2offset.clear();

		for(AS::InstrList *instrs = il; instrs; instrs = instrs->tail){
			TEMP::TempList *regPtr = replaceRegs;
			TEMP::Temp *targetReg = nullptr;
			AS::Instr *instr = instrs->head;
			if(!instr){ continue; }
			if(instr->kind == AS::Instr::OPER || instr->kind == AS::Instr::MOVE){
				if(instr->Dst()){
					// d0 d1
					if(instr->Dst()->tail){assert(0);}
					// d0 s0
					else if(instr->Src()){
						TEMP::Temp *d0 = instr->Dst()->head, *s0 = instr->Src()->head,
							*nd0 = ReplaceTemp(instr->Dst(), d0, F::R12()),
							*ns0 = ReplaceTemp(instr->Src(), s0, F::R13());

						if(d0 != nd0 && s0 != ns0){
							AS::Instr *store = new AS::MoveInstr(
									"movq `s0," + Temp2imm(f, d0) + "(%rsp)",
									nullptr,
									new TEMP::TempList(nd0, nullptr));
							AS::Instr *load = new AS::MoveInstr(
									"movq " + Temp2imm(f, s0) + "(%rsp),`d0",
									new TEMP::TempList(ns0, nullptr),
									nullptr);

							instrs->head = load;
							instrs->tail = new AS::InstrList(store, instrs->tail);
							instrs->tail = new AS::InstrList(instr, instrs->tail);

							instrs = instrs->tail->tail;
						}
						else if(d0 == nd0 && s0 != ns0){
							AS::Instr *load = new AS::MoveInstr(
									"movq " + Temp2imm(f, s0) + "(%rsp),`d0",
									new TEMP::TempList(ns0, nullptr),
									nullptr);

							instrs->head = load;
							instrs->tail = new AS::InstrList(instr, instrs->tail);

							instrs = instrs->tail;
						}
						else if(d0 != nd0 && s0 == ns0){
							AS::Instr *store = new AS::MoveInstr(
									"movq `s0," + Temp2imm(f, d0) + "(%rsp)",
									nullptr,
									new TEMP::TempList(nd0, nullptr));

							instrs->tail = new AS::InstrList(store, instrs->tail);

							instrs = instrs->tail;
						}
					}
					// d0
					else{
						TEMP::Temp *d0 = instr->Dst()->head;
						TEMP::Temp *nd0 = ReplaceTemp(instr->Dst(), d0, F::R12());
						AS::Instr *store = new AS::MoveInstr(
								"movq `s0," + Temp2imm(f, d0) + "(%rsp)",
								nullptr,
								new TEMP::TempList(nd0, nullptr));
						instrs->tail = new AS::InstrList(store, instrs->tail);

						instrs = instrs->tail;
					}
				}
				else if(instr->Src()){
					// s0 s1
					if(instr->Src()->tail){
						TEMP::Temp *s0 = instr->Src()->head, *s1 = instr->Src()->tail->head,
							*ns0 = ReplaceTemp(instr->Src(), s0, F::R12()),
							*ns1 = ReplaceTemp(instr->Src(), s1, F::R13());

						if(s0 != ns0 && s1 != ns1){
							AS::Instr *load0 = new AS::MoveInstr(
									"movq " + Temp2imm(f, s0) + "(%rsp),`d0",
									new TEMP::TempList(ns0, nullptr),
									nullptr);
							AS::Instr *load1 = new AS::MoveInstr(
									"movq " + Temp2imm(f, s1) + "(%rsp),`d0",
									new TEMP::TempList(ns1, nullptr),
									nullptr);

							instrs->head = load0;
							instrs->tail = new AS::InstrList(load1, new AS::InstrList(instr, instrs->tail));

							instrs = instrs->tail->tail;

						}
						else if(s0 != ns0 && s1 == ns1){
							AS::Instr *load0 = new AS::MoveInstr(
									"movq " + Temp2imm(f, s0) + "(%rsp),`d0",
									new TEMP::TempList(ns0, nullptr),
									nullptr);
							instrs->head = load0;
							instrs->tail = new AS::InstrList(instr, instrs->tail);

							instrs = instrs->tail;
						}
						else if(s0 == ns0 && s1 != ns1){
							AS::Instr *load1 = new AS::MoveInstr(
									"movq " + Temp2imm(f, s1) + "(%rsp),`d0",
									new TEMP::TempList(ns1, nullptr),
									nullptr);
							instrs->head = load1;
							instrs->tail = new AS::InstrList(instr, instrs->tail);

							instrs = instrs->tail;
						}

					}
					// s0
					else{
						TEMP::Temp *s0 = instr->Src()->head;
						TEMP::Temp *ns0 = ReplaceTemp(instr->Src(), s0, F::R12());
						AS::Instr *load = new AS::MoveInstr(
								"movq " + Temp2imm(f, s0) + "(%rsp),`d0",
								new TEMP::TempList(ns0, nullptr),
								nullptr);

						instrs->head = load;
						instrs->tail = new AS::InstrList(instr, instrs->tail);

						instrs = instrs->tail;
					}
				}
			}
		}
	}

	void RewriteProgram(F::Frame *f, AS::InstrList *il, TEMP::TempList *spills){
		//temp2offset.clear();

		for(; spills; spills = spills->tail){
			TEMP::Temp* spill_head = spills->head,
				*replace_reg = TEMP::Temp::NewTemp();
			//f->AllocLocal(false);
			
			if(!temp2offset.count(spill_head)){
				F::Access *acc = f->AllocLocal(true);
				temp2offset[spill_head] = dynamic_cast<F::X64Frame*>(f)->size;
			}
			std::string imm = "(" + TEMP::LabelString(f->Name()) + "_framesize-" + std::to_string(temp2offset[spill_head]) + ")";

			for(AS::InstrList *instrs = il; instrs; instrs = instrs->tail){
				AS::Instr *instr = instrs->head;
				if(!instr) continue;
				if(instr->kind == AS::Instr::OPER || instr->kind == AS::Instr::MOVE){
					if(HasTemp(instr->Dst(), spill_head)){
						ReplaceTemp(instr->Dst(), spill_head, replace_reg);
						AS::Instr* store = new AS::OperInstr(
								"movq `s0," + imm + "(%rsp)",
								nullptr,
								new TEMP::TempList(replace_reg, nullptr),
								nullptr);
						instrs->tail = new AS::InstrList(store, instrs->tail);
					}
					else if(HasTemp(instr->Src(), spill_head)){
						ReplaceTemp(instr->Src(), spill_head, replace_reg);
						AS::Instr* load = new AS::OperInstr(
								"movq " + imm + "(%rsp),`d0",
								new TEMP::TempList(replace_reg, nullptr),
								nullptr,
								nullptr);
						instrs->head = load;
						instrs->tail = new AS::InstrList(instr, instrs->tail);
					}
				}
			}
		}
	}

	void showFlowGraph(AS::Instr* inst){
		inst->Print(stderr, nullptr);
	}
	void showLiveness(TEMP::Temp* t){
		std::string regName = "";
		if(t == F::RAX()) regName = "%rax";
		else if(t == F::RBX()) regName = "%rbx";
		else if(t == F::RCX()) regName = "%rcx";
		else if(t == F::RDX()) regName = "%rdx";
		else if(t == F::RSI()) regName = "%rsi";
		else if(t == F::RDI()) regName = "%rdi";
		else if(t == F::RBP()) regName = "%rbp";
		else if(t == F::R8())  regName = "%r8";
		else if(t == F::R9())  regName = "%r9";
		else if(t == F::R10()) regName = "%r10";
		else if(t == F::R11()) regName = "%r11";
		else if(t == F::R12()) regName = "%r12";
		else if(t == F::R13()) regName = "%r13";
		else if(t == F::R14()) regName = "%r14";
		else if(t == F::R15()) regName = "%r15";

		if(regName == "")
			fprintf(stderr, "t: %d\n", t->Int());
		else 
			fprintf(stderr, "t: %s: %d\n", regName.c_str(), t->Int());
	}

} // anonymous namespace

namespace RA {

Result RegAlloc(F::Frame* f, AS::InstrList* il) {
	//EasyRewriteProgram(f, il);

	COL::Result cr;
	// TODO: comment the line below
	cr.coloring = f->RegAlloc(il);

	while(true) {
		G::Graph<AS::Instr>* fg = FG::AssemFlowGraph(il, f);
		//G::Graph<AS::Instr>::Show(stderr, fg->Nodes(), showFlowGraph);

		LIVE::LiveGraph lg = LIVE::Liveness(fg);
		//G::Graph<TEMP::Temp>::Show(stderr, lg.graph->Nodes(), showLiveness);

		cr = COL::Color(lg.graph, f->RegAlloc(il), F::HardRegs(), lg.moves);
		if(cr.spills == nullptr)break;
		RewriteProgram(f, il, cr.spills);
	}
  return Result(cr.coloring, il);
}

}  // namespace RA
