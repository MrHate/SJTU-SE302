#include "tiger/regalloc/color.h"
#include <map>
#include <set>

namespace {
	typedef G::Node<TEMP::Temp> TempNode;
	typedef G::NodeList<TEMP::Temp> TempNodeList;
	typedef G::Graph<TEMP::Temp> TempGraph;

	void build(TempGraph* ig);
	void makeWorkList(TempGraph* ig);
	void simplify();
	void coalesce();
	void freeze();
	void freezeMoves(TempNode* u);
	void selectSpill();
	TempNodeList* adjacent(TempNode* n);
	void addEdge(TempNode* n1, TempNode* n2);
	LIVE::MoveList* nodeMoves(TempNode* n);
	bool moveRelated(TempNode* n);
	void decrementDegree(TempNode* n);
	void enableMoves(TempNodeList* nodes);
	void addWorkList(TempNode* n);
	bool OK(TempNode* t, TempNode* r);
	bool conservative(TempNodeList* nodes);
	TempNode* getAlias(TempNode* n);
	void combine(TempNode* u, TempNode* v);
	void assignColors();

	std::string temp2reg(TEMP::Temp* t);
	bool inTempList(TEMP::TempList* tl, TEMP::Temp* t);
	LIVE::MoveList* catMoveList(LIVE::MoveList* a, LIVE::MoveList* b);
	bool inMoveList(LIVE::MoveList* moves, TempNode* src, TempNode* dst);
	LIVE::MoveList* deleteFromMoveList(LIVE::MoveList* moves, TempNode* src, TempNode* dst);
	bool precolored(TempNode* n);
	bool briggs(TempNode* u, TempNode* v);

	const int K = 15;
	TempNodeList *simplifyWorkList = nullptr,
							 *freezeWorkList = nullptr,
							 *spillWorkList = nullptr,
							 *spilledNodes = nullptr,
							 *coloredNodes = nullptr,
							 *coalescedNodes = nullptr,
							 *selectStack = nullptr;
	std::map<TempNode*, TempNode*> alias;
	std::map<TempNode*, int> degree, color;
	LIVE::MoveList *activeMoves = nullptr,
								 *workListMoves = nullptr,
								 *frozenMoves = nullptr,
								 *coalescedMoves = nullptr,
								 *constrainedMoves = nullptr;
}

namespace COL {

Result Color(G::Graph<TEMP::Temp>* ig, TEMP::Map* initial, TEMP::TempList* regs,
             LIVE::MoveList* moves) {
  // TODO: Put your codes here (lab6).
  return Result();
}

}  // namespace COL

