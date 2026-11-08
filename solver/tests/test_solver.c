/*
 * test_solver.c - Unit and randomized tests for the cube model and solver.
 * Run with: make test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/cube.h"
#include "../src/solver.h"

static int failures = 0, checks = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        checks++;                                                     \
        if (!(cond)) { failures++; printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
    } while (0)

static const char *SOLVED = "UUUUUUUUURRRRRRRRRFFFFFFFFFDDDDDDDDDLLLLLLLLLBBBBBBBBB";

static void test_facelet_roundtrip(void) {
    CubieCube c, d;
    char f[55];
    cube_solved(&c);
    cube_to_facelets(&c, f);
    CHECK(strcmp(f, SOLVED) == 0, "solved cube -> solved facelets");
    CHECK(cube_from_facelets(SOLVED, &d) == CUBE_OK && cube_is_solved(&d), "solved facelets -> solved cube");

    srand(1);
    for (int t = 0; t < 200; t++) {
        cube_solved(&c);
        for (int i = 0; i < 30; i++) cube_apply_move(&c, rand() % 18);
        cube_to_facelets(&c, f);
        CHECK(cube_from_facelets(f, &d) == CUBE_OK && memcmp(&c, &d, sizeof c) == 0, "random facelet roundtrip");
    }
}

static void test_move_identities(void) {
    for (int m = 0; m < 18; m += 3) {
        CubieCube c;
        cube_solved(&c);
        for (int i = 0; i < 4; i++) cube_apply_move(&c, m);
        CHECK(cube_is_solved(&c), "X X X X = identity");
        cube_apply_move(&c, m);
        cube_apply_move(&c, m + 2);
        CHECK(cube_is_solved(&c), "X X' = identity");
    }
    /* (R U R' U') six times is the identity */
    int moves[4];
    move_parse("R U R' U'", moves, 4);
    CubieCube c;
    cube_solved(&c);
    for (int r = 0; r < 6; r++)
        for (int i = 0; i < 4; i++) cube_apply_move(&c, moves[i]);
    CHECK(cube_is_solved(&c), "sexy move x6 = identity");
}

static void test_coordinates(void) {
    CubieCube c;
    for (int i = 0; i < N_TWIST; i++) { cube_solved(&c); set_twist(&c, i); CHECK(get_twist(&c) == i, "twist"); }
    for (int i = 0; i < N_FLIP; i++)  { cube_solved(&c); set_flip(&c, i);  CHECK(get_flip(&c) == i, "flip"); }
    for (int i = 0; i < N_SLICE; i++) { cube_solved(&c); set_slice(&c, i); CHECK(get_slice(&c) == i, "slice"); }
    for (int i = 0; i < N_CPERM; i += 97) { cube_solved(&c); set_cperm(&c, i); CHECK(get_cperm(&c) == i, "cperm"); }
    for (int i = 0; i < N_EPERM; i += 97) { cube_solved(&c); set_eperm(&c, i); CHECK(get_eperm(&c) == i, "eperm"); }
    for (int i = 0; i < N_SPERM; i++) { cube_solved(&c); set_sperm(&c, i); CHECK(get_sperm(&c) == i, "sperm"); }
}

static void test_validation(void) {
    CubieCube c;
    char f[55];
    strcpy(f, SOLVED);
    f[0] = 'R';
    CHECK(cube_from_facelets(f, &c) == CUBE_ERR_COLOR_COUNT, "wrong colour count detected");

    strcpy(f, SOLVED); /* flip the UF edge: U8 <-> F2 */
    f[7] = 'F'; f[19] = 'U';
    CHECK(cube_from_facelets(f, &c) == CUBE_ERR_FLIP, "flipped edge detected");

    cube_solved(&c); /* swap two edges -> parity error */
    c.ep[UR] = UF; c.ep[UF] = UR;
    CHECK(cube_verify(&c) == CUBE_ERR_PARITY, "parity error detected");

    cube_solved(&c); /* twist one corner */
    c.co[URF] = 1;
    CHECK(cube_verify(&c) == CUBE_ERR_TWIST, "twisted corner detected");
}

static void test_solver_random(int count) {
    CubieCube c, s;
    int moves[64];
    long total_len = 0;
    int max_len = 0;
    clock_t t0 = clock();
    srand(42);
    for (int t = 0; t < count; t++) {
        cube_solved(&c);
        for (int i = 0; i < 40; i++) cube_apply_move(&c, rand() % 18);
        int n = solver_solve(&c, moves, 200000, 20);
        CHECK(n >= 0, "solver returned a solution");
        if (n < 0) continue;
        s = c;
        for (int i = 0; i < n; i++) cube_apply_move(&s, moves[i]);
        CHECK(cube_is_solved(&s), "solution actually solves the cube");
        total_len += n;
        if (n > max_len) max_len = n;
    }
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("  %d random cubes: avg %.2f moves, max %d moves, %.1f ms per solve\n",
           count, (double)total_len / count, max_len, secs * 1000 / count);
}

static void test_short_scrambles(void) {
    /* Short scrambles should be undone with no more moves than the scramble. */
    const char *scrambles[] = {"R", "R U", "R U R' U'", "F2 B2 L2 R2", "U R2 F' D L"};
    for (int k = 0; k < 5; k++) {
        CubieCube c;
        int sm[16], moves[64];
        int len = move_parse(scrambles[k], sm, 16);
        cube_solved(&c);
        for (int i = 0; i < len; i++) cube_apply_move(&c, sm[i]);
        int n = solver_solve(&c, moves, 500000, 0);
        CHECK(n >= 0 && n <= len, "short scramble solved optimally-or-better");
    }
    CubieCube c;
    int moves[64];
    cube_solved(&c);
    CHECK(solver_solve(&c, moves, 1000, 0) == 0, "solved cube needs 0 moves");
}

int main(void) {
    printf("Cube model tests\n");
    test_facelet_roundtrip();
    test_move_identities();
    test_coordinates();
    test_validation();

    printf("Building tables...\n");
    clock_t t0 = clock();
    solver_init();
    printf("  done in %.2fs\n", (double)(clock() - t0) / CLOCKS_PER_SEC);

    printf("Solver tests\n");
    test_short_scrambles();
    test_solver_random(200);

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
