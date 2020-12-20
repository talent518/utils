#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum _val_type_t {
	LNG_T=0,
	DBL_T
} val_type_t;

typedef struct _val_t {
	val_type_t t;
	union {
		long int l;
		double f;
	};
} val_t;

typedef enum _word_type_t {
	VAL_WT=0,
	ADD_WT,
	SUB_WT,
	MUL_WT,
	DIV_WT,
	LEFT_WT,
	RIGHT_WT
} word_type_t;

typedef struct _word_t {
	word_type_t t;
	val_t val;
} word_t;

typedef enum _ast_type_t {
	NULL_T=0,
	VAL_T,
	MINUS_T,
	ADD_T,
	SUB_T,
	MUL_T,
	DIV_T
} ast_type_t;

typedef struct _ast_t {
	ast_type_t t;
	val_t val;
	struct _ast_t *left;
	struct _ast_t *right;
} ast_t;

#define PARSE_AST_DEBUG 0
#if PARSE_AST_DEBUG
	#define dprintf(args...) printf(args)
#else
	#define dprintf(args...)
#endif

#define WORDS 100

typedef enum _rule_t {
	R_VAL=0, // 数值: 1 或 2.0
	R_LR, // 括号: ()
	R_SIGN, // 正负号: + -
	R_MUL_DIV, // 乘除运算
	R_ADD_SUB // 加减运算
} rule_t;

void free_ast(ast_t *ast);

int _parse_ast(word_t *words, int i, int n, rule_t r, ast_t *ast) {
	ast_t left = {NULL_T, {0,0}, NULL, NULL}, right = {NULL_T, {0,0}, NULL, NULL};
	int ret = 0, i2;
	ast_type_t t;

	if(i>=n) return 0;

	switch(r) {
		case R_VAL: {
			if(i < n && words[i].t == VAL_WT) {
				ast->t = VAL_T;
				ast->val = words[i].val;
				ret = 1;

				#if PARSE_AST_DEBUG
					if(words[i].val.t == LNG_T) {
						printf("R_VAL: %d L %ld\n", i, words[i].val.l);
					} else {
						printf("R_VAL: %d F %g\n", i, words[i].val.f);
					}
				#endif
			}
			break;
		}
		case R_LR: {
			if(i < n && words[i].t == LEFT_WT) {
				dprintf("R_LR: %d (\n", i);
				ret = _parse_ast(words, i+1, n, R_ADD_SUB, ast);
				if(ret > 0 && i+1+ret < n && words[i+1+ret].t == RIGHT_WT) {
					dprintf("R_LR: %d )\n", i+1+ret);
					ret += 2;
				} else if(ret > 0) {
					ret = 0;
					fprintf(stderr, "Not a ')' character at %d position\n", i+1+ret);
				} else {
					fprintf(stderr, "Not a expression at %d position\n", i+1);
				}
			} else {
				ret = _parse_ast(words, i, n, R_VAL, ast);
			}
			break;
		}
		case R_SIGN: {
			if(i < n && (words[i].t == ADD_WT || words[i].t == SUB_WT)) {
				dprintf("R_SIGN: %d %c\n", i, words[i].t == SUB_WT ? '-' : '+');

				ret = _parse_ast(words, i+1, n, R_LR, &left);
				if(ret > 0) {
					ret++;
					if(words[i].t == SUB_WT) {
						ast->t = MINUS_T;
						ast->left = (ast_t *) malloc(sizeof(ast_t));
						*(ast->left) = left;
					} else {
						*ast = left;
					}
				} else {
					fprintf(stderr, "Not a expression at %d position\n", i+1);
				}
			} else {
				ret = _parse_ast(words, i, n, R_LR, ast);
			}
			break;
		}
		case R_MUL_DIV:
		case R_ADD_SUB: {
			i2 = i;
			t = NULL_T;
			ret = _parse_ast(words, i, n, r == R_MUL_DIV ? R_SIGN : R_MUL_DIV, &left);
			if(ret <= 0) {
				fprintf(stderr, "Not a expression at %d position\n", i);
				free_ast(&left);
				break;
			}
		leftMulDiv:
			if(t != NULL_T) {
				ast->left = (ast_t *) malloc(sizeof(ast_t));
				*(ast->left) = left;
				ast->right = (ast_t *) malloc(sizeof(ast_t));
				*(ast->right) = right;

				left = *ast;
				left.t = t;
				
				right.t = NULL_T;
				right.left = NULL;
				right.right = NULL;

				ast->left = NULL;
				ast->right = NULL;
			}
			i += ret;
			if(i < n && ((r == R_MUL_DIV && (words[i].t == MUL_WT || words[i].t == DIV_WT)) || (r == R_ADD_SUB && (words[i].t == ADD_WT || words[i].t == SUB_WT)))) {
				if(r == R_MUL_DIV) {
					if(words[i].t == MUL_WT) {
						t = MUL_T;
						dprintf("R_MUL_DIV: %d *\n", i);
					} else {
						t = DIV_T;
						dprintf("R_MUL_DIV: %d /\n", i);
					}
				} else {
					if(words[i].t == ADD_WT) {
						t = ADD_T;
						dprintf("R_ADD_SUB: %d +\n", i);
					} else {
						t = SUB_T;
						dprintf("R_ADD_SUB: %d -\n", i);
					}
				}
				i++;
			} else if(t != NULL_T || r == R_MUL_DIV || left.t == MUL_T || left.t == DIV_T) {
				ret = i - i2;

				*ast = left;
				break;
			} else {
				ret = 0;
				fprintf(stderr, "Not a '+' or '-' character at %d position\n", i);
				free_ast(&left);
			}
			if(i<n && (ret = _parse_ast(words, i, n, r == R_MUL_DIV ? R_SIGN : R_MUL_DIV, &right)) > 0) {
				goto leftMulDiv;
			} else {
				fprintf(stderr, "Not a expression at %d position\n", i);
				ret = 0;
				free_ast(&left);
			}
			break;
		}
	}

	return ret;
}

