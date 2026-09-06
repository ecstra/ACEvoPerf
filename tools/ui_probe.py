"""Time the UI's own JavaScript through the Cohtml inspector (`[ui] inspector_port` in the mod's
ini), without the V8 sampling profiler, which crashes the game. A probe script wraps
requestAnimationFrame, the engine event handlers, engine.call, the model updates and the forced
layout reads, and reports counts and milliseconds per second. Needs the `websockets` package.

  ui_probe.py record [--port 9444] [--out DIR] [--minutes 30]
      attaches to the menu view, installs the probe on every page, writes DIR/probe.jsonl and
      DIR/probe.log one line per second, and evaluates every new line of DIR/commands.txt in
      the page (a command channel for switches and experiments)
  ui_probe.py eval EXPR [--port 9444]
      runs one expression in the menu page and prints the value
"""
import argparse
import asyncio
import json
import os
import time
import urllib.request

import websockets

PROBE = r"""
(function () {
  if (window.__acevo) { window.__acevo.hook(); return 'present'; }
  const P = window.__acevo = { t: {}, c: {}, frames: 0, wrapOf: new WeakMap(), mute: {} };
  // Cohtml freezes performance.now() for the whole frame, Date.now() is the wall clock
  const now = () => Date.now();
  function acc(key, ms) { const e = P.t[key] || (P.t[key] = { n: 0, ms: 0, max: 0 }); e.n++; e.ms += ms; if (ms > e.max) e.max = ms; }
  function cnt(key) { P.c[key] = (P.c[key] || 0) + 1; }
  function timed(fn, key) {
    return function () { if (P.mute[key]) { cnt('muted:' + key); return; } const t0 = now(); try { return fn.apply(this, arguments); } finally { acc(key, now() - t0); } };
  }
  function wrapOnce(obj, name, make) {
    const fn = obj && obj[name];
    if (typeof fn !== 'function' || fn.__acevo) return;
    const w = make(fn); w.__acevo = true; obj[name] = w;
  }
  P.hook = function () {
    wrapOnce(window, 'requestAnimationFrame', (raf) => function (cb) {
      return raf.call(window, function (ts) { const t0 = now(); try { return cb(ts); } finally { acc('raf:' + (cb.name || 'anon'), now() - t0); } });
    });
    wrapOnce(Element.prototype, 'getBoundingClientRect', (fn) => function () { cnt('getBoundingClientRect'); const t0 = now(); try { return fn.apply(this, arguments); } finally { acc('gbcr', now() - t0); } });
    wrapOnce(window, 'getComputedStyle', (fn) => function () { cnt('getComputedStyle'); const t0 = now(); try { return fn.apply(window, arguments); } finally { acc('gcs', now() - t0); } });
    wrapOnce(Document.prototype, 'createElement', (fn) => function () { cnt('createElement'); return fn.apply(this, arguments); });
    const e = window.engine;
    if (!e) return;
    wrapOnce(e, 'AddOrRemoveOnHandler', (fn) => function (name, cb, ctx) {
      let w = P.wrapOf.get(cb);
      if (!w) { w = timed(cb, 'on:' + name); P.wrapOf.set(cb, w); }
      return fn.call(this, name, w, ctx);
    });
    wrapOnce(e, 'RemoveOnHandler', (fn) => function (name, cb, ctx) { return fn.call(this, name, P.wrapOf.get(cb) || cb, ctx); });
    wrapOnce(e, 'SendMessage', (fn) => function (name) { cnt('call:' + name); return fn.apply(this, arguments); });
    wrapOnce(e, 'TriggerEvent', (fn) => function (name) { cnt('trigger:' + name); return fn.apply(this, arguments); });
    wrapOnce(e, 'updateWholeModel', (fn) => function () { cnt('updateWholeModel'); return fn.apply(this, arguments); });
    wrapOnce(e, 'synchronizeModels', (fn) => timed(fn, 'synchronizeModels'));
  };
  P.hook();
  (function tick() { P.frames++; window.requestAnimationFrame(tick); })();
  P.flush = function () {
    P.hook();
    const out = { t: P.t, c: P.c, frames: P.frames, href: location.pathname,
      path: (window.ModelMenuState || {}).path || '', elements: document.getElementsByTagName('*').length };
    P.t = {}; P.c = {}; P.frames = 0;
    return JSON.stringify(out);
  };
  return 'installed';
})()
"""


def list_targets(port: int) -> list[dict]:
    with urllib.request.urlopen(f"http://localhost:{port}/json/list", timeout=5) as r:
        return json.load(r)


def menu_target(port: int) -> dict:
    targets = list_targets(port)
    menu = [t for t in targets if "uiresources" in t.get("url", "")] or [t for t in targets if t.get("type") == "page"]
    if not menu:
        raise SystemExit("no page target")
    return menu[0]


