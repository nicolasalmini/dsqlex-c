/**
 * Opaque AST handle. Parse once, evaluate many times.
 */
export type ASTHandle = unknown;

/**
 * Parse a DSQLEX expression into a reusable AST handle.
 */
export function parse(expression: string): ASTHandle;

/**
 * Evaluate a pre-parsed AST against a context object.
 * Decimal results are returned as strings to preserve precision.
 */
export function evalAST(ast: ASTHandle, context: Record<string, unknown>): string | boolean | null;

/**
 * Parse and evaluate a DSQLEX expression in one call.
 */
export function evalString(expression: string, context: Record<string, unknown>): string | boolean | null;
