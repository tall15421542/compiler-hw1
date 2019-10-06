#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "header.h"

#include<iostream>
#include<string>
#include<unordered_map>
using namespace std;

unordered_map<string, char> id_table;
char id_cnt = 'a';
string most_recent_id;

int main( int argc, char *argv[] )
{
    FILE *source, *target;
    Program program;
    SymbolTable symtab;

    if( argc == 3){
        source = fopen(argv[1], "r");
        target = fopen(argv[2], "w");
        if( !source ){
            printf("can't open the source file\n");
            exit(2);
        }
        else if( !target ){
            printf("can't open the target file\n");
            exit(2);
        }
        else{
            program = parser(source);
            fclose(source);
			constant_folding(program);
			test_parser(program);
            symtab = build(program);
            check(&program, &symtab);
            gencode(program, target);
			source = fopen(argv[1], "r");
        }
    }
    else{
        printf("Usage: %s source_file target_file\n", argv[0]);
    }


    return 0;
}

void ungetString(string str, FILE * source){
	for(int i = most_recent_id.size() - 1 ; i >= 0 ; --i){
		ungetc(str[i], source);
	}
}

/********************************************* 
  Scanning 
 *********************************************/
Token getNumericToken( FILE *source, char c )
{
    Token token;
    int i = 0;

    while( isdigit(c) ) {
        token.tok[i++] = c;
        c = fgetc(source);
    }

    if( c != '.' ){
        ungetc(c, source);
        token.tok[i] = '\0';
        token.type = IntValue;
        return token;
    }

    token.tok[i++] = '.';

    c = fgetc(source);
    if( !isdigit(c) ){
        ungetc(c, source);
        printf("Expect a digit : %c\n", c);
        exit(1);
    }

    while( isdigit(c) ){
        token.tok[i++] = c;
        c = fgetc(source);
    }

    ungetc(c, source);
    token.tok[i] = '\0';
    token.type = FloatValue;
    return token;
}

Token scanner( FILE *source )
{
    char c;
    Token token;

    while( !feof(source) ){
        c = fgetc(source);
        while( isspace(c) ) c = fgetc(source);
#ifdef DEBUG
	    cout << "\'" << c << "\'" << endl;	
#endif
        if( isdigit(c) )
            return getNumericToken(source, c);
        token.tok[0] = c;
        token.tok[1] = '\0';
        if( islower(c) ){
			char peek = fgetc(source);
			if(peek == EOF){
				ungetc(' ', source);
			}
			else{
				ungetc(peek, source);
			}
            if( isspace(peek) && c == 'f' )
                token.type = FloatDeclaration;
            else if(isspace(peek) && c == 'i' )
                token.type = IntegerDeclaration;
            else if(isspace(peek) && c == 'p' )
                token.type = PrintOp;
            else{
				string str;
				while(isalpha(c)){
					str += c;
					c = fgetc(source);
				}
				if(!isspace(c)) ungetc(c, source);
#ifdef DEBUG
				cout << "\"" << str << "\"" << endl;
#endif
				unordered_map<string, char>::iterator it = id_table.find(str);
				if(it == id_table.end()){
					id_table.insert(make_pair(str, id_cnt));
					token.tok[0] = id_cnt++;
					if(id_cnt == 'i' || id_cnt == 'p' || id_cnt == 'f') ++id_cnt;
				}else{
					token.tok[0] = it->second;
				}
				most_recent_id = str;
                token.type = Alphabet;
			}
            return token;
        }

        switch(c){
            case '=':
                token.type = AssignmentOp;
                return token;
            case '+':
                token.type = PlusOp;
                return token;
            case '-':
                token.type = MinusOp;
                return token;
            case '*':
                token.type = MulOp;
                return token;
            case '/':
                token.type = DivOp;
                return token;
            case EOF:
                token.type = EOFsymbol;
                token.tok[0] = '\0';
                return token;
            default:
                printf("Invalid character : %c\n", c);
                exit(1);
        }
    }
    token.tok[0] = '\0';
    token.type = EOFsymbol;
    return token;
}


