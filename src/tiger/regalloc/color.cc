#include "tiger/regalloc/color.h"
#include <map>
#include <set>
#include <vector>

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
	void assignColors(TempGraph* ig);

	TempNodeList* subNodeList(TempNodeList* from, TempNodeList* targets); // similar to substring
	LIVE::MoveList* catMoveList(LIVE::MoveList* a, LIVE::MoveList* b); // concatenate two move list
	bool inMoveList(LIVE::MoveList* moves, TempNode* src, TempNode* dst); 
	LIVE::MoveList* deleteFromMoveList(LIVE::MoveList* moves, TempNode* src, TempNode* dst);

	bool precolored(TempNode* n); // 机器寄存器集合，通过函数的方式模拟std::map
	bool briggs(TempNode* u, TempNode* v); // [p169] Briggs合并策略

	const int K = 14;
	std::string hardRegs[15] = {"uncolored", "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"};
	TempNodeList *simplifyWorkList = nullptr, // 低度数的传送无关节点表
							 *freezeWorkList = nullptr, 	// 低度数的传送有关节点表
							 *spillWorkList = nullptr, 		// 高度数的节点表
							 *spilledNodes = nullptr, 		// 在本轮中要被溢出的节点集合，初始为空
							 *coloredNodes = nullptr, 		// 已合并的寄存器集合
							 *coalescedNodes = nullptr, 	// 已合并的寄存器集合。当合并u <- v时，将v加入到这个集合中，u被放回到某个工作表中（或反之）
							 *selectStack = nullptr; 			// 一个包含从图中删除的临时变量的栈
	std::map<TempNode*, TempNode*> alias; 		// 当一条传送指令(u, v)已被合并，并且v已放入已合并节点集合coalescedNodes时，有alias(v) = u
	std::map<TempNode*, int> degree, 					// 每个节点当前度数
													 color; 					// 算法为节点选择的颜色，对于预着色节点，其初值为给定的颜色
	LIVE::MoveList *activeMoves = nullptr, 		// 还未做好合并准备的传送指令集合
								 *workListMoves = nullptr,	// 有可能合并的传送指令集合
								 *frozenMoves = nullptr,		// 不再考虑合并的传送指令集合
								 *coalescedMoves = nullptr,	// 已合并的传送指令集合
								 *constrainedMoves = nullptr; // 源操作数和目标操作数冲突的传送指令集合
}

namespace COL {

Result Color(G::Graph<TEMP::Temp>* ig, TEMP::Map* initial, TEMP::TempList* regs,
             LIVE::MoveList* moves) {
	// LivenessAnalysis located in Liveness.cc
	build(ig);
	makeWorkList(ig);
	while(simplifyWorkList || workListMoves || freezeWorkList || spillWorkList){
		if(simplifyWorkList) simplify();
		else if(workListMoves) coalesce();
		else if(freezeWorkList) freeze();
		else if(spillWorkList) selectSpill();
	}
	assignColors(ig);

	// combine precolored hardregs and the color result comes above as the final color result
	for(TempNodeList *nodes = ig->Nodes(); nodes; nodes = nodes->tail)
		initial->Enter(nodes->head->NodeInfo(), new std::string(hardRegs[color[nodes->head]]));
	
	// collect actually spilled nodes and return to Regalloc
	// Regalloc will check if actualSpills is null and decide either to rewrite then recolor or terminate regallocating
	TEMP::TempList *actualSpills = nullptr;
	for(; spilledNodes; spilledNodes = spilledNodes->tail)
		actualSpills = new TEMP::TempList(spilledNodes->head->NodeInfo(), actualSpills);

	// Rewrite located in Regalloc outside Color
	// Regalloc call Color repeatedly instead of recrusive Color
  return Result(initial, actualSpills);
}

}  // namespace COL

namespace {

