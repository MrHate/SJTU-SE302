#include "tiger/liveness/liveness.h"
#include <map>

namespace {
	typedef G::Node<TEMP::Temp> TempNode;
	typedef G::Graph<TEMP::Temp> TempGraph;

	void conflictHardRegs(TempGraph* g, std::map<TEMP::Temp*, TempNode*>& temp2node);
}

namespace LIVE {

LiveGraph Liveness(G::Graph<AS::Instr>* flowgraph) {
	TempGraph *g = new TempGraph();
	MoveList *moves = nullptr;
	std::map<TEMP::Temp*, TempNode*> temp2node;

	conflictHardRegs(g, temp2node);

  return LiveGraph(g, moves);
}

}  // namespace LIVE

namespace {
	void conflictHardRegs(TempGraph* g, std::map<TEMP::Temp*, TempNode*>& temp2node){
		// ensure this func to be called first with emtpy map
		assert(temp2node.empty());
		for(TEMP::TempList *temps = F::HardRegs(); temps; temps = temps->tail){
			TEMP::Temp *t = temps->head;
			TempNode *tnode = g->NewNode(t);
			temp2node[t] = tnode;
		}

		for(TEMP::TempList *temps = F::HardRegs(); temps; temps = temps->tail)
			for(TEMP::TempList *succTemps = temps->tail; succTemps; succTemps = succTemps->tail){
				TempNode *a = temp2node[temps->head], *b = temp2node[succTemps->head];
				g->AddEdge(a,b);
				g->AddEdge(b,a);
			}
	}
}
