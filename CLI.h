//==============================================================================
// CarLink CLI Library
// SPDX-License-Identifier: GPL-3.0
// Copyright (c) 2024 Kogbi
// Author: Kogbi <kogbi0209@outlook.com>
//==============================================================================

#pragma once

#include <cerrno>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace carlink {
namespace cli {

namespace Color {
constexpr const char* RESET = "\033[0m";
constexpr const char* BOLD = "\033[1m";
constexpr const char* RED = "\033[31m";
constexpr const char* GREEN = "\033[32m";
constexpr const char* YELLOW = "\033[33m";
constexpr const char* CYAN = "\033[36m";
}   // namespace Color

using CommandHandler = std::function<void(const std::vector<std::string>&)>;

// 树形参数补全函数（访问完整参数列表，支持任意深度的依赖关系）
using TreeCompleter = std::function<std::vector<std::string>(
    const std::vector<std::string>& allTokens,   // 所有已输入的 token（包括命令名）
    int paramIndex,                              // 当前补全的参数位置
    const std::string& currentInput              // 当前输入
    )>;

// 参数验证器类型 - 返回空字符串表示通过，返回错误消息表示失败
using ParamValidator = std::function<std::string(const std::vector<std::string>& args)>;

/**
 * 树形参数节点 - 支持任意深度的参数依赖关系
 * candidates: 当前位置可选的值列表（空列表表示数值型参数）
 * children: 子节点映射，key 为 candidates 中的值，value 为该值对应的下一级参数节点
 */
struct ParamNode {
    struct NumericConstraint {
        bool enabled = false;
        long minValue = 0;
        long maxValue = 0;
    };

    std::vector<std::string> candidates;           // 当前层级的候选值
    std::map<std::string, ParamNode> children;     // 子节点映射
    NumericConstraint numeric;                     // 数值参数约束（仅对 candidates 为空时生效）
};

class TreeBuilder {
public:
    explicit TreeBuilder(ParamNode& root)
        : root_(root) {}

    TreeBuilder& root(const std::vector<std::string>& candidates)
    {
        root_.candidates = candidates;
        return *this;
    }

    TreeBuilder& node(const std::vector<std::string>& path,
                      const std::vector<std::string>& candidates)
    {
        ParamNode* current = &root_;
        for (const auto& key : path) {
            current = &current->children[key];
        }
        current->candidates = candidates;
        return *this;
    }