#define SKIP(p) while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++
int parse_ast(const char *s, ast_t *ast) {
	const char *p = s, *p0;
	int n = 0, i, i2;
	word_t words[WORDS];

	memset(words, 0, sizeof(word_t) * WORDS);

	SKIP(p);
	while(*p && n < WORDS) {
		switch(*p) {
			case '(':
				words[n].t = LEFT_WT;
				p++;
				break;
			case ')':
				words[n].t = RIGHT_WT;
				p++;
				break;
			case '+':
				words[n].t = ADD_WT;
				p++;
				break;
			case '-':
				words[n].t = SUB_WT;
				p++;
				break;
			case '*':
				words[n].t = MUL_WT;
				p++;
				break;
			case '/':
				words[n].t = DIV_WT;
				p++;
				break;
			default: {
				words[n].t = VAL_WT;
				words[n].val.t = LNG_T;
				words[n].val.l = 0;
				p0 = p;
				while(*p >= '0' && *p <= '9') {
					words[n].val.l = words[n].val.l * 10 + (*p - '0');
					p++;
				}
				if(words[n].val.t == LNG_T && *p == '.') {
					words[n].val.t = DBL_T;
					words[n].val.f = words[n].val.l;
					p++;
					double f = 1;
					p0 = p;
					while(*p >= '0' && *p <= '9') {
						f = f / 10;
						words[n].val.f += f * (*p - '0');
						p++;
					}
				}
				if(p0 == p) {
					fprintf(stderr, "Column %lu in %s should be dot(.) or digit(0~9)\n", p-s, s);
					return -1;
				}
				break;
			}
		}

		n++;

		SKIP(p);
	}

	if(*p) {
		fprintf(stderr, "No more than %d words\n", WORDS);
		return 0;
	}

#if PARSE_AST_DEBUG
	for(i=0; i<n; i++) {
		switch(words[i].t) {
			case LEFT_WT:
				printf("(");
				break;
			case RIGHT_WT:
				printf(")");
				break;
			case ADD_WT:
				printf(" + ");
				break;
			case SUB_WT:
				printf(" - ");
				break;
			case MUL_WT:
				printf(" * ");
				break;
			case DIV_WT:
				printf(" / ");
				break;
			case VAL_WT:
				if(words[i].val.t == LNG_T) {
					printf("%ldL", words[i].val.l);
				} else {
					printf("%gF", words[i].val.f);
				}
				break;
		}
	}
	printf("\n");
#endif

	i = _parse_ast(words, 0, n, R_ADD_SUB, ast);
	dprintf("i: %d, n: %d\n", i, n);
	return i;
}

