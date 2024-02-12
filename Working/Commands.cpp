#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

#include <cstring>     // For strcpy
#include <fcntl.h>     // For open and its flags  // for `open` and its MACROs
#include <sys/types.h> // For data types          // for `open` and its MACROs
#include <sys/stat.h>  // For mode constants      // for `open` and its MACROs

#define COMMAND_MAX_PATH_LENGTH (80)
#define COMMAND_MAX_LENGTH (80)

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

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
    memset(args[i], 0, s.length() + 1);
    strcpy(args[i], s.c_str());
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
  unsigned int idx = str.find_last_not_of(WHITESPACE);
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
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

/* *
 * Command
 */

Command::Command(const char *cmd_line)
    : m_ground_type((_isBackgroundCommand(cmd_line)) ? (GroundType::Background) : (GroundType::Foreground)),
      m_cmd_line(cmd_line) // (m_ground_type == GroundType::Background) ? _trim(m_remove_background_sign(cmd_line)) : _trim(cmd_line)

{
}

Command::~Command()
{
  // default
}

std::string Command::m_remove_background_sign(const char *cmd_line) const
{
  // ? should we check for std::bad_alloc?
  // copy the cmd_line
  char *cmd_line_copy = new char[strlen(cmd_line) + 1];
  strcpy(cmd_line_copy, cmd_line);

  // remove the background sign from the copy
  _removeBackgroundSign(cmd_line_copy);

  std::string modified_cmd_line(cmd_line_copy);

  // dont forget to delete the allocated memory
  delete[] cmd_line_copy;

  // Return the modified command line
  return modified_cmd_line;
}

/*
 * External Commands
 */

ExternalCommand::Complexity ExternalCommand::_get_complexity_type(const char *cmd_line)
{
  if (std::string(cmd_line).find_first_of("*?") != std::string::npos)
  {
    return Complexity::Complex;
  }
  else
  {
    return Complexity::Simple;
  }
}

ExternalCommand::ExternalCommand(const char *cmd_line)
    : Command(cmd_line),
      m_complexity(_get_complexity_type(cmd_line))
{
  // cant really do any checks for if a command is external or not
}

ExternalCommand::~ExternalCommand()
{
  // default
}

void ExternalCommand::execute()
{

  pid_t pid = fork();

  if (pid == 0) // * son
  {
    if (setpgrp() == -1) // failure
    {
      perror("smash error: setpgrp failed");
      return;
    }

    if (m_complexity == Complexity::Complex)
    {
      // trim the cmd_line and remove back ground sign (also then trim)
      const char *command_line = _trim(Command::m_remove_background_sign(getCMDLine().c_str())).c_str();

      if (execlp("/bin/bash", "/bin/bash", "-c", command_line, nullptr) != 0) // failure
      {
        perror("smash error: execlp failed");
        return;
      }
    }
    else
    {

      char *args[COMMAND_MAX_ARGS + 1] = {0};
      char trimmed_cmd_line[COMMAND_MAX_LENGTH + 1];

      strcpy(trimmed_cmd_line, getCMDLine().c_str());

      _removeBackgroundSign(trimmed_cmd_line);

      _parseCommandLine(_trim(std::string(trimmed_cmd_line)).c_str(), args);

      if (execvp(args[0], args) == -1)
      {
        perror("smash error: setpgrp failed");
        return;
      }
    }
  }
  else // * parent
  {
    m_pid = pid;
    if (isBackground())
    {
      SmallShell::getInstance().getJobsList().addJob(this, m_pid);
    }
    else
    {
      SmallShell::getInstance().setCurrForegroundPID(m_pid);
      if (waitpid(m_pid, nullptr, WUNTRACED) == -1)
      {
        perror("smash error: waitpid failed");
        return;
      }
      SmallShell::getInstance().setCurrForegroundPID(-1);
    }
  }
}

/*
 * Special Commands
 */

// * Special Commands 1 (RedirectionCommand)

bool _is_redirection_command(const char *cmd_line)
{
  if (cmd_line)
  {
    std::string s(cmd_line);
    // if no > were found, return false
    int index_first = s.find_first_of(">");
    //std::cout << "index is " << index_first << "\n";
    if (index_first == -1)
    {
      return false;
    }
    else
    {
      return true;
    }
  }
  return false;
}

