import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// GitHub Pages serves this project at https://<user>.github.io/rubiks-cube-solver/
// Override with BASE_PATH=/ for other hosts.
export default defineConfig({
  base: process.env.BASE_PATH ?? '/rubiks-cube-solver/',
  plugins: [react()],
  worker: { format: 'es' },
});
