/*
 * model.js - Facelet model of the cube used by the UI.
 *
 * A cube state is a 54-character string over "URFDLB" in the order
 * U1..U9, R1..R9, F1..F9, D1..D9, L1..L9, B1..B9 (same as the C solver).
 *
 * Instead of hard-coding 18 permutation tables, every sticker gets a 3D
 * position and normal. A face turn is a 90 degree rotation of the stickers
 * in that layer, and the permutation falls out of the geometry. The same
 * positions are used by the 3D renderer, so the view and the model can
 * never disagree.
 */

export const FACES = ['U', 'R', 'F', 'D', 'L', 'B'];
export const SOLVED = FACES.map((f) => f.repeat(9)).join('');

// Outward axis of each face: [axisIndex (0=x,1=y,2=z), sign]
export const FACE_AXIS = {
  U: [1, 1],
  R: [0, 1],
  F: [2, 1],
  D: [1, -1],
  L: [0, -1],
  B: [2, -1],
};

/** Grid position of sticker (row, col) on a face, in cubie units (-1..1). */
function stickerPosition(face, r, c) {
  switch (face) {
    case 'U': return [c - 1, 1, r - 1];
    case 'R': return [1, 1 - r, 1 - c];
    case 'F': return [c - 1, 1 - r, 1];
    case 'D': return [c - 1, -1, 1 - r];
    case 'L': return [-1, 1 - r, c - 1];
    case 'B': return [1 - c, 1 - r, -1];
    default: throw new Error(`bad face ${face}`);
  }
}

function faceNormal(face) {
  const [axis, sign] = FACE_AXIS[face];
  const n = [0, 0, 0];
  n[axis] = sign;
  return n;
}

/** All 54 stickers with their cubie position and outward normal. */
export const STICKERS = FACES.flatMap((face) =>
  Array.from({ length: 9 }, (_, i) => ({
    index: FACES.indexOf(face) * 9 + i,
    face,
    pos: stickerPosition(face, Math.floor(i / 3), i % 3),
    normal: faceNormal(face),
  })),
);

const keyOf = (pos, normal) => `${pos.join(',')}|${normal.join(',')}`;
const STICKER_BY_KEY = new Map(STICKERS.map((s) => [keyOf(s.pos, s.normal), s.index]));

/**
 * Rotate an integer vector a quarter turn about a coordinate axis.
 * dir = +1 is counter-clockwise when looking at the origin from +axis.
 */
function rotateQuarter(v, axis, dir) {
  const [x, y, z] = v;
  if (axis === 0) return [x, -dir * z, dir * y];
  if (axis === 1) return [dir * z, y, -dir * x];
  return [-dir * y, dir * x, z];
}

/** Parse one move token: "R", "R2", "R'" -> { face, turns } with turns in 1..3. */
export function parseMove(token) {
  const m = /^([URFDLB])(2|'|3)?$/.exec(token.trim());
  if (!m) throw new Error(`Invalid move: ${token}`);
  const turns = m[2] === '2' ? 2 : m[2] === "'" || m[2] === '3' ? 3 : 1;
  return { face: m[1], turns };
}

export function parseMoves(text) {
  return text.trim() === '' ? [] : text.trim().split(/[\s,]+/).map(parseMove);
}

export function formatMove({ face, turns }) {
  return face + (turns === 2 ? '2' : turns === 3 ? "'" : '');
}

export function invertMove({ face, turns }) {
  return { face, turns: 4 - turns }; // R -> R', R2 -> R2, R' -> R
}

// Build the facelet permutation for one clockwise quarter turn of each face.
// perm[dest] = src, so newState[dest] = state[src].
const QUARTER_PERM = {};
for (const face of FACES) {
  const [axis, sign] = FACE_AXIS[face];
  const dir = -sign; // clockwise when looking at the face from outside
  const perm = Array.from({ length: 54 }, (_, i) => i);
  for (const s of STICKERS) {
    if (s.pos[axis] !== sign) continue;
    const dest = STICKER_BY_KEY.get(keyOf(rotateQuarter(s.pos, axis, dir), rotateQuarter(s.normal, axis, dir)));
    perm[dest] = s.index;
  }
  QUARTER_PERM[face] = perm;
}

/** Rotation used by the renderer for a move: axis index and angle in radians. */
export function moveRotation({ face, turns }) {
  const [axis, sign] = FACE_AXIS[face];
  const quarter = turns === 3 ? -1 : turns; // R' is a -90 degree R
  return { axis, layer: sign, angle: (-sign * quarter * Math.PI) / 2 };
}

export function applyMove(state, move) {
  const perm = QUARTER_PERM[move.face];
  let s = state;
  for (let t = 0; t < move.turns; t++) {
    let next = '';
    for (let i = 0; i < 54; i++) next += s[perm[i]];
    s = next;
  }
  return s;
}

export function applyMoves(state, moves) {
  return moves.reduce(applyMove, state);
}

/** Random scramble that never turns the same face (or opposite pair) redundantly. */
export function randomScramble(length = 25, rng = Math.random) {
  const opposite = { U: 'D', D: 'U', R: 'L', L: 'R', F: 'B', B: 'F' };
  const moves = [];
  while (moves.length < length) {
    const face = FACES[Math.floor(rng() * 6)];
    const prev = moves[moves.length - 1];
    const prev2 = moves[moves.length - 2];
    if (prev && prev.face === face) continue;
    if (prev && prev2 && opposite[prev.face] === face && prev2.face === face) continue;
    moves.push({ face, turns: 1 + Math.floor(rng() * 3) });
  }
  return moves;
}

/** Quick client-side checks; the C solver does the full parity/twist checks. */
export function countColors(state) {
  const counts = Object.fromEntries(FACES.map((f) => [f, 0]));
  for (const ch of state) counts[ch] = (counts[ch] ?? 0) + 1;
  return counts;
}
