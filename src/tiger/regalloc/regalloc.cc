#include "tiger/regalloc/regalloc.h"

namespace RA {

Result RegAlloc(F::Frame* f, AS::InstrList* il) {
  // TODO: Put your codes here (lab6).

  return Result(f->RegAlloc(il), il);
	//Result(TEMP::Map* coloring, AS::InstrList* il): coloring(coloring), il(il){}
}

}  // namespace RA
