/*
 * solverCore.js - Thin JavaScript wrapper around the C solver compiled to
 * WebAssembly. Works in the browser (inside a Web Worker) and in Node tests.
 */

export const ERROR_MESSAGES = {
  [-1]: 'Each colour must appear exactly 9 times.',
  [-2]: 'An edge piece is missing or appears twice. Check the edge stickers.',
  [-3]: 'One edge is flipped. This state cannot be reached by turning the cube.',
  [-4]: 'A corner piece is missing or appears twice. Check the corner stickers.',
  [-5]: 'One corner is twisted. This state cannot be reached by turning the cube.',
  [-6]: 'Two pieces are swapped. This state cannot be reached by turning the cube.',
  [-7]: 'The centre stickers must stay in place.',
  [-10]: 'Solver tables are not ready yet.',
  [-11]: 'No solution found within the search limit.',
  [-20]: 'Invalid move sequence.',
};

export async function createSolver(wasmBytes) {
  const { instance } = await WebAssembly.instantiate(wasmBytes, {});
  const ex = instance.exports;
  const encoder = new TextEncoder();
  const decoder = new TextDecoder();

  const writeInput = (text) => {
    const bytes = encoder.encode(text);
    const view = new Uint8Array(ex.memory.buffer, ex.input_buffer(), 256);
    view.fill(0);
    view.set(bytes.subarray(0, 255));
  };
  const readOutput = () => {
    const view = new Uint8Array(ex.memory.buffer, ex.output_buffer(), 256);
    return decoder.decode(view.subarray(0, view.indexOf(0)));
  };

  const t0 = performance.now();
  ex.init();
  const initMs = performance.now() - t0;

  return {
    initMs,
    /** Solve a 54-char facelet string. */
    solve(facelets, { maxNodes = 1_500_000, targetLength = 0 } = {}) {
      writeInput(facelets);
      const start = performance.now();
      const n = ex.solve(maxNodes, targetLength);
      const ms = performance.now() - start;
      if (n < 0) return { ok: false, code: n, error: ERROR_MESSAGES[n] ?? `Solver error ${n}` };
      return { ok: true, length: n, moves: readOutput(), ms, nodes: ex.last_nodes() };
    },
    /** Apply a move string to a facelet string using the C cube model. */
    applyMoves(facelets, moves) {
      writeInput(facelets + ' ' + moves);
      const n = ex.apply_moves();
      if (n < 0) throw new Error(ERROR_MESSAGES[n] ?? `error ${n}`);
      return readOutput();
    },
  };
}
