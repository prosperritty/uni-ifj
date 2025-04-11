/**
 * @file parser.c
 * Project - IFJ Project 2024
 *
 * @author Albert Tikaiev xtikaia00
 * @author Danil Tasmassys xtasmad00
 *
 * @brief Parser source file
 */

#include "parser.h"

// Current token
Token token;
// Root node of AST
ASTStart *ast_start;
// If it is first pass of parser
bool first_pass;
// Current symtable function item
symtable_item_data *current_func_sym;

/**
 * Get next token from token array
 */
void GetNextToken() {
  token = *scanner->tokens[scanner->current_token];
  scanner->current_token++;
}

/**
 * @brief Main function of parser
 */
void Parse() {
  SymtableInit();

  // First pass parsing
  first_pass = true;
  FirstPassSymtable();
  scanner->current_token = 0;

  // Second pass parsing
  first_pass = false;
    ParseProlog();

  //Assert that all function had return(besides void function)
  SymtableAssertReturns();

  //AST is ready, calling code generator
  CgenStart(ast_start);
}

/**
 * @brief Fill symtable with function data
 * First pass of parser
 *
 * Tries to save all attributes of all functions to symtable
 */
void FirstPassSymtable() {
  scanner->current_token = 0;
  GetNextToken();
  // Skip prolog until first function
  while (token.keyword != K_PUB && token.type != T_EOF) {
    GetNextToken();
  }
  ParseFuncDecl(NULL);
  // Main function semantics
  symtable_item_data *main_func = SymtableAssertFunction("main");
  // Main function can only be return type of void
  if (main_func->details.func.returnType != ST_VOID)
    InvokeExit(PARAMETER_RETURN_ERROR);
  // Main function can't have any arguments
  if (main_func->details.func.params->size != 0)
    InvokeExit(PARAMETER_RETURN_ERROR);
}

/**
 * @brief Parsing prolog
 *
 * <PROLOG> -> const ifj = @import("ifj24.zig"); <FIRST_FUNC_DECL>
 */
void ParseProlog() {
  // Get first token
  GetNextToken();

  // PROLOG
  // const ifj = @import("ifj24.zig");
  for (int i = 0; i < 9; i++) {
    switch (i) {
    case 0:
      if (token.type == T_ID && token.keyword == K_CONST)break;
      InvokeExit(SYNTAX_ERROR);break;
    case 1:
      if (token.type == T_ID && token.keyword == K_IFJ)break;
      InvokeExit(SYNTAX_ERROR);break;
    case 2:
      if (token.type == T_ASSIGN)break;
      InvokeExit(SYNTAX_ERROR);break;
    case 3:
      if (token.type== T_AT)break;
      InvokeExit(SYNTAX_ERROR);break;
    case 4:
      if (token.type == T_ID && StringEquals(token.value.string, "import")) break;
      InvokeExit(SYNTAX_ERROR);break;
    case 5:
      if (token.type == T_OPAREN) break;
      InvokeExit(SYNTAX_ERROR);break;
    case 6:
      if (token.type == T_STR && StringEquals(token.value.string, "ifj24.zig")) break;
      InvokeExit(SYNTAX_ERROR);break;
    case 7:
      if (token.type == T_CPAREN) break;
      InvokeExit(SYNTAX_ERROR);break;
    case 8:
      if (token.type == T_SEMICOLON) break;
      InvokeExit(SYNTAX_ERROR);break;
    default:
      break;
    }
    GetNextToken();
  }

  //<FUNC_DECL>
  ast_start = CreateASTStart();
  ParseFuncDecl(&ast_start->funcdecls);
}

/**
 * @brief Parsing function declaration
 *
 * In first pass fill symtable with certain information
 * In second pass fill AST node
 *
 * @param func_decl Pointer to AST function declaration node
 */
