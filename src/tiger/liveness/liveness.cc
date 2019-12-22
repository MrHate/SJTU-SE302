#include "tiger/liveness/liveness.h"
#include <map>
#include <set>

namespace {
	typedef G::Node<TEMP::Temp> TempNode;
	typedef G::Node<AS::Instr> InstrNode;
	typedef G::NodeList<TEMP::Temp> TempNodeList;
	typedef G::Graph<TEMP::Temp> TempGraph;

	static std::map<InstrNode*, TEMP::TempList*> node2outTempList, node2inTempList;

	void conflictHardRegs(TempGraph* g, std::map<TEMP::Temp*, TempNode*>& temp2node);
	bool constructGraphFromBottom(G::NodeList<AS::Instr>* instrs);
	bool tempListEquals(TEMP::TempList* a, TEMP::TempList* b);
	bool inTempList(TEMP::TempList* tl, TEMP::Temp* t);
	bool inMoveList(LIVE::MoveList* ml, TempNode* a, TempNode* b);
	TEMP::TempList* spliceTempList(TEMP::TempList* a, TEMP::TempList* b);
	TEMP::TempList* filterTempList(TEMP::TempList* src, TEMP::TempList* target);
}

namespace LIVE {

LiveGraph Liveness(G::Graph<AS::Instr>* flowgraph) {
	TempGraph *g = new TempGraph();
	MoveList *moves = nullptr;

	std::map<TEMP::Temp*, TempNode*> temp2node;
	conflictHardRegs(g, temp2node);

	node2outTempList.clear();
	node2inTempList.clear();
	while(!constructGraphFromBottom(flowgraph->Nodes()));

	for(std::map<InstrNode*, TEMP::TempList*>::iterator i = node2inTempList.begin(); i != node2inTempList.end(); i++)
		for(TEMP::TempList *temps = i->second; temps; temps = temps->tail)
			if(!temp2node.count(temps->head))
				temp2node[temps->head] = g->NewNode(temps->head);
	for(std::map<InstrNode*, TEMP::TempList*>::iterator i = node2outTempList.begin(); i != node2outTempList.end(); i++)
		for(TEMP::TempList *temps = i->second; temps; temps = temps->tail)
			if(!temp2node.count(temps->head))
				temp2node[temps->head] = g->NewNode(temps->head);

	for(G::NodeList<AS::Instr> *instrNodes = flowgraph->Nodes(); instrNodes; instrNodes = instrNodes->tail){
		InstrNode *curNode = instrNodes->head;
		for(TEMP::TempList *defs = FG::Def(curNode); defs; defs = defs->tail){
			TempNode *a = temp2node[defs->head];
			for(TEMP::TempList *outs = node2outTempList[curNode]; outs; outs = outs->tail){
				TempNode *b = temp2node[outs->head];
				if(a != b && !b->Adj()->InNodeList(a) && (!FG::IsMove(curNode) || inTempList(FG::Use(curNode), outs->head))){
					g->AddEdge(a, b);
					g->AddEdge(b, a);
				}
			}
			if(FG::IsMove(curNode))
				for(TEMP::TempList *uses = FG::Use(curNode); uses; uses = uses->tail){
					TempNode *b = temp2node[uses->head];
					if(b != a && inMoveList(moves, b, a))
						moves = new MoveList(b, a, moves);
				}
		}
	}


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

	bool constructGraphFromBottom(G::NodeList<AS::Instr>* instrs){
		if(!instrs) return true;
		bool flag = constructGraphFromBottom(instrs->tail);

		InstrNode *instr = instrs->head;
		TEMP::TempList *ins = nullptr,
			*outs = nullptr;
		for(G::NodeList<AS::Instr> *succs = instr->Succ(); succs; succs = succs->tail)
			outs = spliceTempList(outs, node2inTempList[succs->head]);

		ins = spliceTempList(FG::Use(instr), filterTempList(outs, FG::Def(instr)));

		bool unchanged = tempListEquals(node2inTempList[instr], ins)
			&& tempListEquals(node2outTempList[instr], outs);

		node2inTempList[instr] = ins;
		node2outTempList[instr] = outs;

		return flag && unchanged;
	}

	bool tempListEquals(TEMP::TempList* a, TEMP::TempList* b){
		for(; a && b; a = a->tail, b = b->tail)
			if(a->head != b->head) return false;
		if(a || b) return false;
		return true;
	}

	TEMP::TempList* spliceTempList(TEMP::TempList* a, TEMP::TempList* b){
		TEMP::TempList *res = nullptr;
		std::set<TEMP::Temp*> ts;
		for(TEMP::TempList *temps = a; temps; temps = temps->tail){
			TEMP::Temp* t = temps->head;
			if(!ts.count(t)){
				ts.insert(t);
				res = new TEMP::TempList(t, res);
			}
		}
		for(TEMP::TempList *temps = b; temps; temps = temps->tail){
			TEMP::Temp* t = temps->head;
			if(!ts.count(t)){
				ts.insert(t);
				res = new TEMP::TempList(t, res);
			}
		}
		return res;
	}

	TEMP::TempList* filterTempList(TEMP::TempList* src, TEMP::TempList* targets){
		TEMP::TempList *res = nullptr;
		std::set<TEMP::Temp*> ts;
		for(TEMP::TempList *temps = targets; temps; temps = temps->tail)
			ts.insert(temps->head);
		for(TEMP::TempList *temps = src; temps; temps = temps->tail)
			if(!ts.count(temps->head))
				res = new TEMP::TempList(temps->head, res);
		return res;
	}

	bool inTempList(TEMP::TempList* tl, TEMP::Temp* t){
		for(; tl; tl = tl->tail)
			if(tl->head == t)return true;
		return false;
	}
	
	bool inMoveList(LIVE::MoveList* ml, TempNode* a, TempNode* b){
		for(; ml; ml = ml->tail)
			if(ml->src == a && ml->dst == b) return true;
		return false;
	}

}
