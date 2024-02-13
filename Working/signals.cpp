#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num)
{
  std::cout << "smash: got ctrl-C\n";
  SmallShell &smash = SmallShell::getInstance();
  if (smash.getCuttForegroundPID() != -1)
  {
    if (kill(smash.getCuttForegroundPID(), SIGKILL) != 0)
    {
      perror("smash error: kill failed");
      return;
    }
    cout << "smash: process " << smash.getCuttForegroundPID() << " was killed\n";
    smash.setCurrForegroundPID(-1);
  }
}
