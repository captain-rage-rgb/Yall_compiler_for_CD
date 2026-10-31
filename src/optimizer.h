#ifndef YALL_OPTIMIZER_H
#define YALL_OPTIMIZER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "tac.h"

/**
 * @file optimizer.h
 * @brief Three-Address Code (TAC) Optimizer for Yall compiler.
 *
 * Implements Phase 5:
 *  - Constant folding & propagation
 *  - Algebraic simplifications
 *  - Branch folding
 *  - Temp forwarding
 *  - Control-flow cleanup (unreachable code elimination, jump threading, unused label removal)
 *  - Dead code elimination
 *
 * Produces optimized TAC and transformation statistics.
 */

struct OptStats {
    std::string functionName;
    int instructionsBefore = 0;
    int instructionsAfter = 0;
    int folded = 0;
    int propagated = 0;
    int forwarded = 0;
    int removed = 0;
};

class TACOptimizer {
public:
    TACOptimizer() = default;

    /**
     * @brief Optimizes a TACProgram in place, returning per-function statistics.
     */
    std::vector<OptStats> optimize(TACProgram& program);

    /**
     * @brief Optimizes a single TACFunction.
     */
    OptStats optimizeFunction(TACFunction& fn);

    /**
     * @brief Prints the optimization summary table.
     */
    static void printStatsTable(const std::vector<OptStats>& stats, std::ostream& out);

private:
    // Sub-passes
    bool passA_ConstantAndCopy(TACFunction& fn, OptStats& stats);
    bool passB_TempForwarding(TACFunction& fn, OptStats& stats);
    bool passC_ControlFlow(TACFunction& fn, OptStats& stats);
    bool passD_DeadCode(TACFunction& fn, OptStats& stats);

    // Helpers
    static bool isPure(Opcode op, const Instruction& inst);
    static bool parseNum(const std::string& str, int64_t& out);
    static bool parseDecimal(const std::string& str, double& out);
};

#endif // YALL_OPTIMIZER_H
