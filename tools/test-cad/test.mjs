// Checks the pure part of ps5/cad.html (code extraction, OpenSCAD messages, the OpenAI stream).
//   node tools/test-cad/test.mjs
import fs from 'fs'; import assert from 'assert/strict';
const html = fs.readFileSync(new URL('../../ps5/cad.html', import.meta.url), 'utf8');
const code = html.slice(html.indexOf('// ---- cad logic'), html.indexOf('// ---- end cad logic'));
const {extractCode, proseOf, scadProblems, messagesFor, sseDeltas, slug} =
  new Function(code + ';return {extractCode, proseOf, scadProblems, messagesFor, sseDeltas, slug};')();
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
