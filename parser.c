#include <stdio.h>
#define ARENA_IMPLEMENTATION
#include <arena.h>
#include <stdbool.h>
#include <mpc.h>
#include <math.h>

static Arena defaultArena = {0};
static Arena *contextArena = &defaultArena;

// Declare a buffer for user input of size 2048
static char input[2048];

void *contextAlloc(size_t size) {
    assert(contextArena);
    return arenaAlloc(contextArena, size);
}

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  //char* cpy = malloc(strlen(buffer)+1);
  char* cpy = contextAlloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#endif

// ENUM of possible LVAL types
enum { LVAL_NUM, LVAL_ERR };

// ENUM of possible error types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// List Value struct
typedef struct {
    int type;
    long num;
    int err;
} lval;

// Create new num type lval
lval lvalNum(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

// Create a new error type lval
lval lvalErr(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

// Print lval type
void lvalPrint(lval v) {
  switch (v.type) {
    // If type is number print it
    // Break out of the switch
    case LVAL_NUM: printf("%li", v.num);

    // If type is an error
    case LVAL_ERR:
      if (v.err == LERR_DIV_ZERO) {
        printf("%s", "ERROR: Division by Zero!");
      }

      if (v.err == LERR_BAD_OP) {
        printf("%s", "ERROR: Invalid Operator!");
      }

      if (v.err == LERR_BAD_NUM) {
        printf("%s", "ERROR: Invalid Number!");
      }
    break;
  }
}

// Print lval with newline
void lvalPrintln(lval v) { lvalPrint(v); putchar('\n'); }

// Operations to perform on operator
lval evalOp(lval x, char* op, lval y) {
  // If either value is an error return it
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  // Otherwise evaluate the numbers
  if (strcmp(op, "+") == 0) { return lvalNum(x.num + y.num); }
  if (strcmp(op, "-") == 0) { return lvalNum(x.num - y.num); }
  if (strcmp(op, "*") == 0) { return lvalNum(x.num * y.num); }
  if (strcmp(op, "%") == 0) { return lvalNum(x.num & y.num); }
  if (strcmp(op, "^") == 0) { return lvalNum((long)pow(x.num, y.num)); }
  if (strcmp(op, "/") == 0) {
    // If second operatore is zero -> return error
    return y.num == 0
      ? lvalErr(LERR_DIV_ZERO)
      : lvalNum(x.num / y.num);
  }

  return lvalErr(LERR_BAD_NUM);

}


// Parse evaluations
lval eval(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {

    // Check for error in conversion
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lvalNum(x) : lvalErr(LERR_BAD_NUM);
  }

  char* op = t->children[1]->contents;
  lval x = eval(t->children[2]);
  
  int i = 3;
  
  // While the AST child tag is expression, call eval func and evaluate the numbers
  while(strstr(t->children[i]->tag, "expr")) {
    x = evalOp(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

int main(int argc, char** argv) {
  
  // Create parsers
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lumina    = mpc_new("lumina");
  
  // Define parsers
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+/ ;                             \
      operator : '+' | '-' | '*' | '/' | '%' | '^' ;            \
      expr     : <number> | '(' <operator> <expr>+ ')' ;  \
      lumina    : /^/ <operator> <expr>+ /$/ ;             \
    ",
    Number, Operator, Expr, Lumina);
  
  printf("%s", "Lumina Version 0.2\n");
  printf("%s", "Press CTRL+C to Exit\n");
  
  while (1) {
  
    char* input = readline("Lumina> ");
    add_history(input);
    
    // Attempt to parse
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lumina, &r)) {
      // On success print and delete the AST
      lval result = eval(r.output);
      lvalPrintln(result);
      mpc_ast_delete(r.output);
    } else {
      // Otherwise print and delete error
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    arenaFree(&defaultArena);
  }
  
  // Undefine and delete our parsers
  mpc_cleanup(4, Number, Operator, Expr, Lumina);
  
  return 0;
}