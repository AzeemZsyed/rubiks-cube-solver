/*
 * api.c - Small string-based API shared by the WebAssembly build and the CLI.
 *
 * JavaScript writes a 54-character facelet string into the input buffer,
 * calls solve(), then reads the move string back from the output buffer.
 */
#include "api.h"
#include "solver.h"

#ifdef __wasm__
#define EXPORT(name) __attribute__((export_name(name)))
#else
#define EXPORT(name)
#endif

static char input_buf[256];
static char output_buf[256];

EXPORT("input_buffer") char *api_input_buffer(void) { return input_buf; }
EXPORT("output_buffer") char *api_output_buffer(void) { return output_buf; }

EXPORT("init") void api_init(void) { solver_init(); }

EXPORT("last_nodes") int api_last_nodes(void) { return (int)solver_last_nodes(); }

/* Facelets in input buffer -> moves in output buffer. Returns length or error. */
EXPORT("solve") int api_solve(int max_nodes, int target_len) {
    CubieCube cube;
    int moves[64];
    output_buf[0] = '\0';
    int err = cube_from_facelets(input_buf, &cube);
    if (err) return err;
    int n = solver_solve(&cube, moves, max_nodes, target_len);
    if (n < 0) return n;
    move_format(moves, n, output_buf, sizeof output_buf);
    return n;
}

/* Input buffer holds "<54 facelets> <moves...>"; writes resulting facelets
 * to the output buffer. Used by tests to cross-check the web renderer. */
EXPORT("apply_moves") int api_apply_moves(void) {
    CubieCube cube;
    int moves[128];
    char facelets[55];
    for (int i = 0; i < 54; i++) facelets[i] = input_buf[i];
    facelets[54] = '\0';
    int err = cube_from_facelets(facelets, &cube);
    if (err) return err;
    int n = move_parse(input_buf + 54, moves, 128);
    if (n < 0) return -20;
    for (int i = 0; i < n; i++) cube_apply_move(&cube, moves[i]);
    cube_to_facelets(&cube, output_buf);
    return n;
}
