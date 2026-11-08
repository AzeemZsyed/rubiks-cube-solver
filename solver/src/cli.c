/*
 * cli.c - Command-line front end for the native build.
 *
 *   ./rubik <54-char facelet string>      solve a cube state
 *   ./rubik -s "R U R' U'"                apply a scramble to a solved cube, then solve it
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "api.h"
#include "cube.h"
#include "solver.h"

static const char *error_message(int code) {
    switch (code) {
    case CUBE_ERR_COLOR_COUNT: return "each colour must appear exactly 9 times";
    case CUBE_ERR_EDGE:        return "an edge piece is missing or duplicated";
    case CUBE_ERR_FLIP:        return "one edge is flipped (unsolvable)";
    case CUBE_ERR_CORNER:      return "a corner piece is missing or duplicated";
    case CUBE_ERR_TWIST:       return "one corner is twisted (unsolvable)";
    case CUBE_ERR_PARITY:      return "two pieces are swapped (unsolvable)";
    case CUBE_ERR_CENTERS:     return "centre stickers must be U R F D L B in face order";
    default:                   return "no solution found";
    }
}

int main(int argc, char **argv) {
    char facelets[55];

    if (argc == 3 && strcmp(argv[1], "-s") == 0) {
        CubieCube c;
        int moves[128];
        int n = move_parse(argv[2], moves, 128);
        if (n < 0) { fprintf(stderr, "Invalid scramble: %s\n", argv[2]); return 1; }
        cube_solved(&c);
        for (int i = 0; i < n; i++) cube_apply_move(&c, moves[i]);
        cube_to_facelets(&c, facelets);
        printf("Scrambled state: %s\n", facelets);
    } else if (argc == 2 && strlen(argv[1]) == 54) {
        memcpy(facelets, argv[1], 55);
    } else {
        fprintf(stderr, "Usage:\n  %s <54-char facelet string>\n  %s -s \"R U R' U'\"\n", argv[0], argv[0]);
        return 1;
    }

    clock_t t0 = clock();
    api_init();
    clock_t t1 = clock();

    strcpy(api_input_buffer(), facelets);
    int n = api_solve(1000000, 0);
    clock_t t2 = clock();

    if (n < 0) {
        fprintf(stderr, "Error: %s\n", error_message(n));
        return 2;
    }
    printf("Solution (%d moves): %s\n", n, api_output_buffer());
    printf("Tables built in %.2fs, search took %.3fs (%d nodes)\n",
           (double)(t1 - t0) / CLOCKS_PER_SEC, (double)(t2 - t1) / CLOCKS_PER_SEC, api_last_nodes());
    return 0;
}
