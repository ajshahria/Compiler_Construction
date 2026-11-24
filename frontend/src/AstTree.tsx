// AstTree.tsx - draws the AST as a tree diagram (bonus: AST visualization).
//
// All three compilers print the AST (--ast) as an indented list, two spaces
// per level, e.g.
//     While (line 10)
//       BinaryOp >
//         Identifier x
// parseAst turns that text into nodes; layoutTree places each node centred
// above its children; the SVG draws boxes coloured by node category and
// curved edges. Clicking a box jumps to its source line; the small round
// button under a box folds or unfolds its subtree. Drag to pan, Ctrl+wheel
// or the buttons to zoom.
import { useEffect, useMemo, useRef, useState } from "react";

interface AstNode {
  id: number;
  kind: string;      // first word, e.g. "BinaryOp"
  detail: string;    // the rest, e.g. ">" or "int x"
  line: number | null;
  children: AstNode[];
}

/* Indented --ast text -> tree. Several top-level lines get a common root. */
export function parseAst(text: string): AstNode | null {
  let nextId = 0;
  const roots: AstNode[] = [];
  const stack: { depth: number; node: AstNode }[] = [];
  for (const raw of text.split(/\r?\n/)) {
    if (!raw.trim() || /^===.*===\s*$/.test(raw.trim())) continue;
    const indent = raw.length - raw.trimStart().length;
    let body = raw.trim();
    let line: number | null = null;
    const m = body.match(/\s*\(line (\d+)\)\s*$/);
    if (m) {
      line = Number(m[1]);
      body = body.slice(0, m.index).trim();
    }
    const space = body.indexOf(" ");
    const kind = (space < 0 ? body : body.slice(0, space)).replace(/:$/, "");
    const detail = space < 0 ? "" : body.slice(space + 1).trim();
    const node: AstNode = { id: nextId++, kind, detail, line, children: [] };
    while (stack.length && stack[stack.length - 1].depth >= indent) stack.pop();
    if (stack.length) stack[stack.length - 1].node.children.push(node);
    else roots.push(node);
    stack.push({ depth: indent, node });
  }
  if (roots.length === 0) return null;
  if (roots.length === 1) return roots[0];
  return { id: nextId, kind: "Program", detail: "", line: null, children: roots };
}

type Category = "stmt" | "expr" | "lit" | "id" | "part";

function category(kind: string): Category {
  if (/Lit(eral)?$|^(Int|Float|Bool|String|Char)/.test(kind)) return "lit";
  if (/^(Identifier|Ident|Id|Var|Variable)$/.test(kind)) return "id";
  if (/Op$|^(Binary|Unary|Ternary|Cast|Conversion|IntToFloat)/.test(kind)) return "expr";
  if (/^(Program|Block|Condition|Body|Then|Else|Init|Update|Cases?|Default|Args?|Arguments)$/.test(kind)) return "part";
  return "stmt";
}

const CHAR_W = 7.4;        // width of one character at 12px monospace
const BOX_H = 40;
const GAP_X = 16;
const GAP_Y = 46;
const MAX_DETAIL = 26;

interface Placed {
  node: AstNode;
  x: number;           // centre
  y: number;           // top
  w: number;
  folded: boolean;
  parent: Placed | null;
}

function shortDetail(d: string) {
  return d.length > MAX_DETAIL ? d.slice(0, MAX_DETAIL - 1) + "…" : d;
}

function boxWidth(n: AstNode) {
  const lineLabel = n.line != null ? `line ${n.line}` : "";
  const top = n.kind.length + (lineLabel ? lineLabel.length + 2 : 0);
  return Math.max(64, Math.max(top, shortDetail(n.detail).length) * CHAR_W + 22);
}

/* Places every visible node: leaves left to right, each parent centred over
 * its children. Returns the nodes and the drawing size. */
