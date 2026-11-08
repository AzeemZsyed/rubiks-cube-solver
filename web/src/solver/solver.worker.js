// Runs the WebAssembly solver off the main thread so the UI never freezes
// while pruning tables are built or a search is running.
import { createSolver } from './solverCore.js';

const ready = (async () => {
  const url = new URL(`${import.meta.env.BASE_URL}solver.wasm`, self.location.origin);
  const bytes = await (await fetch(url)).arrayBuffer();
  const solver = await createSolver(bytes);
  self.postMessage({ type: 'ready', initMs: solver.initMs });
  return solver;
})().catch((err) => {
  self.postMessage({ type: 'fatal', error: String(err) });
  throw err;
});

self.onmessage = async ({ data }) => {
  if (data.type !== 'solve') return;
  const solver = await ready;
  const result = solver.solve(data.facelets, data.options);
  self.postMessage({ type: 'result', id: data.id, ...result });
};
