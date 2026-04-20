package boom.ifu

import chisel3._
import chisel3.util._

import org.chipsalliance.cde.config.{Parameters}

import boom.common._

case class BoomPerceptronParams(
  nSets: Int = 1024,       // Number of perceptron rows
  histLength: Int = 64,    // Global history length
  threshold: Int = 0,      // Prediction threshold
  trainThreshold: Int = 137
)

class PerceptronBranchPredictorBank(params: BoomPerceptronParams = BoomPerceptronParams())(implicit p: Parameters) extends BranchPredictorBank()(p) {
  override val nSets = params.nSets

  require(isPow2(nSets))
  require(params.histLength <= globalHistoryLength)

  class PerceptronMeta extends Bundle {
    val idx = UInt(log2Ceil(nSets).W)
    val pred = Bool()
  }

  val weightBits = 8
  val weightMax = ((1 << (weightBits - 1)) - 1).S(weightBits.W)
  val weightMin = (-(1 << (weightBits - 1))).S(weightBits.W)

  val perceptronTable = SyncReadMem(nSets, Vec(bankWidth, Vec(params.histLength + 1, SInt(weightBits.W))))

  val f3_meta = Wire(Vec(bankWidth, new PerceptronMeta))
  override val metaSz = f3_meta.asUInt.getWidth
  require(metaSz <= bpdMaxMetaLength)

  val mems = Seq(("perceptron_table", nSets, bankWidth * (params.histLength + 1) * weightBits))

  def histBitToSign(hist: UInt, idx: Int): SInt = {
    Mux(hist(idx), 1.S(2.W), (-1).S(2.W))
  }

  def satIncDec(weight: SInt, inc: Bool): SInt = {
    Mux(inc,
      Mux(weight === weightMax, weightMax, weight + 1.S),
      Mux(weight === weightMin, weightMin, weight - 1.S))
  }

  def dotProduct(weights: Vec[SInt], hist: UInt): SInt = {
    val terms = (0 until params.histLength).map { i =>
      weights(i) * histBitToSign(hist, i)
    } :+ weights(params.histLength)
    terms.reduce(_ + _)
  }

  val doing_reset = RegInit(true.B)
  val reset_idx = RegInit(0.U(log2Ceil(nSets).W))
  reset_idx := reset_idx + doing_reset
  when (reset_idx === (nSets - 1).U) { doing_reset := false.B }

  // Prediction logic. Read at f0, consume the registered SRAM data at f2, and
  // keep metadata aligned with the f3 prediction carried by the frontend.
  val s2_weights = RegNext(perceptronTable.read(s0_idx, s0_valid))
  val s2_hist = RegNext(io.f1_ghist)
  val s2_sums = Wire(Vec(bankWidth, SInt()))
  val s2_preds = Wire(Vec(bankWidth, Bool()))

  for (w <- 0 until bankWidth) {
    s2_sums(w) := dotProduct(s2_weights(w), s2_hist)
    s2_preds(w) := s2_valid && !doing_reset && s2_sums(w) >= params.threshold.S

    io.resp.f2(w).taken := s2_preds(w)
    io.resp.f3(w).taken := RegNext(io.resp.f2(w).taken)

    f3_meta(w).idx := RegNext(s2_idx)
    f3_meta(w).pred := RegNext(s2_preds(w))
  }

  io.f3_meta := f3_meta.asUInt

  // Update logic. BOOM's update metadata arrives one cycle after io.update, so
  // read the matching row at s0_update and train from it at s1_update.
  val s1_update_weights = perceptronTable.read(s0_update_idx, s0_update_valid)
  val update_meta = s1_update.bits.meta.asTypeOf(Vec(bankWidth, new PerceptronMeta))
  val update_idx = update_meta(0).idx

  val update_wdata = Wire(Vec(bankWidth, Vec(params.histLength + 1, SInt(weightBits.W))))
  val update_wmask = Wire(Vec(bankWidth, Bool()))

  for (w <- 0 until bankWidth) {
    val is_update_cfi = s1_update.bits.cfi_idx.valid && s1_update.bits.cfi_idx.bits === w.U
    val train_branch = s1_update.valid &&
      s1_update.bits.is_commit_update &&
      (s1_update.bits.br_mask(w) || (is_update_cfi && s1_update.bits.cfi_is_br))
    val actual_taken = is_update_cfi && s1_update.bits.cfi_is_br && s1_update.bits.cfi_taken
    val predicted_taken = update_meta(w).pred
    val sum = dotProduct(s1_update_weights(w), s1_update.bits.ghist)
    val abs_sum = Mux(sum < 0.S, -sum, sum)
    val should_train = train_branch && ((predicted_taken =/= actual_taken) || abs_sum <= params.trainThreshold.S)

    update_wmask(w) := should_train

    for (i <- 0 until params.histLength) {
      val inc = actual_taken === s1_update.bits.ghist(i)
      update_wdata(w)(i) := satIncDec(s1_update_weights(w)(i), inc)
    }
    update_wdata(w)(params.histLength) := satIncDec(s1_update_weights(w)(params.histLength), actual_taken)
  }

  when (doing_reset || (!doing_reset && update_wmask.reduce(_||_))) {
    val reset_wmask = VecInit(Seq.fill(bankWidth) { true.B })
    perceptronTable.write(
      Mux(doing_reset, reset_idx, update_idx),
      Mux(doing_reset,
        VecInit(Seq.fill(bankWidth) { VecInit(Seq.fill(params.histLength + 1) { 0.S(weightBits.W) }) }),
        update_wdata),
      Mux(doing_reset, reset_wmask, update_wmask)
    )
  }
}
