#include "tiger/regalloc/regalloc.h"

#include <map>

namespace {
	std::map<TEMP::Temp*, int> temp2offset;
	TEMP::TempList *replaceRegs = new TEMP::TempList(F::R12(), new TEMP::TempList(F::R13(), nullptr));

	TEMP::TempList* HasTemp(TEMP::TempList *tl, TEMP::Temp *t){
		for(;tl;tl = tl->tail)if(tl->head == t)return tl;
		return nullptr;
	}

	void ReplaceTemp(TEMP::TempList *tl, TEMP::Temp *target, TEMP::Temp *t){
		TEMP::TempList *p = HasTemp(tl,target);
		assert(p);
		p->head = t;
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
						TEMP::Temp *d0 = instr->Dst()->head, *s0 = instr->Src()->head;
						ReplaceTemp(instr->Dst(), d0, F::R12());
						ReplaceTemp(instr->Src(), s0, F::R13());
						AS::Instr *store = new AS::MoveInstr(
								"movq `s0," + Temp2imm(f, d0) + "(%rsp)",
								nullptr,
								new TEMP::TempList(F::R12(), nullptr));
						AS::Instr *load = new AS::MoveInstr(
								"movq " + Temp2imm(f, s0) + "(%rsp),`d0",
								new TEMP::TempList(F::R13(), nullptr),
								nullptr);

						instrs->head = load;
						instrs->tail = new AS::InstrList(store, instrs->tail);
						instrs->tail = new AS::InstrList(instr, instrs->tail);

						instrs = instrs->tail->tail;
					}
					// d0
					else{
						TEMP::Temp *d0 = instr->Dst()->head;
						ReplaceTemp(instr->Dst(), d0, F::R12());
						AS::Instr *store = new AS::MoveInstr(
								"movq `s0," + Temp2imm(f, d0) + "(%rsp)",
								nullptr,
								new TEMP::TempList(F::R12(), nullptr));
						instrs->tail = new AS::InstrList(store, instrs->tail);

						instrs = instrs->tail;
					}
				}
				else if(instr->Src()){
					// s0 s1
					if(instr->Src()->tail){
						TEMP::Temp *s0 = instr->Src()->head, *s1 = instr->Src()->tail->head;
						ReplaceTemp(instr->Src(), s0, F::R12());
						ReplaceTemp(instr->Src(), s1, F::R13());
						AS::Instr *load0 = new AS::MoveInstr(
								"movq " + Temp2imm(f, s0) + "(%rsp),`d0",
								new TEMP::TempList(F::R12(), nullptr),
								nullptr);
						AS::Instr *load1 = new AS::MoveInstr(
								"movq " + Temp2imm(f, s1) + "(%rsp),`d0",
								new TEMP::TempList(F::R13(), nullptr),
								nullptr);

						instrs->head = load0;
						instrs->tail = new AS::InstrList(load1, new AS::InstrList(instr, instrs->tail));

						instrs = instrs->tail->tail;
					}
					// s0
					else{
						TEMP::Temp *s0 = instr->Src()->head;
						ReplaceTemp(instr->Src(), s0, F::R12());
						AS::Instr *load = new AS::MoveInstr(
								"movq " + Temp2imm(f, s0) + "(%rsp),`d0",
								new TEMP::TempList(F::R12(), nullptr),
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
		temp2offset.clear();

		for(; spills; spills = spills->tail){
			TEMP::Temp* spill_head = spills->head,
				*replace_reg = F::R12();
			
			if(!temp2offset.count(spill_head)){
				F::Access *acc = f->AllocLocal(false);
				temp2offset[spill_head] = dynamic_cast<F::X64Frame*>(f)->size;
			}
			std::string imm = "(" + TEMP::LabelString(f->Name()) + "_framesize-" + std::to_string(temp2offset[spill_head]) + ")";

			for(AS::InstrList *instrs = il; instrs; instrs = instrs->tail){
				AS::Instr *instr = instrs->head;
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

	TEMP::TempList* GrabAllTemp(AS::InstrList *il){
		TEMP::TempList *res = nullptr;
		for(; il; il = il->tail){
			AS::Instr *instr = il->head;
			if(!instr){
				il->head = new AS::MoveInstr("", nullptr, nullptr);
				continue;
			}
			if(instr->kind == AS::Instr::OPER || instr->kind == AS::Instr::MOVE){
				TEMP::TempList *p = instr->Dst();
				for(; p; p = p->tail) res = new TEMP::TempList(p->head, res);
				p = instr->Src();
				for(; p; p = p->tail) res = new TEMP::TempList(p->head, res);
			}
		}
		return res;
	}

} // anonymous namespace

namespace RA {

Result RegAlloc(F::Frame* f, AS::InstrList* il) {
	EasyRewriteProgram(f, il);

  return Result(f->RegAlloc(il), il);
	//Result(TEMP::Map* coloring, AS::InstrList* il): coloring(coloring), il(il){}
}

}  // namespace RA
