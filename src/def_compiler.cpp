#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

#include <ast.hpp>
#include <parser.hpp>

extern std::shared_ptr<AST::Program> program;
extern int yyparse();
extern FILE * yyin;

int main(const int argc, const char ** argv) {
    if (argc > 1) {
        std::ifstream in(argv[1]);
        if (!in.is_open())
            throw std::runtime_error("Unable to open file " + std::string(argv[1]));
        yyin = fopen(argv[1], "r");
    }
    if (argc > 2) {
        std::ofstream out(argv[2]);
        if (out.is_open()) {
            yyparse();
            program->codegen(out);
        }
        else
            throw std::runtime_error("Unable to open file " + std::string(argv[2]));
    }
    else {
        yyparse();
        program->codegen(std::cout);
    }
}