/********************************************************
  Parsing
 *********************************************************/
Declaration parseDeclaration( FILE *source, Token token )
{
#ifdef DEBUG
	cout << "parseDeclaration" << endl;
#endif
    Token token2;
    switch(token.type){
        case FloatDeclaration:
        case IntegerDeclaration:
            token2 = scanner(source);
            if (strcmp(token2.tok, "f") == 0 ||
                    strcmp(token2.tok, "i") == 0 ||
                    strcmp(token2.tok, "p") == 0) {
                printf("Syntax Error: %s cannot be used as id\n", token2.tok);
                exit(1);
            }
            return makeDeclarationNode( token, token2 );
        default:
            printf("Syntax Error: Expect Declaration %s\n", token.tok);
            exit(1);
    }
}

Declarations *parseDeclarations( FILE *source )
{
#ifdef DEBUG
	cout << "parseDeclarations" << endl;
#endif
    Token token = scanner(source);
    Declaration decl;
    Declarations *decls;
    switch(token.type){
        case FloatDeclaration:
        case IntegerDeclaration:
            decl = parseDeclaration(source, token);
            decls = parseDeclarations(source);
            return makeDeclarationTree( decl, decls );
        case PrintOp:
			ungetc(token.tok[0], source);
			return NULL;
        case Alphabet:
            ungetString(most_recent_id, source);
			return NULL;
        case EOFsymbol:
            return NULL;
        default:
            printf("Syntax Error: Expect declarations %s\n", token.tok);
            exit(1);
    }
}

Expression *parseFactor( FILE *source )
{
#ifdef DEBUG
	cout << "parseFactor" << endl;
#endif
    Token token = scanner(source);
    Expression *value = (Expression *)malloc( sizeof(Expression) );
    value->leftOperand = value->rightOperand = NULL;

    switch(token.type){
        case Alphabet:
            (value->v).type = Identifier;
            (value->v).val.id = token.tok[0];
            break;
        case IntValue:
            (value->v).type = IntConst;
            (value->v).val.ivalue = atoi(token.tok);
            break;
        case FloatValue:
            (value->v).type = FloatConst;
            (value->v).val.fvalue = atof(token.tok);
            break;
        default:
            printf("Syntax Error: Expect Identifier or a Number %s\n", token.tok);
            exit(1);
    }

    return value;
}

Expression *parseValue(FILE * source){
#ifdef DEBUG
	cout << "parseValue" << endl;
#endif
	Expression * factor = parseFactor(source);
	Token token = scanner(source);
	if(token.type != MulOp && token.type != DivOp){
		if(token.type == Alphabet){
			ungetString(most_recent_id, source);
		}
		else if(token.type != EOFsymbol){
			ungetc(token.tok[0], source);
		}
		return factor;
	}

	Expression *value = (Expression *)malloc(sizeof(Expression));
	value -> leftOperand = factor;
	switch(token.type){
		case MulOp:
			(value->v).type = MulNode;
			(value->v).val.op = Mul;
			break;
		case DivOp:
			(value->v).type = DivNode;
			(value->v).val.op = Div;
			break;
		defult:
			printf("Impossible in parseValue");
	}
	value->rightOperand = parseFactor(source);
	return value;
}

Expression *parseExpressionTail( FILE *source, Expression *lvalue )
{
#ifdef DEBUG
	cout << "parseExpressionTail" << endl;
#endif
    Token token = scanner(source);
    Expression *expr;

    switch(token.type){
        case PlusOp:
            expr = (Expression *)malloc( sizeof(Expression) );
            (expr->v).type = PlusNode;
            (expr->v).val.op = Plus;
            expr->leftOperand = lvalue;
            expr->rightOperand = parseValue(source);
            return parseExpressionTail(source, expr);
        case MinusOp:
            expr = (Expression *)malloc( sizeof(Expression) );
            (expr->v).type = MinusNode;
            (expr->v).val.op = Minus;
            expr->leftOperand = lvalue;
            expr->rightOperand = parseValue(source);
            return parseExpressionTail(source, expr);
        case Alphabet:
			ungetString(most_recent_id, source);
			return lvalue;
			break;
        case PrintOp:
            ungetc(token.tok[0], source);
            return lvalue;
        case EOFsymbol:
            return lvalue;
        default:
            printf("Syntax Error: Expect a numeric value or an identifier %s\n", token.tok);
            exit(1);
    }
}

