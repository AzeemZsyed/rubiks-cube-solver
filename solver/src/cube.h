/*
 * cube.h - Cubie-level model of the 3x3x3 Rubik's Cube.
 *
 * The cube is stored as four arrays:
 *   cp[i] - which corner cubie sits in corner position i
 *   co[i] - orientation (twist) of that corner, 0..2
 *   ep[i] - which edge cubie sits in edge position i
 *   eo[i] - orientation (flip) of that edge, 0..1
 *
 * Facelet strings use 54 characters from "URFDLB" in the order
 * U1..U9, R1..R9, F1..F9, D1..D9, L1..L9, B1..B9 (see README for the net).
 *
 * This file (and every .c file in src/ except cli.c) is freestanding:
 * no libc, so it compiles straight to WebAssembly with clang.
 */
#ifndef CUBE_H
#define CUBE_H

#include <stdint.h>

/* Corner positions */
enum { URF, UFL, ULB, UBR, DFR, DLF, DBL, DRB };
/* Edge positions */
enum { UR, UF, UL, UB, DR, DF, DL, DB, FR, FL, BL, BR };
/* Faces (also the colours of a facelet string) */
enum { FACE_U, FACE_R, FACE_F, FACE_D, FACE_L, FACE_B };

typedef struct {
    uint8_t cp[8];
    uint8_t co[8];
    uint8_t ep[12];
    uint8_t eo[12];
} CubieCube;

/* Coordinate sizes */
#define N_TWIST 2187   /* 3^7  corner orientations            */
#define N_FLIP  2048   /* 2^11 edge orientations              */
#define N_SLICE 495    /* C(12,4) positions of the E-slice    */
#define N_CPERM 40320  /* 8!  corner permutations             */
#define N_EPERM 40320  /* 8!  U/D-layer edge permutations     */
#define N_SPERM 24     /* 4!  E-slice edge permutations       */
#define N_MOVE  18     /* U U2 U' R R2 R' F ... B'            */

/* Error codes returned by cube_from_facelets / cube_verify */
#define CUBE_OK               0
#define CUBE_ERR_COLOR_COUNT -1  /* a colour does not appear exactly 9 times */
#define CUBE_ERR_EDGE        -2  /* an edge piece is missing or duplicated   */
#define CUBE_ERR_FLIP        -3  /* one edge is flipped                      */
#define CUBE_ERR_CORNER      -4  /* a corner piece is missing or duplicated  */
#define CUBE_ERR_TWIST       -5  /* one corner is twisted                    */
#define CUBE_ERR_PARITY      -6  /* two pieces are swapped                   */
#define CUBE_ERR_CENTERS     -7  /* centre stickers are not U R F D L B      */

void cube_solved(CubieCube *c);
void cube_multiply(CubieCube *a, const CubieCube *b); /* a := a * b */
void cube_apply_move(CubieCube *c, int move);
int  cube_is_solved(const CubieCube *c);

int  cube_from_facelets(const char *s, CubieCube *c);
void cube_to_facelets(const CubieCube *c, char *out); /* writes 54 chars + NUL */
int  cube_verify(const CubieCube *c);

/* Move helpers: move = face * 3 + (quarter_turns - 1) */
int  move_parse(const char *s, int *moves, int max_moves); /* "R U2 F'" -> ids, returns count or -1 */
int  move_format(const int *moves, int n, char *out, int out_size);

/* Coordinates */
int  get_twist(const CubieCube *c);
void set_twist(CubieCube *c, int twist);
int  get_flip(const CubieCube *c);
void set_flip(CubieCube *c, int flip);
int  get_slice(const CubieCube *c);
void set_slice(CubieCube *c, int slice);
int  get_cperm(const CubieCube *c);
void set_cperm(CubieCube *c, int idx);
int  get_eperm(const CubieCube *c);  /* only valid in phase 2 (U/D edges in U/D layers) */
void set_eperm(CubieCube *c, int idx);
int  get_sperm(const CubieCube *c);  /* only valid in phase 2 (slice edges in slice)     */
void set_sperm(CubieCube *c, int idx);

#endif
