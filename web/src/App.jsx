import { useCallback, useEffect, useRef, useState } from 'react';

import CubeView from './components/CubeView.jsx';
import FaceletEditor from './components/FaceletEditor.jsx';
import {
  FACES, SOLVED, applyMoves, formatMove, invertMove, parseMoves, randomScramble,
} from './cube/model.js';
import { useSolver } from './solver/useSolver.js';

const SPEEDS = { Slow: 650, Normal: 350, Fast: 150 };

export default function App() {
  const solver = useSolver();
  const cubeRef = useRef(null);

  const [cube, setCube] = useState(SOLVED);
  const [editing, setEditing] = useState(false);
  const [paintColor, setPaintColor] = useState('R');
  const [scrambleText, setScrambleText] = useState('');
  const [solution, setSolution] = useState(null); // { start, moves, ms, nodes }
  const [step, setStep] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [busy, setBusy] = useState(false);
  const [solving, setSolving] = useState(false);
  const [speed, setSpeed] = useState(SPEEDS.Normal);
  const [message, setMessage] = useState(null); // { kind: 'error' | 'info', text }
  const busyRef = useRef(false);

  const clearSolution = () => {
    setSolution(null);
    setStep(0);
    setPlaying(false);
  };

  const runMove = useCallback(async (move) => {
    busyRef.current = true;
    setBusy(true);
    const next = await cubeRef.current.animateMove(move, speed);
    setCube(next);
    busyRef.current = false;
    setBusy(false);
    return next;
  }, [speed]);

  // ---- user actions ---------------------------------------------------------
  const turn = async (token) => {
    if (busyRef.current || editing) return;
    clearSolution();
    setMessage(null);
    await runMove(parseMoves(token)[0]);
  };

  const scramble = () => {
    const moves = randomScramble(22);
    const text = moves.map(formatMove).join(' ');
    setScrambleText(text);
    setCube(applyMoves(SOLVED, moves));
    clearSolution();
    setMessage(null);
  };

  const applyScrambleText = (e) => {
    e.preventDefault();
    try {
      setCube(applyMoves(SOLVED, parseMoves(scrambleText)));
      clearSolution();
      setMessage(null);
    } catch (err) {
      setMessage({ kind: 'error', text: err.message });
    }
  };

  const reset = () => {
    setCube(SOLVED);
    setScrambleText('');
    clearSolution();
    setMessage(null);
  };

  const paintSticker = (index) => {
    if (!editing || index % 9 === 4) return;
    setCube((c) => c.slice(0, index) + paintColor + c.slice(index + 1));
    clearSolution();
  };

  const solve = async () => {
    if (!solver.ready || busyRef.current) return;
    setSolving(true);
    setMessage(null);
    const r = await solver.solve(cube);
    setSolving(false);
    if (!r.ok) {
      setMessage({ kind: 'error', text: r.error });
      return;
    }
    if (r.length === 0) {
      setMessage({ kind: 'info', text: 'The cube is already solved.' });
      return;
    }
    setEditing(false);
    setSolution({ start: cube, moves: parseMoves(r.moves), ms: r.ms, nodes: r.nodes });
    setStep(0);
  };

  const next = useCallback(async () => {
    if (!solution || busyRef.current || step >= solution.moves.length) return;
    await runMove(solution.moves[step]);
    setStep((s) => s + 1);
  }, [solution, step, runMove]);

  const prev = useCallback(async () => {
    if (!solution || busyRef.current || step === 0) return;
    setPlaying(false);
    await runMove(invertMove(solution.moves[step - 1]));
    setStep((s) => s - 1);
  }, [solution, step, runMove]);

  const jumpTo = (k) => {
    if (!solution || busyRef.current) return;
    setPlaying(false);
    setCube(applyMoves(solution.start, solution.moves.slice(0, k)));
    setStep(k);
  };

  // Auto-play: advance one move each time the previous animation finishes.
  useEffect(() => {
    if (!playing || busy || !solution) return;
    if (step >= solution.moves.length) { setPlaying(false); return; }
    next();
  }, [playing, busy, step, solution, next]);

  // Keyboard shortcuts: arrows step, space plays/pauses
  useEffect(() => {
    const onKey = (e) => {
      if (e.target.tagName === 'INPUT' || !solution) return;
      if (e.key === 'ArrowRight') next();
      else if (e.key === 'ArrowLeft') prev();
      else if (e.key === ' ') { e.preventDefault(); setPlaying((p) => !p); }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [solution, next, prev]);

  const done = solution && step === solution.moves.length;

  return (
    <div className="app">
      <header>
        <h1>Rubik's Cube Solver</h1>
        <p className="tagline">
          Kociemba two-phase algorithm written in C, compiled to WebAssembly. BFS-generated pruning
          tables + IDA* search, solving any cube in about 20 moves.
        </p>
      </header>

      <main>
        <section className="stage">
          <CubeView ref={cubeRef} state={cube} editable={editing} onStickerClick={paintSticker} />
          <p className="hint">
            {editing ? 'Click stickers on the cube or the net to paint them.' : 'Drag to rotate the view.'}
          </p>

          {solution && (
            <div className="solution">
              <div className="solution-head">
                <strong>{solution.moves.length} moves</strong>
                <span>
                  found in {solution.ms.toFixed(0)} ms · {solution.nodes.toLocaleString()} nodes searched
                </span>
              </div>
              <ol className="moves">
                {solution.moves.map((m, i) => (
                  <li key={i}>
                    <button
                      type="button"
                      className={i < step ? 'done' : i === step ? 'current' : ''}
                      onClick={() => jumpTo(i)}
                      title={`Jump to move ${i + 1}`}
                    >
                      {formatMove(m)}
                    </button>
                  </li>
                ))}
              </ol>
              <div className="transport">
                <button type="button" onClick={() => jumpTo(0)} disabled={busy || step === 0}>⏮</button>
                <button type="button" onClick={prev} disabled={busy || step === 0}>◀ Prev</button>
                <button type="button" className="primary" onClick={() => setPlaying((p) => !p)} disabled={done}>
                  {playing ? '❚❚ Pause' : '▶ Play'}
                </button>
                <button type="button" onClick={next} disabled={busy || done}>Next ▶</button>
                <select value={speed} onChange={(e) => setSpeed(Number(e.target.value))} aria-label="Speed">
                  {Object.entries(SPEEDS).map(([name, ms]) => <option key={name} value={ms}>{name}</option>)}
                </select>
              </div>
              <p className="step-label">{done ? 'Solved! 🎉' : `Step ${step} of ${solution.moves.length}`}</p>
            </div>
          )}
        </section>

        <aside className="panel">
          <div className="card">
            <h2>1. Set up a cube</h2>
            <div className="row">
              <button type="button" onClick={scramble} disabled={busy}>Random scramble</button>
              <button type="button" onClick={reset} disabled={busy}>Reset</button>
              <button
                type="button"
                className={editing ? 'active' : ''}
                onClick={() => { setEditing((e) => !e); clearSolution(); }}
                disabled={busy}
              >
                {editing ? 'Done editing' : 'Enter colours'}
              </button>
            </div>
            <form onSubmit={applyScrambleText} className="scramble-form">
              <input
                value={scrambleText}
                onChange={(e) => setScrambleText(e.target.value)}
                placeholder="Type a scramble, e.g. R U R' U' F2"
                aria-label="Scramble"
              />
              <button type="submit" disabled={busy}>Apply</button>
            </form>
            <FaceletEditor
              state={cube}
              editable={editing}
              paintColor={paintColor}
              onPaintColor={setPaintColor}
              onSticker={paintSticker}
            />
          </div>

          <div className="card">
            <h2>2. Turn it yourself</h2>
            <div className="turn-grid">
              {FACES.map((f) => ['', "'", '2'].map((s) => (
                <button key={f + s} type="button" onClick={() => turn(f + s)} disabled={busy || editing}>
                  {f + s}
                </button>
              )))}
            </div>
          </div>

          <div className="card">
            <h2>3. Solve</h2>
            <button type="button" className="primary wide" onClick={solve} disabled={!solver.ready || solving || busy}>
              {!solver.ready ? 'Loading solver…' : solving ? 'Searching…' : 'Solve'}
            </button>
            {solver.ready && (
              <p className="muted">Pruning tables built in {solver.initMs.toFixed(0)} ms (in a Web Worker).</p>
            )}
            {solver.error && <p className="message error">Could not load solver: {solver.error}</p>}
            {message && <p className={`message ${message.kind}`}>{message.text}</p>}
          </div>

          <details className="card how">
            <summary>How it works</summary>
            <p>
              <b>Phase 1</b> uses IDA* to reach the subgroup ⟨U, D, R2, L2, F2, B2⟩, where every piece is
              oriented and the middle-layer edges are in the middle layer. <b>Phase 2</b> solves the cube
              inside that subgroup.
            </p>
            <p>
              Both searches are guided by pruning tables, exact distances in smaller coordinate spaces
              that are generated with breadth-first search when the page loads. The solver keeps
              searching for shorter solutions until it reaches its node budget.
            </p>
          </details>
        </aside>
      </main>

      <footer>
        Built by Syed Azeemuddin Hussaini · C · WebAssembly · React · Three.js ·{' '}
        <a href="https://github.com/AzeemZsyed/rubiks-cube-solver">Source on GitHub</a>
      </footer>
    </div>
  );
}
