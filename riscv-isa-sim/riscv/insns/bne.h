const bool taken = RS1 != RS2;
p->note_conditional_branch(pc, taken);
if (taken)
  set_pc(BRANCH_TARGET);