RedirectionCommand::RedirectionType RedirectionCommand::get_redirection_type(const char *cmd_line)
{
  // assumes there is atleast ">" or ">>"
  std::string s(cmd_line);
  unsigned int first_symbol_index = s.find_first_of(">");
  if (s[first_symbol_index + 1] == '>')
  {
    return RedirectionType::Append;
  }
  else
  {
    return RedirectionType::Override;
  }
}

RedirectionCommand::RedirectionCommand(const char *cmd_line)
    : Command(cmd_line),
      m_command(nullptr),
      m_file_path()
{
  if (!_is_redirection_command(cmd_line))
  {
    throw std::logic_error("RedirectionCommand::RedirectionCommand.");
  }
  m_redirection_type = get_redirection_type(cmd_line);

  std::string cmd_str(cmd_line);
  std::string command_str = _trim(cmd_str.substr(0, cmd_str.find_first_of(">")));
  m_command = SmallShell::getInstance().CreateCommand(command_str.c_str());
  if (m_command == nullptr)
  {
    throw std::logic_error("RedirectionCommand::RedirectionCommand.");
  }
  m_file_path = _trim(cmd_str.substr(cmd_str.find_last_of(">") + 1));
}

RedirectionCommand::~RedirectionCommand()
{
  delete m_command;
}

void RedirectionCommand::execute()
{
  // this command will not be tested with &
  enum STANDARD
  {
    IN = 0,
    OUT = 1,
  };

  int original_stdout = dup(STANDARD::OUT); // ? should we use STDOUT_FILENO

  if (original_stdout == -1)
  {
    perror("smash error: dup failed");
    return;
    // exit(EXIT_FAILURE);
  }

  // close the original fd for standard output
  if (close(STANDARD::OUT))
  {
    perror("smash error: close failed");
    return;
  }

  if (m_redirection_type == RedirectionType::Override)
  {
    // open a new/existing file for overriding (its fd will be 1)
    // O_WRONLY: Open for writing only.
    // O_CREAT: Create file if it does not exist.
    // O_TRUNC: Truncate size to 0.
    // 0644: File permission bits (user: read+write, group: read, others: read).
    if (open(m_file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644) == -1)
    {
      perror("smash error: open failed");
      return;
    }
  }
  else // (m_redirection_type == RedirectionType::Append)
  {
    // open a new/existing file for appending (its fd will be 1)
    // O_WRONLY: Open for writing only.
    // O_CREAT: Create file if it does not exist.
    // O_APPEND: Append data at the end of the file.
    // 0644: File permission bits (user: read+write, group: read, others: read).
    if (open(m_file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644) == -1)
    {
      perror("smash error: open failed");
      return;
    }
  }

  // execute command
  m_command->execute();

  // Restore the original stdout
  if (dup2(original_stdout, STANDARD::OUT) == -1)
  {
    perror("smash  error: dup2 failed");
    exit(EXIT_FAILURE);
  }

  // program returned to the original state where stdout in the standard output stream.
}

// * Special Commands 2 (PipeCommand)

bool _is_pipe_command(const char *cmd_line)
{
  if (std::string(cmd_line).find_first_of("|") != std::string::npos)
  {
    return true;
  }
  return false;
}

PipeCommand::PipeType PipeCommand::_get_pipe_type(const char *cmd_line)
{
  std::string s(cmd_line);
  unsigned int index_of_line = s.find_first_of("|");
  unsigned int index_of_amp = s.find_first_of("&");

  if (index_of_line != std::string::npos)
  {
    if (index_of_amp != std::string::npos)
    {
      return PipeType::Standard;
    }
    else
    {
      return PipeType::Error;
    }
  }
  else
  {
    throw std::logic_error("PipeCommand::_get_pipe_type");
  }
}

Command *PipeCommand::_get_cmd_1(const char *cmd_line)
{
  std::string s(cmd_line);
  unsigned int index_of_line = s.find_first_of("|");
  if (index_of_line != std::string::npos)
  {
    Command *command = SmallShell::getInstance().CreateCommand(s.substr(0, index_of_line).c_str());
    if (command)
    {
      return command;
    }
    else
    {
      throw std::logic_error("PipeCommand::_get_cmd_1");
    }
  }
  return nullptr;
}

