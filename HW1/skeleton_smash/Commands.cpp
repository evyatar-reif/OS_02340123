#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <limits.h>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";
const std::string INVALID_PATH = "-1";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h 

Command::Command(const char *cmd_line) {}

BuiltInCommand::BuiltInCommand(const char *cmd_line)
    : Command(cmd_line) {}

SmallShell::SmallShell() : m_prompt("smash"), m_lastPwd(INVALID_PATH) {}

SmallShell::~SmallShell() {
    // TODO: add your implementation
}

void SmallShell::setPrompt(const std::string &newPrompt) {
    m_prompt = newPrompt;
}
std::string SmallShell::getPrompt() const {
    return m_prompt;
}

void SmallShell::setLastPwd(const std::string &newLastPwd) {
    m_lastPwd = newLastPwd;
}
std::string SmallShell::getLastPwd() const {
    return m_lastPwd;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(WHITESPACE));

    if (firstWord.compare("chprompt") == 0) {
        return new ChPromptCommand(cmd_line);
    } 
    else if (firstWord.compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }
    else if (firstWord.compare("pwd") == 0) {
      return new PwdCommand(cmd_line);
    }
    else if (firstWord.compare("cd") == 0) {
      return new ChangeDirCommand(cmd_line, SmallShell::getLastPwd());
    }

    // if (firstWord.compare("pwd") == 0) {
    //   return new GetCurrDirCommand(cmd_line);
    // }
    // else if (firstWord.compare("showpid") == 0) {
    //   return new ShowPidCommand(cmd_line);
    // }
    // else if ...
    // .....
    // else {
    //   return new ExternalCommand(cmd_line);
    // }
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    Command* cmd = CreateCommand(cmd_line);
    if (cmd == nullptr) {
        return;
    }
    cmd->execute();
    delete cmd;
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}

// ------- ChPrompt --------------
ChPromptCommand::ChPromptCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line), m_newPrompt("smash") {
    std::istringstream input(cmd_line);
    std::string commandName;
    input >> commandName;
    input >> m_newPrompt;
}

void ChPromptCommand::execute() {
    SmallShell::getInstance().setPrompt(m_newPrompt);
}

// ------- ShowPidCommand --------------

ShowPidCommand::ShowPidCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
    std::cout << "smash pid is "<< getpid() << std::endl;
}

// ------- PwdCommand --------------

PwdCommand::PwdCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

std::string SmallShell::getCwd() const {
    char buffer[PATH_MAX];
    if(getcwd(buffer, PATH_MAX) != nullptr) {
        std::string path(buffer);
        return path;
    }
    return INVALID_PATH;
}

void PwdCommand::execute() {
    std::string cwd = SmallShell::getInstance().getCwd();
    if (cwd != INVALID_PATH) {
        std::cout << cwd << std::endl;
        return;
    }
    std::cerr << "getcwd() error" << std::endl;
}

// ------- ChangeDirCommand --------------

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, const std::string &lastPwd): BuiltInCommand(cmd_line), m_type(ChangeDirCommand::ChangeDirType::NOOP), m_path(INVALID_PATH) {
    std::istringstream input(cmd_line);
    std::string commandName;
    std::string firstArg, secondArg;
    
    input >> commandName;
    if (input >> firstArg) {
        m_type = ChangeDirCommand::ChangeDirType::PATH;
    }
    if (input >> secondArg) {
        std::cerr << "smash error: cd: too many arguments" << std::endl;
        m_type = ChangeDirCommand::ChangeDirType::NOOP;
        return;
    }
    
    if (firstArg == "..") {
        m_type = ChangeDirCommand::ChangeDirType::DOTDOT;
        m_path = "..";
    } else if (firstArg == "-") {
        m_type = ChangeDirCommand::ChangeDirType::DASH;
        m_path = lastPwd;
    }

    if (m_type == ChangeDirCommand::ChangeDirType::DASH && lastPwd == INVALID_PATH) {
        std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
        m_type = ChangeDirCommand::ChangeDirType::NOOP;
        return;
    }

    if (m_type == ChangeDirCommand::ChangeDirType::PATH) {
        m_path = firstArg;
    }
}

void ChangeDirCommand::execute() {
    if (m_type == ChangeDirCommand::ChangeDirType::NOOP) {
        return;
    }

    std::string cwd = SmallShell::getInstance().getCwd();
    if (cwd == INVALID_PATH) {
        std::perror("smash error: getcwd failed");
        return;
    }
    if (chdir(m_path.c_str()) != 0) {
        std::perror("smash error: chdir failed");
        return;
    }

    SmallShell::getInstance().setLastPwd(cwd);
}