namespace {
	void build(TempGraph* ig){
		// init degrees
		for(TempNodeList *nodes = ig->Nodes(); nodes; nodes = nodes->tail){
			TempNode *n = nodes->head;
			int curDegree = 0;
			for(TempNodeList *succs = n->Succ(); succs; succs = succs->tail) ++curDegree;
			degree[n] = curDegree;
		}
	}
	void makeWorkList(TempGraph* ig){
		for(TempNodeList *nodes = ig->Nodes(); nodes; nodes = nodes->tail){
			TempNode *n = nodes->head;
			if(color.count(n)){
				if(degree[n] >= K)
					spillWorkList = new TempNodeList(n, spillWorkList);
				else if(moveRelated(n))
					freezeWorkList = new TempNodeList(n, freezeWorkList);
				else
					simplifyWorkList = new TempNodeList(n, simplifyWorkList);
			}
		}
	}
	void simplify(){
		TempNode *n = simplifyWorkList->head;
		simplifyWorkList = simplifyWorkList->tail;
		selectStack = new TempNodeList(n, selectStack);
		for(TempNodeList *m = adjacent(n); m; m = m->tail)
			decrementDegree(m->head);
	}
	void coalesce(){
		TempNode *u, *v,
						 *x = workListMoves->src,
						 *y = workListMoves->dst;

		if(precolored(getAlias(y))){
			u = getAlias(y);
			v = getAlias(x);
		}
		else {
			u = getAlias(x);
			v = getAlias(y);
		}
		workListMoves = workListMoves->tail;
		if(u == v){
			coalescedMoves = new LIVE::MoveList(x, y, coalescedMoves);
			addWorkList(u);
		}
		else if(precolored(v) || v->Adj()->InNodeList(u)){
			constrainedMoves = new LIVE::MoveList(x, y, constrainedMoves);
			addWorkList(u);
			addWorkList(v);
		}
		else if(precolored(u) && OK(v, u)){
			coalescedMoves = new LIVE::MoveList(x, y, coalescedMoves);
			combine(u, v);
			addWorkList(u);
		}
		else if(!precolored(u) && briggs(u, v)){
			coalescedMoves = new LIVE::MoveList(x, y, coalescedMoves);
			combine(u, v);
			addWorkList(u);
		}
		else {
			activeMoves = new LIVE::MoveList(x, y, activeMoves);
		}
	}
	void freeze(){
		TempNode *n = freezeWorkList->head;
		freezeWorkList = freezeWorkList->tail;
		simplifyWorkList = new TempNodeList(n, simplifyWorkList);
		freezeMoves(n);
	}
	void freezeMoves(TempNode* u){
		for(LIVE::MoveList *moves = nodeMoves(u); moves; moves = moves->tail){
			TempNode *src = moves->src, *dst = moves->dst, *v;
			if(getAlias(src) == getAlias(u)) v = getAlias(src);
			else v = getAlias(dst);
			activeMoves = deleteFromMoveList(activeMoves, src, dst);
			frozenMoves = new LIVE::MoveList(src, dst, frozenMoves);
			if(nodeMoves(v) == nullptr && degree[v] < K){
				freezeWorkList = TempNodeList::DeleteNode(v, freezeWorkList);
				simplifyWorkList = new TempNodeList(v, simplifyWorkList);
			}
		}
	}
	void selectSpill(){
		TempNode *m = nullptr;
		int max = 0;
		for(TempNodeList *p = spillWorkList; p; p = p->tail){
			TempNode *n = p->head;
			TEMP::Temp *t = n->NodeInfo();
			if(degree[n] > max){
				max = degree[n];
				m = n;
			}
		}

		if(m != nullptr){
			spillWorkList = TempNodeList::DeleteNode(m, spillWorkList);
			simplifyWorkList = new TempNodeList(m, simplifyWorkList);
			freezeMoves(m);
		}
		else
			assert(0);
	}
	TempNodeList* adjacent(TempNode* n){
		TempNodeList *res = n->Succ();
		for(TempNodeList *nodes = selectStack; nodes; nodes = nodes->tail)
			res = TempNodeList::DeleteNode(nodes->head, res);
		for(TempNodeList *nodes = coalescedNodes; nodes; nodes = nodes->tail)
			res = TempNodeList::DeleteNode(nodes->head, res);
		return res;
	}
	void addEdge(TempNode* u, TempNode* v){
		if(!v->Adj()->InNodeList(u) && u != v){
			if(!precolored(u)){
				++ degree[u];
				TempGraph::AddEdge(u, v);
			}
			if(!precolored(v)){
				++ degree[v];
				TempGraph::AddEdge(v, u);
			}
		}
	}
	LIVE::MoveList* nodeMoves(TempNode* n){
		LIVE::MoveList *moves = nullptr;
		TempNode *m = getAlias(n);
		for(LIVE::MoveList *p = catMoveList(activeMoves, workListMoves); p; p = p->tail)
			if(getAlias(p->src) == m || getAlias(p->dst) == m)
				moves = new LIVE::MoveList(p->src, p->dst, moves);
		return moves;
	}
	bool moveRelated(TempNode* n){
		return nodeMoves(n) != nullptr;
	}
	void decrementDegree(TempNode* m){
		int d = degree[m];
		degree[m] = d - 1;
		if(d == K){
			enableMoves(new TempNodeList(m, adjacent(m)));
			spillWorkList = TempNodeList::DeleteNode(m, spillWorkList);
			if(moveRelated(m)) freezeWorkList = new TempNodeList(m, freezeWorkList);
			else simplifyWorkList = new TempNodeList(m, simplifyWorkList);
		}
	}
	void enableMoves(TempNodeList* nodes){
		for(; nodes; nodes = nodes->tail){
			TempNode *n = nodes->head;
			for(LIVE::MoveList *m = nodeMoves(n); m; m = m->tail)
				if(inMoveList(activeMoves, m->src, m->dst)){
					activeMoves = deleteFromMoveList(activeMoves, m->src, m->dst);
					workListMoves = new LIVE::MoveList(m->src, m->dst, workListMoves);
				}
		}
	}
	void addWorkList(TempNode* u){
		if(!precolored(u) && !moveRelated(u) && degree[u] < K){
			freezeWorkList = TempNodeList::DeleteNode(u, freezeWorkList);
			simplifyWorkList = new TempNodeList(u, simplifyWorkList);
		}
	}
	bool OK(TempNode* t, TempNode* r){
		for(TempNodeList *p = adjacent(t); p; p = p->tail){
			TempNode *n = p->head;
			if(degree[n] < K && precolored(n) || precolored(n) || r->Adj()->InNodeList(n)) continue;
			return false;
		}
		return true;
	}
	bool conservative(TempNodeList* nodes){
		int k = 0;
		for(; nodes; nodes = nodes->tail){
			TempNode *n = nodes->head;
			if(degree[n] >= K) ++k;
		}
		return k < K;
	}
	TempNode* getAlias(TempNode* n){
		if(alias.count(n))
			return getAlias(alias[n]);
		return n;
	}
	void combine(TempNode* u, TempNode* v){
		if(freezeWorkList->InNodeList(v))
			freezeWorkList = TempNodeList::DeleteNode(v, freezeWorkList);
		else 
			spillWorkList = TempNodeList::DeleteNode(v, spillWorkList);
		coalescedNodes = new TempNodeList(v, coalescedNodes);

		alias[v] = u;

		for(TempNodeList *t = adjacent(v); t; t = t->tail){
			addEdge(t->head, u);
			decrementDegree(t->head);
		}

		if(degree[u] >= K && freezeWorkList->InNodeList(u)){
			freezeWorkList = TempNodeList::DeleteNode(u, freezeWorkList);
			spillWorkList = new TempNodeList(u, spillWorkList);
		}
	}
	void assignColors(TempGraph* ig){
		while(selectStack){
			TempNode *n = selectStack->head; selectStack = selectStack->tail;
			bool usedColors[K + 2] = {0};
			for(TempNodeList *adjs = n->Succ(); adjs; adjs = adjs->tail)
				usedColors[color[adjs->head]] = true;
			int i = 1;
			bool realSpill = true;
			for(; i < K + 1; i++)
				if(!usedColors[i]){
					realSpill = false;
					break;
				}
			if(realSpill)
				spilledNodes = new TempNodeList(n, spilledNodes);
			else
				color[n] = i;
		}

		for(TempNodeList *p = ig->Nodes(); p; p = p->tail)
			color[p->head] = color[getAlias(p->head)];
	}
	std::string temp2reg(TEMP::Temp* t){
		static std::map<TEMP::Temp*, std::string> mm;
		if(mm.empty()){
			mm[F::RAX()] = "%rax"; mm[F::RBX()] = "%rbx"; mm[F::RCX()] = "%rcx";
			mm[F::RDX()] = "%rdx"; mm[F::RDI()] = "%rdi"; mm[F::RSI()] = "%rsi";
			mm[F::R8()]  = "%r8" ; mm[F::R9()]  = "%r9" ; mm[F::R10()] = "%r10";
			mm[F::R11()] = "%r11"; mm[F::R12()] = "%r12"; mm[F::R13()] = "%r13";
			mm[F::R14()] = "%r14"; mm[F::R15()] = "%r15"; mm[F::RSP()] = "%rsp";
		}
		return mm[t];
	}
	bool inMoveList(LIVE::MoveList* moves, TempNode* src, TempNode* dst){
		for(; moves; moves = moves->tail)
			if(moves->src == src && moves->dst == dst)
				return true;
		return false;
	}
	LIVE::MoveList* deleteFromMoveList(LIVE::MoveList* moves, TempNode* src, TempNode* dst){
		LIVE::MoveList *res = nullptr;
		for(; moves; moves = moves->tail)
			if(moves->src != src || moves->dst != dst)
				res = new LIVE::MoveList(moves->src, moves->dst, res);
		return res;
	}
	bool precolored(TempNode* n){
		for(TEMP::TempList *tl = F::HardRegs(); tl; tl = tl->tail)
			if(tl->head == n->NodeInfo()) return true;
		return false;
	}
	bool inTempList(TEMP::TempList* tl, TEMP::Temp* t){
		for(; tl; tl = tl->tail)
			if(tl->head == t)return true;
		return false;
	}
	LIVE::MoveList* catMoveList(LIVE::MoveList* a, LIVE::MoveList* b){
		LIVE::MoveList* res = nullptr;
		for(; a; a = a->tail)
			res = new LIVE::MoveList(a->src, a->dst, res);
		for(; b; b = b->tail)
			res = new LIVE::MoveList(b->src, b->dst, res);
		return res;
	}
	bool briggs(TempNode* u, TempNode* v){
		int cnt = 0;
		std::set<TempNode*> nodeSet;
		TempNodeList *nodes = nullptr;
		for(TempNodeList* p = adjacent(u); p; p = p->tail)
			if(!nodeSet.count(p->head)){
				nodes = new TempNodeList(p->head, nodes);
				nodeSet.insert(p->head);
			}
		for(TempNodeList* p = adjacent(v); p; p = p->tail)
			if(!nodeSet.count(p->head)){
				nodes = new TempNodeList(p->head, nodes);
				nodeSet.insert(p->head);
			}
		for(; nodes; nodes = nodes->tail)
			if(degree[nodes->head] >= K) ++cnt;
		return cnt < K;
	}
}
