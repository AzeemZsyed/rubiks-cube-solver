/*
 * cube.c - Cubie model, facelet conversion, validation and coordinates.
 */
#include "cube.h"

/* ---------------------------------------------------------------------
 * The six basic clockwise face turns, written as cubie permutations.
 * Entry i says which cubie moves INTO position i, and with what twist.
 * ------------------------------------------------------------------- */
static const uint8_t move_cp[6][8] = {
    {UBR, URF, UFL, ULB, DFR, DLF, DBL, DRB}, /* U */
    {DFR, UFL, ULB, URF, DRB, DLF, DBL, UBR}, /* R */
    {UFL, DLF, ULB, UBR, URF, DFR, DBL, DRB}, /* F */
    {URF, UFL, ULB, UBR, DLF, DBL, DRB, DFR}, /* D */
    {URF, ULB, DBL, UBR, DFR, UFL, DLF, DRB}, /* L */
    {URF, UFL, UBR, DRB, DFR, DLF, ULB, DBL}, /* B */
};
static const uint8_t move_co[6][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {2, 0, 0, 1, 1, 0, 0, 2},
    {1, 2, 0, 0, 2, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 2, 0, 0, 2, 1, 0},
    {0, 0, 1, 2, 0, 0, 2, 1},
};
static const uint8_t move_ep[6][12] = {
    {UB, UR, UF, UL, DR, DF, DL, DB, FR, FL, BL, BR},
    {FR, UF, UL, UB, BR, DF, DL, DB, DR, FL, BL, UR},
    {UR, FL, UL, UB, DR, FR, DL, DB, UF, DF, BL, BR},
    {UR, UF, UL, UB, DF, DL, DB, DR, FR, FL, BL, BR},
    {UR, UF, BL, UB, DR, DF, FL, DB, FR, UL, DL, BR},
    {UR, UF, UL, BR, DR, DF, DL, BL, FR, FL, UB, DB},
};
static const uint8_t move_eo[6][12] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1},
};

/* Facelet indices (0..53) of each corner / edge position, U or D sticker first. */
static const uint8_t corner_facelet[8][3] = {
    {8, 9, 20}, {6, 18, 38}, {0, 36, 47}, {2, 45, 11},
    {29, 26, 15}, {27, 44, 24}, {33, 53, 42}, {35, 17, 51},
};
static const uint8_t edge_facelet[12][2] = {
    {5, 10}, {7, 19}, {3, 37}, {1, 46}, {32, 16}, {28, 25},
    {30, 43}, {34, 52}, {23, 12}, {21, 41}, {50, 39}, {48, 14},
};
/* Colours of each corner / edge cubie in the same sticker order. */
static const uint8_t corner_color[8][3] = {
    {FACE_U, FACE_R, FACE_F}, {FACE_U, FACE_F, FACE_L}, {FACE_U, FACE_L, FACE_B},
    {FACE_U, FACE_B, FACE_R}, {FACE_D, FACE_F, FACE_R}, {FACE_D, FACE_L, FACE_F},
    {FACE_D, FACE_B, FACE_L}, {FACE_D, FACE_R, FACE_B},
};
static const uint8_t edge_color[12][2] = {
    {FACE_U, FACE_R}, {FACE_U, FACE_F}, {FACE_U, FACE_L}, {FACE_U, FACE_B},
    {FACE_D, FACE_R}, {FACE_D, FACE_F}, {FACE_D, FACE_L}, {FACE_D, FACE_B},
    {FACE_F, FACE_R}, {FACE_F, FACE_L}, {FACE_B, FACE_L}, {FACE_B, FACE_R},
};

static const char FACE_CHARS[6] = {'U', 'R', 'F', 'D', 'L', 'B'};

static int face_of_char(char ch) {
    for (int i = 0; i < 6; i++)
        if (FACE_CHARS[i] == ch) return i;
    return -1;
}

/* ------------------------------------------------------------------ */

void cube_solved(CubieCube *c) {
    for (int i = 0; i < 8; i++) { c->cp[i] = (uint8_t)i; c->co[i] = 0; }
    for (int i = 0; i < 12; i++) { c->ep[i] = (uint8_t)i; c->eo[i] = 0; }
}

void cube_multiply(CubieCube *a, const CubieCube *b) {
    CubieCube r;
    for (int i = 0; i < 8; i++) {
        r.cp[i] = a->cp[b->cp[i]];
        r.co[i] = (uint8_t)((a->co[b->cp[i]] + b->co[i]) % 3);
    }
    for (int i = 0; i < 12; i++) {
        r.ep[i] = a->ep[b->ep[i]];
        r.eo[i] = (uint8_t)((a->eo[b->ep[i]] + b->eo[i]) & 1);
    }
    *a = r;
}

