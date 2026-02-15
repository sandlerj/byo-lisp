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
enum { LVAL_NUM, LVAL_ERR};

/* lval err enums */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Lisp Value */
typedef struct {
	int type;
	long num;
	int err;
} lval;

/* Create a new number type lval */
lval lval_num(long x) {
	lval v;
	v.type = LVAL_NUM;
	v.num = x;
	return v;
}

/* Create a new lval error */
lval lval_err(int x) {
	lval v;
	v.type = LVAL_ERR;
	v.err = x;
	return v;
}

/* print an lval */
void lval_print(lval v) {
	switch (v.type) {
		case LVAL_NUM: printf("%li", v.num); break;

		/* Error case handling */
		case LVAL_ERR:
			if (v.err == LERR_DIV_ZERO) {
				printf("Error: Div by zero!");
			}
			if (v.err == LERR_BAD_OP) {
				printf("Error: Invalid Operator!");
			}
			if (v.err == LERR_BAD_NUM) {
				printf("Error: Invalid Number!");
			}
			break;
	}
}

void lval_println(lval v) { lval_print(v); putchar('\n'); }

lval eval_op(lval x, char* op, lval y) {
	
	/* If either value is an error, return it */
	if (x.type == LVAL_ERR) {return x;}
	if (y.type == LVAL_ERR) {return y;}

	if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
	if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
	if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
	if (strcmp(op, "/") == 0) { 
		return y.num == 0
			? lval_err(LERR_DIV_ZERO)
			: lval_num(x.num / y.num);
	}
	if (strcmp(op, "%") == 0) { return lval_num(x.num % y.num); }
	if (strcmp(op, "min") == 0) { return lval_num(fminl(x.num, y.num)); }
	if (strcmp(op, "max") == 0) { return lval_num(fmaxl(x.num, y.num)); }
	return lval_err(LERR_BAD_OP);
}

lval evalExpr(mpc_ast_t* t) {
	if (strstr(t->tag, "number")) {
		errno = 0;
		long x = strtol(t->contents, NULL, 10);
		return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
	}

	mpc_ast_t* second_child = t->children[1];
	char* operator = second_child->contents;

	lval x = evalExpr(t->children[2]);

	int i = 3;
	if (strstr(t->children[i]->tag, "expr") == 0
		&& strcmp(operator, "-") == 0) {
		/* i.e. there was only a single expresion provided and op is -*/
		x.num *= -1;
		return x;
	}

	while (strstr(t->children[i]->tag, "expr")) {
		x = eval_op(x, operator, evalExpr(t->children[i]));
		i++;
	}

	return x;
}

int main(int argc, char** argv) {
	mpc_parser_t* Number 	= mpc_new("number");
	mpc_parser_t* Operator 	= mpc_new("operator");
	mpc_parser_t* Expr 	= mpc_new("expr");
	mpc_parser_t* Lispy 	= mpc_new("lispy");

	/* Define those parsers with the following language */
	mpca_lang(MPCA_LANG_DEFAULT,
		"								\
			number	: /-?[0-9]+/;					\
			operator: '+' | '-' | '*' | '/' | '%' \
				| \"max\" | \"min\";	\
			expr	: <number> | '(' <operator> <expr>+ ')'; 	\
			lispy	: /^/ <operator> <expr>+ /$/ ;			\
		",
		Number, Operator, Expr, Lispy);

	puts("Lispy Version 0.0.0.1");
	puts("Press Ctrl+c to Exit\n");

	while(1) {
		char* input = readline("lispy> ");

		add_history(input);

		/* try parse */
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Lispy, &r)) {
			lval result = evalExpr(r.output);
			lval_println(result);
			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input);
	}

	mpc_cleanup(4, Number, Operator, Expr, Lispy);
	return 0;
}
