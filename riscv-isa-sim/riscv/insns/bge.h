const bool taken = sreg_t(RS1) >= sreg_t(RS2);
p->note_conditional_branch(pc, taken);
if (taken)
  set_pc(BRANCH_TARGET);
