/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-03-03
Date Last Modified: 2026-03-03
Module: System Tools
File: calculator.c
About: OE System Calculator – Robust expression engine
       using Shunting Yard algorithm.

       Supports:
       - +  -  *  /  //  %  ^
       - Decimal numbers
       - Unary negative
       - (), [], {}
       - Strict syntax validation
       - Division by zero detection
       - Infinite loop until exit

Revisions:
- 2026-03-03  Initial integration as system tool
- 2026-03-03  Integrated PAL math abstraction support
- 2026-03-03  Removed std C headers (OE compliant)
- 2026-03-03  Added strict syntax error detection
- 2026-03-03  Added bracket mismatch validation
- 2026-03-03  Added operand expectation state logic
- 2026-03-03  Integrated pal_ftoa for output formatting
- 2026-03-03  Fixed static analyzer buffer warnings
- 2026-03-03  Improved internal stack safety
- 2026-04-12  New Coordinates mapped
- 2026-08-26  BUG FIX: pal_ftoa() formats with sprintf("%.*f", ...)
              into a caller-supplied buffer with no length check.
              "^" has no result-magnitude guard, so an expression
              like 9999^9999 produced a value needing ~300+ digits
              in fixed-point notation — a stack buffer overflow into
              the old char buffer[64]. Widened to fit any finite
              double in fixed notation (DBL_MAX needs ~309 integer
              digits).
------------------------------------------------------------
*/

#include "calculator.h"
#include "pal.h"
#include "pal_math.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "utils.h"

#define MAX 512

/* ====================== CALCULATOR INTERNAL RETURN CODES ====================== */

#define CALC_OK               1
#define CALC_SYNTAX_ERROR    -1
#define CALC_DIV_ZERO        -2

/* ====================== STACK STRUCTURES ====================== */

typedef struct {
    double data[MAX];
    int top;
} ValueStack;

typedef struct {
    /* 4 bytes safe for "//" + '\0' */
    char data[MAX][4];
    int top;
} OpStack;

/* ====================== STACK OPERATIONS – VALUE STACK ====================== */

static void initVal(ValueStack* s) { s->top = -1; }
static void initOp(OpStack* s) { s->top = -1; }

static int pushVal(ValueStack* s, double v) {
    if (s->top >= MAX - 1) return 0;
    s->data[++s->top] = v;
    return 1;
}

static double popVal(ValueStack* s) {
    if (s->top < 0) return 0;
    return s->data[s->top--];
}

/* ====================== STACK OPERATIONS – OPERATOR STACK ====================== */

static int pushOp(OpStack* s, const char* op) {

    if (s->top >= MAX - 1) return 0;

    int i = 0;

    /* manual safe copy (max 3 chars + null) */
    while (op[i] && i < 3) {
        s->data[s->top + 1][i] = op[i];
        i++;
    }

    s->data[s->top + 1][i] = '\0';
    s->top++;

    return 1;
}

static char* popOp(OpStack* s) {
    if (s->top < 0) return 0;
    return s->data[s->top--];
}

static char* peekOp(OpStack* s) {
    if (s->top < 0) return 0;
    return s->data[s->top];
}

/* ======================   OPERATOR UTILITY FUNCTIONS   ====================== */

static int precedence(const char* op) {
    if (pal_strcmp(op, "^") == 0) return 4;
    if (pal_strcmp(op, "*") == 0 || pal_strcmp(op, "/") == 0 ||
        pal_strcmp(op, "//") == 0 || pal_strcmp(op, "%") == 0) return 3;
    if (pal_strcmp(op, "+") == 0 || pal_strcmp(op, "-") == 0) return 2;
    return 0;
}

static int isRightAssociative(const char* op) {
    return pal_strcmp(op, "^") == 0;
}

static int isOpening(char c) {
    return (c == '(' || c == '[' || c == '{');
}

static int isClosing(char c) {
    return (c == ')' || c == ']' || c == '}');
}

static int match(char o, char c) {
    return (o == '(' && c == ')') ||
        (o == '[' && c == ']') ||
        (o == '{' && c == '}');
}