class Cdp:
    def __init__(self, ws) -> None:
        self.ws = ws
        self.next_id = 1
        self.pending: dict[int, asyncio.Future] = {}

    async def reader(self) -> None:
        async for raw in self.ws:
            msg = json.loads(raw)
            if "id" in msg and msg["id"] in self.pending:
                self.pending.pop(msg["id"]).set_result(msg)

    async def call(self, method: str, params: dict | None = None, timeout: float = 20) -> dict:
        mid = self.next_id
        self.next_id += 1
        fut = asyncio.get_running_loop().create_future()
        self.pending[mid] = fut
        await self.ws.send(json.dumps({"id": mid, "method": method, "params": params or {}}))
        msg = await asyncio.wait_for(fut, timeout)
        if "error" in msg:
            raise RuntimeError(f"{method}: {msg['error']}")
        return msg.get("result", {})

    async def evaluate(self, expression: str):
        res = await self.call("Runtime.evaluate", {"expression": expression, "returnByValue": True})
        r = res.get("result", {})
        if "exceptionDetails" in res:
            return f"exception: {res['exceptionDetails'].get('text')} {r.get('description', '')}"
        return r.get("value", r.get("description"))


def format_report(rep: dict) -> str:
    t = rep["t"]
    c = rep["c"]
    top = sorted(t.items(), key=lambda kv: -kv[1]["ms"])[:8]
    parts = [f"{k} {v['ms']:.1f}ms/{v['n']} max {v['max']:.1f}" for k, v in top]
    calls = sum(v for k, v in c.items() if k.startswith("call:"))
    counts = f"calls {calls} uwm {c.get('updateWholeModel', 0)} gbcr {c.get('getBoundingClientRect', 0)} gcs {c.get('getComputedStyle', 0)} create {c.get('createElement', 0)}"
    return f"{rep['href']} {rep['path']} el={rep['elements']} fps={rep['frames']} | {counts} | " + " ; ".join(parts)


async def record(port: int, out: str, minutes: float) -> None:
    os.makedirs(out, exist_ok=True)
    target = menu_target(port)
    print("attaching to", target["url"], flush=True)
    async with websockets.connect(f"ws://localhost:{port}/devtools/page/{target['id']}", max_size=None, ping_interval=None) as ws:
        cdp = Cdp(ws)
        reader = asyncio.create_task(cdp.reader())
        await cdp.call("Runtime.enable")
        for method, params in (("Page.enable", {}), ("Page.addScriptToEvaluateOnNewDocument", {"source": PROBE})):
            try:
                res = await cdp.call(method, params)
                print(method, "ok", res, flush=True)
            except Exception as e:
                print(method, "failed:", e, flush=True)
        print("install:", await cdp.evaluate(PROBE), flush=True)
        raw = open(os.path.join(out, "probe.jsonl"), "a", encoding="utf-8")
        log = open(os.path.join(out, "probe.log"), "a", encoding="utf-8")
        end = time.time() + minutes * 60
        cmd_path = os.path.join(out, "commands.txt")
        done_cmds = 0
        while time.time() < end:
            await asyncio.sleep(1)
            try:
                if os.path.exists(cmd_path):
                    with open(cmd_path, encoding="utf-8") as f:
                        cmds = [l.strip() for l in f if l.strip()]
                    for expr in cmds[done_cmds:]:
                        result = await cdp.evaluate(expr)
                        line = f"{time.strftime('%H:%M:%S')} CMD {expr[:120]} -> {result}"
                        print(line, flush=True)
                        log.write(line + "\n")
                    done_cmds = len(cmds)
                value = await cdp.evaluate("window.__acevo ? window.__acevo.flush() : 'missing'")
            except Exception as e:
                print("lost:", e, flush=True)
                break
            stamp = time.strftime("%H:%M:%S")
            if value == "missing" or not isinstance(value, str) or not value.startswith("{"):
                installed = await cdp.evaluate(PROBE)
                line = f"{stamp} probe {value} -> {installed}"
            else:
                rep = json.loads(value)
                raw.write(json.dumps({"time": stamp, **rep}) + "\n")
                line = f"{stamp} {format_report(rep)}"
            print(line, flush=True)
            log.write(line + "\n")
            log.flush()
            raw.flush()
        reader.cancel()


async def evaluate_once(port: int, expression: str) -> None:
    target = menu_target(port)
    async with websockets.connect(f"ws://localhost:{port}/devtools/page/{target['id']}", max_size=None, ping_interval=None) as ws:
        cdp = Cdp(ws)
        reader = asyncio.create_task(cdp.reader())
        await cdp.call("Runtime.enable")
        print(await cdp.evaluate(expression))
        reader.cancel()


def main() -> None:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    rec = sub.add_parser("record")
    rec.add_argument("--port", type=int, default=9444)
    rec.add_argument("--out", default="ui_probe")
    rec.add_argument("--minutes", type=float, default=30)
    ev = sub.add_parser("eval")
    ev.add_argument("expr")
    ev.add_argument("--port", type=int, default=9444)
    args = ap.parse_args()
    if args.cmd == "record":
        asyncio.run(record(args.port, args.out, args.minutes))
    else:
        asyncio.run(evaluate_once(args.port, args.expr))


if __name__ == "__main__":
    main()
