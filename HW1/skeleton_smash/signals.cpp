#include <cerrno>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace
{
    volatile sig_atomic_t ctrlCKillError = 0;
    volatile sig_atomic_t timeoutKillError = 0;
    volatile sig_atomic_t timeoutClockError = 0;
    volatile sig_atomic_t timeoutWaitError = 0;
    volatile sig_atomic_t timeoutTimerError = 0;

    constexpr long long nanosecondsPerSecond = 1000000000LL;
    constexpr long long nanosecondsPerMicrosecond = 1000;
    constexpr long long microsecondsPerSecond = 1000000;
    constexpr std::size_t maxTimedCommands = 100;
    constexpr char timeoutPrefix[] = "smash: ";
    constexpr char timeoutSuffix[] = " timed out!\n";

    struct TimedCommand
    {
        pid_t pid;
        long long startTime;
        unsigned int duration;
        char message[COMMAND_MAX_LENGTH + sizeof(timeoutPrefix) + sizeof(timeoutSuffix)];
        std::size_t messageLength;

        long long deadline() const
        {
            return startTime + duration * nanosecondsPerSecond;
        }
    };

    // SIGALRM and SIGCHLD are blocked whenever the main shell updates this list.
    TimedCommand timedCommands[maxTimedCommands] = {};
    long long armedDeadline = 0;
    sigset_t timeoutSignals;

    long long monotonicNanoseconds()
    {
        timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
        {
            timeoutClockError = errno;
            return -1;
        }
        return now.tv_sec * nanosecondsPerSecond + now.tv_nsec;
    }

    void scheduleAlarm(long long now)
    {
        long long nextDeadline = 0;
        for (const TimedCommand &command : timedCommands)
        {
            const long long deadline = command.deadline();
            if (command.pid > 0 && (nextDeadline == 0 || deadline < nextDeadline))
            {
                nextDeadline = deadline;
            }
        }

        if (nextDeadline == armedDeadline)
        {
            return;
        }
        itimerval timer = {};
        if (nextDeadline != 0)
        {
            const long long remaining = nextDeadline - now;
            const long long microseconds = remaining <= 0
                ? 1
                : (remaining + nanosecondsPerMicrosecond - 1) / nanosecondsPerMicrosecond;
            timer.it_value.tv_sec = microseconds / microsecondsPerSecond;
            timer.it_value.tv_usec = microseconds % microsecondsPerSecond;
        }

        // Rearm the one-shot SIGALRM timer without rounding to whole seconds.
        if (syscall(SYS_setitimer, ITIMER_REAL, &timer, nullptr) == -1)
        {
            timeoutTimerError = errno;
            return;
        }
        armedDeadline = nextDeadline;
    }

    bool childExited(pid_t pid)
    {
        // Leave exit status available for the shell's normal waitpid calls.
        siginfo_t status;
        status.si_pid = 0;
        const long result = syscall(SYS_waitid, P_PID, pid, &status, WEXITED | WNOHANG | WNOWAIT, nullptr);
        if (result == 0)
        {
            return status.si_pid != 0;
        }
        if (errno == ECHILD)
        {
            return true;
        }
        if (errno != EINTR)
        {
            timeoutWaitError = errno;
        }
        return false;
    }

    void removeExitedProcesses()
    {
        for (TimedCommand &command : timedCommands)
        {
            if (command.pid > 0 && childExited(command.pid))
            {
                command.pid = 0;
            }
        }
    }

    void timeoutChildHandler(int)
    {
        const int savedErrno = errno;
        removeExitedProcesses();
        const long long now = monotonicNanoseconds();
        if (now >= 0)
        {
            scheduleAlarm(now);
        }
        errno = savedErrno;
    }

    void reportSignalError(volatile sig_atomic_t &pendingError, const char *operation)
    {
        const int error = pendingError;
        if (error != 0)
        {
            pendingError = 0;
            const int savedErrno = errno;
            errno = error;
            Msg.serr(operation);
            errno = savedErrno;
        }
    }
}

