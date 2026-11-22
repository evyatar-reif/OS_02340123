#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <limits.h>
#include <ostream>

using namespace std;

const ErrorReporter Error;

void ErrorReporter::smash(const char *operation) const
{
    const int errorNumber = errno;
    const std::string message = std::string("smash error: ") + operation + " failed";
    errno = errorNumber;
    std::perror(message.c_str());
}

void ErrorReporter::regular(const char *message) const
{
    std::cerr << "smash error: " << message << std::endl;
}

const std::string WHITESPACE = " \n\r\t\f\v";
const std::string INVALID_PATH = "-1";

const char STAR = '*';
const char QMARK = '?';

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

    string cmd_s = _trim(string(cmd_line));
    if (cmd_s.empty())
    {
        return nullptr;
    }
    bool isBackground = _isBackgroundCommand(cmd_s.c_str());
    if (isBackground)
    {
        _removeBackgroundSign(&cmd_s[0]);
        cmd_s.resize(std::strlen(cmd_s.c_str()));
        cmd_s = _rtrim(cmd_s);
    }

    if (cmd_s.empty()) // could be empty after removing & (if the line only has & and WHITESPACE)
    {
        return nullptr;
    }

    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(WHITESPACE));
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
    Error.smash("getcwd");
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
        Error.regular("cd: too many arguments");
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
        Error.regular("cd: OLDPWD not set");
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
        Error.smash("chdir");
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
            Error.smash("setpgrp");
            _exit(1);
        }
        execvp(m_args[0], m_args);
        Error.smash("execvp");
        _exit(1);
    }
    else if (pid == -1)
    {
        Error.smash("fork");
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
            Error.smash("waitpid");
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
            Error.smash("waitpid");
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
    : BuiltInCommand(cmd_line), m_jib(0), m_foundJob(nullptr), m_jobs(jobs)
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
            Error.regular("fg: jobs list is empty");
        }
        return;
    }

    std::istringstream jid(jobArgument);
    // fail if there's an extra arg, or jid isn't a number, or a non valid job id
    if ((input >> extraArgument) || !(jid >> m_jib) || !jid.eof())
    {
        Error.regular("fg: invalid arguments");
        return;
    }

    m_foundJob = m_jobs->getJobById(m_jib);

    if (m_foundJob == nullptr)
    {
        std::string message = "fg: job-id " + std::to_string(m_jib) + " does not exist";
        Error.regular(message.c_str());
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
        Error.smash("waitpid");
    }
}