function layoutTree(root: AstNode, folded: Set<number>) {
  const placed: Placed[] = [];
  let maxBottom = 0;

  // width of the subtree that hangs under a node
  const span = new Map<number, number>();
  const measure = (n: AstNode): number => {
    const own = boxWidth(n);
    const kids = folded.has(n.id) ? [] : n.children;
    const kidsWidth = kids.reduce((sum, c) => sum + measure(c), 0) + GAP_X * Math.max(0, kids.length - 1);
    const s = Math.max(own, kidsWidth);
    span.set(n.id, s);
    return s;
  };
  const total = measure(root);

  const place = (n: AstNode, left: number, depth: number, parent: Placed | null) => {
    const s = span.get(n.id)!;
    const p: Placed = {
      node: n, x: left + s / 2, y: depth * (BOX_H + GAP_Y), w: boxWidth(n),
      folded: folded.has(n.id), parent,
    };
    placed.push(p);
    maxBottom = Math.max(maxBottom, p.y + BOX_H);
    const kids = folded.has(n.id) ? [] : n.children;
    const kidsWidth = kids.reduce((sum, c) => sum + span.get(c.id)!, 0) + GAP_X * Math.max(0, kids.length - 1);
    let x = left + (s - kidsWidth) / 2;
    for (const c of kids) {
      place(c, x, depth + 1, p);
      x += span.get(c.id)! + GAP_X;
    }
  };
  place(root, 0, 0, null);
  return { placed, width: total, height: maxBottom + 14 };
}

interface Props {
  text: string;
  onJump: (line: number) => void;
}

const PAD = 24;