Command *PipeCommand::_get_cmd_2(const char *cmd_line)
{

  std::string s(cmd_line);
  unsigned int index_of_line = s.find_first_of("|");
  unsigned int index_of_amp = s.find_first_of("&");

  if (index_of_line != std::string::npos)
  {
    Command *command;
    if (index_of_amp != std::string::npos)
    {
      command = SmallShell::getInstance().CreateCommand(s.substr(index_of_amp + 1).c_str());
    }
    else
    {
      command = SmallShell::getInstance().CreateCommand(s.substr(index_of_line + 1).c_str());
    }

    if (command)
    {
      return command;
    }
    else
    {
      throw std::logic_error("PipeCommand::_get_cmd_2");
    }
  }
  return nullptr;
}

PipeCommand::PipeCommand(const char *cmd_line)
    : Command(cmd_line)
{
  if (_is_pipe_command(cmd_line))
  {
    m_pipe_type = _get_pipe_type(cmd_line);
    m_cmd_1 = _get_cmd_1(cmd_line);
    m_cmd_2 = _get_cmd_2(cmd_line);
    if (m_cmd_1 == nullptr || m_cmd_2 == nullptr) // no need but will keep for more safety
    {
      throw std::logic_error("PipeCommand::PipeCommand");
    }
  }
  else
  {
    throw std::logic_error("PipeCommand::PipeCommand");
  }
}

PipeCommand::~PipeCommand()
{
  delete m_cmd_1;
  delete m_cmd_2;
}

void PipeCommand::execute() //!!!!!!!!!! we nee dto be careful, the pipe could get full, i think we need fork
{
  // this command will not be tested with &
  enum PIPE
  {
    READ = 0,
    WRITE = 1
  };

  enum STANDARD
  {
    IN = 0,
    OUT = 1,
    ERR = 2
  };

  // create the pipe
  int files[] = {-1, -1};
  if (pipe(files) == -1)
  {
    perror("smash error: pipe failed");
    return;
  }

  // change the write file from stdout/err
  int original_write = -1;
  if (m_pipe_type == PipeType::Standard) // we have to close/dup 1
  {
    original_write = dup(STANDARD::OUT); // stdout
    if (original_write == -1)
    {
      perror("smash error: dup failed");
      return;
    }
    if (dup2(files[PIPE::WRITE], STANDARD::OUT))
    {
      perror("smash error: dup2 failed");
      return;
    }
  }
  else // close/dup 2
  {
    original_write = dup(STANDARD::ERR); // stderr
    if (original_write == -1)
    {
      perror("smash error: dup failed");
      return;
    }
    if (dup2(files[PIPE::WRITE], STANDARD::ERR))
    {
      perror("smash error: dup2 failed");
      return;
    }
  }

  // execute the first command
  m_cmd_1->execute();

  // return back the original write stream
  if (dup2(original_write, files[PIPE::WRITE]))
  {
    perror("smash error: dup2 failed");
    return;
  }

  // change the read file from stdin
  int original_read = dup(STANDARD::IN); // stdin
  if (original_read == -1)
  {
    perror("smash error: dup failed");
    return;
  }
  if (dup2(files[PIPE::READ], STANDARD::IN))
  {
    perror("smash error: dup2 failed");
    return;
  }

  // execute the second command
  m_cmd_2->execute();

  // return back the original read stream
  if (dup2(original_read, files[PIPE::READ]))
  {
    perror("smash error: dup2 failed");
    return;
  }
}

// * Special Commands 3 (ChmodCommand) , actually inherits from BuiltInCommand

ChmodCommand::ChmodCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "chmod")
  {
    throw std::logic_error("ChmodCommand::ChmodCommand");
  }

  if (getArgs().size() != 2)
  {
    std::cerr << "smash error: chmod: invalid arguments\n";
    throw std::logic_error("ChmodCommand::ChmodCommand");
  }

  std::string modes = getArgs().front();
  std::string file_path = getArgs().back();

  // if the string is NOT from 3 octal digits
  if ((modes.size() != 3) || (modes.find_first_not_of("01234567") != std::string::npos))
  {
    std::cerr << "smash error: chmod: invalid arguments\n";
    throw std::logic_error("ChmodCommand::ChmodCommand");
  }
}