static int isOperatorChar(char c) {
    return (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^');
}

/* ======================   APPLY OPERATOR TO TOP TWO VALUES   ====================== */

static int apply(ValueStack* vals, const char* op) {

    if (vals->top < 1)
        return CALC_SYNTAX_ERROR;

    double b = popVal(vals);
    double a = popVal(vals);

    if (pal_strcmp(op, "+") == 0) pushVal(vals, a + b);
    else if (pal_strcmp(op, "-") == 0) pushVal(vals, a - b);
    else if (pal_strcmp(op, "*") == 0) pushVal(vals, a * b);

    else if (pal_strcmp(op, "/") == 0) {
        if (b == 0) return CALC_DIV_ZERO;
        pushVal(vals, a / b);
    }

    else if (pal_strcmp(op, "//") == 0) {
        if (b == 0) return CALC_DIV_ZERO;
        pushVal(vals, pal_floor(a / b));
    }

    else if (pal_strcmp(op, "%") == 0) {
        if (b == 0) return CALC_DIV_ZERO;
        pushVal(vals, pal_fmod(a, b));
    }

    else if (pal_strcmp(op, "^") == 0) {
        pushVal(vals, pal_pow(a, b));
    }

    return CALC_OK;
}

/* ======================   CORE EXPRESSION EVALUATION ENGINE Implements Shunting Yard Algorithm   ====================== */

static int evaluate(char* expr, double* result) {

    ValueStack values;
    OpStack ops;

    initVal(&values);
    initOp(&ops);

    int expect_operand = 1;
    int i = 0;

    while (expr[i]) {

        if (expr[i] == ' ' || expr[i] == '\t') { i++; continue; }

        /* ----- NUMBER / UNARY MINUS ----- */
        if ((expr[i] >= '0' && expr[i] <= '9') ||
            (expr[i] == '-' && expect_operand))
        {
            char num[64];
            int j = 0;
            int dot_count = 0;

            if (expr[i] == '-')
                num[j++] = expr[i++];

            while ((expr[i] >= '0' && expr[i] <= '9') || expr[i] == '.') {

                if (expr[i] == '.') {
                    dot_count++;
                    if (dot_count > 1)
                        return CALC_SYNTAX_ERROR;
                }

                if (j >= 63)
                    return CALC_SYNTAX_ERROR;

                num[j++] = expr[i++];
            }

            num[j] = '\0';

            if (!pushVal(&values, pal_atof(num)))
                return CALC_SYNTAX_ERROR;

            expect_operand = 0;
        }

        /* ----- OPENING BRACKET ----- */
        else if (isOpening(expr[i])) {
            char temp[2] = { expr[i], '\0' };
            if (!pushOp(&ops, temp))
                return CALC_SYNTAX_ERROR;
            i++;
            expect_operand = 1;
        }

        /* ----- CLOSING BRACKET ----- */
        else if (isClosing(expr[i])) {

            while (ops.top >= 0 && !isOpening(peekOp(&ops)[0])) {
                int code = apply(&values, popOp(&ops));
                if (code != CALC_OK)
                    return code;
            }

            if (ops.top < 0 || !match(peekOp(&ops)[0], expr[i]))
                return CALC_SYNTAX_ERROR;

            popOp(&ops);
            i++;
            expect_operand = 0;
        }

        /* ----- OPERATOR ----- */
        else if (isOperatorChar(expr[i])) {

            if (expect_operand)
                return CALC_SYNTAX_ERROR;

            char current[3] = { 0 };

            if (expr[i] == '/' && expr[i + 1] == '/') {
                current[0] = '/';
                current[1] = '/';
                current[2] = '\0';
                i += 2;
            }
            else {
                current[0] = expr[i];
                current[1] = '\0';
                i++;
            }

            while (ops.top >= 0 && !isOpening(peekOp(&ops)[0]) &&
                (precedence(peekOp(&ops)) > precedence(current) ||
                    (precedence(peekOp(&ops)) == precedence(current) &&
                        !isRightAssociative(current))))
            {
                int code = apply(&values, popOp(&ops));
                if (code != CALC_OK)
                    return code;
            }

            if (!pushOp(&ops, current))
                return CALC_SYNTAX_ERROR;

            expect_operand = 1;
        }

        else {
            return CALC_SYNTAX_ERROR;
        }
    }

    /* ----- APPLY REMAINING OPERATORS ----- */

    while (ops.top >= 0) {

        if (isOpening(peekOp(&ops)[0]))
            return CALC_SYNTAX_ERROR;

        int code = apply(&values, popOp(&ops));
        if (code != CALC_OK)
            return code;
    }

    if (values.top != 0)
        return CALC_SYNTAX_ERROR;

    *result = popVal(&values);
    return CALC_OK;
}

/* ======================   ENTRY FUNCTION – UI INTEGRATION LAYER   ====================== */

void calculator(void) {

    char input[512];

    while (1) {

        ui_init(); 
        ui_title(UI_CALC_TITLE_ROW, UI_CALC_TITLE_COL, red bold underline, "OE SYSTEM CALCULATOR");
        ui_title(UI_CALC_SUPPORT_ROW, UI_CALC_SUPPORT_COL, green bold, "Supported: + - * / // % ^ () [] {}");
        ui_title(UI_CALC_HINT_ROW, UI_CALC_HINT_COL, blue bold, "Type E to return");

        ui_title(UI_CALC_INPUT_ROW, UI_CALC_INPUT_COL, yellow bold, "Enter Expression(max length 510): " reset);

        pal_readline(input, sizeof(input));
        trim_whitespace(input);

        if (input[0] == 'e' || input[0] == 'E')
            break;

        double result;
        int code = evaluate(input, &result);

        if (code == CALC_SYNTAX_ERROR) {
            ui_status(STATUS_SYNTAX_ERROR);
            continue;
        }
        else if (code == CALC_DIV_ZERO) {
            ui_status(STATUS_DIVISION_BY_ZERO);
            continue;
        }

        ui_title(UI_CALC_RESULT_ROW, UI_CALC_RESULT_COL, cyan bold, "Result: " reset);

        char buffer[350];   /* fits any finite double in fixed-point notation */
        pal_ftoa(result, buffer, 8);
        pal_print(buffer);
		ui_status(STATUS_SUCCESS);
    }
}