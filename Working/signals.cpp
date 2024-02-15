#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num)
{
  std::cout << "smash: got ctrl-C\n";
  SmallShell &smash = SmallShell::getInstance();
  if (smash.getCurrForegroundPID() != -1)
  {
    if (kill(smash.getCurrForegroundPID(), SIGKILL) == -1)
    {
      perror("smash error: kill failed");
      return;
    }
    cout << "smash: process " << smash.getCurrForegroundPID() << " was killed\n";
    smash.setCurrForegroundPID(-1);
  }
}
