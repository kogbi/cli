//==============================================================================
// CarLink Service Control CLI
// Date: 2026-02-11
// Copyright (c) 2024 Kogbi
// Author: Kogbi
//==============================================================================

#include "CLI.h"
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <poll.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace carlink {
namespace cli {

CLI* CLI::instance_ = nullptr;

static void signalHandler(int signum)
{
    (void)signum;

    if (CLI::isCommandRunning()) {
        return;
    }

    std::cout << "\n"
              << Color::YELLOW
              << "Use 'exit' or Ctrl+D to quit"
              << Color::RESET << std::endl;

    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

CLI::CLI()
    : running_(true)
{
    instance_ = this;

    signal(SIGINT, signalHandler);    // Ctrl+C
    signal(SIGQUIT, signalHandler);   // Ctrl+backslash
    signal(SIGTSTP, signalHandler);   // Ctrl+Z

    registerCommand("help", "Show available commands",
                    [this](const std::vector<std::string>& args) { cmdHelp(args); });

    registerCommand("exit", "Exit the CLI",
                    [this](const std::vector<std::string>& args) { cmdExit(args); });

    registerCommand("quit", "Exit the CLI (alias for exit)",
                    [this](const std::vector<std::string>& args) { cmdExit(args); });

    registerCommand("clear", "Clear the screen",
                    [this](const std::vector<std::string>& args) { cmdClear(args); });
}

bool CLI::isCommandRunning()
{
    if (!instance_) {
        return false;
    }
    return instance_->commandRunning_.load();
}

void CLI::registerCommand(const std::string& name,
                         const std::string& description,
                         CommandHandler handler,
                         TreeCompleter completer,
                         ParamValidator validator)
{
    CommandInfo info;
    info.description = description;
    info.handler = handler;
    info.completer = completer;
    info.validator = validator;
    commands_[name] = info;
}

void CLI::printWelcome()
{
    std::cout << Color::CYAN << Color::BOLD;
    std::cout << R"(
    ╔════════════════════════════════════════╗
    ║   CarLink Service Control CLI v1.0     ║
    ║   Type 'help' for available commands   ║
    ╚════════════════════════════════════════╝
    )" << Color::RESET
              << std::endl;
}

void CLI::printPrompt()
{
    std::cout << Color::GREEN << "carlink> " << Color::RESET;
    std::cout.flush();
}

std::vector<std::string> CLI::parseCommandLine(const std::string& line)
{
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

bool CLI::executeCommand(const std::vector<std::string>& tokens)
{
    if (tokens.empty()) {
        return true;
    }

    const std::string& cmd = tokens[0];

    auto it = commands_.find(cmd);
    if (it != commands_.end()) {
        return invokeCommand(it->second, tokens, true);
    }

    std::cout << Color::RED << "Unknown command: " << cmd
              << ". Type 'help' for available commands."
              << Color::RESET << std::endl;
    return false;
}

bool CLI::invokeCommand(const CommandInfo& info,
                        const std::vector<std::string>& tokens,
                        bool monitorCtrlD)
{
    try {
        if (info.validator) {
            std::string error = info.validator(tokens);
            if (!error.empty()) {
                std::cout << Color::RED << error << Color::RESET << std::endl;
                return true;
            }
        }

        if (!monitorCtrlD) {
            info.handler(tokens);
            return true;
        }

        std::exception_ptr workerException;
        std::atomic<bool> finished{false};
        commandRunning_.store(true);

        std::thread worker([&]() {
            try {
                info.handler(tokens);
            }
            catch (...) {
                workerException = std::current_exception();
            }
            finished.store(true);
        });

        while (!finished.load()) {
            struct pollfd pfd;
            pfd.fd = STDIN_FILENO;
            pfd.events = POLLIN | POLLHUP | POLLERR;
            pfd.revents = 0;

            int ret = poll(&pfd, 1, 100);
            if (ret <= 0) {
                continue;
            }

            if (pfd.revents & (POLLIN | POLLHUP)) {
                char buffer[64] = {0};
                ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
                if (n == 0) {
                    std::cout << Color::CYAN << "\nGoodbye!" << Color::RESET << std::endl;
                    std::cout.flush();
                    std::_Exit(0);
                }

                for (ssize_t i = 0; i < n; ++i) {
                    if (buffer[i] == 0x04) {
                        std::cout << Color::CYAN << "\nGoodbye!" << Color::RESET << std::endl;
                        std::cout.flush();
                        std::_Exit(0);
                    }
                }
            }
        }

        worker.join();
        commandRunning_.store(false);

        if (workerException) {
            std::rethrow_exception(workerException);
        }
    }
    catch (const std::exception& e) {
        commandRunning_.store(false);
        std::cout << Color::RED << "Error: " << e.what()
                  << Color::RESET << std::endl;
    }

    return true;
}

void CLI::cmdHelp(const std::vector<std::string>& args)
{
    (void)args;

    std::cout << Color::CYAN << Color::BOLD
              << "\nAvailable Commands:"
              << Color::RESET << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    std::vector<std::pair<std::string, CommandInfo>> sorted_commands(
        commands_.begin(), commands_.end());
    std::sort(sorted_commands.begin(), sorted_commands.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (const auto& [name, info] : sorted_commands) {
        std::cout << Color::YELLOW << "  " << name << Color::RESET;

        int padding = 15 - name.length();
        if (padding > 0) {
            std::cout << std::string(padding, ' ');
        }
        else {
            std::cout << " ";
        }

        std::cout << info.description << std::endl;
    }

    std::cout << std::endl;
}

void CLI::cmdExit(const std::vector<std::string>& args)
{
    (void)args;

    std::cout << Color::CYAN << "Goodbye!" << Color::RESET << std::endl;
    running_ = false;
}

void CLI::cmdClear(const std::vector<std::string>& args)
{
    (void)args;

    // 清屏：ANSI 转义序列
    std::cout << "\033[2J\033[H";
    std::cout.flush();
}

int CLI::run(int argc, char** argv)
{
    if (argc > 1) {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        runSingleCommand(args);
        return 0;
    }

    printWelcome();
    runInteractiveShell();
    return 0;
}

void CLI::runInteractiveShell()
{
    // 设置 readline 自动补全
    rl_attempted_completion_function = commandCompletion;

    while (running_) {
        char* input = readline("carlink> ");

        if (!input) {
            // Ctrl+D
            std::cout << Color::CYAN << "\nGoodbye!" << Color::RESET << std::endl;
            break;
        }

        std::string line(input);
        free(input);

        // 跳过空行
        if (line.empty() || line.find_first_not_of(' ') == std::string::npos) {
            continue;
        }

        // 添加到历史记录
        add_history(line.c_str());

        // 解析并执行命令
        auto tokens = parseCommandLine(line);
        executeCommand(tokens);
    }
}

void CLI::runSingleCommand(const std::vector<std::string>& args)
{
    if (args.empty()) {
        return;
    }

    const std::string& cmd = args[0];
    auto it = commands_.find(cmd);
    if (it != commands_.end()) {
        invokeCommand(it->second, args, false);
        return;
    }

    std::cout << Color::RED << "Unknown command: " << cmd
              << ". Type 'help' for available commands."
              << Color::RESET << std::endl;
}

std::vector<std::string> CLI::getCommandList() const
{
    std::vector<std::string> commands;
    for (const auto& [name, _] : commands_) {
        commands.push_back(name);
    }
    return commands;
}

char** CLI::commandCompletion(const char* text, int start, int end)
{
    (void)end;

    if (!instance_) {
        return nullptr;
    }

    // 获取整个输入行
    std::string line(rl_line_buffer);
    
    // 如果处于行首，补全命令名
    if (start == 0) {
        return rl_completion_matches(text, commandGenerator);
    }
    
    // 否则尝试补全参数
    size_t max_pos = std::min((size_t)(start + strlen(text)), line.length());
    auto tokens = instance_->parseCommandLine(line.substr(0, max_pos));
    
    if (tokens.empty()) {
        return nullptr;
    }
    
    std::string cmd = tokens[0];
    auto it = instance_->commands_.find(cmd);
    
    // 如果命令不存在或没有参数补全函数，返回 nullptr
    if (it == instance_->commands_.end() || !it->second.hasCompleter()) {
        return nullptr;
    }
    
    // 当前参数的索引
    int paramIndex;
    if (strlen(text) == 0) {
        paramIndex = tokens.size();
    } else {
        paramIndex = tokens.size() - 1;
    }
    
    std::string currentInput(text);
    
    // 调用树形补全函数
    std::vector<std::string> candidates = it->second.completer(tokens, paramIndex, currentInput);
    
    // 如果没有补全候选，返回 nullptr，允许 readline 做文件补全
    if (candidates.empty()) {
        return nullptr;
    }
    
    // 将补全候选转换为 readline 格式
    std::string prefix = "";
    if (candidates.size() == 1) {
        prefix = candidates[0];
    } else if (candidates.size() > 1) {
        prefix = candidates[0];
        for (size_t i = 1; i < candidates.size(); ++i) {
            size_t j = 0;
            while (j < prefix.length() && j < candidates[i].length() && 
                   prefix[j] == candidates[i][j]) {
                j++;
            }
            prefix = prefix.substr(0, j);
        }
    }
    
    char** completions = (char**)malloc((candidates.size() + 2) * sizeof(char*));
    completions[0] = strdup(prefix.c_str());
    for (size_t i = 0; i < candidates.size(); ++i) {
        completions[i + 1] = strdup(candidates[i].c_str());
    }
    completions[candidates.size() + 1] = nullptr;
    
    return completions;
}

char* CLI::commandGenerator(const char* text, int state)
{
    static std::vector<std::string> matches;
    static size_t matchIndex;

    if (state == 0) {
        matches.clear();
        matchIndex = 0;

        std::string prefix(text);
        auto commands = instance_->getCommandList();

        for (const auto& cmd : commands) {
            if (cmd.find(prefix) == 0) {
                matches.push_back(cmd);
            }
        }
    }

    if (matchIndex < matches.size()) {
        return strdup(matches[matchIndex++].c_str());
    }

    return nullptr;
}

}   // namespace cli
}   // namespace carlink