Expression *parseExpression( FILE *source, Expression *lvalue )
{
#ifdef DEBUG
	cout << "parseExpression" << endl;
#endif
    Token token = scanner(source);
    Expression *expr;

    switch(token.type){
        case PlusOp:
            expr = (Expression *)malloc( sizeof(Expression) );
            (expr->v).type = PlusNode;
            (expr->v).val.op = Plus;
            expr->leftOperand = lvalue;
            expr->rightOperand = parseValue(source);
            return parseExpressionTail(source, expr);
        case MinusOp:
            expr = (Expression *)malloc( sizeof(Expression) );
            (expr->v).type = MinusNode;
            (expr->v).val.op = Minus;
            expr->leftOperand = lvalue;
            expr->rightOperand = parseValue(source);
            return parseExpressionTail(source, expr);
        case Alphabet:
			ungetString(most_recent_id, source);
			return NULL;
			break;
        case PrintOp:
            ungetc(token.tok[0], source);
            return NULL;
        case EOFsymbol:
            return NULL;
        default:
            printf("Syntax Error: Expect a numeric value or an identifier %s\n", token.tok);
            exit(1);
    }
}

Statement parseStatement( FILE *source, Token token )
{
#ifdef DEBUG
	cout << "parseStatement" << endl;
#endif
    Token next_token;
    Expression *value, *expr;

    switch(token.type){
        case Alphabet:
            next_token = scanner(source);
            if(next_token.type == AssignmentOp){
                value = parseValue(source);
                expr = parseExpression(source, value);
                return makeAssignmentNode(token.tok[0], value, expr);
            }
            else{
                printf("Syntax Error: Expect an assignment op %s\n", next_token.tok);
                exit(1);
            }
        case PrintOp:
            next_token = scanner(source);
            if(next_token.type == Alphabet)
                return makePrintNode(next_token.tok[0]);
            else{
                printf("Syntax Error: Expect an identifier %s\n", next_token.tok);
                exit(1);
            }
            break;
        default:
            printf("Syntax Error: Expect a statement %s\n", token.tok);
            exit(1);
    }
}

Statements *parseStatements( FILE * source )
{
#ifdef DEBUG
	cout << "parseStatements" << endl;
#endif
    Token token = scanner(source);
    Statement stmt;
    Statements *stmts;

    switch(token.type){
        case Alphabet:
        case PrintOp:
            stmt = parseStatement(source, token);
            stmts = parseStatements(source);
            return makeStatementTree(stmt , stmts);
        case EOFsymbol:
            return NULL;
        default:
            printf("Syntax Error: Expect statements %s\n", token.tok);
            exit(1);
    }
}


/*********************************************************************
  Build AST
 **********************************************************************/
Declaration makeDeclarationNode( Token declare_type, Token identifier )
{
    Declaration tree_node;

    switch(declare_type.type){
        case FloatDeclaration:
            tree_node.type = Float;
            break;
        case IntegerDeclaration:
            tree_node.type = Int;
            break;
        default:
            break;
    }
    tree_node.name = identifier.tok[0];

    return tree_node;
}

Declarations *makeDeclarationTree( Declaration decl, Declarations *decls )
{
    Declarations *new_tree = (Declarations *)malloc( sizeof(Declarations) );
    new_tree->first = decl;
    new_tree->rest = decls;

    return new_tree;
}


Statement makeAssignmentNode( char id, Expression *v, Expression *expr_tail )
{
    Statement stmt;
    AssignmentStatement assign;

    stmt.type = Assignment;
    assign.id = id;
    if(expr_tail == NULL)
        assign.expr = v;
    else
        assign.expr = expr_tail;
    stmt.stmt.assign = assign;

    return stmt;
}