    TreeBuilder& numeric(const std::vector<std::string>& path, long minValue, long maxValue)
    {
        ParamNode* current = &root_;
        for (const auto& key : path) {
            current = &current->children[key];
        }
        current->candidates.clear();
        current->numeric = ParamNode::NumericConstraint{true, minValue, maxValue};
        return *this;
    }

private:
    ParamNode& root_;
};


/**
 * 创建树形依赖的补全器和验证器
 * 
 * 用法示例:
 * ParamNode root;
 * root.candidates = {"device1", "device2", "timeout"};
 * 
 * // device1 分支
 * root.children["device1"].candidates = {"light", "sound"};
 * root.children["device1"].children["light"].candidates = {"0", "1", "2"};
 * 
 * // timeout 分支（数值参数）
 * root.children["timeout"].candidates = {};  // 空列表，表示数值参数
 * root.children["timeout"].numeric = {true, 1, 600};
 * 
 * auto [completer, validator] = makeTreeParamMap(root);
 * cli.registerCommand("set", "Set configuration", handler, completer, validator);
 */
inline std::pair<TreeCompleter, ParamValidator> makeTreeParamMap(const ParamNode& root)
{
    // 使用 shared_ptr 确保树的生命周期
    auto rootPtr = std::make_shared<ParamNode>(root);
    
    auto completer = [rootPtr](const std::vector<std::string>& allTokens, int paramIndex, const std::string& input) -> std::vector<std::string> {
        if (paramIndex < 1) return {};
        
        const ParamNode* current = rootPtr.get();
        if (!current) return {};
        
        // 遍历树找到当前节点
        for (int i = 1; i < paramIndex; ++i) {
            if (i >= (int)allTokens.size()) return {};
            
            const std::string& token = allTokens[i];
            auto it = current->children.find(token);
            if (it == current->children.end()) {
                return {};  // 路径不存在
            }
            current = &(it->second);
            if (!current) return {};
        }
        
        // 返回当前节点的候选值（前缀匹配）
        std::vector<std::string> matches;
        for (const auto& candidate : current->candidates) {
            if (candidate.find(input) == 0) {
                matches.push_back(candidate);
            }
        }
        return matches;
    };
    
    auto validator = [rootPtr](const std::vector<std::string>& args) -> std::string {
        if (args.size() < 2) {
            return "Missing arguments";
        }
        
        const ParamNode* current = rootPtr.get();
        if (!current) return "Internal error: invalid tree";
        
        bool lastValueIsLeaf = false;
        
        auto parseLong = [](const std::string& value, long& outValue) -> bool {
            if (value.empty()) return false;
            char* end = nullptr;
            errno = 0;
            long parsed = std::strtol(value.c_str(), &end, 10);
            if (errno != 0 || end == value.c_str() || *end != '\0') {
                return false;
            }
            outValue = parsed;
            return true;
        };

        for (size_t i = 1; i < args.size(); ++i) {
            const std::string& value = args[i];
            lastValueIsLeaf = false;
            
            if (!current) {
                return "Internal error: null pointer";
            }
            
            // 如果候选列表为空，处理数值型参数
            if (current->candidates.empty()) {
                if (current->numeric.enabled) {
                    long parsed = 0;
                    if (!parseLong(value, parsed)) {
                        return "Invalid number '" + value + "' at position " + std::to_string(i);
                    }
                    if (parsed < current->numeric.minValue || parsed > current->numeric.maxValue) {
                        return "Number out of range at position " + std::to_string(i) +
                               ". Expected: " + std::to_string(current->numeric.minValue) +
                               " to " + std::to_string(current->numeric.maxValue);
                    }
                }
                if (!current->children.empty()) {
                    auto it = current->children.find(value);
                    if (it != current->children.end()) {
                        current = &(it->second);
                        continue;
                    }
                }
                lastValueIsLeaf = true;
                continue;
            }
            
            // 验证值是否在候选列表中
            bool found = false;
            for (const auto& candidate : current->candidates) {
                if (value == candidate) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                std::string validValues;
                for (const auto& v : current->candidates) {
                    if (!validValues.empty()) validValues += ", ";
                    validValues += v;
                }
                return "Invalid value '" + value + "' at position " + std::to_string(i) + 
                       ". Valid values: " + validValues;
            }
            
            // 尝试移动到子节点
            auto it = current->children.find(value);
            if (it != current->children.end()) {
                current = &(it->second);
                if (!current) {
                    return "Internal error: null pointer after move";
                }
                lastValueIsLeaf = false;
            } else {
                // 该值在candidates中，但没有子节点，是叶子值
                lastValueIsLeaf = true;
                if (i < args.size() - 1) {
                    return "Too many arguments after '" + value + "'";
                }
                break;
            }
        }
        
        // 如果最后一个值是叶子值，验证通过
        if (lastValueIsLeaf) {
            return "";
        }
        
        // 否则检查当前节点是否还需要更多参数
        if (current && (!current->candidates.empty() || !current->children.empty())) {
            if (!current->candidates.empty()) {
                std::string currentOptions;
                for (const auto& opt : current->candidates) {
                    if (!currentOptions.empty()) currentOptions += ", ";
                    currentOptions += opt;
                }
                return "Missing argument. Expected one of: " + currentOptions;
            }
            
            if (!current->children.empty()) {
                std::string nextOptions;
                for (const auto& [key, _] : current->children) {
                    if (!nextOptions.empty()) nextOptions += ", ";
                    nextOptions += key;
                }
                return "Missing argument. Expected: " + nextOptions;
            }
        }
        
        return "";  // 验证通过
    };
    
    return {completer, validator};
}

class CLI
{
public:
    CLI();
    ~CLI() = default;

    /**
     * 运行 CLI
     * - 无参数：进入交互模式
     * - 有参数：执行单条命令后退出
     */
    int run(int argc, char** argv);

    /**
     * 注册自定义命令（树形依赖参数）
     * @param name 命令名称
     * @param description 命令描述
     * @param handler 命令处理函数
     * @param completer 树形参数补全函数（可选）
     * @param validator 参数验证器（可选）
     */
    void registerCommand(const std::string& name,
                         const std::string& description,
                         CommandHandler handler,
                         TreeCompleter completer = nullptr,
                         ParamValidator validator = nullptr);

private:
    // 内置命令
    void cmdHelp(const std::vector<std::string>& args);
    void cmdExit(const std::vector<std::string>& args);
    void cmdClear(const std::vector<std::string>& args);

    // 交互式 Shell
    void runInteractiveShell();

    // 单命令模式
    void runSingleCommand(const std::vector<std::string>& args);

    // 工具函数
    void printWelcome();
    void printPrompt();
    std::vector<std::string> parseCommandLine(const std::string& line);
    bool executeCommand(const std::vector<std::string>& tokens);

    // Tab 自动补全
    static CLI* instance_;
    std::vector<std::string> getCommandList() const;
    static char** commandCompletion(const char* text, int start, int end);
    static char* commandGenerator(const char* text, int state);

    // 命令注册表
    struct CommandInfo
    {
        std::string description;
        CommandHandler handler;
        TreeCompleter completer;   // 树形补全函数
        ParamValidator validator;  // 验证器

        bool hasCompleter() const { return completer != nullptr; }
    };
    std::map<std::string, CommandInfo> commands_;

    bool running_;
};

}   // namespace cli
}   // namespace carlink
