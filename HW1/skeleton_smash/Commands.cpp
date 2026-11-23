#include <regex>
#include <tuple>
#include <unordered_set>
#include <unistd.h>
#include <cstdlib>
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <limits.h>
#include <signal.h>
#include <ostream>

using namespace std;

const MessageReporter Msg;

void MessageReporter::serr(const char *operation) const
{
    const int errorNumber = errno;
    const std::string message = std::string("smash error: ") + operation + " failed";
    errno = errorNumber;
    std::perror(message.c_str());
}

void MessageReporter::rerr(const char *message) const
{
    std::cerr << "smash error: " << message << std::endl;
}

void MessageReporter::smsg(const char *message) const
{
    std::cout << "smash: " << message << std::endl;
}

void MessageReporter::msg(const char *message) const
{
    std::cout << message << std::endl;
}

const std::string WHITESPACE = " \n\r\t\f\v";
const std::string INVALID_PATH = "-1";
static const unordered_set<std::string> RESERVED_KEYWORDS = {
    "chprompt",
    "showpid",
    "pwd",
    "cd",
    "jobs",
    "fg",
    "quit",
    "kill",
    "alias",
    "unalias",
    "unsetenv",
    "getfiletype",
    "du",
    "whoami",
};

const char STAR = '*';
const char QMARK = '?';

static bool isOnlyDigits(const std::string &value)
{
    return !value.empty() && value.find_first_not_of("0123456789") == std::string::npos;
}