void cube_apply_move(CubieCube *c, int move) {
    int face = move / 3, turns = move % 3 + 1;
    CubieCube m;
    for (int i = 0; i < 8; i++) { m.cp[i] = move_cp[face][i]; m.co[i] = move_co[face][i]; }
    for (int i = 0; i < 12; i++) { m.ep[i] = move_ep[face][i]; m.eo[i] = move_eo[face][i]; }
    for (int t = 0; t < turns; t++) cube_multiply(c, &m);
}

int cube_is_solved(const CubieCube *c) {
    for (int i = 0; i < 8; i++) if (c->cp[i] != i || c->co[i]) return 0;
    for (int i = 0; i < 12; i++) if (c->ep[i] != i || c->eo[i]) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */

int cube_from_facelets(const char *s, CubieCube *c) {
    int f[54], count[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 54; i++) {
        f[i] = face_of_char(s[i]);
        if (f[i] < 0) return CUBE_ERR_COLOR_COUNT;
        count[f[i]]++;
    }
    for (int i = 0; i < 6; i++) {
        if (count[i] != 9) return CUBE_ERR_COLOR_COUNT;
        if (f[i * 9 + 4] != i) return CUBE_ERR_CENTERS;
    }

    for (int i = 0; i < 8; i++) c->cp[i] = 255;
    for (int i = 0; i < 12; i++) c->ep[i] = 255;

    for (int i = 0; i < 8; i++) {
        int ori;
        for (ori = 0; ori < 3; ori++) {
            int col = f[corner_facelet[i][ori]];
            if (col == FACE_U || col == FACE_D) break;
        }
        if (ori == 3) return CUBE_ERR_CORNER;
        int col1 = f[corner_facelet[i][(ori + 1) % 3]];
        int col2 = f[corner_facelet[i][(ori + 2) % 3]];
        for (int j = 0; j < 8; j++) {
            if (col1 == corner_color[j][1] && col2 == corner_color[j][2] &&
                f[corner_facelet[i][ori]] == corner_color[j][0]) {
                c->cp[i] = (uint8_t)j;
                c->co[i] = (uint8_t)ori;
                break;
            }
        }
        if (c->cp[i] == 255) return CUBE_ERR_CORNER;
    }

    for (int i = 0; i < 12; i++) {
        int a = f[edge_facelet[i][0]], b = f[edge_facelet[i][1]];
        for (int j = 0; j < 12; j++) {
            if (a == edge_color[j][0] && b == edge_color[j][1]) { c->ep[i] = (uint8_t)j; c->eo[i] = 0; break; }
            if (a == edge_color[j][1] && b == edge_color[j][0]) { c->ep[i] = (uint8_t)j; c->eo[i] = 1; break; }
        }
        if (c->ep[i] == 255) return CUBE_ERR_EDGE;
    }
    return cube_verify(c);
}

void cube_to_facelets(const CubieCube *c, char *out) {
    for (int i = 0; i < 6; i++) out[i * 9 + 4] = FACE_CHARS[i];
    for (int i = 0; i < 8; i++) {
        int j = c->cp[i], ori = c->co[i];
        for (int k = 0; k < 3; k++)
            out[corner_facelet[i][(k + ori) % 3]] = FACE_CHARS[corner_color[j][k]];
    }
    for (int i = 0; i < 12; i++) {
        int j = c->ep[i], ori = c->eo[i];
        for (int k = 0; k < 2; k++)
            out[edge_facelet[i][(k + ori) % 2]] = FACE_CHARS[edge_color[j][k]];
    }
    out[54] = '\0';
}

static int perm_parity(const uint8_t *p, int n) {
    int s = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (p[j] < p[i]) s++;
    return s & 1;
}

int cube_verify(const CubieCube *c) {
    int seen_e[12] = {0}, seen_c[8] = {0}, sum = 0;
    for (int i = 0; i < 12; i++) {
        if (c->ep[i] > 11 || seen_e[c->ep[i]]++) return CUBE_ERR_EDGE;
        sum += c->eo[i];
    }
    if (sum % 2) return CUBE_ERR_FLIP;
    sum = 0;
    for (int i = 0; i < 8; i++) {
        if (c->cp[i] > 7 || seen_c[c->cp[i]]++) return CUBE_ERR_CORNER;
        sum += c->co[i];
    }
    if (sum % 3) return CUBE_ERR_TWIST;
    if (perm_parity(c->cp, 8) != perm_parity(c->ep, 12)) return CUBE_ERR_PARITY;
    return CUBE_OK;
}

/* ------------------------------------------------------------------ */

int move_parse(const char *s, int *moves, int max_moves) {
    int n = 0;
    while (*s) {
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == ',') s++;
        if (!*s) break;
        int face = face_of_char(*s++);
        if (face < 0 || n >= max_moves) return -1;
        int turns = 1;
        if (*s == '2') { turns = 2; s++; }
        else if (*s == '\'' || *s == '3') { turns = 3; s++; }
        moves[n++] = face * 3 + turns - 1;
    }
    return n;
}