export function AstTree({ text, onJump }: Props) {
  const root = useMemo(() => parseAst(text), [text]);
  const [folded, setFolded] = useState<Set<number>>(new Set());
  const [zoom, setZoom] = useState(1);
  const viewRef = useRef<HTMLDivElement>(null);
  const [panning, setPanning] = useState(false);

  useEffect(() => setFolded(new Set()), [text]);   // a new program starts unfolded

  const layout = useMemo(() => (root ? layoutTree(root, folded) : null), [root, folded]);

  // After a zoom change that asks for it, scroll so the root node is centred.
  const centreRoot = useRef(false);
  useEffect(() => {
    const view = viewRef.current;
    if (!centreRoot.current || !view || !layout) return;
    centreRoot.current = false;
    view.scrollLeft = (layout.placed[0].x + PAD) * zoom - view.clientWidth / 2;
    view.scrollTop = 0;
  }, [zoom, layout]);

  // Fit: the whole tree in the view (never enlarged past 100%).
  const fit = () => {
    const view = viewRef.current;
    if (!view || !layout) return;
    const z = Math.min(1, (view.clientWidth - 20) / (layout.width + 2 * PAD), (view.clientHeight - 20) / (layout.height + 2 * PAD));
    centreRoot.current = true;
    setZoom(clampZoom(z));
  };

  // A new tree opens readable: as wide as the view allows, but never below
  // 60% (a wide program then scrolls sideways), with its root in the middle.
  useEffect(() => {
    const view = viewRef.current;
    if (!view || !layout) return;
    const fitWidth = (view.clientWidth - 20) / (layout.width + 2 * PAD);
    centreRoot.current = true;
    setZoom(clampZoom(Math.min(1, Math.max(0.6, fitWidth))));
  }, [root]);   // eslint-disable-line react-hooks/exhaustive-deps

  // Ctrl + wheel zooms (a plain wheel scrolls).
  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;
    const onWheel = (e: WheelEvent) => {
      if (!e.ctrlKey) return;
      e.preventDefault();
      setZoom((z) => clampZoom(z * (e.deltaY < 0 ? 1.1 : 1 / 1.1)));
    };
    view.addEventListener("wheel", onWheel, { passive: false });
    return () => view.removeEventListener("wheel", onWheel);
  }, []);

  // Drag on the background to pan.
  const startPan = (e: React.PointerEvent) => {
    if ((e.target as Element).closest(".ast-node, .ast-toggle")) return;
    const view = viewRef.current!;
    const start = { x: e.clientX, y: e.clientY, left: view.scrollLeft, top: view.scrollTop };
    setPanning(true);
    const move = (ev: PointerEvent) => {
      view.scrollLeft = start.left - (ev.clientX - start.x);
      view.scrollTop = start.top - (ev.clientY - start.y);
    };
    const up = () => {
      setPanning(false);
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", up);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  };

  if (!root || !layout) return <div className="ast-empty">&lt;no AST&gt;</div>;

  const toggle = (id: number) =>
    setFolded((f) => {
      const next = new Set(f);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return next;
    });

  const w = (layout.width + 2 * PAD) * zoom;
  const h = (layout.height + 2 * PAD) * zoom;

  return (
    <>
      <div ref={viewRef} className={`ast-view ${panning ? "panning" : ""}`} onPointerDown={startPan}>
        <svg width={w} height={h} viewBox={`${-PAD} ${-PAD} ${layout.width + 2 * PAD} ${layout.height + 2 * PAD}`}
             role="img" aria-label="Abstract syntax tree diagram">
          {layout.placed.map((p) =>
            p.parent ? (
              <path
                key={`e${p.node.id}`}
                className="ast-edge"
                d={`M ${p.parent.x} ${p.parent.y + BOX_H} C ${p.parent.x} ${p.parent.y + BOX_H + GAP_Y / 2}, ${p.x} ${p.y - GAP_Y / 2}, ${p.x} ${p.y}`}
              />
            ) : null,
          )}
          {layout.placed.map((p) => {
            const n = p.node;
            const cat = category(n.kind);
            const detail = shortDetail(n.detail);
            const left = p.x - p.w / 2;
            return (
              <g key={n.id}>
                <g
                  className="ast-node"
                  onClick={() => n.line != null && onJump(n.line)}
                >
                  <title>{`${n.kind}${n.detail ? " " + n.detail : ""}${n.line != null ? ` (line ${n.line}) - click to show it in the editor` : ""}`}</title>
                  <rect
                    className="box"
                    x={left} y={p.y} width={p.w} height={BOX_H} rx={10}
                    fill={`var(--ast-${cat})`} stroke={`var(--ast-${cat}-edge)`} strokeWidth={1.5}
                  />
                  <text x={left + 11} y={p.y + (detail ? 16 : 25)} fontSize={12} className="ast-kind">{n.kind}</text>
                  {n.line != null && (
                    <text x={left + p.w - 10} y={p.y + (detail ? 16 : 25)} fontSize={10.5} textAnchor="end" className="ast-line-no">
                      {`line ${n.line}`}
                    </text>
                  )}
                  {detail && (
                    <text x={left + 11} y={p.y + 32} fontSize={12}>{detail}</text>
                  )}
                </g>
                {n.children.length > 0 && (
                  <g className="ast-toggle" onClick={() => toggle(n.id)}>
                    <title>{p.folded ? `Show ${n.children.length} child node(s)` : "Fold this subtree"}</title>
                    <circle cx={p.x} cy={p.y + BOX_H + 1} r={7.5} />
                    <text x={p.x} y={p.y + BOX_H + 5}>{p.folded ? "+" : "−"}</text>
                  </g>
                )}
              </g>
            );
          })}
        </svg>
      </div>
      <div className="ast-zoom" aria-label="Zoom">
        <button onClick={() => setZoom((z) => clampZoom(z / 1.2))} title="Zoom out (Ctrl + wheel)">−</button>
        <span>{Math.round(zoom * 100)}%</span>
        <button onClick={() => setZoom((z) => clampZoom(z * 1.2))} title="Zoom in (Ctrl + wheel)">+</button>
        <button onClick={fit} title="Fit the whole tree">Fit</button>
        <button onClick={() => { centreRoot.current = true; setZoom(1); }} title="Actual size">1:1</button>
      </div>
      <div className="ast-legend" aria-hidden="true">
        <span><i style={{ background: "var(--ast-stmt)", borderColor: "var(--ast-stmt-edge)" }} />statement</span>
        <span><i style={{ background: "var(--ast-expr)", borderColor: "var(--ast-expr-edge)" }} />operator</span>
        <span><i style={{ background: "var(--ast-id)", borderColor: "var(--ast-id-edge)" }} />identifier</span>
        <span><i style={{ background: "var(--ast-lit)", borderColor: "var(--ast-lit-edge)" }} />literal</span>
      </div>
    </>
  );
}

function clampZoom(z: number) {
  return Math.min(2.5, Math.max(0.2, Math.round(z * 100) / 100));
}
