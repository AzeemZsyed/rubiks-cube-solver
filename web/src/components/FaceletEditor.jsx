import { FACES, countColors } from '../cube/model.js';
import { FACE_COLORS, FACE_NAMES } from '../cube/colors.js';

// Where each face sits in the unfolded net (column, row in units of 3 stickers)
const NET = { U: [1, 0], L: [0, 1], F: [1, 1], R: [2, 1], B: [3, 1], D: [1, 2] };

/** Unfolded 2D net. Click a sticker to paint it with the selected colour. */
export default function FaceletEditor({ state, paintColor, onPaintColor, onSticker, editable }) {
  const counts = countColors(state);

  return (
    <div className="editor">
      <div className="net" role="grid" aria-label="Cube net">
        {FACES.map((face, f) => {
          const [col, row] = NET[face];
          return (
            <div key={face} className="net-face" style={{ gridColumn: col + 1, gridRow: row + 1 }}>
              {Array.from({ length: 9 }, (_, i) => {
                const index = f * 9 + i;
                const isCenter = i === 4;
                return (
                  <button
                    key={i}
                    type="button"
                    className={`net-sticker${isCenter ? ' center' : ''}`}
                    style={{ background: FACE_COLORS[state[index]] }}
                    disabled={!editable || isCenter}
                    onClick={() => onSticker(index)}
                    aria-label={`${face}${i + 1}: ${FACE_NAMES[state[index]]}`}
                  >
                    {isCenter ? face : ''}
                  </button>
                );
              })}
            </div>
          );
        })}
      </div>

      {editable && (
        <div className="palette">
          {FACES.map((face) => (
            <button
              key={face}
              type="button"
              className={`swatch${paintColor === face ? ' selected' : ''}`}
              style={{ background: FACE_COLORS[face] }}
              onClick={() => onPaintColor(face)}
              title={FACE_NAMES[face]}
            >
              <span className={counts[face] === 9 ? 'count ok' : 'count bad'}>{counts[face]}/9</span>
            </button>
          ))}
        </div>
      )}
    </div>
  );
}