void free_ast(ast_t *ast) {
	if(ast->left) {
		free_ast(ast->left);
		free(ast->left);
		ast->left = NULL;
	}
	if(ast->right) {
		free_ast(ast->right);
		free(ast->right);
		ast->right = NULL;
	}
}

void calc_ast(ast_t *ast) {
	switch(ast->t) {
		case NULL_T:
		case VAL_T:
			#if PARSE_AST_DEBUG
				if(ast->val.t == LNG_T) {
					printf("VAL_T: %ldL\n", ast->val.l);
				} else {
					printf("VAL_T: %gF\n", ast->val.f);
				}
			#endif
			break;
		case MINUS_T:
			calc_ast(ast->left);
			ast->val.t = ast->left->val.t;
			if(ast->left->val.t == LNG_T) {
				ast->val.l = -ast->left->val.l;
				dprintf("MINUS_T: %ldL\n", ast->left->val.l);
			} else {
				ast->val.f = -ast->left->val.f;
				dprintf("MINUS_T: %gF\n", ast->left->val.f);
			}
			break;
		case ADD_T:
			calc_ast(ast->left);
			calc_ast(ast->right);
			if(ast->left->val.t == LNG_T && ast->right->val.t == LNG_T) {
				ast->val.t = LNG_T;
				ast->val.l = ast->left->val.l + ast->right->val.l;

				dprintf("ADD_T: %ldL + %ldL = %ldL\n", ast->left->val.l, ast->right->val.l, ast->val.l);
			} else if(ast->left->val.t == DBL_T && ast->right->val.t == DBL_T) {
				ast->val.t = DBL_T;
				ast->val.f = ast->left->val.f + ast->right->val.f;

				dprintf("ADD_T: %gF + %gF = %gF\n", ast->left->val.f, ast->right->val.f, ast->val.f);
			} else if(ast->left->val.t == LNG_T) {
				ast->val.t = DBL_T;
				ast->val.f = ast->left->val.l + ast->right->val.f;

				dprintf("ADD_T: %ldL + %gF = %gF\n", ast->left->val.l, ast->right->val.f, ast->val.f);
			} else {
				ast->val.t = DBL_T;
				ast->val.f = ast->left->val.f + ast->right->val.l;

				dprintf("ADD_T: %gF + %ld = %gF\n", ast->left->val.f, ast->right->val.l, ast->val.f);
			}
			break;
		case SUB_T:
			calc_ast(ast->left);
			calc_ast(ast->right);
			if(ast->left->val.t == LNG_T && ast->right->val.t == LNG_T) {
				ast->val.t = LNG_T;
				ast->val.l = ast->left->val.l - ast->right->val.l;

				dprintf("SUB_T: %ldL - %ldL = %ldL\n", ast->left->val.l, ast->right->val.l, ast->val.l);
			} else if(ast->left->val.t == DBL_T && ast->right->val.t == DBL_T) {
				ast->val.t = DBL_T;
				ast->val.f = ast->left->val.f - ast->right->val.f;

				dprintf("SUB_T: %gF - %gF = %gF\n", ast->left->val.f, ast->right->val.f, ast->val.f);
			} else if(ast->left->val.t == LNG_T) {
				ast->val.t = DBL_T;
				ast->val.f = ast->left->val.l - ast->right->val.f;

				dprintf("SUB_T: %ldL - %gF = %gF\n", ast->left->val.l, ast->right->val.f, ast->val.f);
			} else {
				ast->val.t = DBL_T;
				ast->val.f = ast->left->val.f - ast->right->val.l;

				dprintf("SUB_T: %gF - %ld = %gF\n", ast->left->val.f, ast->right->val.l, ast->val.f);
			}
			break;
		case MUL_T:
			calc_ast(ast->left);
			calc_ast(ast->right);
			if(ast->left->val.t == LNG_T && ast->right->val.t == LNG_T) {
				ast->val.t = LNG_T;
				ast->val.l = ast->left->val.l * ast->right->val.l;

				dprintf("MUL_T: %ldL * %ldL = %ldL\n", ast->left->val.l, ast->right->val.l, ast->val.l);
			} else if(ast->left->val.t == DBL_T && ast->right->val.t == DBL_T) {
				ast->val.t = DBL_T;
				ast->val.f = ast->left->val.f * ast->right->val.f;

				dprintf("MUL_T: %gF * %gF = %gF\n", ast->left->val.f, ast->right->val.f, ast->val.f);
			} else if(ast->left->val.t == LNG_T) {
				ast->val.t = DBL_T;
				ast->val.f = ast->left->val.l * ast->right->val.f;

				dprintf("MUL_T: %ldL * %gF = %gF\n", ast->left->val.l, ast->right->val.f, ast->val.f);
			} else {
				ast->val.t = DBL_T;
				ast->val.f = ast->left->val.f * ast->right->val.l;

				dprintf("MUL_T: %gF * %ld = %gF\n", ast->left->val.f, ast->right->val.l, ast->val.f);
			}
			break;
		case DIV_T:
			calc_ast(ast->left);
			calc_ast(ast->right);
			if(ast->left->val.t == LNG_T && ast->right->val.t == LNG_T) {
				ast->val.t = DBL_T;
				ast->val.f = (double) ast->left->val.l / (double) ast->right->val.l;

				dprintf("DIV_T: %ldL / %ldL = %ldL\n", ast->left->val.l, ast->right->val.l, ast->val.l);
			} else if(ast->left->val.t == DBL_T && ast->right->val.t == DBL_T) {
				ast->val.t = DBL_T;
				ast->val.f = ast->left->val.f / (double) ast->right->val.f;

				dprintf("DIV_T: %gF / %gF = %gF\n", ast->left->val.f, ast->right->val.f, ast->val.f);
			} else if(ast->left->val.t == LNG_T) {
				ast->val.t = DBL_T;
				ast->val.f = (double) ast->left->val.l / ast->right->val.f;

				dprintf("DIV_T: %ldL / %gF = %gF\n", ast->left->val.l, ast->right->val.f, ast->val.f);
			} else {
				ast->val.t = DBL_T;
				ast->val.f = ast->left->val.f / (double) ast->right->val.l;

				dprintf("DIV_T: %gF / %ld = %gF\n", ast->left->val.f, ast->right->val.l, ast->val.f);
			}
			break;
		default:
			fprintf(stderr, "unknown ast type: %d\n", ast->t);
			break;
	}

	if(ast->left) {
		free(ast->left);
		ast->left = NULL;
	}
	if(ast->right) {
		free(ast->right);
		ast->right = NULL;
	}
	ast->t = NULL_T;
}

int main(int argc, char *argv[]) {
	ast_t ast = {NULL_T, {0,0}, NULL, NULL};
	char buf[1024];
	int i;

	if(argc < 2) {
		while(printf("Enter an expression to evaluate or exit q：") && scanf("%s", buf) && strcmp(buf, "q")) {
			if(parse_ast(buf, &ast)<=0) continue;
			calc_ast(&ast);
			if(ast.val.t == LNG_T) {
				printf("%s = %ld\n", buf, ast.val.l);
			} else {
				printf("%s = %g\n", buf, ast.val.f);
			}
		}
	} else {
		for(i=1; i<argc; i++) {
			if(parse_ast(argv[i], &ast) <= 0) continue;
			calc_ast(&ast);
			if(ast.val.t == LNG_T) {
				printf("%s = %ld\n", argv[i], ast.val.l);
			} else {
				printf("%s = %g\n", argv[i], ast.val.f);
			}
		}
	}

	free_ast(&ast);

	return 0;
}
