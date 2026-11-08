/*
 * solver.h - Kociemba two-phase solver.
 *
 * Phase 1: IDA* search from any cube into the subgroup
 *          H = <U, D, R2, L2, F2, B2>, where every corner and edge is
 *          oriented and the E-slice edges are inside the E-slice.
 * Phase 2: IDA* search inside H using only H moves until solved.
 *
 * Both searches are guided by pruning tables (exact distances in a
 * reduced coordinate space) that are generated with breadth-first search.
 */
#ifndef SOLVER_H
#define SOLVER_H

#include "cube.h"

#define SOLVER_MAX_LENGTH 30
#define SOLVER_ERR_NOT_INIT  -10
#define SOLVER_ERR_NOT_FOUND -11

/* Builds all move and pruning tables (about 7 MB). Call once. */
void solver_init(void);
int  solver_ready(void);

/*
 * Solves the cube. Writes the move sequence into moves[] and returns its
 * length, or a negative error code.
 *   max_nodes  - search budget after the first solution is found; larger
 *                values spend more time looking for a shorter solution.
 *   target_len - stop early once a solution this short is found.
 */
int  solver_solve(const CubieCube *cube, int *moves, long max_nodes, int target_len);

/* Statistics from the last solve (for the UI / benchmarks). */
long solver_last_nodes(void);

#endif
