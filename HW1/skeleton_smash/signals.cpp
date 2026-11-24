#include <cerrno>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <unistd.h>

namespace
{
volatile sig_atomic_t ctrlCKillError = 0;
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

void reportCtrlCKillError()
{
    const int killError = ctrlCKillError;
    if (killError != 0)
    {
        ctrlCKillError = 0;
        const int savedErrno = errno;
        errno = killError;
        Msg.serr("kill");
        errno = savedErrno;
    }
}

void alarmHandler(int sig_num)
{
    // TODO: Add your implementation **BONUS: timeout command**
}
