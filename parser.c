#include <stdio.h>
#include <stdbool.h>
#include <mpc.h>
#include <math.h>

// Declare a buffer for user input of size 2048
static char buffer[2048];

#ifdef _WIN32

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

// ENUM of possible LVAL types
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

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

lval* lvalSym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM; // Set type to Symbol
  v->sym = malloc(strlen(s) + 1); // Get length of string
  strcpy(v->sym, s); // Set the symbol
  return v;
}

lval* lvalSexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
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

    // If type is S-Expression -> delete all elements and free
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
    if (strcmp(op, "%") == 0) { x->num &= y->num; }
    if (strcmp(op, "^") == 0) { (long)pow(x->num, y->num); }
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
  lval* result = builtinOp(v, f->sym);

  lvalDel(f);
  return result;
}

lval* lvalEval(lval* v) {
  // Evaluate S-Expression
  if (v->type == LVAL_SEXPR) { return lvalEvalSexpr(v); }

  // Don't modify other types if not S-Expression
  return v;
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
  if (strcmp(t->tag, "sexpr")) { x = lvalSexpr(); }

  // Fill the list with valid expressions contained within
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
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
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lumina    = mpc_new("lumina");
  
  // Define parsers
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                   \
      number : /-?[0-9]+/ ;                             \
      symbol : '+' | '-' | '*' | '/' | '%' | '^' ;      \
      sexpr  : '(' <expr>* ')' ;                        \
      expr   : <number> | <symbol> | <sexpr> ;          \
      lumina : /^/ <expr>* /$/ ;                        \
    ",
    Number, Symbol, Sexpr, Expr, Lumina);
  
  printf("%s", "Lumina Version 0.2\n");
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
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lumina);
  
  return 0;
}