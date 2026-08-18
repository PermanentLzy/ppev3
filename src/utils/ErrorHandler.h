#pragma once

#include <string>
#include <iostream>

// ====== 语言开关：1=中文  0=英文 ======
#define LANGUAGE 0

#if LANGUAGE == 1
#define MSG(cn, en) std::string(cn)
#else
#define MSG(cn, en) std::string(en)
#endif

namespace MyCompiler {

/// @brief 统一错误报告器
class ErrorHandler {
public:
    /// 报告词法错误
    static void lexError(int line, int col, const std::string& msg);

    /// 报告语法错误
    static void parseError(int line, int col, const std::string& msg);

    /// 报告语义错误
    static void semanticError(const std::string& msg);

    /// 报告内部错误
    static void internalError(const std::string& msg);

    /// 报告警告
    static void warning(int line, int col, const std::string& msg);

    /// 获取已报告的错误总数
    static int errorCount() { return errors_; }

    /// 重置错误计数
    static void reset() { errors_ = 0; }

private:
    static int errors_;
};

} // namespace MyCompiler
