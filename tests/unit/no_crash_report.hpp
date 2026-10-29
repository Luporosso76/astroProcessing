#pragma once

#include <csignal>
#include <cstdio>
#include <cstdlib>

#include <unistd.h>

// Un abort voluto dentro un death test termina con _exit: nessun core dump, quindi nessun report di crash del
// sistema. Il messaggio dell'asserzione e' gia' su stderr, dove il death test lo confronta.
inline void exit_instead_of_core_dump() {
  struct sigaction action {};
  action.sa_handler = [](int) { _exit(134); };
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGABRT, &action, nullptr) != 0) {
    std::fputs("sigaction(SIGABRT) fallita\n", stderr);
    std::_Exit(1);
  }
}
