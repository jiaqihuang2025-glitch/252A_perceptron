require_extension(EXT_ZCA);
const bool taken = RVC_RS1S == 0;
p->note_conditional_branch(pc, taken);
if (taken)
  set_pc(pc + insn.rvc_b_imm());
