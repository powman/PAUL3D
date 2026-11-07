// Checks the pure part of ps5/cad.html (code extraction, OpenSCAD messages, the OpenAI stream).
//   node tools/test-cad/test.mjs
import fs from 'fs'; import assert from 'assert/strict';
const html = fs.readFileSync(new URL('../../ps5/cad.html', import.meta.url), 'utf8');
const code = html.slice(html.indexOf('// ---- cad logic'), html.indexOf('// ---- end cad logic'));
const {extractCode, proseOf, scadProblems, messagesFor, sseDeltas, slug, paul3dAsk} =
  new Function(code + ';return {extractCode, proseOf, scadProblems, messagesFor, sseDeltas, slug, paul3dAsk};')();
const t = (name, f) => { f(); console.log('ok  ', name); };

t('code block tagged openscad', () => assert.equal(extractCode('Aqui:\n```openscad\ncube(10);\n```\nPronto.'), 'cube(10);'));
t('last openscad block wins', () => assert.equal(extractCode('```openscad\ncube(1);\n```\n```openscad\ncube(2);\n```'), 'cube(2);'));
t('untagged block', () => assert.equal(extractCode('```\nsphere(3);\n```'), 'sphere(3);'));
t('think block is ignored', () => assert.equal(extractCode('<think>```openscad\nbad();\n```</think>```scad\ncube(3);\n```'), 'cube(3);'));
t('unfinished block while streaming', () => assert.equal(extractCode('```openscad\nw=10;\ncube(w'), 'w=10;\ncube(w'));
t('bare code without fences', () => assert.equal(extractCode('cube([10,10,2]);'), 'cube([10,10,2]);'));
t('plain prose is not code', () => assert.equal(extractCode('Não sei fazer isso.'), ''));
t('prose without code', () => assert.equal(proseOf('```openscad\ncube(1);\n```\nCaixa simples.'), 'Caixa simples.'));
t('parser error is a problem', () => assert.deepEqual(scadProblems(['ERROR: Parser error: syntax error in file /in.scad, line 2'], false), ['ERROR: Parser error: syntax error in file /in.scad, line 2']));
t('empty result is a problem', () => assert.equal(scadProblems(['Current top level object is empty.'], false).length, 1));
t('2D result is a problem', () => assert.ok(scadProblems(['Current top level object is a 2D object:'], true).some(p => /2D/.test(p))));
t('font noise is not a problem', () => assert.deepEqual(scadProblems(['Fontconfig error: x', 'ECHO: 1'], true), []));
t('unknown module warning counts', () => assert.equal(scadProblems(["WARNING: Ignoring unknown module 'roundedBox'"], true).length, 1));
t('fix message carries code and errors', () => { const m = messagesFor('fix', {code: 'cube(', errors: ['ERROR: x']}); assert.equal(m.length, 2); assert.match(m[1].content, /cube\(/); assert.match(m[1].content, /ERROR: x/); });
t('change message carries current code', () => assert.match(messagesFor('change', {prompt: 'mais alto', code: 'cube(1);'})[1].content, /cube\(1\);[\s\S]*mais alto/));
t('stream deltas, split line kept', () => {
  const a = sseDeltas('data: {"choices":[{"delta":{"content":"cu"}}]}\n\ndata: {"choices":[{"delta":{"content":"be"}}]}\ndata: {"choi');
  assert.equal(a.text, 'cube'); assert.equal(a.rest, 'data: {"choi'); assert.equal(a.done, false);
  const b = sseDeltas(a.rest + 'ces":[{"delta":{"reasoning_content":"hm"}}]}\ndata: [DONE]\n');
  assert.equal(b.reasoning, 'hm'); assert.equal(b.done, true);
});
t('file name from prompt', () => assert.equal(slug('Caixa 60 × 40 com Tampa!'), 'caixa-60-40-com-tampa'));

// ---- paul3dAsk: the console's own text engine (POST /api/text, then GET /api/text/<id>)
const json = (status, body) => ({ok: status < 400, status, json: async () => body});
function fakeServer(steps) {  // steps: what GET answers, one per call; POST answers {id:'t1'} unless steps.post is set
  const calls = [];
  const fetchFn = async (url, opts = {}) => {
    calls.push((opts.method || 'GET') + ' ' + url);
    if (opts.method === 'POST') return typeof steps.post === 'function' ? steps.post() : (steps.post || json(202, {id: 't1'}));
    const s = steps.get.shift();
    if (s instanceof Error) throw s;
    return s;
  };
  return {fetchFn, calls};
}
const noSleep = async () => {};
const ta = async (name, f) => { try { await f(); console.log('ok  ', name); } catch (e) { console.log('FAIL', name, e.message); process.exitCode = 1; } };
await ta('paul3dAsk: partial text, then the answer', async () => {
  const srv = fakeServer({get: [json(200, {status: 'loading', text: ''}), json(200, {status: 'generating', text: 'cube('}),
    json(200, {status: 'done', text: 'cube(1);'})]});
  const parts = [];
  const out = await paul3dAsk([{role: 'user', content: 'x'}], {fetchFn: srv.fetchFn, sleep: noSleep, onPartial: p => parts.push(p)});
  assert.equal(out, 'cube(1);'); assert.deepEqual(parts, ['cube(', 'cube(1);']);
  assert.equal(srv.calls[0], 'POST /api/text'); assert.equal(srv.calls[1], 'GET /api/text/t1');
});
await ta('paul3dAsk: the title restarting (network errors) is waited out', async () => {
  const srv = fakeServer({get: [new TypeError('Failed to fetch'), new TypeError('Failed to fetch'), new TypeError('Failed to fetch'),
    json(200, {status: 'done', text: 'ok'})]});
  assert.equal(await paul3dAsk([], {fetchFn: srv.fetchFn, sleep: noSleep}), 'ok');
});
await ta('paul3dAsk: a POST lost while restarting is sent again', async () => {
  let n = 0;
  const srv = fakeServer({post: () => { if (n++ < 2) throw new TypeError('Failed to fetch'); return json(202, {id: 't9'}); },
    get: [json(200, {status: 'done', text: 'yes'})]});
  assert.equal(await paul3dAsk([], {fetchFn: srv.fetchFn, sleep: noSleep}), 'yes');
});
await ta('paul3dAsk: no model is MISSING_MODEL', async () => {
  const srv = fakeServer({post: json(409, {error: 'missing_model'}), get: []});
  await assert.rejects(paul3dAsk([], {fetchFn: srv.fetchFn, sleep: noSleep}), /MISSING_MODEL/);
});
await ta('paul3dAsk: a failed answer carries its error', async () => {
  const srv = fakeServer({get: [json(200, {status: 'failed', text: '', error: 'the prompt is longer than the context'})]});
  await assert.rejects(paul3dAsk([], {fetchFn: srv.fetchFn, sleep: noSleep}), /longer than the context/);
});
await ta('paul3dAsk: gives up after 60 s without the console', async () => {
  let clock = 0;
  const errs = Array.from({length: 200}, () => new TypeError('Failed to fetch'));
  const srv = fakeServer({get: errs});
  await assert.rejects(paul3dAsk([], {fetchFn: srv.fetchFn, sleep: async ms => { clock += ms; }, now: () => clock}), /console/);
});