ChmodCommand::~ChmodCommand()
{
  // default
}

void ChmodCommand::execute()
{
  mode_t mode = static_cast<mode_t>(std::stoi(getArgs().front(), nullptr, 8)); // shouldn't throw

  if (chmod(getArgs().back().c_str(), mode) == -1) // getArgs().back() is the file path
  {
    perror("smash error: chmod failed");
    return;
  }
}

/*
 * Built In Commands
 */

BuiltInCommand::BuiltInCommand(const char *cmd_line)
    : Command(cmd_line),
      m_name(m_parse_name(cmd_line)),
      m_args(m_parse_args(cmd_line))
{
}

BuiltInCommand::~BuiltInCommand()
{
  // default
}

std::string BuiltInCommand::m_parse_name(const char *cmd_line) const
{
  std::istringstream iss(_trim(string(cmd_line)).c_str());

  // if there is no first string then an empty string has been given (shouldn't be tested)
  std::string cmd_name;
  if (!(iss >> cmd_name))
  {
    // ? should not throw i think but lets keep it in case of debugging
    throw std::logic_error("MY_ERROR: BuiltInCommand::m_parse_name.");
  }
  return cmd_name;
}

std::vector<std::string> BuiltInCommand::m_parse_args(const char *cmd_line) const
{
  std::vector<std::string> args;
  std::istringstream iss(_trim(string(cmd_line)).c_str());

  // ignore the first string
  std::string cmd_name;
  if (!(iss >> cmd_name))
  {
    // ? should not throw i think but lets keep it in case of debugging
    throw std::logic_error("MY_ERROR: BuiltInCommand::m_parse_args.");
  }

  // push the other strings
  for (std::string s; iss >> s;)
  {
    args.push_back(s);
  }
  return args;
}

// * BuiltInCommand 1 (ChangePromptCommand)

ChangePromptCommand::ChangePromptCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "chprompt")
  {
    throw std::logic_error("ChangePromptCommand::ChangePromptCommand");
  }
}

ChangePromptCommand::~ChangePromptCommand()
{
  // default
}

void ChangePromptCommand::execute()
{
  // ? Any special checking for the name validity
  SmallShell::getInstance().setPrompt(
      (getArgs().size() == 0) ? SmallShell::DEFAULT_PROMPT : getArgs().front());
}

// * BuiltInCommand 2 (ShowPidCommand)
ShowPidCommand::ShowPidCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "showpid")
  {
    throw std::logic_error("ShowPidCommand::ShowPidCommand");
  }
}

ShowPidCommand::~ShowPidCommand()
{
  // default
}

void ShowPidCommand::execute()
{
  // `getpid()` is always successful and does not have an error return.
  std::cout << "smash pid is " << getpid() << '\n';
}

// * BuiltInCommand 3 (GetCurrDirCommand)
GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "pwd")
  {
    throw std::logic_error("GetCurrDirCommand::GetCurrDirCommand");
  }
}

GetCurrDirCommand::~GetCurrDirCommand()
{
  // default
}

void GetCurrDirCommand::execute()
{
  char path[COMMAND_MAX_PATH_LENGTH + 1];
  // `getcwd()` writes the absolute pathname of the current working directory to the `path` array
  if (getcwd(path, COMMAND_MAX_PATH_LENGTH + 1) != nullptr) // success
  {
    std::cout << std::string(path) << '\n';
  }
  else
  {
    // ? should we print an error
    perror("smash error: getcwd failed");
  }
}

// * BuiltInCommand 4 (ChangeDirCommand)

/* static variables */
std::list<std::string> ChangeDirCommand::CD_PATH_HISTORY; // default c'tor will be called

ChangeDirCommand::ChangeDirCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "cd")
  {
    throw std::logic_error("ChangeDirCommand::ChangeDirCommand");
  }
  // 0 arguments will NOT be tested
  if (getArgs().size() > 1) // more than one argument
  {
    std::cerr << "smash error: cd: too many arguments\n";
    throw std::logic_error("ChangeDirCommand::ChangeDirCommand");
  }
}

ChangeDirCommand::~ChangeDirCommand()
{
  // default
}

