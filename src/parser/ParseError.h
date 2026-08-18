#pragma once

#include <string>
#include <stdexcept>

namespace MyCompiler {

/// @brief 语法错误类型
class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& msg, int line, int column)
        : std::runtime_error(
            "Parse error at " + std::to_string(line) + ":" +
            std::to_string(column) + " - " + msg),
          line_(line), column_(column) {}

    int line()   const { return line_; }
    int column() const { return column_; }

private:
    int line_;
    int column_;
};

} // namespace MyCompiler
