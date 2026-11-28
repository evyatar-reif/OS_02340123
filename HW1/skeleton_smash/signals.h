#ifndef SMASH__SIGNALS_H_
#define SMASH__SIGNALS_H_

#include <string>
#include <sys/types.h>

void ctrlCHandler(int sig_num);
void reportSignalErrors();

void alarmHandler(int sig_num);
bool installTimeoutHandlers();
void registerTimeout(unsigned int duration, const std::string &commandLine, pid_t pid);

#endif //SMASH__SIGNALS_H_