Statement makePrintNode( char id )
{
    Statement stmt;
    stmt.type = Print;
    stmt.stmt.variable = id;

    return stmt;
}

Statements *makeStatementTree( Statement stmt, Statements *stmts )
{
    Statements *new_tree = (Statements *)malloc( sizeof(Statements) );
    new_tree->first = stmt;
    new_tree->rest = stmts;

    return new_tree;
}

/* parser */
Program parser( FILE *source )
{
    Program program;

    program.declarations = parseDeclarations(source);
    program.statements = parseStatements(source);

    return program;
}


/********************************************************
  Build symbol table
 *********************************************************/
void InitializeTable( SymbolTable *table )
{
    int i;

    for(i = 0 ; i < 26; i++)
        table->table[i] = Notype;
}

void add_table( SymbolTable *table, char c, DataType t )
{
    int index = (int)(c - 'a');

    if(table->table[index] != Notype)
        printf("Error : id %c has been declared\n", c);//error
    table->table[index] = t;
}

SymbolTable build( Program program )
{
    SymbolTable table;
    Declarations *decls = program.declarations;
    Declaration current;

    InitializeTable(&table);

    while(decls !=NULL){
        current = decls->first;
        add_table(&table, current.name, current.type);
        decls = decls->rest;
    }

    return table;
}


/********************************************************************
  Type checking
 *********************************************************************/


void convertType2(Expression *old, DataType type){
	if(old->v.type == FloatConst && type == Int){
        printf("error2 : can't convert float to integer\n");
        return;
	}
	if(old->v.type == IntConst && type == Float){
        Expression *tmp = (Expression *)malloc( sizeof(Expression) );
        if(old->v.type == Identifier)
            printf("convert to float %c \n",old->v.val.id);
        else
            printf("convert to float %d \n", old->v.val.ivalue);
        tmp->v = old->v;
        tmp->leftOperand = old->leftOperand;
        tmp->rightOperand = old->rightOperand;
        tmp->type = old->type;

        Value v;
        v.type = IntToFloatConvertNode;
        v.val.op = IntToFloatConvert;
		v.val.fvalue = (float)old->v.val.ivalue;
        old->v = v;
        old->type = Int;
        old->leftOperand = tmp;
        old->rightOperand = NULL;
	}
}


void convertType( Expression * old, DataType type )
{
    if(old->type == Float && type == Int){
        printf("error : can't convert \'%c\' float to integer\n", old->v.val.id);
        return;
    }
    if(old->type == Int && type == Float){
        Expression *tmp = (Expression *)malloc( sizeof(Expression) );
        if(old->v.type == Identifier)
            printf("convert to float %c \n",old->v.val.id);
        else
            printf("convert to float %d \n", old->v.val.ivalue);
        tmp->v = old->v;
        tmp->leftOperand = old->leftOperand;
        tmp->rightOperand = old->rightOperand;
        tmp->type = old->type;

        Value v;
        v.type = IntToFloatConvertNode;
        v.val.op = IntToFloatConvert;
		v.val.fvalue = (float)old->v.val.ivalue;
        old->v = v;
        old->type = Int;
        old->leftOperand = tmp;
        old->rightOperand = NULL;
    }
}

DataType generalize2(Expression *left, Expression * right){
	if(left->v.type == FloatConst || right->v.type == FloatConst)
		return Float;
	return Int;
}
DataType generalize( Expression *left, Expression *right )
{
    if(left->type == Float || right->type == Float){
        printf("generalize : float\n");
        return Float;
    }
    printf("generalize : int\n");
    return Int;
}

DataType lookup_table( SymbolTable *table, char c )
{
    int id = c-'a';
    if( table->table[id] != Int && table->table[id] != Float)
        printf("Error : identifier %c is not declared\n", c);//error
    return table->table[id];
}

