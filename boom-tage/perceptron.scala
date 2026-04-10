package boom.ifu

import chisel3._
import chisel3.util._

import org.chipsalliance.cde.config.{Field, Parameters}
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.tilelink._

import boom.common._
import boom.util.{BoomCoreStringPrefix}

case class BoomPerceptronParams(
  nSets: Int = 1024,  // Number of perceptron entries
  histLength: Int = 64,  // Global history length
  threshold: Int = 0  // Prediction threshold (0 for standard perceptron)
)

class PerceptronBranchPredictorBank(params: BoomPerceptronParams = BoomPerceptronParams())(implicit p: Parameters) extends BranchPredictorBank()(p) {
  class PerceptronMeta extends Bundle {
    val idx = UInt(log2Ceil(params.nSets).W)
    val pred = Bool()
  }

  val perceptronTable = SyncReadMem(params.nSets, Vec(params.histLength + 1, SInt(8.W)))  // Weights + bias

  val f3_meta = Wire(new PerceptronMeta)
  override val metaSz = f3_meta.asUInt.getWidth
  require(metaSz <= bpdMaxMetaLength)

  val mems = Seq(("perceptron_table", params.nSets, (params.histLength + 1) * 8))

  // Prediction logic
  val p_s1_idx = fetchIdx(io.f0_pc)
  val p_s1_hist = io.f1_ghist

  val s1_weights = perceptronTable.read(p_s1_idx, io.f0_valid)
  val s1_sum = s1_weights.zipWithIndex.map { case (w, i) =>
    val bit = if (i < params.histLength) p_s1_hist(i).asSInt else 1.S
    w * bit
  }.reduce(_ + _)

  for (w <- 0 until bankWidth) {
    io.resp.f2(w).taken := RegNext(s1_sum >= params.threshold.S)
    io.resp.f3(w).taken := RegNext(io.resp.f2(w).taken)
  }

  f3_meta.idx := p_s1_idx
  f3_meta.pred := s1_sum >= params.threshold.S
  io.f3_meta := f3_meta.asUInt

  // Update logic (on commit/mispredict)
  val update_meta = s1_update.bits.meta.asTypeOf(new PerceptronMeta)
  val update_idx = update_meta.idx
  val predicted_taken = update_meta.pred
  val actual_taken = io.update.bits.cfi_taken
  val wrong = predicted_taken =/= actual_taken

  val update_weights = perceptronTable.read(update_idx)
  val new_weights = VecInit(update_weights.zipWithIndex.map { case (w, i) =>
    val bit = if (i < params.histLength) s1_update.bits.ghist(i).asSInt else 1.S
    val delta = Mux(actual_taken, bit, -bit)
    val clipped = (w + delta).max(-127.S).min(127.S)
    clipped
  })

  when (io.update.valid && wrong) {
    perceptronTable.write(update_idx, new_weights)
  }
}