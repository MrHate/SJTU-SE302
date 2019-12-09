#include "tiger/regalloc/regalloc.h"

namespace {
	TEMP::TempList* HasTemp(TEMP::TempList *tl, TEMP::Temp *t){
		for(;tl;tl = tl->tail)if(tl->head == t)return tl;
		return nullptr;
	}

	void ReplaceTemp(TEMP::TempList *tl, TEMP::Temp *target, TEMP::Temp *t){
		TEMP::TempList *p = HasTemp(tl,target);
		assert(p);
		p->head = t;
	}

	void RewriteProgram(F::Frame *f, AS::InstrList *il, TEMP::TempList *spills){
		for(; spills; spills = spills->tail){
			TEMP::Temp* spill_head = spills->head,
				*replace_reg = F::R12();
			dynamic_cast<F::X64Frame*>(f)->size += F::X64Frame::wordSize;

			for(AS::InstrList *instrs = il; instrs; instrs = instrs->tail){
				AS::Instr *instr = instrs->head;
				if(instr->kind == AS::Instr::OPER || instr->kind == AS::Instr::MOVE){
					if(HasTemp(instr->Dst(), spill_head)){
						ReplaceTemp(instr->Dst(), spill_head, replace_reg);
						AS::Instr* store = new AS::OperInstr(
								"movq `s0," + TEMP::LabelString(f->Name()) + "(%rsp)",
								nullptr,
								new TEMP::TempList(replace_reg, nullptr),
								nullptr);
						instrs->tail = new AS::InstrList(store, instrs->tail);
					}
					else if(HasTemp(instr->Src(), spill_head)){
						ReplaceTemp(instr->Src(), spill_head, replace_reg);
						AS::Instr* load = new AS::OperInstr(
								"movq " + TEMP::LabelString(f->Name()) + "(%rsp),`d0",
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

}

namespace RA {

Result RegAlloc(F::Frame* f, AS::InstrList* il) {
  // TODO: Put your codes here (lab6).
	RewriteProgram(f, il, GrabAllTemp(il));

  return Result(f->RegAlloc(il), il);
	//Result(TEMP::Map* coloring, AS::InstrList* il): coloring(coloring), il(il){}
}

}  // namespace RA
