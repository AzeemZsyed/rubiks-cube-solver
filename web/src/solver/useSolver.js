import { useCallback, useEffect, useRef, useState } from 'react';

/** React hook that owns the solver Web Worker. */
export function useSolver() {
  const workerRef = useRef(null);
  const pending = useRef(new Map());
  const nextId = useRef(1);
  const [status, setStatus] = useState({ ready: false, initMs: null, error: null });

  useEffect(() => {
    const worker = new Worker(new URL('./solver.worker.js', import.meta.url), { type: 'module' });
    workerRef.current = worker;
    worker.onmessage = ({ data }) => {
      if (data.type === 'ready') setStatus({ ready: true, initMs: data.initMs, error: null });
      else if (data.type === 'fatal') setStatus({ ready: false, initMs: null, error: data.error });
      else if (data.type === 'result') {
        const resolve = pending.current.get(data.id);
        pending.current.delete(data.id);
        resolve?.(data);
      }
    };
    return () => worker.terminate();
  }, []);

  const solve = useCallback((facelets, options) => {
    const id = nextId.current++;
    return new Promise((resolve) => {
      pending.current.set(id, resolve);
      workerRef.current.postMessage({ type: 'solve', id, facelets, options });
    });
  }, []);

  return { ...status, solve };
}