void ChangeDirCommand::execute()
{
  // 0 arguments for cd will NOT be tested

  // get the current working directory
  char cwd[COMMAND_MAX_PATH_LENGTH + 1];
  // `getcwd()` writes the absolute pathname of the current working directory to the `path` array
  if (getcwd(cwd, COMMAND_MAX_PATH_LENGTH + 1) == nullptr) // failure
  {
    perror("smash error: getcwd failed");
    return;
  }
  std::string curr_dir(cwd);

  // the ctor guarantees there will be 1 argument only
  if ("-" == getArgs().back())
  {
    if (CD_PATH_HISTORY.size() == 0)
    {
      std::cerr << "smash error: cd: OLDPWD not set\n";
      return;
    }
    else
    {
      // CD_PATH_HISTORY has paths in it
      if (0 == chdir(CD_PATH_HISTORY.back().c_str())) // success
      {
        CD_PATH_HISTORY.pop_back();
        CD_PATH_HISTORY.push_back(curr_dir);
      }
      else
      {
        perror("smash error: chdir failed");
        return;
      }
    }
  }
  else if (".." == getArgs().back())
  {
    // get the parent directory
    std::string parent_dir = get_parent_directory(cwd);
    // change dir to the parent
    if (0 == chdir(parent_dir.c_str())) // success
    {
      CD_PATH_HISTORY.push_back(curr_dir);
    }
    else
    {
      perror("smash error: chdir failed");
      return;
    }
  }
  else // "normal path"
  {
    // change dir to the path given
    if (0 == chdir(getArgs().front().c_str())) // success
    {
      CD_PATH_HISTORY.push_back(getArgs().front());
    }
    else
    {
      perror("smash error: chdir failed");
      return;
    }
  }
}

/* methods */
std::string ChangeDirCommand::get_parent_directory(const std::string &path) const
{
  size_t lastSlashPos = path.find_last_of("/");

  // If there's no slash, or only one at the beginning (root), return as is or some indication
  if (lastSlashPos == std::string::npos || (lastSlashPos == 0 && path.length() == 1))
  {
    return path; // No parent directory, or it's the root
  }
  else if (lastSlashPos == 0)
  {
    return "/"; // The parent directory is the root
  }
  else
  {
    return path.substr(0, lastSlashPos); // Return the substring up to the last slash
  }
}

// * BuiltInCommand 5 (JobsCommand)

JobsCommand::JobsCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "jobs")
  {
    throw std::logic_error("JobsCommand::JobsCommand");
  }
}

JobsCommand::~JobsCommand()
{
  // default
}

void JobsCommand::execute()
{
  // TODO: figure out what are they yapping about on " if the job was added again then the timer should reset. "
  // getJobsList() always return an updated JobsList
  SmallShell::getInstance().getJobsList().printJobsList();
}

// * BuiltInCommand 6 (ForegroundCommand)

ForegroundCommand::ForegroundCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "fg")
  {
    throw std::logic_error("ForegroundCommand::ForegroundCommand");
  }
  auto jobslist = SmallShell::getInstance().getJobsList();

  if (getArgs().size() == 0 && jobslist.size() == 0)
  {
    std::cerr << "smash error: fg: jobs list is empty\n";
    throw std::logic_error("ForegroundCommand::ForegroundCommand");
  }

  try
  {
    if (getArgs().size() > 0)
    {
      m_id = std::stoi(getArgs().front());
    }
    else
    {
      m_id = jobslist.getLastJob()->getJobID();
    }
  }
  catch (...)
  {
    std::cerr << "smash error: fg: invalid arguments\n";
    throw std::logic_error("ForegroundCommand::ForegroundCommand");
  }

  if (getArgs().size() > 0 && jobslist.getJobById(m_id) == nullptr)
  {
    std::cerr << "smash error: fg: job-id " << m_id << " does not exist\n";
    throw std::logic_error("ForegroundCommand::ForegroundCommand");
  }

  if (getArgs().size() > 1)
  {
    std::cerr << "smash error: fg: invalid arguments\n";
    throw std::logic_error("ForegroundCommand::ForegroundCommand");
  }
}

ForegroundCommand::~ForegroundCommand()
{
  // default
}