void checkexpression( Expression * expr, SymbolTable * table )
{
    char c;
    if(expr->leftOperand == NULL && expr->rightOperand == NULL){
        switch(expr->v.type){
            case Identifier:
                c = expr->v.val.id;
                printf("identifier : %c\n",c);
                expr->type = lookup_table(table, c);
                break;
            case IntConst:
                printf("constant : int\n");
                expr->type = Int;
                break;
            case FloatConst:
                printf("constant : float\n");
                expr->type = Float;
                break;
                //case PlusNode: case MinusNode: case MulNode: case DivNode:
            default:
                break;
        }
    }
    else{
        Expression *left = expr->leftOperand;
        Expression *right = expr->rightOperand;

        checkexpression(left, table);
        checkexpression(right, table);

        DataType type = generalize(left, right);
        convertType(left, type);//left->type = type;//converto
        convertType(right, type);//right->type = type;//converto
        expr->type = type;
    }
}

void checkstmt( Statement *stmt, SymbolTable * table )
{
    if(stmt->type == Assignment){
        AssignmentStatement assign = stmt->stmt.assign;
        printf("assignment : %c \n",assign.id);
        checkexpression(assign.expr, table);
        stmt->stmt.assign.type = lookup_table(table, assign.id);
        if (assign.expr->type == Float && stmt->stmt.assign.type == Int) {
            printf("error : can't convert float to integer\n");
        } else {
            convertType(assign.expr, stmt->stmt.assign.type);
        }
    }
    else if (stmt->type == Print){
        printf("print : %c \n",stmt->stmt.variable);
        lookup_table(table, stmt->stmt.variable);
    }
    else printf("error : statement error\n");//error
}

void check( Program *program, SymbolTable * table )
{
    Statements *stmts = program->statements;
    while(stmts != NULL){
        checkstmt(&stmts->first,table);
        stmts = stmts->rest;
    }
}


/***********************************************************************
  Code generation
 ************************************************************************/
void fprint_op( FILE *target, ValueType op )
{
    switch(op){
        case MinusNode:
            fprintf(target,"-\n");
            break;
        case PlusNode:
            fprintf(target,"+\n");
            break;
        case MulNode:
            fprintf(target,"*\n");
            break;
        case DivNode:
            fprintf(target,"/\n");
            break;
        default:
            fprintf(target,"Error in fprintf_op ValueType = %d\n",op);
            break;
    }
}

void fprint_expr( FILE *target, Expression *expr)
{

    if(expr->leftOperand == NULL){
        switch( (expr->v).type ){
            case Identifier:
                fprintf(target,"l%c\n",(expr->v).val.id);
                break;
            case IntConst:
                fprintf(target,"%d\n",(expr->v).val.ivalue);
                break;
            case FloatConst:
                fprintf(target,"%f\n", (expr->v).val.fvalue);
                break;
            default:
                fprintf(target,"Error In fprint_left_expr. (expr->v).type=%d\n",(expr->v).type);
                break;
        }
    }
    else{
        fprint_expr(target, expr->leftOperand);
        if(expr->rightOperand == NULL){
            fprintf(target,"5k\n");
        }
        else{
            //	fprint_right_expr(expr->rightOperand);
            fprint_expr(target, expr->rightOperand);
            fprint_op(target, (expr->v).type);
        }
    }
}

void gencode(Program prog, FILE * target)
{
    Statements *stmts = prog.statements;
    Statement stmt;

    while(stmts != NULL){
        stmt = stmts->first;
        switch(stmt.type){
            case Print:
                fprintf(target,"l%c\n",stmt.stmt.variable);
                fprintf(target,"p\n");
                break;
            case Assignment:
                fprint_expr(target, stmt.stmt.assign.expr);
                /*
                   if(stmt.stmt.assign.type == Int){
                   fprintf(target,"0 k\n");
                   }
                   else if(stmt.stmt.assign.type == Float){
                   fprintf(target,"5 k\n");
                   }*/
                fprintf(target,"s%c\n",stmt.stmt.assign.id);
                fprintf(target,"0 k\n");
                break;
        }
        stmts=stmts->rest;
    }

}