#if 0
#define FUNC_ENTRY() \
    cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT() \
    cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s)
{
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args)
{
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;)
    {
        args[i] = (char *)malloc(s.length() + 1);
        std::memset(args[i], 0, s.length() + 1);
        std::strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundCommand(const char *cmd_line)
{
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line)
{
    const string str(cmd_line);
    // find last character other than spaces
    const auto idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos)
    {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&')
    {
        return;
    }
    cmd_line[idx] = '\0';
}

// TODO: Add your implementation for classes in Commands.h

Command::Command(const char *cmd_line) : m_cmdLine(cmd_line) {}

const std::string &Command::getCmdLine() const
{
    return m_cmdLine;
}

BuiltInCommand::BuiltInCommand(const char *cmd_line)
    : Command(cmd_line) {}

SmallShell::SmallShell() : m_prompt("smash"), m_lastPwd(INVALID_PATH) {}

SmallShell::~SmallShell()
{
}

void SmallShell::addJob(const std::string &cmdLine, pid_t pid, bool isStopped)
{
    m_jobsList.addJob(cmdLine, pid, isStopped);
}

void SmallShell::printAlias(const std::pair<std::string, std::string> &alias) const
{
    const std::string line = alias.first + "='" + alias.second + "'";
    Msg.msg(line.c_str());
}

void SmallShell::printAllAliases() const
{
    for (const auto &alias : m_aliases)
    {
        printAlias(alias);
    }
}

const std::pair<std::string, std::string> *SmallShell::findAlias(const std::string &name) const
{
    for (const auto &alias : m_aliases)
    {
        if (alias.first == name)
        {
            return &alias;
        }
    }
    return nullptr;
}

bool SmallShell::addAlias(const std::string &name, const std::string &command)
{
    if (RESERVED_KEYWORDS.count(name) != 0 || findAlias(name) != nullptr)
    {
        return false;
    }
    m_aliases.emplace_back(name, command);
    return true;
}

bool SmallShell::removeAlias(const std::string &name)
{
    for (auto alias = m_aliases.begin(); alias != m_aliases.end(); ++alias)
    {
        if (alias->first == name)
        {
            m_aliases.erase(alias);
            return true;
        }
    }
    return false;
}

void SmallShell::setPrompt(const std::string &newPrompt)
{
    m_prompt = newPrompt;
}
std::string SmallShell::getPrompt() const
{
    return m_prompt;
}

void SmallShell::setLastPwd(const std::string &newLastPwd)
{
    m_lastPwd = newLastPwd;
}
std::string SmallShell::getLastPwd() const
{
    return m_lastPwd;
}

/**
 * Creates and returns a pointer to Command class which matches the given command line (cmd_line)
 */
Command *SmallShell::CreateCommand(const char *cmd_line)
{
    std::string cmd_s = _trim(string(cmd_line));
    bool isBackground = !cmd_s.empty() && cmd_s.back() == '&';
    if (isBackground)
    {
        cmd_s.pop_back();
        cmd_s = _rtrim(cmd_s);
    }
    if (cmd_s.empty())
    {
        return nullptr;
    }

    std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(WHITESPACE));
    if (auto *alias = findAlias(firstWord))
    {
        cmd_s.replace(0, firstWord.size(), alias->second);
        cmd_s = _trim(cmd_s);
        if (!cmd_s.empty() && cmd_s.back() == '&')
        {
            isBackground = true;
            cmd_s.pop_back();
            cmd_s = _rtrim(cmd_s);
        }
        if (cmd_s.empty())
        {
            return nullptr;
        }
        firstWord = cmd_s.substr(0, cmd_s.find_first_of(WHITESPACE));
    }

    if (firstWord.compare("chprompt") == 0)
    {
        return new ChPromptCommand(cmd_s.c_str());
    }
    else if (firstWord.compare("showpid") == 0)
    {
        return new ShowPidCommand(cmd_s.c_str());
    }
    else if (firstWord.compare("pwd") == 0)
    {
        return new PwdCommand(cmd_s.c_str());
    }
    else if (firstWord.compare("cd") == 0)
    {
        return new ChangeDirCommand(cmd_s.c_str(), SmallShell::getLastPwd());
    }
    else if (firstWord.compare("jobs") == 0)
    {
        return new JobsCommand(cmd_s.c_str(), &m_jobsList);
    }
    else if (firstWord.compare("fg") == 0)
    {
        return new ForegroundCommand(cmd_s.c_str(), &m_jobsList);
    }
    else if (firstWord.compare("quit") == 0)
    {
        return new QuitCommand(cmd_s.c_str(), &m_jobsList);
    }
    else if (firstWord.compare("kill") == 0)
    {
        return new KillCommand(cmd_s.c_str(), &m_jobsList);
    }
    else if (firstWord == "alias")
    {
        return new AliasCommand(cmd_s.c_str());
    }
    else if (firstWord == "unalias")
    {
        return new UnAliasCommand(cmd_s.c_str());
    }
    else if (firstWord == "unsetenv")
    {
        return new UnSetEnvCommand(cmd_s.c_str());
    }

    return new ExternalCommand(cmd_line, cmd_s.c_str(), isBackground);
}

void SmallShell::executeCommand(const char *cmd_line)
{
    m_jobsList.removeFinishedJobs();
    Command *cmd = CreateCommand(cmd_line);
    if (cmd == nullptr)
    {
        return;
    }
    cmd->execute();
    delete cmd;
}

// ------- ChPrompt --------------
ChPromptCommand::ChPromptCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line), m_newPrompt("smash")
{
    std::istringstream input(cmd_line);
    std::string commandName;
    input >> commandName;
    input >> m_newPrompt;
}

void ChPromptCommand::execute()
{
    SmallShell::getInstance().setPrompt(m_newPrompt);
}

// ------- ShowPidCommand --------------

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute()
{
    std::cout << "smash pid is " << getpid() << std::endl;
}

// ------- PwdCommand --------------

PwdCommand::PwdCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

std::string SmallShell::getCwd() const
{
    char buffer[PATH_MAX];
    if (getcwd(buffer, PATH_MAX) != nullptr)
    {
        std::string path(buffer);
        return path;
    }
    Msg.serr("getcwd");
    return INVALID_PATH;
}