	void build(TempGraph* ig){
		// init degrees and set color for hardregs
		// conflict graph has been calculated in liveness
		for(TempNodeList *nodes = ig->Nodes(); nodes; nodes = nodes->tail){
			TempNode *n = nodes->head;
			TEMP::Temp *nt = n->NodeInfo();

			if(nt == F::RAX()) color[n] = 1;
			else if(nt == F::RBX()) color[n] = 2;
			else if(nt == F::RCX()) color[n] = 3;
			else if(nt == F::RDX()) color[n] = 4;
			else if(nt == F::RSI()) color[n] = 5;
			else if(nt == F::RDI()) color[n] = 6;
			else if(nt == F::R8())  color[n] = 7;
			else if(nt == F::R9())  color[n] = 8;
			else if(nt == F::R10()) color[n] = 9;
			else if(nt == F::R11()) color[n] = 10;
			else if(nt == F::R12()) color[n] = 11;
			else if(nt == F::R13()) color[n] = 12;
			else if(nt == F::R14()) color[n] = 13;
			else if(nt == F::R15()) color[n] = 14;
			else if(nt == F::RBP()) assert(0);
			else color[n] = 0;
			
			// calculate degree according to conflict graph
			// for two nodes u and v
			// if conflicted there will be two edges u <- v and v <- u
			int curDegree = 0;
			for(TempNodeList *succs = n->Succ(); succs; succs = succs->tail) ++curDegree;
			degree[n] = curDegree;
		}
	}

	void makeWorkList(TempGraph* ig){
		// scan all nodes
		// and build worklist according to the characteristic of certain nodes
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
		// process simplification on the simplifyWorkList
		
		//fprintf(stderr, "start simplify\n");
		TempNode *n = simplifyWorkList->head;
		simplifyWorkList = simplifyWorkList->tail;
		selectStack = new TempNodeList(n, selectStack);
		for(TempNodeList *m = adjacent(n); m; m = m->tail)
			decrementDegree(m->head);
	}

	void coalesce(){
		// 合并阶段只考虑worklistMoves中的传送指令，当合并一条传送指令时，它设计的那两个点可能不再是传送有关的，因而可用过程AddWorkList将它们加入简化工作表中
		TempNode *u, *v,
						 *x = workListMoves->src,
						 *y = workListMoves->dst;

		// ensure to combine nodes to hardRegs
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
			// an move instruction for an single node itself
			coalescedMoves = new LIVE::MoveList(x, y, coalescedMoves);
			addWorkList(u);
		}
		else if(precolored(v) || v->Adj()->InNodeList(u)){
			// check if being constrained
			constrainedMoves = new LIVE::MoveList(x, y, constrainedMoves);
			addWorkList(u);
			addWorkList(v);
		}
		else if(precolored(u) && OK(v, u)){
			// if a hard register involved 
			// check if this move passes OK-check
			coalescedMoves = new LIVE::MoveList(x, y, coalescedMoves);
			combine(u, v);
			addWorkList(u);
		}
		else if(!precolored(u) && briggs(u, v)){
			// if no hard register involved
			// check if passes certain heuristic check
			coalescedMoves = new LIVE::MoveList(x, y, coalescedMoves);
			combine(u, v);
			addWorkList(u);
		}
		else {
			// stop coalescing and wait for second chance
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
		// return the set of the nodes adjacent but not 
		//  - simplified into selectStack
		//  - coalesced with other nodes
		return subNodeList(subNodeList(n->Adj(), selectStack), coalescedNodes);
	}
	