void ParseFuncDecl(ASTFuncDecl **func_decl) {
  // pub fn
  if (token.type == T_ID && token.keyword == K_PUB)GetNextToken();
  else InvokeExit(SYNTAX_ERROR);
  if (token.type == T_ID && token.keyword == K_FN)GetNextToken();
  else InvokeExit(SYNTAX_ERROR);

  //<ID>
  if (token.type != T_ID || token.keyword != K_UNKNOWN) InvokeExit(SYNTAX_ERROR);

  //fill symtable
  if (first_pass) {
    //Semantic check(no overloading in IFJ24)
    if (SymtableFind(token.value.string->str) != NULL) InvokeExit(REDIFINE_ERROR);
    symtable_item_data *func_item = SymtableAdd(token.value.string->str, ST_FUNC);
    current_func_sym = func_item;
  }
  //fill AST node
  else {
    current_func_sym = SymtableFind(token.value.string->str);
    (*func_decl) = CreateASTFuncDecl();
    (*func_decl)->name = token.value.string->str;
  }
  GetNextToken();

  //Function parameters
  if (token.type == T_OPAREN) {
    // consume '('
    GetNextToken();
    if (first_pass) ParseFuncParams(NULL);
    else {
      //Entered scope with function parameters
      SymtableEnterScope();
      ParseFuncParams(&((*func_decl)->params));
    }
  }
  else InvokeExit(SYNTAX_ERROR);

  // consume ')'
  if (token.type == T_CPAREN)
    GetNextToken();
  else InvokeExit(SYNTAX_ERROR);

  //Function return type
  if ((token.type == T_ID && token.keyword != K_UNKNOWN) || token.type == T_QUESTM || token.type == T_OSQUARE) {
    if (first_pass) {
      DataType return_function_type = ParseType();
      if(return_function_type == ST_NOT_DEFINED) InvokeExit(SYNTAX_ERROR);
      if(return_function_type == ST_VOID) current_func_sym->details.func.returnMade = true;
      current_func_sym->details.func.returnType = return_function_type;
    }
  }
  else InvokeExit(SYNTAX_ERROR);
  if(token.type == T_QUESTM) GetNextToken();
    if(token.type == T_OSQUARE) {
        GetNextToken();
        GetNextToken();
    }
  GetNextToken();

  //If it first pass we don't want togo further
  //And trying to find another function declaration
  if (first_pass) {
    while (true) {
      if (token.type == T_ID && token.keyword == K_PUB)
        break;
      if (token.type == T_EOF)
        break;
      GetNextToken();
    }
    ParseFuncDeclNext(NULL);
    return;
  }

  //Otherwise we continue to parse function body
  else ParseBody(&(*func_decl)->body, true);

  // All variables that was declared in function
  (*func_decl)->variables = current_func_sym->details.func.variables;

  ParseFuncDeclNext(func_decl);
}

/**
 * @brief Try to parse next function declaration
 *
 * @param func_decl Pointer to already parsed AST function declaration node
 */
void ParseFuncDeclNext(ASTFuncDecl **func_decl) {
  // If there is no function, that means we parsed all code
  if (token.type == T_EOF)
    return;

  if (first_pass)
    ParseFuncDecl(NULL);
  else
    //Parse another function declaration for next node
    ParseFuncDecl(&(*func_decl)->next);
}

/**
 * @brief Parse function declaration parameters
 *
 * @param params First AST param node from function declaration node
 */
void ParseFuncParams(ASTParam **params) {
  if (token.type != T_CPAREN)
    ParseParam(params);
}

/**
 * @brief Parse one function parameter
 *
 * @param params Current AST param node
 */
void ParseParam(ASTParam **params) {
  // In second pass of parser we want to work with ast node
  if (!first_pass) (*params) = CreateASTParam();

  Param *param_func = NULL;
  symtable_item_data *param_var = NULL;
  //fill symtable
  if (first_pass) {
    if (token.type == T_ID && token.keyword == K_UNKNOWN) {
      param_func = InvokeAlloc(sizeof(Param));
      param_func->name = token.value.string->str;
      InvokeAddVarsArray(current_func_sym->details.func.params, param_func);
    }
    else InvokeExit(SYNTAX_ERROR);
  }
  //fill AST node
  else {
    if(SymtableFind(token.value.string->str) != NULL) InvokeExit(REDIFINE_ERROR);
    param_var = SymtableAdd(token.value.string->str, ST_VAR_CONST);
    (*params)->name = token.value.string->str;
  }
  GetNextToken();

  // : type
  if(token.type != T_COLON) InvokeExit(SYNTAX_ERROR);
  GetNextToken();

  DataType type = ParseType();
  if (first_pass) {
    switch (type) {
    case ST_NOT_DEFINED:
    case ST_VOID:
      InvokeExit(SYNTAX_ERROR);
      break;
    default:
      param_func->type = type;
      break;
    }
  }
  else{
    param_var->details.var.type = type;
  }
  // consumes type
  GetNextToken();

  if (first_pass)
    ParseParamNext(NULL);
  else
    ParseParamNext(params);
}