void PwdCommand::execute()
{
    std::string cwd = SmallShell::getInstance().getCwd();
    if (cwd != INVALID_PATH)
    {
        std::cout << cwd << std::endl;
        return;
    }
}

// ------- ChangeDirCommand --------------

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, const std::string &lastPwd) : BuiltInCommand(cmd_line), m_type(ChangeDirCommand::ChangeDirType::NOOP), m_path(INVALID_PATH)
{
    std::istringstream input(cmd_line);
    std::string commandName;
    std::string firstArg, secondArg;

    input >> commandName;
    if (input >> firstArg)
    {
        m_type = ChangeDirCommand::ChangeDirType::PATH;
    }
    if (input >> secondArg)
    {
        Msg.rerr("cd: too many arguments");
        m_type = ChangeDirCommand::ChangeDirType::NOOP;
        return;
    }

    if (firstArg == "..")
    {
        m_type = ChangeDirCommand::ChangeDirType::DOTDOT;
        m_path = "..";
    }
    else if (firstArg == "-")
    {
        m_type = ChangeDirCommand::ChangeDirType::DASH;
        m_path = lastPwd;
    }

    if (m_type == ChangeDirCommand::ChangeDirType::DASH && lastPwd == INVALID_PATH)
    {
        Msg.rerr("cd: OLDPWD not set");
        m_type = ChangeDirCommand::ChangeDirType::NOOP;
        return;
    }

    if (m_type == ChangeDirCommand::ChangeDirType::PATH)
    {
        m_path = firstArg;
    }
}

void ChangeDirCommand::execute()
{
    if (m_type == ChangeDirCommand::ChangeDirType::NOOP)
    {
        return;
    }

    std::string cwd = SmallShell::getInstance().getCwd();
    if (cwd == INVALID_PATH)
    {
        return;
    }
    if (chdir(m_path.c_str()) != 0)
    {
        Msg.serr("chdir");
        return;
    }

    SmallShell::getInstance().setLastPwd(cwd);
}

// ------- ExternalCommand --------------

bool ExternalCommand::checkIsComplexCommand(const char *cmd_line) const
{
    if (std::strchr(cmd_line, QMARK) != nullptr || std::strchr(cmd_line, STAR) != nullptr)
    {
        return true;
    }
    return false;
}

ExternalCommand::ExternalCommand(const char *cmd_line, const char *exec_line, bool isBackground)
    : Command(cmd_line), m_args(), m_isBackground(isBackground)
{
    if (checkIsComplexCommand(exec_line))
    {
        m_bashCommand = exec_line;
        m_args[0] = const_cast<char *>("/bin/bash");
        m_args[1] = const_cast<char *>("-c");
        m_args[2] = &m_bashCommand[0];
        m_args[3] = nullptr;
    }
    else
    {
        _parseCommandLine(exec_line, m_args);
    }
}

void ExternalCommand::execute()
{
    pid_t pid = fork();
    if (pid == 0)
    {
        if (setpgrp() == -1)
        {
            Msg.serr("setpgrp");
            _exit(1);
        }
        execvp(m_args[0], m_args);
        Msg.serr("execvp");
        _exit(1);
    }
    else if (pid == -1)
    {
        Msg.serr("fork");
        return;
    }
    else
    {
        if (m_isBackground)
        {
            SmallShell::getInstance().addJob(getCmdLine(), pid);
            return;
        }
        pid_t result;
        do
        {
            result = waitpid(pid, nullptr, 0);
        } while (result == -1 && errno == EINTR);

        if (result == -1)
        {
            Msg.serr("waitpid");
        }
    }
}

// ------- JobList --------------

JobsList::JobEntry::JobEntry(int jid, pid_t pid, const std::string &cmdLine, bool isStopped)
    : m_jid(jid), m_pid(pid), m_cmdLine(cmdLine), m_isStopped(isStopped) {}

int JobsList::JobEntry::getJobId() const
{
    return m_jid;
}

