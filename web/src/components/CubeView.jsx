import { forwardRef, useEffect, useImperativeHandle, useRef } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';

import { STICKERS, applyMove, moveRotation } from '../cube/model.js';
import { FACE_COLORS } from '../cube/colors.js';

const AXES = ['x', 'y', 'z'];

function roundedSquare(size, radius) {
  const h = size / 2;
  const s = new THREE.Shape();
  s.moveTo(-h + radius, -h);
  s.lineTo(h - radius, -h);
  s.quadraticCurveTo(h, -h, h, -h + radius);
  s.lineTo(h, h - radius);
  s.quadraticCurveTo(h, h, h - radius, h);
  s.lineTo(-h + radius, h);
  s.quadraticCurveTo(-h, h, -h, h - radius);
  s.lineTo(-h, -h + radius);
  s.quadraticCurveTo(-h, -h, -h + radius, -h);
  return new THREE.ShapeGeometry(s, 6);
}

const easeInOut = (t) => (t < 0.5 ? 2 * t * t : 1 - (-2 * t + 2) ** 2 / 2);

/**
 * 3D Rubik's Cube built from 26 cubies. Colours always come from the
 * facelet string; turns are animated by temporarily attaching one layer of
 * cubies to a pivot, then snapping everything back and repainting.
 */
const CubeView = forwardRef(function CubeView({ state, onStickerClick, editable }, ref) {
  const mountRef = useRef(null);
  const sceneRef = useRef(null); // { root, cubies, stickerMeshes, painted, animation, ... }
  const clickRef = useRef(onStickerClick);
  clickRef.current = onStickerClick;
  const editableRef = useRef(editable);
  editableRef.current = editable;

  // ---- one-time scene setup ---------------------------------------------
  useEffect(() => {
    const mount = mountRef.current;
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(32, 1, 0.1, 100);
    camera.position.set(5.2, 4.4, 6.6);

    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    mount.appendChild(renderer.domElement);

    scene.add(new THREE.AmbientLight(0xffffff, 1.6));
    const key = new THREE.DirectionalLight(0xffffff, 1.4);
    key.position.set(5, 8, 6);
    scene.add(key);

    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enablePan = false;
    controls.enableDamping = true;
    controls.minDistance = 6;
    controls.maxDistance = 16;

    const root = new THREE.Group();
    scene.add(root);

    const bodyGeo = new THREE.BoxGeometry(0.97, 0.97, 0.97);
    const bodyMat = new THREE.MeshStandardMaterial({ color: 0x15161a, roughness: 0.6 });
    const stickerGeo = roundedSquare(0.84, 0.12);
    const zAxis = new THREE.Vector3(0, 0, 1);

    const cubies = [];
    const stickerMeshes = new Array(54);
    for (let x = -1; x <= 1; x++)
      for (let y = -1; y <= 1; y++)
        for (let z = -1; z <= 1; z++) {
          if (!x && !y && !z) continue;
          const group = new THREE.Group();
          group.position.set(x, y, z);
          group.userData.home = [x, y, z];
          group.add(new THREE.Mesh(bodyGeo, bodyMat));
          for (const s of STICKERS) {
            if (s.pos[0] !== x || s.pos[1] !== y || s.pos[2] !== z) continue;
            const n = new THREE.Vector3(...s.normal);
            const mesh = new THREE.Mesh(
              stickerGeo,
              new THREE.MeshStandardMaterial({ roughness: 0.35, side: THREE.DoubleSide }),
            );
            mesh.position.copy(n.clone().multiplyScalar(0.487));
            mesh.quaternion.setFromUnitVectors(zAxis, n);
            mesh.userData.index = s.index;
            stickerMeshes[s.index] = mesh;
            group.add(mesh);
          }
          root.add(group);
          cubies.push(group);
        }

    const ctx = { scene, camera, renderer, controls, root, cubies, stickerMeshes, painted: null, animation: null };
    sceneRef.current = ctx;

    // Resize with the container
    const resize = () => {
      const { clientWidth: w, clientHeight: h } = mount;
      renderer.setSize(w, h);
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
    };
    const ro = new ResizeObserver(resize);
    ro.observe(mount);
    resize();

    // Click (not drag) on a sticker -> onStickerClick(index)
    const raycaster = new THREE.Raycaster();
    let down = null;
    const onDown = (e) => { down = { x: e.clientX, y: e.clientY }; };
    const onUp = (e) => {
      if (!down || !editableRef.current) return;
      const moved = Math.hypot(e.clientX - down.x, e.clientY - down.y);
      down = null;
      if (moved > 5) return;
      const rect = renderer.domElement.getBoundingClientRect();
      const pointer = new THREE.Vector2(
        ((e.clientX - rect.left) / rect.width) * 2 - 1,
        -((e.clientY - rect.top) / rect.height) * 2 + 1,
      );
      raycaster.setFromCamera(pointer, camera);
      const hit = raycaster.intersectObjects(stickerMeshes, false)[0];
      if (hit) clickRef.current?.(hit.object.userData.index);
    };
    renderer.domElement.addEventListener('pointerdown', onDown);
    renderer.domElement.addEventListener('pointerup', onUp);

    // Render loop
    let frame;
    const tick = (now) => {
      const anim = ctx.animation;
      if (anim) {
        if (anim.start === null) anim.start = now;
        const t = Math.min(1, (now - anim.start) / anim.duration);
        anim.pivot.rotation[AXES[anim.axis]] = anim.angle * easeInOut(t);
        if (t >= 1) finishAnimation(ctx);
      }
      controls.update();
      renderer.render(scene, camera);
      frame = requestAnimationFrame(tick);
    };
    frame = requestAnimationFrame(tick);

    return () => {
      cancelAnimationFrame(frame);
      ro.disconnect();
      controls.dispose();
      renderer.dispose();
      mount.removeChild(renderer.domElement);
    };
  }, []);

  // ---- repaint whenever the state prop changes ------------------------------
  useEffect(() => {
    const ctx = sceneRef.current;
    if (!ctx || ctx.animation || ctx.painted === state) return;
    paint(ctx, state);
  }, [state]);

  useImperativeHandle(ref, () => ({
    /** Animate one move; resolves with the new facelet string. */
    animateMove(move, duration = 300) {
      const ctx = sceneRef.current;
      return new Promise((resolve) => {
        const { axis, layer, angle } = moveRotation(move);
        const pivot = new THREE.Group();
        ctx.root.add(pivot);
        for (const c of ctx.cubies) if (c.userData.home[axis] === layer) pivot.attach(c);
        ctx.animation = { pivot, axis, angle, duration, start: null, move, resolve };
      });
    },
  }));

  return <div className={`cube-view${editable ? ' editing' : ''}`} ref={mountRef} />;
});

function paint(ctx, state) {
  for (let i = 0; i < 54; i++) ctx.stickerMeshes[i].material.color.set(FACE_COLORS[state[i]]);
  ctx.painted = state;
}

function finishAnimation(ctx) {
  const { pivot, move, resolve } = ctx.animation;
  // Snap every cubie back home and repaint from the model, so floating-point
  // drift can never accumulate across hundreds of turns.
  for (const c of [...pivot.children]) {
    ctx.root.add(c);
    c.position.set(...c.userData.home);
    c.rotation.set(0, 0, 0);
  }
  ctx.root.remove(pivot);
  const next = applyMove(ctx.painted, move);
  paint(ctx, next);
  ctx.animation = null;
  resolve(next);
}

export default CubeView;
