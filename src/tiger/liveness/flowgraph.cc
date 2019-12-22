#include "tiger/liveness/flowgraph.h"
#include <map>

namespace FG {

TEMP::TempList* Def(G::Node<AS::Instr>* n) {
  return n->NodeInfo()->Dst();
}

TEMP::TempList* Use(G::Node<AS::Instr>* n) {
  return n->NodeInfo()->Src();
}

bool IsMove(G::Node<AS::Instr>* n) {
  return n->NodeInfo()->kind == AS::Instr::MOVE;
}

G::Graph<AS::Instr>* AssemFlowGraph(AS::InstrList* il, F::Frame* f) {
	G::Graph<AS::Instr> *g = new G::Graph<AS::Instr>();
	G::Node<AS::Instr> *prevInstr = nullptr;
	std::map<TEMP::Label*, G::Node<AS::Instr>*> label2Instr;

	for(AS::InstrList *i = il; i; i = i->tail){
		AS::Instr *instr = i->head;
		G::Node<AS::Instr> *node = g->NewNode(instr);
		switch(instr->kind){
			case AS::Instr::OPER:
				if(prevInstr) g->AddEdge(prevInstr, node);
				if(dynamic_cast<AS::OperInstr*>(instr)->assem.substr(0,3) == "jmp") prevInstr = nullptr;
				else prevInstr = node;
				break;
			case AS::Instr::MOVE:
				if(prevInstr) g->AddEdge(prevInstr, node);
				prevInstr = node;
				break;
			case AS::Instr::LABEL:
				label2Instr[dynamic_cast<AS::LabelInstr*>(instr)->label] = node;
				break;

			default: assert(0);
		}
	}

	for(G::NodeList<AS::Instr> *nodes = g->Nodes(); nodes; nodes = nodes->tail){
		AS::Instr *instr = nodes->head->NodeInfo();
		if(instr->kind == AS::Instr::OPER){
			AS::OperInstr *opInstr = dynamic_cast<AS::OperInstr*>(instr);
			if(opInstr->jumps != nullptr)
				for(TEMP::LabelList *targets = opInstr->jumps->labels; targets; targets = targets->tail)
					g->AddEdge(nodes->head, label2Instr[targets->head]);
		}
	}

  return g;
}

}  // namespace FG