pid_t JobsList::JobEntry::getPid() const
{
    return m_pid;
}

const std::string &JobsList::JobEntry::getCmdLine() const
{
    return m_cmdLine;
}

bool JobsList::JobEntry::isStopped() const
{
    return m_isStopped;
}

void JobsList::JobEntry::setStopped(bool isStopped)
{
    m_isStopped = isStopped;
}

std::ostream &operator<<(std::ostream &out, const JobsList::JobEntry &job)
{
    return out << '[' << job.getJobId() << "] " << job.getCmdLine();
}

JobsList::JobsList() : m_currentJid(0), m_jobs() {}

JobsList::~JobsList() = default;

void JobsList::addJob(const std::string &cmdLine, pid_t pid, bool isStopped)
{
    removeFinishedJobs();
    m_jobs.emplace_back(m_currentJid + 1, pid, cmdLine, isStopped);
    updateCurrentJobId();
}

void JobsList::updateCurrentJobId()
{
    if (m_jobs.empty())
    {
        m_currentJid = 0;
        return;
    }
    int currentMax = 0;
    for (JobEntry &job : m_jobs)
    {
        currentMax = std::max(currentMax, job.getJobId());
    }
    m_currentJid = currentMax;
}

void JobsList::printJobsList()
{
    removeFinishedJobs();
    for (const JobEntry &job : m_jobs)
    {
        std::cout << job << '\n';
    }
}

JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs)
    : BuiltInCommand(cmd_line), m_jobs(jobs) {}

void JobsCommand::execute()
{
    m_jobs->printJobsList();
}

void JobsList::removeJobById(int jobId)
{
    if (jobId <= 0)
    {
        return;
    }

    for (auto it = m_jobs.begin(); it != m_jobs.end(); ++it)
    {
        if (it->getJobId() == jobId)
        {
            m_jobs.erase(it);
            updateCurrentJobId();
            return;
        }
    }
}

void JobsList::removeFinishedJobs()
{
    for (std::size_t index = 0; index < m_jobs.size();)
    {
        JobEntry &job = m_jobs[index];
        int status;
        pid_t result;
        do
        {
            result = waitpid(job.getPid(), &status, WNOHANG | WUNTRACED);
        } while (result == -1 && errno == EINTR);

        if (result == -1)
        {
            const int waitError = errno;
            Msg.serr("waitpid");
            if (waitError == ECHILD)
            {
                removeJobById(job.getJobId());
                continue;
            }
        }
        if (result > 0)
        {
            if (WIFEXITED(status) || WIFSIGNALED(status))
            {
                removeJobById(job.getJobId());
                continue;
            }
            if (WIFSTOPPED(status))
            {
                job.setStopped(true);
            }
        }
        ++index;
    }
}

JobsList::JobEntry *JobsList::getJobById(const int jid)
{
    for (JobEntry &job : m_jobs)
    {
        if (job.getJobId() == jid)
        {
            return &job;
        }
    }
    return nullptr;
}

// ------- ForegroundCommand --------------

JobsList::JobEntry *JobsList::getMaxJob()
{
    JobEntry *maxJob = nullptr;
    for (JobEntry &job : m_jobs)
    {
        if (maxJob == nullptr || job.getJobId() > maxJob->getJobId())
        {
            maxJob = &job;
        }
    }
    return maxJob;
}

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs)
    : BuiltInCommand(cmd_line), m_jid(-1), m_foundJob(nullptr), m_jobs(jobs)
{
    // ASKED A QUESTION IN THE PIAZZA ABOUT THE ERROR MESSAGES - DO NOT SUBMIT WITHOUT MAKING SURE IT FITS THE GIVEN ANSWER
    std::istringstream input(cmd_line);
    std::string commandName, jobArgument, extraArgument;
    input >> commandName;
    if (!(input >> jobArgument))
    {
        // maximal job will always be the last one
        m_foundJob = m_jobs->getMaxJob();
        if (m_foundJob == nullptr)
        {
            Msg.rerr("fg: jobs list is empty");
        }
        return;
    }

    std::istringstream jid(jobArgument);
    if ((input >> extraArgument) ||
        !isOnlyDigits(jobArgument) ||
        !(jid >> m_jid))
    {
        Msg.rerr("fg: invalid arguments");
        return;
    }

    m_foundJob = m_jobs->getJobById(m_jid);

    if (m_foundJob == nullptr)
    {
        std::string message = "fg: job-id " + std::to_string(m_jid) + " does not exist";
        Msg.rerr(message.c_str());
    }
}