void ForegroundCommand::execute()
{
  auto jobslist = SmallShell::getInstance().getJobsList();
  JobsList::JobEntry *job = jobslist.getJobById(m_id);
  if (!job)
  {
    return;
  }
  pid_t pid = job->getJobPid();
  SmallShell::getInstance().setCurrForegroundPID(pid);
  std::cout << job->getCommand()->getCMDLine() << " " << pid << "\n";
  jobslist.removeJobById(m_id);
  if (waitpid(pid, nullptr, WUNTRACED) != 0) // options == 0 will wait for the process to finish
  {
    perror("smash error: waitpid failed");

    return;
  }

  SmallShell::getInstance().setCurrForegroundPID(-1);
}

// * BuiltInCommand 7 (QuitCommand)

QuitCommand::QuitCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "quit")
  {
    throw std::logic_error("QuitCommand::QuitCommand");
  }
}

QuitCommand::~QuitCommand()
{
  // default
}

void QuitCommand::execute()
{
  if (getArgs().size() > 0)
  {
    if (getArgs().front() == "kill")
    {
      JobsList jobs = SmallShell::getInstance().getJobsList();
      std::cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:\n";
      if(jobs.size() > 0)
      {
        jobs.killAllJobs();
      }
    }
    // else, if other arguments other than "kill" were provided they will be ignored
  }
  // exit the smash
  delete this;
  exit(0);
}

// * BuiltInCommand 8 (KillCommand)

KillCommand::KillCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "kill")
  {
    throw std::logic_error("KillCommand::KillCommand");
  }

  if (getArgs().size() != 2)
  {
    std::cerr << "smash error: kill: invalid arguments\n";
    throw std::logic_error("KillCommand::KillCommand");
  }

  try
  {
    // should remove the - before the signal number before std::stoi
    std::string signal_string = getArgs().front();
    if (!signal_string.empty() && signal_string.front() == '-')
    {
      signal_string.erase(0, 1); // Erase the first character
    }
    else // signal didn't have a dash before it
    {
      std::cerr << "smash error: kill: invalid arguments\n";
      throw std::logic_error("KillCommand::KillCommand");
    }

    m_signal_number = std::stoi(signal_string);
    m_job_id = std::stoi(getArgs().back());
  }
  catch (const std::exception &e)
  {
    std::cerr << "smash error: kill: invalid arguments\n";
    throw std::logic_error("KillCommand::KillCommand");
  }

  // check if the job id actually exists
  if (SmallShell::getInstance().getJobsList().getJobById(m_job_id) == nullptr)
  {
    std::cerr << "smash error: kill: job-id " << m_job_id << " does not exist\n";
    throw std::logic_error("KillCommand::KillCommand");
  }
}

KillCommand::~KillCommand()
{
  // default
}

void KillCommand::execute()
{
  JobsList &job_list = SmallShell::getInstance().getJobsList();
  JobsList::JobEntry* job = job_list.getJobById(m_job_id);
  if (job != nullptr)
  {
    if (kill(job->getJobPid(), m_signal_number) != 0) // failure
    {
      perror("smash error: kill failed");
      return;
    }
    std::cout << "signal number "<< m_signal_number <<" was sent to pid " << job->getJobPid() << "\n";
  }
}

/* *
 * The JobsList class
 */

/* The JobEntry class methods */
JobsList::JobEntry::JobEntry(Command *command, pid_t job_pid, int job_id)
    : m_command(command),
      m_job_pid(job_pid),
      m_job_id(job_id)
{
}

Command *JobsList::JobEntry::getCommand()
{
  return m_command;
}

pid_t JobsList::JobEntry::getJobPid()
{
  return m_job_pid;
}

int JobsList::JobEntry::getJobID()
{
  return m_job_id;
}

/* The JobList class methods */
int JobsList::size() const
{
  return m_jobs.size();
}

std::list<JobsList::JobEntry> &JobsList::getList()
{
  return m_jobs;
}

JobsList::JobsList()
{
  // default
}

JobsList::~JobsList()
{
  // default
}

// assumes a valid command
void JobsList::addJob(Command *cmd, pid_t pid)
{
  if (cmd && waitpid(pid, nullptr, WNOHANG) != -1)
  {
    getList().push_back(JobEntry(
        cmd,
        pid,
        getList().size() ? getLastJob()->getJobID() + 1 : 1 // if there is jobs (size is true) get the last job then add 1, else give it 1 as a job id
        ));
  }
}

