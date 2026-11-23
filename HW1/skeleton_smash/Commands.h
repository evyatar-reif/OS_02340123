// Ver: 20-10-2026
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <iosfwd>
#include <sys/types.h>
#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class MessageReporter
{
public:
    void serr(const char *operation) const;
    void rerr(const char *message) const;
    void smsg(const char *message) const;
    void msg(const char *message) const;
};

extern const MessageReporter Msg;

class Command
{
protected:
    std::string m_cmdLine;

public:
    Command(const char *cmd_line);

    virtual ~Command() = default;

    const std::string &getCmdLine() const;

    virtual void execute() = 0;

    // virtual void prepare();
    // virtual void cleanup();
};

class BuiltInCommand : public Command
{
public:
    BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand()
    {
    }
};

class ChPromptCommand : public BuiltInCommand
{
private:
    std::string m_newPrompt;

public:
    ChPromptCommand(const char *cmd_line);
    void execute() override;
};

class PwdCommand : public BuiltInCommand
{
public:
    PwdCommand(const char *cmd_line);
    void execute() override;
};

class ExternalCommand : public Command
{
private:
    char *m_args[COMMAND_MAX_ARGS + 2];
    bool m_isBackground;
    std::string m_bashCommand;

public:
    ExternalCommand(const char *cmd_line, const char *exec_line, bool isBackground);

    virtual ~ExternalCommand()
    {
    }

    void execute() override;

    bool checkIsComplexCommand(const char *cmd_line) const;
};

class RedirectionCommand : public Command
{
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand()
    {
    }

    void execute() override;
};

class PipeCommand : public Command
{
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand()
    {
    }

    void execute() override;
};

class DiskUsageCommand : public Command
{
public:
    DiskUsageCommand(const char *cmd_line);

    virtual ~DiskUsageCommand()
    {
    }

    void execute() override;
};

class WhoAmICommand : public Command
{
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand()
    {
    }

    void execute() override;
};

class TimeoutCommand : public Command
{
    // TODO: Add your data members **BONUS: 10 Points**
public:
    TimeoutCommand(const char *cmd_line);

    virtual ~TimeoutCommand()
    {
    }

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand
{
    enum ChangeDirType
    {
        PATH,
        DASH,
        DOTDOT,
        NOOP
    };
    // TODO: Add your data members
private:
    ChangeDirType m_type;
    std::string m_path;

public:
    ChangeDirCommand(const char *cmd_line, const std::string &lastPwd);

    virtual ~ChangeDirCommand()
    {
    }

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand
{
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand()
    {
    }

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand
{
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand()
    {
    }

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand
{
private:
    bool m_isKill;
    JobsList *m_jobs;

public:
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand()
    {
    }

    void execute() override;
};

class JobsList
{
public:
    class JobEntry
    {
    private:
        int m_jid;
        pid_t m_pid;
        std::string m_cmdLine;
        bool m_isStopped;

        friend std::ostream &operator<<(std::ostream &out, const JobEntry &job);

    public:
        JobEntry(int jid, pid_t pid, const std::string &cmdLine, bool isStopped);

        int getJobId() const;
        pid_t getPid() const;
        const std::string &getCmdLine() const;
        bool isStopped() const;
        void setStopped(bool isStopped);
    };

    JobsList();

    ~JobsList();

    void addJob(const std::string &cmdLine, pid_t pid, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(const int jobId);

    void removeJobById(int jobId);

    JobEntry *getMaxJob();

    JobEntry *getLastStoppedJob(int *jobId);

    // TODO: Add extra methods or modify exisitng ones as needed
    void updateCurrentJobId();

    int getSize() const;

private:
    int m_currentJid;
    std::vector<JobEntry> m_jobs;
};

class JobsCommand : public BuiltInCommand
{
private:
    JobsList *m_jobs;

public:
    JobsCommand(const char *cmd_line, JobsList *jobs);

    virtual ~JobsCommand()
    {
    }

    void execute() override;
};

class KillCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand()
    {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand
{
    // TODO: Add your data members
    int m_jib;
    JobsList::JobEntry *m_foundJob;
    JobsList *m_jobs;

public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand()
    {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand
{
public:
    AliasCommand(const char *cmd_line);

    virtual ~AliasCommand()
    {
    }

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand
{
public:
    UnAliasCommand(const char *cmd_line);

    virtual ~UnAliasCommand()
    {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand
{
public:
    UnSetEnvCommand(const char *cmd_line);

    virtual ~UnSetEnvCommand()
    {
    }

    void execute() override;
};

class GetFileTypeCommand : public BuiltInCommand
{
public:
    GetFileTypeCommand(const char *cmd_line);

    virtual ~GetFileTypeCommand()
    {
    }

    void execute() override;
};

class SmallShell
{
private:
    // TODO: Add your data members
    std::string m_prompt;
    std::string m_lastPwd;
    JobsList m_jobsList;
    SmallShell();

public:
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete;     // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance()             // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);
    void addJob(const std::string &cmdLine, pid_t pid, bool isStopped = false);

    // TODO: add extra methods as needed
    void setPrompt(const std::string &newPrompt);
    std::string getPrompt() const;

    void setLastPwd(const std::string &newLastPwd);
    std::string getLastPwd() const;

    std::string getCwd() const;
};

#endif // SMASH_COMMAND_H_
