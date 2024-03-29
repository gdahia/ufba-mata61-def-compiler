#include <ast.hpp>
#include <parser.hpp>

using namespace AST;

void Program::codegen(std::ostream & os) {
    codegen(os, table);
}

template<typename Base, typename T>
bool instanceof(const T * ptr) {
    return dynamic_cast<const Base *>(ptr) != nullptr;
}

void Program::codegen(std::ostream & os, SymbolTable & table) {
    // alocate all globals
    os << ".data" << std::endl;
    for (auto i = instr->rbegin(); i != instr->rend(); i++)
        if (DecVar * decvar = dynamic_cast<DecVar *>(i->get()))
            os << "__" << decvar->get_name() << ": .word 0" << std::endl;
    
    // start code
    os << std::endl << ".text" << std::endl;
    
    // implement print function
    os << "_f_print:" << std::endl;
    os << "lw $a0, 4($sp)" << std::endl;
    os << "li $v0, 1" << std::endl;
    os << "syscall" << std::endl;
    os << "li $v0, 11" << std::endl;
    os << "li $a0, 0x0a" << std::endl;
    os << "syscall" << std::endl;
    os << "addiu $sp, $sp, 4" << std::endl;
    os << "lw $fp, 4($sp)" << std::endl;
    os << "addiu $sp, $sp, 4" << std::endl;
    os << "j $ra" << std::endl << std::endl;
    
    // generate code for each function
    for (auto i = instr->rbegin(); i != instr->rend(); i++)
        if (instanceof<DecFunc>(i->get()))
            (*i)->codegen(os, table);
    
    // call program main function
    os << "main:" << std::endl;
    
    // compute starting value of each global function
    for (auto i = instr->rbegin(); i != instr->rend(); i++)
        if (instanceof<DecVar>(i->get()))
            (*i)->codegen(os, table);
    
    // push frame pointer
    os << "sw $fp, 0($sp)" << std::endl;
    os << "addiu $sp, $sp, -4" << std::endl;
    
    // reserve space for locals
    const int n_vars = table.n_local_vars("main");
    if (n_vars)
        os << "addiu $sp, $sp, " << -4 * n_vars << std::endl;
    
    // call main
    os << "jal _f_main" << std::endl;
    
    // end program
    os << "li $v0, 10" << std::endl;
    os << "syscall" << std::endl;
}

void Param::codegen(std::ostream & os, SymbolTable & table) {
    table.add_name(*name);
}

void Block::codegen(std::ostream & os, SymbolTable & table) {
    table.add_namespace();
    for (auto var = vars->rbegin(); var != vars->rend(); var++)
        (*var)->codegen(os, table);
    
    for (auto stmt = stmts->rbegin(); stmt != stmts->rend(); stmt++)
        (*stmt)->codegen(os, table);
    table.pop_namespace();
}

void Assign::codegen(std::ostream & os, SymbolTable & table) {
    rhs->codegen(os, table);
    os << "sw $a0, " << table.var_name(*lhs) << std::endl;
}

void If::codegen(std::ostream & os, SymbolTable & table) {
    // adjust symbol table
    table.add_if();
    const unsigned int label = table.get_current_if();
    
    // get expr result
    expr->codegen(os, table);
    
    // if true, jump to begin if
    os << "bne $a0, $0, _if_" << label << std::endl;
    
    // else block
    if (else_block)
        else_block->codegen(os, table);
    
    // jump to endif
    os << "b _fi_" << label << std::endl;
    
    // if block
    os << "_if_" << label << ":" << std::endl;
    if_block->codegen(os, table);
    
    // end if
    os << "_fi_" << label << ":" << std::endl;
}

void Return::codegen(std::ostream & os, SymbolTable & table) {
    // put return value in $a0, if any
    if (expr)
        expr->codegen(os, table);
    
    // exit function
    os << "b _end_f_" << table.get_func_name() << std::endl;
}

void Continue::codegen(std::ostream & os, SymbolTable & table) {
    os << "b _while_" << table.get_current_while() << std::endl;
}

void Break::codegen(std::ostream & os, SymbolTable & table) {
    os << "b _end_while_" << table.get_current_while() << std::endl;
}

void While::codegen(std::ostream & os, SymbolTable & table) {
    // adjust symbol table
    table.add_while();
    const unsigned int label = table.get_current_while();
    
    // begin while
    os << "_while_" << label << ":" << std::endl;
    
    // get expr result
    expr->codegen(os, table);
    
    // if false, jump to end while
    os << "beq $a0, $0, _end_while_" << label << std::endl;
    
    // while block
    block->codegen(os, table);
    
    // loop
    os << "b _while_" << label << std::endl;
    
    // end while
    os << "_end_while_" << label << ":" << std::endl;
    
    table.pop_while();
}

void DecVar::codegen(std::ostream & os, SymbolTable & table) {
    table.add_name(*name);
    if (rhs) {
        rhs->codegen(os, table);
        os << "sw $a0, " << table.var_name(*name) << std::endl;
    }
    else
        os << "sw $0, " << table.var_name(*name) << std::endl;
}