void JobsList::printJobsList()
{
  for (auto &job : getList())
  {
    std::cout << "[" << job.getJobID() << "] " << job.getCommand()->getCMDLine() << "\n";
  }
}

void JobsList::killAllJobs()
{
  for (auto &job : getList())
  {
    std::cout << job.getJobPid() << ": " << job.getCommand()->getCMDLine() << "\n";
    if (kill(job.getJobPid(), SIGKILL) != 0) // failure
    {
      perror("smash error: kill failed");
      return;
    }
  }
  getList().clear();
}

void JobsList::removeFinishedJobs()
{
  std::vector<int> ids;

    for (auto &job : getList()){
      if (waitpid(job.getJobPid(), nullptr, WNOHANG) > 0)
      {
        ids.push_back(job.getJobID());
      }
    }

    for (int id : ids) 
    {
      removeJobById(id);
    }
    
}

JobsList::JobEntry *JobsList::getJobById(int jobId)
{
  for (auto &job : getList())
  {
    if (job.getJobID() == jobId)
    {
      return &job;
    }
  }
  return nullptr;
}

void JobsList::removeJobById(int jobId)
{
  for (std::list<JobEntry>::iterator it = getList().begin(); it != getList().end(); ++it)
  {
    if ((*it).getJobID() == jobId)
    {
      getList().erase(it);
      break;
    }
  }
}

JobsList::JobEntry *JobsList::getLastJob()
{
  return getList().size() ? &getList().back() : nullptr;
}



/* *
 * The Small Shell class
 */

// * SmallShell Public

// initialize the static variable in SmallShell
const std::string SmallShell::DEFAULT_PROMPT = "smash";

SmallShell::~SmallShell()
{
  // default
}

/**
 * Creates and returns a pointer to Command class which matches the given command line (cmd_line)
 */
Command *SmallShell::CreateCommand(const char *cmd_line)
{
  return CreateCommand_aux(cmd_line);
}

void SmallShell::executeCommand(const char *cmd_line)
{
  Command *cmd = CreateCommand(cmd_line);
  if (cmd)
  {
    try
    {
      cmd->execute();
    }
    catch (const std::exception &e)
    {
      //std::cerr << e.what() << '\n';
    }
  }
}

// * SmallShell Private

SmallShell::SmallShell()
    : m_prompt(DEFAULT_PROMPT),
      m_background_jobs(), // default c'tor (empty list)
      m_currForegroundPID(-1)
{
}

JobsList &SmallShell::getJobsList()
{
  // update the list before any operation on it
  m_background_jobs.removeFinishedJobs();
  // return the updated list
  return m_background_jobs;
}

const std::string &SmallShell::getPrompt() const
{
  return m_prompt;
}

void SmallShell::setPrompt(const std::string &newPrompt)
{
  // TODO piazza: any validity checks for the newPrompt?
  m_prompt = newPrompt;
}

Command *SmallShell::CreateCommand_aux(const char *cmd_line)
{
  if(*cmd_line == '\0')
  {
    return nullptr;
  }
  try
  {
    return new RedirectionCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    //std::cout << e.what() << '\n';
  }

  try
  {
    return new PipeCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    //std::cout << e.what() << '\n';
  }

  try
  {
    return new ChangePromptCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    //std::cout << e.what() << '\n';
  }

  try
  {
    return new ShowPidCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    //std::cout << e.what() << '\n';
  }

  try
  {
    return new GetCurrDirCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    //std::cout << e.what() << '\n';
  }

  try
  {
    return new JobsCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    //std::cout << e.what() << '\n';
  }

  try
  {
    return new ForegroundCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    //std::cout << e.what() << '\n';
  }

  try
  {
    return new QuitCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    //std::cout << e.what() << '\n';
  }

  try
  {
    return new KillCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    //std::cout << e.what() << '\n';
  }

  try
  {
    return new ChmodCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    //std::cout << e.what() << '\n';
  }

  try
  {
    return new ExternalCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    //std::cout << e.what() << '\n';
  }

  return nullptr;
}