/**
 * @brief Try to parse next function parameter
 *
 * @param params Pointer to already parsed AST param node
 */
void ParseParamNext(ASTParam **params) {
  if (token.type == T_CPAREN)
    return;
  if (token.type != T_COMMA)
    InvokeExit(SYNTAX_ERROR);

  // consumes ','
  GetNextToken();

  if (first_pass)
    ParseParam(NULL);
  else
    //Parse next parameter for next node
    ParseParam(&(*params)->next);
}

/**
 * @brief Parse body of function, if, else and while
 *
 * @param body Pointer to AST node of body
 * @param already_entered If SymtableEnterScope was already called
 */
void ParseBody(ASTBody **body, bool already_entered) {
  // try to consume '{'
  if (token.type != T_OCURLY)
    InvokeExit(SYNTAX_ERROR);

  if (!already_entered) {
    SymtableEnterScope();
  }

  GetNextToken();
  (*body) = CreateASTBody();
  ParseStatementNext(&(*body)->statement);

  // try to consume '}'
  if (token.type != T_CCURLY)
    InvokeExit(SYNTAX_ERROR);

  SymtableLeaveScope();

  GetNextToken();
}

/**
 * @brief Parse statement in body
 *
 * @param statement Pointer to general AST statement node that has to be created
 */
void ParseStatement(ASTStatement **statement) {
  (*statement) = CreateASTStatement();

  //If and else statement parsing
  if (token.keyword == K_IF) {
    (*statement)->type = T_IF;
    (*statement)->statement = CreateASTIfStatement();
    ParseIfStatement((ASTIfStatement **)&(*statement)->statement);

    //AST node for if statement also has else body
    if (token.keyword == K_ELSE)
      ParseElseStatement((ASTIfStatement **)&(*statement)->statement);
    else InvokeExit(SYNTAX_ERROR);
  }

  //While statement parsing
  else if (token.keyword == K_WHILE) {
    (*statement)->type = T_WHILE;
    (*statement)->statement = CreateASTWhileStatement();
    ParseWhileStatement((ASTWhileStatement **)&(*statement)->statement);
  }

  //Variable and constant declaration parsing
  else if (token.type == T_ID && token.keyword == K_CONST) {
    (*statement)->type = T_VARDECLDEF;
    (*statement)->statement = CreateASTVarDeclDef();
    ParseVarDecl(ST_VAR_CONST, (*statement)->statement);
  } else if (token.type == T_ID && token.keyword == K_VAR) {
    (*statement)->type = T_VARDECLDEF;
    (*statement)->statement = CreateASTVarDeclDef();
    ParseVarDecl(ST_VAR_VAR, (*statement)->statement);
  }

  //Return statement parsing
  else if (token.type == T_ID && token.keyword == K_RETURN) {
    (*statement)->type = T_RETURN;
    (*statement)->statement = CreateASTReturn();
    ParseReturn((*statement)->statement);
    GetNextToken();
  }

  //ifj functions parsing
  else if (token.type == T_ID && token.keyword == K_IFJ) {
    // consume ifj
    GetNextToken();
    (*statement)->type = T_FCALL;
    (*statement)->statement = CreateASTFuncCall();
    if (ParseFuncCall((*statement)->statement) != ST_VOID)
      InvokeExit(RETURN_ERROR);
    // consume )
    GetNextToken();
    if (token.type != T_SEMICOLON)
      InvokeExit(SYNTAX_ERROR);
    GetNextToken();
  }

  //Variable redefinition and users function call parsing
  else if (token.type == T_ID && token.keyword == K_UNKNOWN) {
    char *name = token.value.string->str;
    GetNextToken();

    //Variable redefinition in case next token is =
    if (token.type == T_ASSIGN) {
      (*statement)->type = T_VARDECLDEF;
      (*statement)->statement = CreateASTVarDeclDef();
      ASTVarDeclDef *varDecl = (*statement)->statement;

      SymtableAssertVariable(name);
      symtable_item_data *var = SymtableUpdate_isModified(name);
      SymtableUpdate_isUsed(name);

      varDecl->name = name;
      DataType redef_type = ParseVarDef(varDecl);
      
      //Check nullable types compatibilities
      if(CheckNullableTypes(var->details.var.type, redef_type) == true){}
      else if(redef_type != var->details.var.type) InvokeExit(TYPE_ERROR);
    }

    //Void function call in case next token is (
    else if (token.type == T_OPAREN) {
      (*statement)->type = T_FCALL;
      (*statement)->statement = CreateASTFuncCall();
      if (ParseFuncCall((*statement)->statement) != ST_VOID)
        InvokeExit(PARAMETER_RETURN_ERROR);

      // Consume )
      GetNextToken();

      // Try to consume ;
      if (token.type != T_SEMICOLON)
        InvokeExit(SYNTAX_ERROR);
      GetNextToken();
    }

    else InvokeExit(SYNTAX_ERROR);
  }

  //_ = ...
  //Ignoring return from expression/function call
  else if (token.type == T_UNDERSCORE) {
    GetNextToken();
    if (token.type == T_ASSIGN) {
      (*statement)->type = T_VARDECLDEF;
      (*statement)->statement = CreateASTVarDeclDef();
      ASTVarDeclDef *varDecl = (*statement)->statement;
      ParseVarDef(varDecl);
      varDecl->isIgnoring = true;
    }
    else InvokeExit(SYNTAX_ERROR);
  }

  else {
    InvokeExit(SYNTAX_ERROR);
  }

  ParseStatementNext(&(*statement)->next);
}