void DecFunc::codegen(std::ostream & os, SymbolTable & table) {
    // set function name
    table.add_func(INT, *name, (paramlist ? paramlist->size() : 0));
    
    // add params space to symbol table
    if (paramlist) {
        table.add_namespace();
        for (auto param = paramlist->rbegin(); param != paramlist->rend(); param++)
            param->codegen(os, table);
    }
    
    // function label
    os << "_f_" << *name << ":" << std::endl;
    
    // turn stack pointer into new frame pointer
    os << "move $fp, $sp" << std::endl;
    
    // push return address
    os << "sw $ra, 0($sp)" << std::endl;
    os << "addiu $sp, $sp, -4" << std::endl;
    
    // generate code for function
    block->codegen(os, table);
    
    // label for exiting function
    os << "_end_f_" << *name << ":" << std::endl;
    
    // pop return address
    os << "lw $ra, 4($sp)" << std::endl;
    os << "addiu $sp, $sp, 4" << std::endl;
    
    // pop parameters, if any
    if (paramlist) {
        table.pop_namespace();
        os << "addiu $sp, $sp, " << 4*paramlist->size() << std::endl;
    }
        
    // pop locals
    os << "addiu $sp, $sp, " << 4 * table.n_local_vars(*name) << std::endl;
    
    // pop frame pointer
    os << "lw $fp, 4($sp)" << std::endl;
    os << "addiu $sp, $sp, 4" << std::endl;
    
    // returns to caller
    os << "j $ra" << std::endl << std::endl;
}

void BinOp::codegen(std::ostream & os, SymbolTable & table) {
    // compute lhs
    left->codegen(os, table);
    
    // push lhs
    os << "sw $a0, 0($sp)" << std::endl;
    os << "addiu $sp, $sp, -4" << std::endl;
    
    // compute rhs
    right->codegen(os, table);
    
    // pop lhs in $t1
    os << "lw $t1, 4($sp)" << std::endl;
    os << "addiu $sp, $sp, 4" << std::endl;
}

void Add::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "add $a0, $t1, $a0" << std::endl;
}

void Sub::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "sub $a0, $t1, $a0" << std::endl;
}

void Or::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "sltu $a0, $0, $a0" << std::endl;
    os << "sltu $t1, $0, $t1" << std::endl;
    os << "or $a0, $t1, $a0" << std::endl;
}

void And::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "sltu $a0, $0, $a0" << std::endl;
    os << "sltu $t1, $0, $t1" << std::endl;
    os << "and $a0, $t1, $a0" << std::endl;
}

void Times::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "mul $a0, $t1, $a0" << std::endl;
}

void Div::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "div $t1, $a0" << std::endl;
    os << "mflo $a0" << std::endl;
}

void Not::codegen(std::ostream & os, SymbolTable & table) {
    expr->codegen(os, table);
    os << "sltu $a0, $0, $a0" << std::endl;
    os << "xori $a0, $a0, 1" << std::endl;
}

void Opp::codegen(std::ostream & os, SymbolTable & table) {
    expr->codegen(os, table);
    os << "li $t1, -1" << std::endl;
    os << "mul $a0, $t1, $a0" << std::endl;
}

void Less::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "slt $a0, $t1, $a0" << std::endl;
}

void Leq::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "slt $a0, $a0, $t1" << std::endl;
    os << "xori $a0, $a0, 1" << std::endl;
}

void Great::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "slt $a0, $a0, $t1" << std::endl;
}

void Geq::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "slt $a0, $t1, $a0" << std::endl;
    os << "xori $a0, $a0, 1" << std::endl;
}

void Eq::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "sub $a0, $t1, $a0" << std::endl;
    os << "sltu $a0, $0, $a0" << std::endl;
    os << "xori $a0, $a0, 1" << std::endl;
}

void Diff::codegen(std::ostream & os, SymbolTable & table) {
    BinOp::codegen(os, table);
    os << "sub $a0, $t1, $a0" << std::endl;
    os << "sltu $a0, $0, $a0" << std::endl;
}

void Number::codegen(std::ostream & os, SymbolTable & table) {
    os << "li $a0, " << val << std::endl;
}

void Var::codegen(std::ostream & os, SymbolTable & table) {
    os << "lw $a0, " << table.var_name(*name) << std::endl;
}

void FuncCall::codegen(std::ostream & os, SymbolTable & table) {
    // push frame pointer
    os << "sw $fp, 0($sp)" << std::endl;
    os << "addiu $sp, $sp, -4" << std::endl;
    
    // reserve space for locals
    const int n_vars = table.n_local_vars(*name);
    if (n_vars)
        os << "addiu $sp, $sp, " << -4 * n_vars << std::endl;
    
    // push arguments
    if (args) {
        for (auto arg : *args) {
            arg->codegen(os, table);
            os << "sw $a0, 0($sp)" << std::endl;
            os << "addiu $sp, $sp, -4" << std::endl;
        }
    }
    
    // goto function
    os << "jal _f_" << *name << std::endl;
}