void ForegroundCommand::execute()
{
    if (m_foundJob == nullptr)
    {
        return;
    }
    // wait for command to finish - moved to foreground.
    pid_t pid = m_foundJob->getPid();
    pid_t result;

    std::cout << m_foundJob->getCmdLine() << " " << m_foundJob->getPid() << std::endl;
    m_jobs->removeJobById(m_foundJob->getJobId());

    do
    {
        result = waitpid(pid, nullptr, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1)
    {
        Msg.serr("waitpid");
    }
}

// ------- QuitCommand --------------

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), m_isKill(false), m_jobs(jobs)
{
    std::istringstream input(cmd_line);
    std::string commandName, killArgument;
    input >> commandName;

    m_isKill = ((input >> killArgument) && killArgument == "kill");
}

void QuitCommand::execute()
{
    if (m_isKill)
    {
        m_jobs->killAllJobs();
    }

    std::exit(0);
}

int JobsList::getSize() const
{
    return static_cast<int>(m_jobs.size());
}

void JobsList::killAllJobs()
{
    const std::string msg = "sending SIGKILL signal to " + std::to_string(m_jobs.size()) + " jobs:";
    Msg.smsg(msg.c_str());

    for (JobEntry &job : m_jobs)
    {
        const std::string jobMsg = std::to_string(job.getPid()) + ": " + job.getCmdLine();
        Msg.msg(jobMsg.c_str());
        if (kill(job.getPid(), SIGKILL) == -1)
        {
            Msg.serr("kill");
        }
    }
    updateCurrentJobId();
}

// ------- KillCommand --------------

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), m_jid(-1), m_foundJob(nullptr), m_jobs(jobs), m_signal(-1)
{
    std::istringstream input(cmd_line);
    std::string commandName, jobArgument, signalArgument, extraArgument;
    if (!(input >> commandName >> signalArgument >> jobArgument) ||
        (input >> extraArgument) || signalArgument.size() < 2 || signalArgument.front() != '-')
    {
        Msg.rerr("kill: invalid arguments");
        return;
    }

    std::istringstream jid(jobArgument);
    const std::string signalDigits = signalArgument.substr(1);
    std::istringstream signal(signalDigits);
    if (!isOnlyDigits(signalDigits) || !isOnlyDigits(jobArgument) ||
        !(signal >> m_signal) || !(jid >> m_jid))
    {
        Msg.rerr("kill: invalid arguments");
        return;
    }

    m_foundJob = m_jobs->getJobById(m_jid);

    if (m_foundJob == nullptr)
    {
        std::string message = "kill: job-id " + std::to_string(m_jid) + " does not exist";
        Msg.rerr(message.c_str());
    }
}

void KillCommand::execute()
{
    if (m_foundJob == nullptr)
    {
        return;
    }

    if (kill(m_foundJob->getPid(), m_signal) == -1)
    {
        Msg.serr("kill");
        return;
    }
    const std::string jobMsg = "signal number " + std::to_string(m_signal) + " was sent to pid " + std::to_string(m_foundJob->getPid());
    Msg.msg(jobMsg.c_str());
}

// ------- AliasCommand --------------

static bool checkAliasLegality(const std::string &argument)
{
    static const std::regex pattern(R"([a-zA-Z0-9_]+='[^']*')");
    return std::regex_match(argument, pattern);
}