int move_format(const int *moves, int n, char *out, int out_size) {
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (k + 4 >= out_size) return -1;
        if (i) out[k++] = ' ';
        out[k++] = FACE_CHARS[moves[i] / 3];
        int t = moves[i] % 3;
        if (t == 1) out[k++] = '2';
        else if (t == 2) out[k++] = '\'';
    }
    out[k] = '\0';
    return k;
}

/* ---------------------------------------------------------------------
 * Coordinates: map parts of the cube state to small integers so that
 * move tables and pruning tables can be plain arrays.
 * ------------------------------------------------------------------- */

static int binom(int n, int k) {
    if (k < 0 || k > n) return 0;
    int r = 1;
    for (int i = 1; i <= k; i++) r = r * (n - k + i) / i;
    return r;
}

int get_twist(const CubieCube *c) {
    int r = 0;
    for (int i = 0; i < 7; i++) r = r * 3 + c->co[i];
    return r;
}

void set_twist(CubieCube *c, int twist) {
    int parity = 0;
    for (int i = 6; i >= 0; i--) {
        c->co[i] = (uint8_t)(twist % 3);
        parity += c->co[i];
        twist /= 3;
    }
    c->co[7] = (uint8_t)((3 - parity % 3) % 3);
}

int get_flip(const CubieCube *c) {
    int r = 0;
    for (int i = 0; i < 11; i++) r = r * 2 + c->eo[i];
    return r;
}

void set_flip(CubieCube *c, int flip) {
    int parity = 0;
    for (int i = 10; i >= 0; i--) {
        c->eo[i] = (uint8_t)(flip & 1);
        parity += c->eo[i];
        flip >>= 1;
    }
    c->eo[11] = (uint8_t)(parity & 1);
}

/* Which 4 of the 12 edge positions hold the E-slice edges (FR, FL, BL, BR),
 * ranked with the combinatorial number system. */
int get_slice(const CubieCube *c) {
    int r = 0, k = 0;
    for (int i = 0; i < 12; i++)
        if (c->ep[i] >= FR) r += binom(i, ++k);
    return r;
}

void set_slice(CubieCube *c, int slice) {
    int is_slice[12] = {0};
    int p = 11;
    for (int k = 4; k >= 1; k--) {
        while (binom(p, k) > slice) p--;
        is_slice[p] = 1;
        slice -= binom(p, k);
        p--;
    }
    int next_slice = FR, next_other = UR;
    for (int i = 0; i < 12; i++)
        c->ep[i] = (uint8_t)(is_slice[i] ? next_slice++ : next_other++);
}

/* Lehmer-code rank / unrank of a permutation of 0..n-1 */
static int perm_rank(const uint8_t *p, int n) {
    int r = 0;
    for (int i = 0; i < n; i++) {
        int smaller = 0;
        for (int j = i + 1; j < n; j++) if (p[j] < p[i]) smaller++;
        r = r * (n - i) + smaller;
    }
    return r;
}

static void perm_unrank(uint8_t *p, int n, int r) {
    int digit[12], used[12] = {0};
    for (int i = n - 1; i >= 0; i--) { digit[i] = r % (n - i); r /= (n - i); }
    for (int i = 0; i < n; i++) {
        int skip = digit[i];
        for (int v = 0; v < n; v++) {
            if (used[v]) continue;
            if (skip-- == 0) { p[i] = (uint8_t)v; used[v] = 1; break; }
        }
    }
}

int get_cperm(const CubieCube *c) { return perm_rank(c->cp, 8); }
void set_cperm(CubieCube *c, int idx) { perm_unrank(c->cp, 8, idx); }

int get_eperm(const CubieCube *c) { return perm_rank(c->ep, 8); }
void set_eperm(CubieCube *c, int idx) {
    perm_unrank(c->ep, 8, idx);
    for (int i = 8; i < 12; i++) c->ep[i] = (uint8_t)i;
}

int get_sperm(const CubieCube *c) {
    uint8_t s[4];
    for (int i = 0; i < 4; i++) s[i] = (uint8_t)(c->ep[8 + i] - 8);
    return perm_rank(s, 4);
}
void set_sperm(CubieCube *c, int idx) {
    uint8_t s[4];
    perm_unrank(s, 4, idx);
    for (int i = 0; i < 8; i++) c->ep[i] = (uint8_t)i;
    for (int i = 0; i < 4; i++) c->ep[8 + i] = (uint8_t)(8 + s[i]);
}