/**
 * @brief Try to parse next statement of body
 *
 * @param statement Pointer to next AST statement node
 */
void ParseStatementNext(ASTStatement **statement) {
  if (token.type == T_CCURLY)
    return;
  else
    ParseStatement(statement);
}

/**
 * @brief Parse if statement
 *
 * @param statement Pointer to AST If statement node
 */
void ParseIfStatement(ASTIfStatement **statement) {
  // consume If
  GetNextToken();
  (*statement)->expr = CreateASTExpression();
  DataType if_expr = ParseExpr((*statement)->expr, true, false);
  // consume )
  GetNextToken();

  //Default bool condition
  if(if_expr == ST_BOOL) {
    ParseBody(&(*statement)->ifBody, false);
  }

  //If it's nullable it must continue with new variable in |var|
  else if(if_expr == ST_NULLI32 || if_expr == ST_NULLU8 || if_expr == ST_NULLF64 || if_expr == ST_NULL) {
    SymtableEnterScope();

    (*statement)->notNullID = ParseNotNull(if_expr);

    ParseBody(&(*statement)->ifBody, true);
  }
  else InvokeExit(TYPE_ERROR);
}

/**
 * @brief Parse else statement
 *
 * @param statement Pointer to AST If statement node
 */
void ParseElseStatement(ASTIfStatement **statement) {
  //consume else
  GetNextToken();
  ParseBody(&(*statement)->elseBody, false);
}

/**
 * @brief Parse while statement
 *
 * @param statement Pointer to AST while statement node
 */
void ParseWhileStatement(ASTWhileStatement **statement) {
  // consume WHILE
  GetNextToken();
  (*statement)->expr = CreateASTExpression();
  DataType while_expr = ParseExpr((*statement)->expr, true, false);
  GetNextToken();

  //Default bool condition
  if(while_expr == ST_BOOL) {
    ParseBody(&(*statement)->whileBody, false);
  }

  //If it's nullable it must continue with new variable in |var|
  else if(while_expr == ST_NULLI32 || while_expr == ST_NULLU8 || while_expr == ST_NULLF64 || while_expr == ST_NULL) {
    SymtableEnterScope();

    (*statement)->notNullID = ParseNotNull(while_expr);

    ParseBody(&(*statement)->whileBody, true);
  }
  else InvokeExit(TYPE_ERROR);
}

/**
 * @brief Parse not null id
 *
 * @param nullable_type which nullable type was expression
 *
 * @return name of not null variable in this scope
 */
