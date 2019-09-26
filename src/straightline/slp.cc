#include "straightline/slp.h"

namespace A {
int A::CompoundStm::MaxArgs() const {
  return std::max(stm1->MaxArgs(),stm2->MaxArgs());
}

Table *A::CompoundStm::InterpStm(Table *t) const {
	Table *stm1t = stm1->InterpStm(t);
	Table *stm2t = stm2->InterpStm(stm1t);
	return stm2t;
}

int A::AssignStm::MaxArgs() const {
  return exp->MaxArgs();
}

Table *A::AssignStm::InterpStm(Table *t) const {
	IntAndTable expit = exp->InterpExp(t);
	return new Table(id,expit.i,expit.t);
}

int A::PrintStm::MaxArgs() const {
  return exps->MaxArgs(0);
}

Table *A::PrintStm::InterpStm(Table *t) const {
	// TODO:Do print work here!
	return exps->PrintExps(t).t;
}

IntAndTable IdExp::InterpExp(Table *t) const {
	int value = t->Lookup(id);
	return IntAndTable(value,t);
}

IntAndTable NumExp::InterpExp(Table *t) const {
  	return IntAndTable(num,t);
}

IntAndTable OpExp::InterpExp(Table *t) const {
	IntAndTable leftit = left->InterpExp(t);
    IntAndTable rightit = right->InterpExp(leftit.t);
	int leftv = leftit.i, rightv = rightit.i;
	int retv;
	switch(oper){
		case PLUS: retv = leftv + rightv;break;
		case MINUS: retv = leftv - rightv;break;
		case TIMES: retv = leftv * rightv;break;
		case DIV: retv = leftv / rightv;break;
		default:;
	}
	return IntAndTable(retv,rightit.t);
}

IntAndTable EseqExp::InterpExp(Table *t) const {
	Table *stmt = stm->InterpStm(t);
	return exp->InterpExp(stmt);

}

int PairExpList::MaxArgs(int count) const {
	return tail->MaxArgs(count+1);
}

IntAndTable PairExpList::InterpExp(Table *t) const {
	IntAndTable headit = head->InterpExp(t);
	return tail->InterpExp(headit.t);
}

IntAndTable PairExpList::PrintExps(Table *t) const {
	IntAndTable headit = head->InterpExp(t);
	std::printf("%d ",headit.i);
	return tail->PrintExps(headit.t);
}

IntAndTable LastExpList::InterpExp(Table *t) const {
	return last->InterpExp(t);
}

IntAndTable LastExpList::PrintExps(Table *t) const {
	IntAndTable expit = last->InterpExp(t);
	std::printf("%d\n",expit.i);
	return expit;
}

int LastExpList::MaxArgs(int count) const {
	return std::max(count+1,last->MaxArgs());
}

int Table::Lookup(std::string key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(std::string key, int value) const {
  return new Table(key, value, this);
}
}  // namespace A
