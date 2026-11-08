/*
 * solver.c - Move tables, BFS pruning tables and two-phase IDA* search.
 */
#include "solver.h"

/* ---- Move tables: coordinate x move -> new coordinate ---------------- */
static uint16_t twist_move[N_TWIST][N_MOVE];
static uint16_t flip_move[N_FLIP][N_MOVE];
static uint16_t slice_move[N_SLICE][N_MOVE];
static uint16_t cperm_move[N_CPERM][N_MOVE];
static uint16_t eperm_move[N_EPERM][N_MOVE];
static uint16_t sperm_move[N_SPERM][N_MOVE];

/* ---- Pruning tables: exact distance to solved in a reduced space ----- */
static uint8_t prune_slice_twist[N_SLICE * N_TWIST];
static uint8_t prune_slice_flip[N_SLICE * N_FLIP];
static uint8_t prune_sperm_cperm[N_SPERM * N_CPERM];
static uint8_t prune_sperm_eperm[N_SPERM * N_EPERM];

static int solved_twist, solved_flip, solved_slice;
static int solved_cperm, solved_eperm, solved_sperm;
static int initialized;

/* Moves that keep the cube inside subgroup H: U, U2, U', D, D2, D', R2, F2, L2, B2 */
static const int PHASE2_MOVES[10] = {0, 1, 2, 4, 7, 9, 10, 11, 13, 16};
static const int ALL_MOVES[18] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
static int is_phase2_move[N_MOVE];

typedef int (*coord_get)(const CubieCube *);
typedef void (*coord_set)(CubieCube *, int);

static void build_move_table(uint16_t (*table)[N_MOVE], int n, coord_get get, coord_set set,
                             int phase2_only) {
    CubieCube c, d;
    for (int i = 0; i < n; i++) {
        cube_solved(&c);
        set(&c, i);
        for (int face = 0; face < 6; face++) {
            d = c;
            for (int t = 0; t < 3; t++) {
                int m = face * 3 + t;
                cube_apply_move(&d, face * 3); /* one more quarter turn */
                table[i][m] = (phase2_only && !is_phase2_move[m]) ? 0 : (uint16_t)get(&d);
            }
        }
    }
}

/*
 * Breadth-first search over the product space (coord1, coord2).
 * table[a * n2 + b] = minimum number of moves to reach the solved pair.
 */
static void build_prune_table(uint8_t *table, int n1, int n2,
                              uint16_t (*mt1)[N_MOVE], uint16_t (*mt2)[N_MOVE],
                              int solved1, int solved2, const int *moves, int n_moves) {
    int total = n1 * n2;
    for (int i = 0; i < total; i++) table[i] = 0xFF;
    table[solved1 * n2 + solved2] = 0;

    int filled = 1;
    for (int depth = 0; filled < total; depth++) {
        int added = 0;
        for (int i = 0; i < total; i++) {
            if (table[i] != depth) continue;
            int a = i / n2, b = i % n2;
            for (int k = 0; k < n_moves; k++) {
                int m = moves[k];
                int j = mt1[a][m] * n2 + mt2[b][m];
                if (table[j] == 0xFF) {
                    table[j] = (uint8_t)(depth + 1);
                    added++;
                }
            }
        }
        if (!added) break;
        filled += added;
    }
}

void solver_init(void) {
    if (initialized) return;
    for (int i = 0; i < 10; i++) is_phase2_move[PHASE2_MOVES[i]] = 1;

    CubieCube solved;
    cube_solved(&solved);
    solved_twist = get_twist(&solved);
    solved_flip = get_flip(&solved);
    solved_slice = get_slice(&solved);
    solved_cperm = get_cperm(&solved);
    solved_eperm = get_eperm(&solved);
    solved_sperm = get_sperm(&solved);

    build_move_table(twist_move, N_TWIST, get_twist, set_twist, 0);
    build_move_table(flip_move, N_FLIP, get_flip, set_flip, 0);
    build_move_table(slice_move, N_SLICE, get_slice, set_slice, 0);
    build_move_table(cperm_move, N_CPERM, get_cperm, set_cperm, 1);
    build_move_table(eperm_move, N_EPERM, get_eperm, set_eperm, 1);
    build_move_table(sperm_move, N_SPERM, get_sperm, set_sperm, 1);

    build_prune_table(prune_slice_twist, N_SLICE, N_TWIST, slice_move, twist_move,
                      solved_slice, solved_twist, ALL_MOVES, 18);
    build_prune_table(prune_slice_flip, N_SLICE, N_FLIP, slice_move, flip_move,
                      solved_slice, solved_flip, ALL_MOVES, 18);
    build_prune_table(prune_sperm_cperm, N_SPERM, N_CPERM, sperm_move, cperm_move,
                      solved_sperm, solved_cperm, PHASE2_MOVES, 10);
    build_prune_table(prune_sperm_eperm, N_SPERM, N_EPERM, sperm_move, eperm_move,
                      solved_sperm, solved_eperm, PHASE2_MOVES, 10);

    initialized = 1;
}