char* ParseNotNull(DataType nullable_type) {
  char* notnull_name = NULL;

  //Other semantic error, because LL grammar let us go without this part
  if(token.type != T_PIPE) InvokeExit(OTHER_SEMANTIC_ERROR);
  GetNextToken();

  if(token.type == T_ID && token.keyword == K_UNKNOWN) {
    notnull_name = token.value.string->str;

    //Already entered scope before
    //Adding this variable to this scope
    if(SymtableFind(token.value.string->str) != NULL) InvokeExit(REDIFINE_ERROR);
    symtable_item_data *nullable_var = SymtableAdd(token.value.string->str, ST_VAR_CONST);
    InvokeAddVarsArray(current_func_sym->details.func.variables, token.value.string->str);

    if(nullable_type==ST_NULLI32) nullable_var->details.var.type = ST_I32;
    else if(nullable_type==ST_NULLF64) nullable_var->details.var.type = ST_F64;
    else if(nullable_type==ST_NULLU8) nullable_var->details.var.type = ST_U8;
    else if(nullable_type==ST_NULL) nullable_var->details.var.type = ST_NULL;
  }
  //Ignoring variable
  else if(token.type == T_UNDERSCORE) notnull_name = "_";
  else InvokeExit(SYNTAX_ERROR);
  GetNextToken();
  if(token.type != T_PIPE) InvokeExit(SYNTAX_ERROR);
  GetNextToken();

  return notnull_name;
}

/**
 * @brief Parse variable/constant declaration
 *
 * @param const_type if variable is const or var
 * @param var_decl
 */
void ParseVarDecl(SymbolType const_type, ASTVarDeclDef *var_decl) {
  // consume const/var
  GetNextToken();
  symtable_item_data *var_item = NULL;

  //id
  if (token.type == T_ID && token.keyword == K_UNKNOWN) {

    // Semantic check on redefining variable with the same name in certain scope
    if (SymtableFind(token.value.string->str) != NULL)
      InvokeExit(REDIFINE_ERROR);

    var_item = SymtableAdd(token.value.string->str, const_type);

    var_decl->name = token.value.string->str;
    InvokeAddVarsArray(current_func_sym->details.func.variables,
              token.value.string->str);
    GetNextToken();

    //type which is optional in var/const declaration
    DataType var_type = ST_NOT_DEFINED;
    if (token.type == T_COLON) {
      GetNextToken();
      var_type = ParseType();
      GetNextToken();
    }
    var_item->details.var.type = var_type;

    //definition in declaration is necessary
    if (token.type != T_ASSIGN)
      InvokeExit(SYNTAX_ERROR);
    DataType decl_type = ParseVarDef(var_decl);

    //All nullable types can be NULL or not nullable type of the same or nullable type of the same type
    if(CheckNullableTypes(var_type, decl_type) == true){}
    //Can't decide what type(null or string literal) when there wasn't variable type in declaration
    else if((decl_type == ST_NULL || decl_type == ST_STRING) && var_type == ST_NOT_DEFINED) InvokeExit(TYPE_DEDUCTION_ERROR);
    //Deciding what type from expression if there wasn't type in declaration part
    else if(var_type == ST_NOT_DEFINED) var_item->details.var.type = decl_type;
    //If types isn't the same, then type error
    else if(var_type != decl_type) InvokeExit(TYPE_ERROR);

    //If it is const/literals
    if(isknown_return && var_item->symType == ST_VAR_CONST) var_item->details.var.isKnown = true;
  }
  else if(token.keyword == K_IFJ) InvokeExit(REDIFINE_ERROR);
  else InvokeExit(SYNTAX_ERROR);
}

/**
 * @brief Parse variable definition
 *
 * @param var_decl Pointer to AST node, where definition expression will be
 *
 * @return Type of definition expression
 */
DataType ParseVarDef(ASTVarDeclDef *var_decl) {
  var_decl->expr = CreateASTExpression();
  DataType var_def_type = ParseExpr(var_decl->expr, false, false);
  //Empty expression in variable re/defenition expression is syntax error
  if(var_def_type == ST_NOT_DEFINED) InvokeExit(SYNTAX_ERROR);
  // consume ';'
  GetNextToken();
  return var_def_type;
}


/**
 * @brief Parse return statement
 *
 * @param statement_return Pointer to AST return statement node
 */
void ParseReturn(ASTReturn *statement_return) {
  //Void function return must not have an expression
  if(current_func_sym->details.func.returnType == ST_VOID) {
    GetNextToken();
    if(token.type != T_SEMICOLON) InvokeExit(RETURN_ERROR);
  }
  else {
    statement_return->expr = CreateASTExpression();
    DataType return_type = ParseExpr(statement_return->expr, false, false);
    if(return_type == ST_NOT_DEFINED) InvokeExit(RETURN_ERROR);
    else if(CheckNullableTypes(current_func_sym->details.func.returnType, return_type) == true){}
    else if(return_type != current_func_sym->details.func.returnType) InvokeExit(PARAMETER_RETURN_ERROR);
    current_func_sym->details.func.returnMade = true;
  }
}

