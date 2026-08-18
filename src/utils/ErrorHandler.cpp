#include "ErrorHandler.h"
#include <iostream>

namespace MyCompiler {

int ErrorHandler::errors_ = 0;

void ErrorHandler::lexError(int line, int col, const std::string& msg) {
    std::cerr << "[Lex Error] " << line << ":" << col << " - " << msg << '\n';
    errors_++;
}

void ErrorHandler::parseError(int line, int col, const std::string& msg) {
    std::cerr << "[Parse Error] " << line << ":" << col << " - " << msg << '\n';
    errors_++;
}

void ErrorHandler::semanticError(const std::string& msg) {
    std::cerr << "[Semantic Error] " << msg << '\n';
    errors_++;
}

void ErrorHandler::internalError(const std::string& msg) {
    std::cerr << "[Internal Error] " << msg << '\n';
    errors_++;
}

void ErrorHandler::warning(int line, int col, const std::string& msg) {
    std::cerr << "[Warning] " << line << ":" << col << " - " << msg << '\n';
}

} // namespace MyCompiler
