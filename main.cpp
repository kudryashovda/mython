#include "lexer.h"
#include "parse.h"
#include "runtime.h"
#include "statement.h"
#include "test_runner_p.h"

#include <fstream>
#include <iostream>

using namespace std;

namespace parse {
    void RunOpenLexerTests(TestRunner& tr);
} // namespace parse

namespace ast {
    void RunUnitTests(TestRunner& tr);
}
namespace runtime {
    void RunObjectHolderTests(TestRunner& tr);
    void RunObjectsTests(TestRunner& tr);
    void RunUserTests(TestRunner& tr);
} // namespace runtime

void TestParseProgram(TestRunner& tr);

void TestAll() {
    TestRunner tr;
    parse::RunOpenLexerTests(tr);
    runtime::RunObjectHolderTests(tr);
    runtime::RunObjectsTests(tr);
    runtime::RunUserTests(tr);
    ast::RunUnitTests(tr);
    TestParseProgram(tr);
}

void LoadRunMythonProgram(std::istream& input, std::ostream& output) {
    string filename = "../test.py"s;

    ifstream in(filename);

    if (!in.is_open()) {
        return;
    }

    parse::Lexer lexer(in);
    auto program = ParseProgram(lexer);

    runtime::SimpleContext context{ output };
    runtime::Closure closure;
    program->Execute(closure, context);
}

int main() {
    try {
        // TestAll();

        LoadRunMythonProgram(cin, cout);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}