/***************************************
  For our debug,
  you can omit them.
 ****************************************/
void print_expr(Expression *expr)
{
    if(expr == NULL)
        return;
    else{
        print_expr(expr->leftOperand);
        switch((expr->v).type){
            case Identifier:
                printf("%c ", (expr->v).val.id);
                break;
            case IntConst:
                printf("%d ", (expr->v).val.ivalue);
                break;
            case FloatConst:
                printf("%f ", (expr->v).val.fvalue);
                break;
            case PlusNode:
                printf("+ ");
                break;
            case MinusNode:
                printf("- ");
                break;
            case MulNode:
                printf("* ");
                break;
            case DivNode:
                printf("/ ");
                break;
            case IntToFloatConvertNode:
                printf("(float) ");
                break;
            default:
                printf("error ");
                break;
        }
        print_expr(expr->rightOperand);
    }
}

void test_parser(Program program)
{
    Declarations *decls;
    Statements *stmts;
    Declaration decl;
    Statement stmt;

    decls = program.declarations;

    while(decls != NULL){
        decl = decls->first;
        if(decl.type == Int)
            printf("i ");
        if(decl.type == Float)
            printf("f ");
        printf("%c ",decl.name);
        decls = decls->rest;
    }

    stmts = program.statements;

    while(stmts != NULL){
        stmt = stmts->first;
        if(stmt.type == Print){
            printf("p %c ", stmt.stmt.variable);
        }

        if(stmt.type == Assignment){
            printf("%c = ", stmt.stmt.assign.id);
            print_expr(stmt.stmt.assign.expr);
        }
        stmts = stmts->rest;
    }

}

/* constant folding */
void constant_opt_subtree(Expression * expression){
	if(expression == NULL) return;
	Expression * left = expression->leftOperand;
	Expression * right = expression->rightOperand;
	constant_opt_subtree(left);
	constant_opt_subtree(right);
	if(left == NULL || right == NULL) return;
	if((left->v.type == IntConst || left->v.type == FloatConst) &&
		(right->v.type == IntConst || right->v.type == FloatConst)){
        DataType type = generalize2(left, right);
        convertType2(left, type);//left->type = type;//converto
        convertType2(right, type);//right->type = type;//converto
		switch(type){
			case Int:
				switch(expression->v.val.op){
					case Plus:
						expression->v.val.ivalue = left->v.val.ivalue + right->v.val.ivalue;
						break;
					case Minus:
						expression->v.val.ivalue = left->v.val.ivalue - right->v.val.ivalue;
						break;
					case Mul:
						expression->v.val.ivalue = left->v.val.ivalue * right->v.val.ivalue;
						break;
					case Div:
						expression->v.val.ivalue = left->v.val.ivalue / right->v.val.ivalue;
						break;
				}
				expression->v.type = IntConst;
				expression->type = Int;
				break;
			case Float:
				switch(expression->v.val.op){
					case Plus:
						expression->v.val.fvalue = left->v.val.fvalue + right->v.val.fvalue;
						break;
					case Minus:
						expression->v.val.fvalue = left->v.val.fvalue - right->v.val.fvalue;
						break;
					case Mul:
						expression->v.val.fvalue = left->v.val.fvalue * right->v.val.fvalue;
						break;
					case Div:
						expression->v.val.fvalue = left->v.val.fvalue / right->v.val.fvalue;
						break;
				}
				expression->v.type = FloatConst;
				expression->type = Float;
				break;
		}
		free(right);
		free(left);
		expression->rightOperand = expression->leftOperand = NULL;
		return;
	}
	return;
}
void constant_folding(Program program){
    Statements *stmts;
    Statement stmt;
	stmts = program.statements;
	while(stmts != NULL){
		stmt = stmts->first;
		if(stmt.type == Assignment){
			constant_opt_subtree(stmt.stmt.assign.expr);
		}
		stmts = stmts->rest;
	}
}