int solver_ready(void) { return initialized; }

/* ---- Search state ------------------------------------------------------ */
static CubieCube start_cube;
static int path[64];
static int best[64];
static int best_len;
static int target;
static int stop;
static int found_any;
static long nodes;
static long node_budget;
static long budget_end;

static inline int max2(int a, int b) { return a > b ? a : b; }

static inline int h_phase1(int slice, int twist, int flip) {
    return max2(prune_slice_twist[slice * N_TWIST + twist], prune_slice_flip[slice * N_FLIP + flip]);
}

static inline int h_phase2(int sperm, int cperm, int eperm) {
    return max2(prune_sperm_cperm[sperm * N_CPERM + cperm], prune_sperm_eperm[sperm * N_EPERM + eperm]);
}

/* Skip moves on the same face as the previous move, and only allow
 * opposite faces in one order (U D, never D U) to avoid duplicate paths. */
static inline int redundant(int face, int last_face) {
    return face == last_face || face == last_face - 3;
}

static int phase2(int cperm, int eperm, int sperm, int depth, int togo, int last_face) {
    if (togo == 0)
        return cperm == solved_cperm && eperm == solved_eperm && sperm == solved_sperm;
    nodes++;
    for (int k = 0; k < 10; k++) {
        int m = PHASE2_MOVES[k], face = m / 3;
        if (redundant(face, last_face)) continue;
        int nc = cperm_move[cperm][m], ne = eperm_move[eperm][m], ns = sperm_move[sperm][m];
        if (h_phase2(ns, nc, ne) > togo - 1) continue;
        path[depth] = m;
        if (phase2(nc, ne, ns, depth + 1, togo - 1, face)) return 1;
    }
    return 0;
}

static void try_phase2(int d1) {
    CubieCube c = start_cube;
    for (int i = 0; i < d1; i++) cube_apply_move(&c, path[i]);
    int cp = get_cperm(&c), ep = get_eperm(&c), sp = get_sperm(&c);
    int last_face = d1 > 0 ? path[d1 - 1] / 3 : -1;
    int limit = best_len - 1 - d1; /* only accept strictly shorter solutions */

    for (int d2 = h_phase2(sp, cp, ep); d2 <= limit; d2++) {
        if (phase2(cp, ep, sp, d1, d2, last_face)) {
            best_len = d1 + d2;
            for (int i = 0; i < best_len; i++) best[i] = path[i];
            if (!found_any) {
                found_any = 1;
                budget_end = nodes + node_budget;
            }
            if (best_len <= target) stop = 1;
            return;
        }
    }
}

static void phase1(int twist, int flip, int slice, int depth, int togo, int last_face) {
    if (togo == 0) {
        if (twist == solved_twist && flip == solved_flip && slice == solved_slice) {
            /* If the last move were an H-move, a shorter phase 1 already existed. */
            if (depth > 0 && is_phase2_move[path[depth - 1]]) return;
            try_phase2(depth);
        }
        return;
    }
    nodes++;
    if (found_any && nodes > budget_end) { stop = 1; return; }

    for (int m = 0; m < N_MOVE; m++) {
        int face = m / 3;
        if (redundant(face, last_face)) continue;
        int nt = twist_move[twist][m], nf = flip_move[flip][m], ns = slice_move[slice][m];
        if (h_phase1(ns, nt, nf) > togo - 1) continue;
        path[depth] = m;
        phase1(nt, nf, ns, depth + 1, togo - 1, face);
        if (stop) return;
    }
}

int solver_solve(const CubieCube *cube, int *moves, long max_nodes, int target_len) {
    if (!initialized) return SOLVER_ERR_NOT_INIT;
    int err = cube_verify(cube);
    if (err) return err;

    start_cube = *cube;
    best_len = SOLVER_MAX_LENGTH + 1;
    target = target_len;
    node_budget = max_nodes;
    nodes = 0;
    stop = 0;
    found_any = 0;

    int tw = get_twist(cube), fl = get_flip(cube), sl = get_slice(cube);
    for (int d1 = h_phase1(sl, tw, fl); d1 < best_len && d1 <= 20 && !stop; d1++)
        phase1(tw, fl, sl, 0, d1, -1);

    if (best_len > SOLVER_MAX_LENGTH) return SOLVER_ERR_NOT_FOUND;
    for (int i = 0; i < best_len; i++) moves[i] = best[i];
    return best_len;
}

long solver_last_nodes(void) { return nodes; }
