#include <stdio.h>
#include <stdbool.h>
#include <mpc.h>
#include <math.h>

// Declare a buffer for user input of size 2048

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#endif

// Macro for error handling
#define LASSERT(args, cond, err) \
  if (!(cond)) { lvalDel(args); return lvalErr(err); }

// ENUM of possible LVAL types
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

// List Value struct
typedef struct lval {
  int type;
  long num;
  char* err;
  char* sym;
  int count;
  struct lval** cell;
} lval;

// Create new num type lval
lval* lvalNum(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM; // Set type of number
  v->num = x; // Set the number to value of X param
  return v;
}

// Create a pointer to a new error type lval
lval* lvalErr(char* m) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1); // Get length of string
  strcpy(v->err, m); // Set the err to the error message
  return v;
}

// Symbol constructor
lval* lvalSym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM; // Set type to Symbol
  v->sym = malloc(strlen(s) + 1); // Get length of string
  strcpy(v->sym, s); // Set the symbol
  return v;
}

// S-Expression constructor
lval* lvalSexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

// Q-Expression constructor
lval* lvalQexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

// Delete and release memory allocation after inspecting LVAL types to prevent memory leaks
void lvalDel(lval* v) {
  switch (v->type) {
   
    // If type is number - do nothing
    case LVAL_NUM: break;

    // If type is an error or symbol free the data
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;

    // If type is S-Expression or Q-Expression -> delete all elements and free
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lvalDel(v->cell[i]);
      }
      
      free(v->cell);
    break;
  }

  // Free the allocated meemory for the lval struct
  free(v);
}

// Add elements to an S-Expression
lval* lvalAdd(lval* v, lval* x) {
  v->count++; // Incerease amount of space required
  v->cell = realloc(v->cell, sizeof(lval*) * v->count); // Reallocate the amount required by v->cell
  v->cell[v->count-1] = x; // Set final value to x LVAL
  return v;
}

// Extract S-Expression at index 1 and shift the rest of the list backwards.
lval* lvalPop(lval* v, int i) {
  // Find the item in i
  lval* x = v->cell[i];

  // Shif memory
  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

  // Decrease count of items in the list
  v->count--;

  // Rellocate the memory
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lvalJoin(lval* x, lval* y) {
  // For each cell in [y] add it to [x]
  while (y->count) {
    x = lvalAdd(x, lvalPop(y, 0));
  }

  // Delete empty [y] and return [x]
  lvalDel(y);
  return x;
}

// Deletes the list it has extracted the element from.
lval* lvalTake(lval* v, int i) {
  lval* x = lvalPop(v, i);
  lvalDel(v);
  return x;
}

void lvalPrint(lval* v);

void lvalExprPrint(lval* v, char open, char close) {
  putchar(open);

  for (int i = 0; i < v->count; i++) {
    // Print value
    lvalPrint(v->cell[i]);

    // If last element, don't print trailing space
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

// Print lval types
void lvalPrint(lval* v) {
  switch (v->type) {
    case LVAL_NUM: printf("%li", v->num); break;
    case LVAL_ERR: printf("%s", v->err); break;
    case LVAL_SYM: printf("%s", v->sym); break;
    case LVAL_SEXPR: lvalExprPrint(v, '(', ')'); break;
    case LVAL_QEXPR: lvalExprPrint(v, '{', '}'); break;
  }
}

// Print lval with newline
void lvalPrintln(lval* v) { lvalPrint(v); putchar('\n'); }

lval* builtinOp(lval* a, char* op) {
  
  // Check that all arguments are numbers
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lvalDel(a);
      return lvalErr("Cannot operate on a non-number!");
    }
  }

  // Pop the first element
  lval* x = lvalPop(a, 0);

  // If no argument and sub then perform unary negation
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  // Whle elements remain
  while(a->count > 0) {
    // Pop the next element
    lval* y = lvalPop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "%") == 0) { x->num %= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lvalDel(x); lvalDel(y);
        x = lvalErr("Division By Zero!"); break;
      }
      x->num /= y->num;
    }

    lvalDel(y);
  }

  lvalDel(a);
  return x;
}

lval* lvalEval(lval* v);
lval* builtin(lval* a, char* func);

lval* lvalEvalSexpr(lval* v) {
  // Evaluate children
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lvalEval(v->cell[i]);
  }

  // Check for errors
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lvalTake(v, i); }
  }

  // Empty expression
  if (v->count == 0) { return v; }

  // Single expression
  if (v->count == 1) { return lvalTake(v, 0); }

  // If first element is symbol
  lval* f = lvalPop(v, 0);
  if (f->type != LVAL_SYM) {
    lvalDel(f); lvalDel(v);
    return lvalErr("S-Expression does not start with a symbol!");
  }
  
  // Call builtin with operator
  lval* result = builtin(v, f->sym);

  lvalDel(f);
  return result;
}