std::pair<std::string, std::string> splitAliasCommand(std::string argument)
{
    const auto equals = argument.find('=');
    return {
        argument.substr(0, equals),
        argument.substr(equals + 2, argument.size() - equals - 3)};
}

AliasCommand::AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line), m_alias(""), m_command(""), m_isBlank(false)
{

    std::istringstream input(cmd_line);
    std::string commandName, secondArgument;

    if (!(input >> commandName))
    {
        Msg.rerr("alias: invalid alias format");
        return;
    }

    if (!(input >> secondArgument))
    {
        // command is only alias
        m_isBlank = true;
        return;
    }

    std::string remainder;
    std::getline(input, remainder);
    const std::string assignment = _rtrim(secondArgument + remainder);

    if (!checkAliasLegality(assignment))
    {
        Msg.rerr("alias: invalid alias format");
        return;
    }

    std::tie(m_alias, m_command) = splitAliasCommand(assignment);
}

void AliasCommand::execute()
{
    if (m_isBlank)
    {
        SmallShell::getInstance().printAllAliases();
        return;
    }

    if (!m_alias.empty() && !SmallShell::getInstance().addAlias(m_alias, m_command))
    {
        const std::string message = "alias: " + m_alias + " already exists or is a reserved command";
        Msg.rerr(message.c_str());
    }
}

// ------- UnAliasCommand --------------

UnAliasCommand::UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
    std::istringstream input(cmd_line);
    std::string commandName;
    input >> commandName;
    std::string alias;
    while (input >> alias)
    {
        m_aliases.push_back(alias);
    }

    if (m_aliases.empty())
    {
        Msg.rerr("unalias: not enough arguments");
    }
}

void UnAliasCommand::execute()
{
    if (m_aliases.empty())
    {
        return;
    }

    for (std::string &alias : m_aliases)
    {
        if (!SmallShell::getInstance().removeAlias(alias))
        {
            // unalias: <name> alias does not exist
            const std::string message = "unalias: " + alias + " alias does not exist";
            Msg.rerr(message.c_str());
            return;
        }
    }
}

// ------- UnSetEnvCommand --------------

UnSetEnvCommand::UnSetEnvCommand(const char *cmd_line) : BuiltInCommand(cmd_line), m_variables()
{
    std::istringstream input(cmd_line);
    std::string commandName;
    input >> commandName;

    for (std::string variable; input >> variable;)
    {
        m_variables.push_back(variable);
    }

    if (m_variables.empty())
    {
        Msg.rerr("unsetenv: not enough arguments");
    }
}

void UnSetEnvCommand::execute()
{
    if (m_variables.empty())
    {
        return;
    }

    const std::string path = "/proc/" + std::to_string(getpid()) + "/environ";
    std::ifstream envFile(path, std::ios::binary);
    if (!envFile)
    {
        Msg.serr("open");
        return;
    }

    std::unordered_set<std::string> environmentNames;
    std::string entry;
    while (std::getline(envFile, entry, '\0'))
    {
        const auto equals = entry.find('=');
        if (equals != std::string::npos)
        {
            environmentNames.insert(entry.substr(0, equals));
        }
    }
    if (envFile.bad())
    {
        Msg.serr("read");
        return;
    }

    for (const std::string &variable : m_variables)
    {
        if (environmentNames.erase(variable) == 0)
        {
            const std::string message = "unsetenv: " + variable + " does not exist";
            Msg.rerr(message.c_str());
            return;
        }

        const std::string prefix = variable + '=';
        char **destination = __environ;
        for (char **source = __environ; source != nullptr && *source != nullptr; ++source)
        {
            if (std::strncmp(*source, prefix.c_str(), prefix.size()) != 0)
            {
                *destination++ = *source;
            }
        }
        if (destination != nullptr)
        {
            *destination = nullptr;
        }
    }
}