/**
 * @brief Parse expression
 * Parser will switch on precedence analysis
 *
 * @param expression Pointer to AST expression node
 * @param additional_parenthesis If there can be additional ')'
 * @param additional_comma If there can be comma in expression(function parameters)
 *
 * @return Type of expression
 */
DataType ParseExpr(ASTExpression *expression, bool additional_parenthesis, bool additional_comma) {
  expression->exprStack = InvokeAlloc(sizeof(stack));
  InitStack(expression->exprStack);
  DataType expr_type = PrecedeneParseExpression(&expression->exprStack, additional_parenthesis, additional_comma);
  expression->returnType = expr_type;
  return expr_type;
}

/**
 * @brief Parsing function call
 *
 * @param func_call Pointer to AST funcction call node
 *
 * @return Return type of function
 */
DataType ParseFuncCall(ASTFuncCall *func_call) {
  //Return data type of function
  DataType return_type = ST_NOT_DEFINED;
  //Type of current parsing parameter
  DataType param_type = ST_NOT_DEFINED;

  //Helps to decide if error is 4 or 2
  int expected_args = 0;

  // get id by updating previous tokens
  scanner->current_token -= 2;
  GetNextToken();

  // IFJ functions
  if (token.keyword == K_IFJ) {
    GetNextToken();
    if (token.type != T_DOT)
      InvokeExit(SYNTAX_ERROR);
    GetNextToken();
    if (token.type != T_ID || token.keyword != K_UNKNOWN)
      InvokeExit(SYNTAX_ERROR);

    //Save current token with string value
    Token ifj_token = token;
    GetNextToken();

    //consume (
    if(token.type != T_OPAREN)InvokeExit(SYNTAX_ERROR);
    GetNextToken();

    // ifj.write(term) void
    if (StringEquals(ifj_token.value.string, "write")) {
      expected_args=1;
      func_call->name = "ifj.write";
      //no parameters
      if(token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);

      //term | term can't be void
      param_type = ParseCallParam(&func_call->params);
      if(CheckParamTypes(ST_VOID, param_type)) InvokeExit(PARAMETER_RETURN_ERROR);
      return_type = ST_VOID;
    }

    // ifj.readstr() ?[]u8
    else if (StringEquals(ifj_token.value.string, "readstr")) {
      expected_args=0;
      func_call->name = "ifj.readstr";
      return_type = ST_NULLU8;
    }

    // ifj.readi32() ?i32
    else if (StringEquals(ifj_token.value.string, "readi32")) {
      expected_args=0;
      func_call->name = "ifj.readi32";
      return_type = ST_NULLI32;
    }

    // ifj.readf64() ?f64
    else if (StringEquals(ifj_token.value.string, "readf64")) {
      expected_args=0;
      func_call->name = "ifj.readf64";
      return_type = ST_NULLF64;
    }

    // ifj.i2f(term ∶ i32) f64
    else if (StringEquals(ifj_token.value.string, "i2f")) {
      expected_args=1;
      func_call->name = "ifj.i2f";
      if(token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      //term
      param_type = ParseCallParam(&func_call->params);
      if(CheckParamTypes(ST_I32, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      return_type = ST_F64;
    }

    // ifj.f2i(term ∶ f64) i32
    else if (StringEquals(ifj_token.value.string, "f2i")) {
      expected_args=1;
      func_call->name = "ifj.f2i";
      if(token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      //term
      param_type = ParseCallParam(&func_call->params);
      if(CheckParamTypes(ST_F64, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      return_type = ST_I32;
    }

    // ifj.string(term) []u8
    else if (StringEquals(ifj_token.value.string, "string")) {
      expected_args=1;
      func_call->name = "ifj.string";
      if(token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      //term must be string literal or u8
      param_type = ParseCallParam(&func_call->params);
      if(CheckParamTypes(ST_STRING, param_type) == false &&
              CheckParamTypes(ST_U8, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      return_type = ST_U8;
    }

    // ifj.length(s : []u8) i32
    else if (StringEquals(ifj_token.value.string, "length")) {
      expected_args=1;
      func_call->name = "ifj.length";
      if(token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      // s
      param_type = ParseCallParam(&func_call->params);
      if(CheckParamTypes(ST_U8, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      return_type = ST_I32;
    }

    // ifj.concat(s1 : []u8, s2 : []u8) []u8
    else if (StringEquals(ifj_token.value.string, "concat")) {
      expected_args=2;
      func_call->name = "ifj.concat";

      if(token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      // s1
      param_type = ParseCallParam(&func_call->params);
      if(CheckParamTypes(ST_U8, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      if (token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      if (token.type != T_COMMA) InvokeExit(SYNTAX_ERROR);
      GetNextToken();

      // s2
      param_type = ParseCallParam(&func_call->params->next);
      if(CheckParamTypes(ST_U8, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      return_type = ST_U8;
    }

    // ifj.substring(s : []u8, i : i32, j : i32) ?[]u8
    else if (StringEquals(ifj_token.value.string, "substring")) {
      expected_args=3;
      func_call->name = "ifj.substring";
      if(token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      // s
      param_type = ParseCallParam(&func_call->params);
      if(CheckParamTypes(ST_U8, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      if (token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      if (token.type != T_COMMA) InvokeExit(SYNTAX_ERROR);
      GetNextToken();
      // i
      param_type = ParseCallParam(&func_call->params->next);
      if(CheckParamTypes(ST_I32, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      if (token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      if (token.type != T_COMMA) InvokeExit(SYNTAX_ERROR);
      GetNextToken();
      // j
      param_type = ParseCallParam(&func_call->params->next->next);
      if (CheckParamTypes(ST_I32, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      return_type = ST_NULLU8;
    }

    // ifj.strcmp(s1 : []u8, s2 : []u8) i32
    else if (StringEquals(ifj_token.value.string, "strcmp")) {
      expected_args=2;
      func_call->name = "ifj.strcmp";

      if(token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      // s1
      param_type = ParseCallParam(&func_call->params);
      if (CheckParamTypes(ST_U8, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      if (token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      if (token.type != T_COMMA) InvokeExit(SYNTAX_ERROR);
      GetNextToken();
      // s2
      param_type = ParseCallParam(&func_call->params->next);
      if (CheckParamTypes(ST_U8, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      return_type = ST_I32;
    }

    // ifj.ord(s : []u8, i : i32) i32
    else if (StringEquals(ifj_token.value.string, "ord")) {
      expected_args=2;
      func_call->name = "ifj.ord";
      if(token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      // s1
      param_type = ParseCallParam(&func_call->params);
      if (CheckParamTypes(ST_U8, param_type) == false)InvokeExit(PARAMETER_RETURN_ERROR);
      if (token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      if (token.type != T_COMMA) InvokeExit(SYNTAX_ERROR);
      GetNextToken();
      // i
      param_type = ParseCallParam(&func_call->params->next);
      if (CheckParamTypes(ST_I32, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      return_type = ST_I32;
    }

    // ifj.chr(i : i32) []u8
    else if (StringEquals(ifj_token.value.string, "chr")) {
      expected_args=1;
      func_call->name = "ifj.chr";
      if(token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
      //i
      param_type = ParseCallParam(&func_call->params);
      if (CheckParamTypes(ST_I32, param_type) == false) InvokeExit(PARAMETER_RETURN_ERROR);
      return_type = ST_U8;
    }

    else InvokeExit(DEFINITION_ERROR);
  }

  // DEFAULT FUNCTION CALL
  else {
    func_call->name = token.value.string->str;

    // Find function in symtable with semantics check
    symtable_item_data *func = SymtableAssertFunction(func_call->name);
    //Count of function parameters
    expected_args=func->details.func.params->size;
    return_type = func->details.func.returnType;
    // consume id
    GetNextToken();

    // Consume (
    GetNextToken();

    ASTParamCall **current = &func_call->params;

    // Cycle through params in symtable function item
    for (size_t i = 0; i < func->details.func.params->size; i++) {
      Param *symtable_param = func->details.func.params->array[i];

      param_type = ParseCallParam(current);
      if (CheckParamTypes(symtable_param->type,param_type) == false)InvokeExit(PARAMETER_RETURN_ERROR);

      // try to consume comma if it isn't last param
      if (i != func->details.func.params->size - 1) {
        if(token.type == T_CPAREN) InvokeExit(PARAMETER_RETURN_ERROR);
        else if (token.type != T_COMMA)InvokeExit(SYNTAX_ERROR);
        GetNextToken();
      }
      current = &(*current)->next;
    }

  }

  ASTParamCall *temp = NULL;
  //If there is parameter in function that must have zero -> 4 error
  if(expected_args == 0 && token.type != T_CPAREN){
    ParseCallParam(&temp);
    if(token.type == T_CPAREN || token.type == T_COMMA)InvokeExit(PARAMETER_RETURN_ERROR);
    //but if function parameter hasn't written rigth it's syntax error
    InvokeExit(SYNTAX_ERROR);
  }
  //If there is one more parameter than needed -> 4 error
  else if(expected_args != 0 && token.type == T_COMMA){
    GetNextToken();
    ParseCallParam(&temp);
    if(token.type == T_CPAREN || token.type == T_COMMA)InvokeExit(PARAMETER_RETURN_ERROR);
    //but if function parameter hasn't written rigth it's syntax error again
    InvokeExit(SYNTAX_ERROR);
  }
  temp = NULL;
  //if function call parameters is not closed with close parenthesis -> 2
  if(token.type != T_CPAREN) InvokeExit(SYNTAX_ERROR);
  return return_type;
}

/**
 * @brief Parsing expression in function call parameter
 *
 * @param func_call_param Pointer to AST function call parameter
 *
 * @return Type of expression in parameter
 */
DataType ParseCallParam(ASTParamCall **func_call_param) {
  *func_call_param = CreateASTParamCall();
  (*func_call_param)->expr = CreateASTExpression();
  scanner->current_token--;
  DataType paramtype = ParseExpr((*func_call_param)->expr,true, true);
  //update current token
  scanner->current_token--;
  GetNextToken();
  return paramtype;
}

/**
 * @brief Parse a type
 *
 * @return DataType type
 */
DataType ParseType() {
  //?...
  bool is_nullable = false;
  if(token.type == T_QUESTM) {
    is_nullable = true;
    GetNextToken();
  }

  // []u8 / ?[]u8
  if (token.type == T_OSQUARE) {
    GetNextToken();
    if (token.type == T_CSQUARE) {
      GetNextToken();
      if (token.type == T_ID && token.keyword == K_U8) {
        return is_nullable? ST_NULLU8 : ST_U8;
      }
      else InvokeExit(SYNTAX_ERROR);
    }
    else InvokeExit(SYNTAX_ERROR);
  }
  // i32 / ?i32
  else if (token.type == T_ID && token.keyword == K_I32) {
    return is_nullable ? ST_NULLI32 : ST_I32;
  }
  // f64 / ?f64
  else if (token.type == T_ID && token.keyword == K_F64) {
    return  is_nullable ? ST_NULLF64 : ST_F64;
  }
  // void
  else if (token.type == T_ID && token.keyword == K_VOID) {
    //?void is syntax error
    if(is_nullable) InvokeExit(SYNTAX_ERROR);
    return ST_VOID;
  }
  return ST_NOT_DEFINED;
}

/**
 * @brief Semantics check in nullable types
 *
 * @param base Known type
 * @param from_expression Type from expression
 *
 * @return True if nullable types is compatible, otherwise false
 */
bool CheckNullableTypes(DataType base, DataType from_expression){
    if(base == ST_NULLI32 && (from_expression == ST_NULL || from_expression == ST_NULLI32 || from_expression == ST_I32)) return true;
    else if(base == ST_NULLU8 && (from_expression == ST_NULL || from_expression == ST_NULLU8 || from_expression == ST_U8)) return true;
    else if(base == ST_NULLF64 && (from_expression == ST_NULL || from_expression == ST_NULLF64 || from_expression == ST_F64)) return true;
    return false;
}

/**
 * @brief  Semantics check in function call parameters
 *
 * @param expected Expected type from function declaration
 * @param actual Actual type of parsed parameter
 *
 * @return True if parameter types is compatible with function declaration, otherwise false
 */
bool CheckParamTypes(DataType expected, DataType actual){
  //empty expression is syntax error
  if (actual == ST_NOT_DEFINED) InvokeExit(SYNTAX_ERROR);
  else if(CheckNullableTypes(expected, actual)) return true;
  else if(expected == actual) return true;
  return false;
}