lval* lvalEval(lval* v) {
  // Evaluate S-Expression
  if (v->type == LVAL_SEXPR) { return lvalEvalSexpr(v); }

  // Don't modify other types if not S-Expression
  return v;
}

// Pop and delete the item and index 1 and return
lval* builtinHead(lval* a) {

  // Check error conditions
  LASSERT(a, a->count == 1, 
    "Function 'head' passed too many arguments!");

  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, 
    "Function 'head' passed incorrect types! Are you sure you passed Q-Expression?");

  LASSERT(a, a->cell[0]->count != 0,
    "Function 'head' cannot be empty!");

  // Take first argument
  lval* v = lvalTake(a, 0);

  // Delete all elements that are not head and return
  while (v->count > 1) { lvalDel(lvalPop(v, 1)); }
  return v;
}

// Pop and delete item and index 0 and leave the rest remaining
lval* builtinTail(lval* a) {
  // Check for error conditions
  if (a->count != 1) {
    lvalDel(a);
    return lvalErr("Function 'tail' passed too many arguments!");
  }

  if (a->cell[0]->type != LVAL_QEXPR) {
    lvalDel(a);
    return lvalErr("Function 'tail' passed incorrect type! Are you sure you passed Q-Expression?");
  }

  if (a->cell[0]->count == 0) {
    lvalDel(a);
    return lvalErr("Function 'tail' passed {}!");
  }

  // Take first argument
  lval* v = lvalTake(a, 0);

  // Delete first element and return
  lvalDel(lvalPop(v, 0));

  return v;
}

// Convert Q-Expression to S-Expression and return
lval* builtinList(lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

// Conver to Q-Expression to S-Expression and evaluate
lval* builtinEval(lval* a) {
  LASSERT(a, a->count == 1,
    "Function 'eval' passed too many arguments!");
  
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'tail' passed incorrect type! Are you sure you passed Q-Expression?");

  lval* x = lvalTake(a, 0);
  x->type = LVAL_SEXPR;
  return lvalEval(x);
}

lval* builtinJoin(lval* a) {

  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
      "Function 'tail' passed incorrect type! Are you sure you passed Q-Expression?");
  }

  lval* x = lvalPop(a, 0);

  while (a->count) {
    x = lvalJoin(x, lvalPop(a, 0));
  }

  lvalDel(a);
  return x;
}

lval* builtin(lval* a, char* func) {
  if (strcmp("list", func) == 0) { return builtinList(a); }
  if (strcmp("head", func) == 0) { return builtinHead(a); }
  if (strcmp("tail", func) == 0) { return builtinTail(a); }
  if (strcmp("join", func) == 0) { return builtinJoin(a); }
  if (strcmp("eval", func) == 0) { return builtinEval(a); }
  if (strstr("+-/*", func)) { return builtinOp(a, func); }
  lvalDel(a);
  return lvalErr("Unknown Function!");
}

lval* lvalReadNum(mpc_ast_t* t) {
  errno = 0;

  long x = strtol(t->contents, NULL, 10);

  // Check for error in conversion
  return errno != ERANGE ?
    lvalNum(x) : lvalErr("ERROR: Invalid Number!");
}


lval* lvalRead(mpc_ast_t* t) {

  // If symbol or number then return conversion to that type
  if (strstr(t->tag, "number")) { return lvalReadNum(t); }
  if (strstr(t->tag, "symbol")) { return lvalSym(t->contents); }

  // If root (>) or sexpr then create empty list
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lvalSexpr(); }
  if (strstr(t->tag, "sexpr")) { x = lvalSexpr(); }
  if (strstr(t->tag, "qexpr")) { x = lvalQexpr(); }

  // Fill the list with valid expressions contained within
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }

    if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
    x = lvalAdd(x, lvalRead(t->children[i]));
  }

  return x;
}

int main(int argc, char** argv) {
  
  // Create parsers
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lumina    = mpc_new("lumina");
  
  // Define parsers
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                    \
      number : /-?[0-9]+/ ;                              \
      symbol : '+' | '-' | '*' | '/' | '%'               \
             | \"list\" | \"head\" | \"tail\"            \
             | \"join\" | \"eval\" ;                     \
      sexpr  : '(' <expr>* ')' ;                         \
      qexpr  : '{' <expr>* '}' ;                         \
      expr   : <number> | <symbol> | <sexpr> | <qexpr> ; \
      lumina : /^/ <expr>* /$/ ;                         \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lumina);
  
  printf("%s", "Lumina Version 0.3\n");
  printf("%s", "Press CTRL+C to Exit\n");
  
  while (1) {
  
    char* input = readline("Lumina> ");
    add_history(input);
    
    // Attempt to parse
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lumina, &r)) {
      // On success print and delete the AST
      lval* x = lvalEval(lvalRead(r.output));
      lvalPrintln(x);
      lvalDel(x);
      mpc_ast_delete(r.output);
    } else {
      // Otherwise print and delete error
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    free(input);
  }
  
  // Undefine and delete our parsers
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lumina);
  
  return 0;
}