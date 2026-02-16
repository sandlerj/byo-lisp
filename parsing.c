#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include <editline/history.h>
#include "mpc.h"


int number_of_nodes(mpc_ast_t* t) {
	if (t->children_num == 0) { return 1; }
	if (t->children_num >= 1) {
		int total = 1;
		for (int i = 0; i < t->children_num; i++) {
			total += number_of_nodes(t->children[i]);
		}
		return total;
	}
	return 0;
}

/* lval type enums */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR};

/* lval err enums */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Lisp Value */
typedef struct lval {
	int type;
	long num;
	/* Error and symbol types have string data */
	char* err;
	char* sym;

	struct lval* cell;
} lval;

/* Create a new number type lval */
lval* lval_num(long x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

/* Create a new lval error */
lval* lval_err(char* m) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(m) + 1);
	strcpy(v->err, m);
	return v;
}

lval* lval_sexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->cell = NULL;
	return v;
}

lval* lval_sym(char* s) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

void lval_del(lval* v) {
	
	switch (v->type) {
		case LVAL_NUM: break;
		case LVAL_ERR: free(v->err); break;
		case LVAL_SYM: free(v->sym); break;
		case LVAL_SEXPR:
			lval_del(v->cell);
			break;
	}
	free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE ?
		lval_num(x) : lval_err("invalid number");
}


/* Append to end of linked list (cell) */
lval* lval_add(lval* v, lval* x) {
	lval* end = v->cell;
	if (end == NULL) {
		v->cell = x;
		return;
	}
	while (end->cell != NULL) {
		end = end->cell;
	}
	end->cell = x;
	return v;
}

lval* lval_read(mpc_ast_t* t) {
	/* If sym or num return conversion to that type */
	if (strstr(t->tag, "number")) { return lval_read_num(t); }
	if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

	/* if root or sexpr, create empty list */
	lval* x = NULL;
	if (strcmp(t->tag, ">") == 0) 	{ x = lval_sexpr(); }
	if (strstr(t->tag, "sexpr"))	{ x = lval_sexpr(); }

	/* fill in the list with any valid contents */
	for (int i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->contents, "(") == 0 ) { continue; }
		if (strcmp(t->children[i]->contents, ")") == 0 ) { continue; }
		if (strcmp(t->children[i]->tag, "regex") == 0 ) { continue; }
		x = lval_add(x, lval_read(t->children[i]));
	}
	return x;
}
void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
	putchar(open);

	lval* ptr = v->cell;
	while (ptr != NULL) {
		lval_print(ptr);
		
		if (ptr->cell != NULL) {
			putchar(' ');
		}
		ptr = ptr->cell;
	}

	putchar(close);
}

/* print an lval */
void lval_print(lval* v) {
	switch (v->type) {
		case LVAL_NUM: 	printf("%li", v->num); break;
		case LVAL_ERR: 	printf("Error: %s", v->err); break;
		case LVAL_SYM:	printf("%s", v->sym); break;
		case LVAL_SEXPR:lval_expr_print(v, '(', ')'); break;
	}
}

lval* lval_eval(lval* v);
lval* lval_take(lval* v, int i);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* builtin_op(lval* a, char* op);

lval* lval_eval_sexpr(lval* v) {
	/* Eval any children */
	lval* prev = v;
	lval* ptr = prev->cell;
	while (ptr != null) {
		prev->cell = lval_eval(ptr);
		prev = prev->cell;
		ptr = prev->cell;
	}

	/* Check all for errors */

	// TODO - continue with ll re-write
	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
	}

	if (v->count == 0) { return v; }

	if (v->count == 1) { return lval_take(v, 0); }

	/* Ensure first element is a symbol */
	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_SYM) {
		lval_del(f); lval_del(v);
		return lval_err("S-expression does not start with symbol!");
	}

	/* Call with built in op */
	lval* result = builtin_op(v, f->sym);
	lval_del(f);
	return result;
}

lval* lval_eval(lval* v) {
	/* Evaluate s-expresion */
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }

	return v;
}

lval* lval_pop(lval* v, int i) {
	/* find the item at i */
	lval* x = v->cell[i];

	/* Shift memory after item at i over the top */
	memmove(&v->cell[i], &v->cell[i+1],
			sizeof(lval*) * (v->count-i-1));

	v->count--;

	/* reallocate to small size */
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);

	return x;
}

/* Take lval at i and delete the rest */
lval* lval_take(lval* v, int i) {
	lval* x = lval_pop(v, i);
	lval_del(v);
	return x;
}

lval* builtin_op(lval* a, char* op) {
	for (int i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			lval_del(a);
			return lval_err("Cannot operate on non-numbers");
		}
	}

	/* pop first element */
	lval* x = lval_pop(a, 0);

	/* if no args and sub then do unary negation */
	if ((strcmp(op, "-") == 0) && a->count == 0 ) {
		x->num = -x->num;
	}

	while (a->count > 0) {
		lval* y = lval_pop(a, 0);

		if (strcmp(op, "+") == 0 ) { x->num += y->num; }
		if (strcmp(op, "-") == 0 ) { x->num -= y->num; }
		if (strcmp(op, "*") == 0 ) { x->num *= y->num; }
		if (strcmp(op, "/") == 0 || strcmp(op, "%") == 0 ) { 
			if (y->num == 0) {
				lval_del(x); lval_del(y);
				x = lval_err("Division by Zero!"); break;
			}
			if (strcmp(op, "/") == 0 ) {
				x->num /= y->num;
			} else {
				x->num %= y->num;
			}
		}
		if (strcmp(op, "min") == 0 ) { x->num = fminl(x->num, y->num); }
		if (strcmp(op, "max") == 0 ) { x->num = fmaxl(x->num, y->num); }
		
		lval_del(y);
	}

	lval_del(a);
	return x;
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }
int main(int argc, char** argv) {
	mpc_parser_t* Number 	= mpc_new("number");
	mpc_parser_t* Symbol 	= mpc_new("symbol");
	mpc_parser_t* Sexpr	= mpc_new("sexpr");
	mpc_parser_t* Expr 	= mpc_new("expr");
	mpc_parser_t* Lispy 	= mpc_new("lispy");

	/* Define those parsers with the following language */
	mpca_lang(MPCA_LANG_DEFAULT,
		"								\
			number	: /-?[0-9]+/;					\
			symbol	: '+' | '-' | '*' | '/' | '%' 			\
				| \"max\" | \"min\" ;				\
			sexpr	: '(' <expr>* ')' ;			 	\
			expr	: <number> | <symbol> | <sexpr> ;		\
			lispy	: /^/ <expr>* /$/ ;				\
		",
		Number, Symbol, Sexpr, Expr, Lispy);

	puts("Lispy Version 0.0.0.1");
	puts("Press Ctrl+c to Exit\n");

	while(1) {
		char* input = readline("lispy> ");

		add_history(input);

		/* try parse */
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Lispy, &r)) {
			lval* x = lval_eval(lval_read(r.output));
			lval_println(x);
			lval_del(x);
			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input);
	}

	mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
	return 0;
}