void ctrlCHandler(int sig_num)
{
    const int savedErrno = errno;
    const char introMsg[] = "smash: got ctrl-C\n";
    (void)write(STDOUT_FILENO, introMsg, sizeof(introMsg) - 1);

    SmallShell &shell = SmallShell::getInstance();
    pid_t fgPid = shell.getFgPid();
    if (fgPid <= 0)
    {
        errno = savedErrno;
        return;
    }

    if (kill(fgPid, SIGKILL) == -1)
    {
        ctrlCKillError = errno;
        errno = savedErrno;
        return;
    }

    char pid_str[21];
    char *end = pid_str + sizeof(pid_str);
    char *start = end;

    for (pid_t remaining = fgPid; remaining > 0; remaining /= 10)
    {
        *--start = '0' + remaining % 10;
    }

    const char prefix[] = "smash: process ";
    const char suffix[] = " was killed\n";
    (void)write(STDOUT_FILENO, prefix, sizeof(prefix) - 1);
    (void)write(STDOUT_FILENO, start, end - start);
    (void)write(STDOUT_FILENO, suffix, sizeof(suffix) - 1);

    errno = savedErrno;
}

void reportSignalErrors()
{
    reportSignalError(ctrlCKillError, "kill");
    reportSignalError(timeoutKillError, "kill");
    reportSignalError(timeoutClockError, "clock_gettime");
    reportSignalError(timeoutWaitError, "waitid");
    reportSignalError(timeoutTimerError, "setitimer");
}

bool installTimeoutHandlers()
{
    sigemptyset(&timeoutSignals);
    sigaddset(&timeoutSignals, SIGALRM);
    sigaddset(&timeoutSignals, SIGCHLD);

    struct sigaction action = {};
    action.sa_mask = timeoutSignals;
    action.sa_flags = SA_RESTART;
    action.sa_handler = alarmHandler;
    if (sigaction(SIGALRM, &action, nullptr) == -1)
    {
        Msg.serr("sigaction");
        return false;
    }

    action.sa_flags |= SA_NOCLDSTOP;
    action.sa_handler = timeoutChildHandler;
    if (sigaction(SIGCHLD, &action, nullptr) == -1)
    {
        Msg.serr("sigaction");
        return false;
    }
    return true;
}

void registerTimeout(unsigned int duration, const std::string &commandLine, pid_t pid)
{
    const std::string message = timeoutPrefix + commandLine + timeoutSuffix;
    sigset_t oldMask;
    if (sigprocmask(SIG_BLOCK, &timeoutSignals, &oldMask) == -1)
    {
        Msg.serr("sigprocmask");
        return;
    }

    removeExitedProcesses();
    const long long now = monotonicNanoseconds();
    if (now >= 0)
    {
        if (!childExited(pid))
        {
            for (TimedCommand &command : timedCommands)
            {
                if (command.pid == 0)
                {
                    command.pid = pid;
                    command.startTime = now;
                    command.duration = duration;
                    command.messageLength = message.copy(command.message, sizeof(command.message));
                    break;
                }
            }
        }
        scheduleAlarm(now);
    }

    if (sigprocmask(SIG_SETMASK, &oldMask, nullptr) == -1)
    {
        Msg.serr("sigprocmask");
    }
}

void alarmHandler(int)
{
    const int savedErrno = errno;
    constexpr char message[] = "smash: got an alarm\n";
    (void)write(STDOUT_FILENO, message, sizeof(message) - 1);

    armedDeadline = 0;
    removeExitedProcesses();
    const long long now = monotonicNanoseconds();
    if (now >= 0)
    {
        for (TimedCommand &command : timedCommands)
        {
            if (command.pid <= 0 || command.deadline() > now)
            {
                continue;
            }

            if (kill(command.pid, SIGKILL) == 0)
            {
                (void)write(STDOUT_FILENO, command.message, command.messageLength);
            }
            else if (errno != ESRCH)
            {
                timeoutKillError = errno;
            }
            command.pid = 0;
        }
        scheduleAlarm(now);
    }
    errno = savedErrno;
}
