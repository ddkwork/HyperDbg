#define WIN32_LEAN_AND_MEAN
//#define WINSOCK_DEPRECATED_NO_WARNINGS
#include "../../../pch.h"

IMPORT_EXPORT_LIBHYPERDBG bool startHttpServer();

IMPORT_EXPORT_LIBHYPERDBG void stopHttpServer();


#define package struct
//mock go std lib
#include <string>
#include <cstring>
#include <functional>
#include <vector>
#include <algorithm>

// 模拟 Go 的 strings 包
struct strings {
    // 检查前缀 - 完全匹配 Go 的签名
    static bool HasPrefix(const std::string& s, const std::string& prefix) {
        return s.size() >= prefix.size() &&
               s.compare(0, prefix.size(), prefix) == 0;
    }

    // 检查后缀
    static bool HasSuffix(const std::string& s, const std::string& suffix) {
        return s.size() >= suffix.size() &&
               s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    // 查找子串位置
    static int Index(const std::string& s, const std::string& substr) {
        size_t pos = s.find(substr);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    // 查找最后出现的位置
    static int LastIndex(const std::string& s, const std::string& substr) {
        size_t pos = s.rfind(substr);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    // 分割字符串
    static std::vector<std::string> Split(const std::string& s, const std::string& sep) {
        std::vector<std::string> result;
        size_t start = 0, end = 0;

        while ((end = s.find(sep, start)) != std::string::npos) {
            result.push_back(s.substr(start, end - start));
            start = end + sep.size();
        }

        if (start < s.size()) {
            result.push_back(s.substr(start));
        }

        return result;
    }

    // 替换字符串
    static std::string Replace(const std::string& s, const std::string& old,
                               const std::string& newStr, int n = -1) {
        std::string result = s;
        size_t pos = 0;
        int count = 0;

        while ((n < 0 || count < n) &&
               ((pos = result.find(old, pos)) != std::string::npos)) {
            result.replace(pos, old.size(), newStr);
            pos += newStr.size();
            count++;
        }

        return result;
    }

    // 修剪空白字符
    static std::string TrimSpace(const std::string& s) {
        auto start = s.begin();
        auto end = s.end();

        // 跳过开头空白
        while (start != end && std::isspace(static_cast<unsigned char>(*start))) {
            start++;
        }

        // 跳过结尾空白
        do {
            end--;
        } while (std::distance(start, end) > 0 &&
                 std::isspace(static_cast<unsigned char>(*end)));

        return std::string(start, end + 1);
    }

    // 包含判断
    static bool Contains(const std::string& s, const std::string& substr) {
        return s.find(substr) != std::string::npos;
    }

    // 计数
    static int Count(const std::string& s, const std::string& substr) {
        if (substr.empty()) return static_cast<int>(s.size()) + 1;

        int count = 0;
        size_t pos = 0;

        while ((pos = s.find(substr, pos)) != std::string::npos) {
            count++;
            pos += substr.size();
        }

        return count;
    }
};

// 命令执行结果类型
struct CommandResult {
    std::string output;
    int exitCode;
};

// 模拟命令执行
CommandResult RunCommand(const std::string& command) {
    // 实际实现中这里会执行真实命令
    if (command == "xxoo") {
        return {"err: something went wrong", 1};
    }
    return {"success: command executed", 0};
}

// 使用示例
int Test_std_lib() {
    auto result = RunCommand("xxoo");

    // 完全符合 Go 风格的调用方式
    if (strings::HasPrefix(result.output, "err")) {
        // 错误处理逻辑
        printf("Error occurred: %s\n", result.output.c_str());

        // 进一步错误分析
        auto parts = strings::Split(result.output, ":");
        if (parts.size() > 1) {
            printf("Error type: %s\n", parts[0].c_str());
            printf("Error details: %s\n",
                   strings::TrimSpace(parts[1]).c_str());
        }
    } else {
        // 成功处理逻辑
        printf("Command succeeded: %s\n", result.output.c_str());
    }

    // 其他常见用法
    std::string url = "https://example.com";
    if (strings::HasPrefix(url, "https")) {
        printf("Secure URL detected\n");
    }

    std::string path = "/user/profile";
    if (strings::HasSuffix(path, "/profile")) {
        printf("Profile path detected\n");
    }

    return 0;
}
package strconv {
};
package hex {
};
package json {
};
package log {
};

package fmt {
};
package net {
};
package http {
};
package os {
};
package io {
};
package time {
};

package encoding {
};
package errors {
};
package sync {
};
package regexp {
};
package runtime {
};
package flag {
};
package crypto {
};
package tls {
};
package x509 {
};