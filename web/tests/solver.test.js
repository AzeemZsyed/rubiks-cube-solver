// Cross-checks the JavaScript cube model against the C solver (via WebAssembly).
// Run: npm test
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

import { SOLVED, FACES, applyMoves, parseMoves, formatMove, randomScramble, invertMove } from '../src/cube/model.js';
import { createSolver } from '../src/solver/solverCore.js';

const wasm = await readFile(new URL('../public/solver.wasm', import.meta.url));
const solver = await createSolver(wasm);

// Deterministic RNG so failures are reproducible
function mulberry32(seed) {
  return () => {
    seed |= 0; seed = (seed + 0x6d2b79f5) | 0;
    let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

test('every single move matches the C model', () => {
  for (const face of FACES) {
    for (const suffix of ['', '2', "'"]) {
      const m = face + suffix;
      assert.equal(applyMoves(SOLVED, parseMoves(m)), solver.applyMoves(SOLVED, m), m);
    }
  }
});

test('random sequences match the C model', () => {
  const rng = mulberry32(7);
  for (let i = 0; i < 100; i++) {
    const text = randomScramble(30, rng).map(formatMove).join(' ');
    assert.equal(applyMoves(SOLVED, parseMoves(text)), solver.applyMoves(SOLVED, text));
  }
});

test('inverse moves undo a scramble', () => {
  const scramble = randomScramble(25, mulberry32(3));
  const undo = [...scramble].reverse().map(invertMove);
  assert.equal(applyMoves(applyMoves(SOLVED, scramble), undo), SOLVED);
});

test('solver solutions solve random cubes', () => {
  const rng = mulberry32(99);
  let total = 0;
  for (let i = 0; i < 50; i++) {
    const state = applyMoves(SOLVED, randomScramble(30, rng));
    const r = solver.solve(state);
    assert.ok(r.ok, r.error);
    assert.ok(r.length <= 23, `solution too long: ${r.length}`);
    assert.equal(applyMoves(state, parseMoves(r.moves)), SOLVED);
    total += r.length;
  }
  console.log(`  avg solution length ${(total / 50).toFixed(2)} moves, tables built in ${solver.initMs.toFixed(0)} ms`);
});

test('invalid states are rejected with a helpful message', () => {
  const twisted = SOLVED.slice(0, 7) + 'F' + SOLVED.slice(8, 19) + 'U' + SOLVED.slice(20);
  const r = solver.solve(twisted);
  assert.equal(r.ok, false);
  assert.match(r.error, /flipped/);
});