	void addEdge(TempNode* u, TempNode* v){
		if(!v->Succ()->InNodeList(u) && u != v){
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
		// return the move instructions node n related
		LIVE::MoveList *moves = nullptr;
		TempNode *m = getAlias(n);
		for(LIVE::MoveList *p = catMoveList(activeMoves, workListMoves); p; p = p->tail)
			if(getAlias(p->src) == m || getAlias(p->dst) == m)
				moves = new LIVE::MoveList(p->src, p->dst, moves);
		return moves;
	}
	
	bool moveRelated(TempNode* n){
		// check whether node n appears in any move instructions
		return nodeMoves(n) != nullptr;
	}
	
	void decrementDegree(TempNode* m){
		// [p179] 从图中去掉一个节点需要减少该节点的当前各个邻节点的度数，如果某个邻节点的degree已经小于K-1，则这个邻节点一定是传送有关的
		// 因此不将它加入到simplifyWorklist中。
		// 当邻节点的度数从K变到K-1时，与它的邻节点相关的传送指令将有可能变成合并的
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
		// an second chance to retry the move instruction the certain nodes involved 
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
		// append node u to simplifyWorkList if
		// - node u is not a hard register
		// - ndoe u is not move related
		// - the degree of node u is under K
		if(!precolored(u) && !moveRelated(u) && degree[u] < K){
			freezeWorkList = TempNodeList::DeleteNode(u, freezeWorkList);
			simplifyWorkList = new TempNodeList(u, simplifyWorkList);
		}
	}
	
	bool OK(TempNode* t, TempNode* r){
		// 合并一个预着色寄存器所使用的启发式函数。
		for(TempNodeList *p = adjacent(t); p; p = p->tail){
			TempNode *n = p->head;
			if(degree[n] < K && precolored(n) || precolored(n) || r->Adj()->InNodeList(n)) continue;
			return false;
		}
		return true;
	}
	
	bool conservative(TempNodeList* nodes){
		// 实现保守合并的启发式函数
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
		spilledNodes = nullptr;
		while(selectStack){
			TempNode *n = selectStack->head; selectStack = selectStack->tail;
			bool usedColors[K + 2] = {0};

			//fprintf(stderr, "assigning color for r%d: ", n->NodeInfo()->Int());
			for(TempNodeList *adjs = n->Succ(); adjs; adjs = adjs->tail){
				//fprintf(stderr, "%d,", color[adjs->head]);
				usedColors[color[adjs->head]] = true;
			}
			//fprintf(stderr, "\n");
			int i = 1;
			bool realSpill = true;
			for(; i < K + 1; i++)
				if(!usedColors[i]){
					realSpill = false;
					break;
				}
			if(realSpill)
				spilledNodes = new TempNodeList(n, spilledNodes);
			else {
				//fprintf(stderr, "assigned color: %s\n", hardRegs[i].c_str());
				color[n] = i;
			}
		}

		// assign the coalesced with the same color
		for(TempNodeList *p = ig->Nodes(); p; p = p->tail)
			color[p->head] = color[getAlias(p->head)];
	}
	
	bool precolored(TempNode* n){
		for(TEMP::TempList *tl = F::HardRegs(); tl; tl = tl->tail)
			if(tl->head == n->NodeInfo()) return true;
		return false;
	}

	/*
	 * Auxiliary functions
	*/
	
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
	
	LIVE::MoveList* catMoveList(LIVE::MoveList* a, LIVE::MoveList* b){
		LIVE::MoveList* res = nullptr;
		for(; a; a = a->tail)
			res = new LIVE::MoveList(a->src, a->dst, res);
		for(; b; b = b->tail)
			res = new LIVE::MoveList(b->src, b->dst, res);
		return res;
	}
	
	bool briggs(TempNode* a, TempNode* b){
		// 如果节点a和b合并产生的节点ab的高度数（即度≥ K）邻节点的个数小于K，则a和b可以被合并
		int cnt = 0;
		std::set<TempNode*> nodeSet;
		TempNodeList *nodes = nullptr;
		for(TempNodeList* p = adjacent(a); p; p = p->tail)
			if(!nodeSet.count(p->head)){
				nodes = new TempNodeList(p->head, nodes);
				nodeSet.insert(p->head);
			}
		for(TempNodeList* p = adjacent(b); p; p = p->tail)
			if(!nodeSet.count(p->head)){
				nodes = new TempNodeList(p->head, nodes);
				nodeSet.insert(p->head);
			}
		for(; nodes; nodes = nodes->tail)
			if(degree[nodes->head] >= K) ++cnt;
		return cnt < K;
	}
	
	TempNodeList* subNodeList(TempNodeList* from, TempNodeList* targets){
		TempNodeList *res = nullptr;
		for(TempNodeList *nodes = from; nodes; nodes = nodes->tail)
			if(!targets->InNodeList(nodes->head))
				res = new TempNodeList(nodes->head, res);
		return res;
	}
}
