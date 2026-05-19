/**
 * Tests for dsqlex-c Node.js bindings.
 */
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const { parse, evalAST, evalString } = require('../src/index.js');

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    console.log(`  ${name}... OK`);
    passed++;
  } catch (e) {
    console.log(`  ${name}... FAIL: ${e.message}`);
    failed++;
  }
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg || 'Assertion failed');
}

function assertEqual(a, b) {
  if (a !== b) throw new Error(`Expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`);
}

// Tests
test('simple field', () => {
  assertEqual(evalString('field1', { field1: '42' }), '42');
});

test('arithmetic', () => {
  const ctx = { a: 10, b: 3 };
  assertEqual(evalString('a + b', ctx), '13');
  assertEqual(evalString('a - b', ctx), '7');
  assertEqual(evalString('a * b', ctx), '30');
});

test('string literal', () => {
  assertEqual(evalString("'hello'", {}), 'hello');
});

test('boolean literals', () => {
  assertEqual(evalString('TRUE', {}), true);
  assertEqual(evalString('FALSE', {}), false);
});

test('null literal', () => {
  assertEqual(evalString('NULL', {}), null);
});

test('comparison', () => {
  assertEqual(evalString('a < b', { a: 10, b: 20 }), true);
  assertEqual(evalString('a > b', { a: 10, b: 20 }), false);
});

test('CASE expression', () => {
  const ctx = { status: 'active', amount: 100 };
  const result = evalString("CASE WHEN status = 'active' THEN amount ELSE 0 END", ctx);
  assertEqual(result, '100');
});

test('ROUND function', () => {
  assertEqual(evalString('ROUND(3.14159, 2)', {}), '3.14');
  assertEqual(evalString('ROUND(2.555, 2)', {}), '2.56');
});

test('COALESCE function', () => {
  assertEqual(evalString('COALESCE(NULL, NULL, 42)', {}), '42');
  assertEqual(evalString("COALESCE(NULL, 'hello')", {}), 'hello');
  assertEqual(evalString('COALESCE(NULL, NULL)', {}), null);
});

test('UPPER/LOWER functions', () => {
  assertEqual(evalString("UPPER('hello')", {}), 'HELLO');
  assertEqual(evalString("LOWER('HELLO')", {}), 'hello');
});

test('IN operator', () => {
  assertEqual(evalString("status IN ('active', 'pending')", { status: 'active' }), true);
  assertEqual(evalString("status IN ('deleted')", { status: 'active' }), false);
});

test('LIKE operator', () => {
  assertEqual(evalString("name LIKE '%world%'", { name: 'Hello World' }), true);
  assertEqual(evalString("name LIKE '%xyz%'", { name: 'Hello World' }), false);
});

test('parse once eval many', () => {
  const ast = parse('a * b');
  for (let i = 0; i < 100; i++) {
    const result = evalAST(ast, { a: 10, b: i });
    assertEqual(result, String(10 * i));
  }
});

test('error handling', () => {
  try {
    evalString("'unterminated", {});
    throw new Error('Should have thrown');
  } catch (e) {
    assert(e.message.includes('Unterminated'), `Expected unterminated error, got: ${e.message}`);
  }
});

test('real-world expression', () => {
  const ctx = { currency: 'BRL', amount_local: 500, amount_usd: 100 };
  const result = evalString(
    "CASE WHEN currency = 'USD' THEN amount_usd WHEN currency = 'BRL' THEN amount_local ELSE NULL END",
    ctx
  );
  assertEqual(result, '500');
});

console.log(`\n=== Node.js: ${passed}/${passed + failed} passed ===`);
process.exit(failed > 0 ? 1 : 0);
