/**
 * dsqlex-c — Node.js bindings for the DSQLEX C library.
 *
 * Returns decimal results as strings to preserve precision.
 * Use a Decimal library (e.g. decimal.js) if you need arithmetic.
 */

const path = require('path');

// Try to load the native addon
let native;
try {
  native = require(path.join(__dirname, '..', 'build', 'Release', 'dsqlex_napi.node'));
} catch {
  try {
    native = require(path.join(__dirname, '..', 'build', 'Debug', 'dsqlex_napi.node'));
  } catch (e) {
    throw new Error(
      'Failed to load dsqlex native addon. Run "npm run build" first.\n' + e.message
    );
  }
}

/**
 * Parse a DSQLEX expression into a reusable AST handle.
 * @param {string} expression
 * @returns {object} Opaque AST handle
 */
function parse(expression) {
  if (typeof expression !== 'string') throw new TypeError('expression must be a string');
  return native.parse(expression);
}

/**
 * Evaluate a pre-parsed AST against a context object.
 * @param {object} ast - AST handle from parse()
 * @param {object} context - Key-value context
 * @returns {string|boolean|null} Result value (decimals returned as strings)
 */
function evalAST(ast, context) {
  if (typeof context !== 'object' || context === null) throw new TypeError('context must be an object');
  return native.evalAST(ast, context);
}

/**
 * Parse and evaluate in one call.
 * @param {string} expression
 * @param {object} context
 * @returns {string|boolean|null}
 */
function evalString(expression, context) {
  if (typeof expression !== 'string') throw new TypeError('expression must be a string');
  if (typeof context !== 'object' || context === null) throw new TypeError('context must be an object');
  return native.evalString(expression, context);
}

module.exports = { parse, evalAST, evalString };
