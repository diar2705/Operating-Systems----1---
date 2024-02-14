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

  if (pid == -1)
  {
    perror("smash error: fork failed");
    return;
  }

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
        perror("smash error: execvp failed");
      }
      for (int i = 0; i <= COMMAND_MAX_ARGS; i++)
      {
        if (args[i] != nullptr)
        {
          free(args[i]);
        }
      }
      exit(0);
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
    // std::cout << "index is " << index_first << "\n";
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
      m_command(),
      m_file_path()
{
  if (!_is_redirection_command(cmd_line))
  {
    throw std::logic_error("wrong name");
  }
  m_redirection_type = get_redirection_type(cmd_line);

  std::string cmd_str(cmd_line);
  m_command = _trim(cmd_str.substr(0, cmd_str.find_first_of(">")).c_str());

  m_file_path = _trim(cmd_str.substr(cmd_str.find_last_of(">") + 1));
}

RedirectionCommand::~RedirectionCommand()
{
}

void RedirectionCommand::execute()
{
  SmallShell &smash = SmallShell::getInstance();
  int new_fd;
  int dupped_fd = dup(1);
  close(1);
  if (m_redirection_type == RedirectionType::Override)
  {
    new_fd = open(m_file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0655);
    if (new_fd == -1)
    {
      perror("smash error: open failed"); ////////////
      dup2(dupped_fd, 1);
      close(dupped_fd);
      return;
    }
  }
  else if (m_redirection_type == RedirectionType::Append)
  {
    new_fd = open(m_file_path.c_str(), O_RDWR | O_CREAT | O_APPEND, 0655);
    if (new_fd == -1)
    {
      perror("smash error: open failed");
      dup2(dupped_fd, 1);
      close(dupped_fd);
      return;
    }
  }

  smash.executeCommand(m_command.c_str());

  if (close(new_fd) == -1)
  {
    perror("smash error: close failed");
    exit(0);
  }
  dup2(dupped_fd, 1);
  close(dupped_fd);
}

/*void RedirectionCommand::execute()
{
  // this command will not be tested with &
  {
    IN = 0,
    OUT = 1,
  };
  enum STANDARD

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
    int file_d = open(m_file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_d == -1)
    {
      perror("smash error: open failed");
      if (dup2(original_stdout, STANDARD::OUT) == -1)
      {
        perror("smash  error: dup2 failed");
        return;
      }
      return;
    }
    else if (file_d != STANDARD::OUT)
    {
      std::cout << "sadasda\n";
    }
  }
  else // (m_redirection_type == RedirectionType::Append)
  {
    // open a new/existing file for appending (its fd will be 1)
    // O_WRONLY: Open for writing only.
    // O_CREAT: Create file if it does not exist.
    // O_APPEND: Append data at the end of the file.
    // 0644: File permission bits (user: read+write, group: read, others: read).
    if (open(m_file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666) == -1)
    {
      perror("smash error: open failed");
      if (dup2(original_stdout, STANDARD::OUT) == -1)
      {
        perror("smash  error: dup2 failed");
        return;
      }
      return;
    }
  }

  // execute command
  SmallShell::getInstance().executeCommand(m_command.c_str());

  // Restore the original stdout
  if (dup2(original_stdout, STANDARD::OUT) == -1)
  {
    perror("smash  error: dup2 failed");
    return;
  }


  // program returned to the original state where stdout in the standard output stream.
}
*/

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
    throw std::logic_error("PipeCommand::PipeCommand");
  }
}

std::string PipeCommand::_get_cmd_1(const char *cmd_line)
{
  std::string s(cmd_line);
  return s.substr(0, s.find_first_of("|"));
}

std::string PipeCommand::_get_cmd_2(const char *cmd_line)
{

  std::string s(cmd_line);
  unsigned int index_of_line = s.find_first_of("|");
  unsigned int index_of_amp = s.find_first_of("&");

  if (index_of_amp != std::string::npos)
  {
    return s.substr(index_of_amp + 1, s.size() - index_of_amp);
  }
  std::stirng cmd2 = s.substr(index_of_line + 1, s.size() - index_of_line);
  std::cout << "cmd 2 " << cmd2 << "\n";
  return cmd2;
}

PipeCommand::PipeCommand(const char *cmd_line)
    : Command(cmd_line)
{
  if (_is_pipe_command(cmd_line))
  {
    m_pipe_type = _get_pipe_type(cmd_line);
    m_cmd_1 = _get_cmd_1(cmd_line);
    m_cmd_2 = _get_cmd_2(cmd_line);
    if (m_cmd_1 == "" || m_cmd_2 == "")
    {
      throw std::logic_error("PipeCommand::PipeCommand");
    }
  }
  else
  {
    throw std::logic_error("wrong name");
  }
}

PipeCommand::~PipeCommand()
{
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

  // smash instance
  SmallShell &smash = SmallShell::getInstance();

  // fork and execute the first command
  pid_t pid1 = fork();
  if (pid1 == -1)
  {
    perror("smash error: fork failed");
    close(files[0]);
    close(files[1]);
    return;
  }

  if (pid1 == 0)
  {
    setpgrp();
    if (m_pipe_type == PipeType::Standard)
    {
      dup2(files[1], STDOUT_FILENO);
    }
    else //(m_pipe_type == PipeType::Error)
    {
      dup2(files[1], STDERR_FILENO);
    }

    close(files[0]);
    close(files[1]);
    smash.executeCommand(m_cmd_1.c_str());
    exit(0);
  }
  else
  {
    if (waitpid(pid1, nullptr, WUNTRACED) == -1)
    {
      perror("smash error: waitpid failed");
      return;
    }
  }

  // fork and execute the second command

  pid_t pid2 = fork();
  if (pid2 == -1)
  {
    perror("smash error: fork failed");
  }

  if (pid2 == 0)
  {
    setpgrp();
    dup2(files[0], STDIN_FILENO);
    close(files[0]);
    close(files[1]);
    smash.executeCommand(m_cmd_2.c_str());
    exit(0);
  }
  else
  {
    if (waitpid(pid2, nullptr, WUNTRACED) == -1)
    {
      perror("smash error: waitpid failed");
      return;
    }
  }

  close(files[0]);
  close(files[1]);

  /*

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
    SmallShell::getInstance().executeCommand(m_cmd_1);

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
    SmallShell::getInstance().executeCommand(m_cmd_2);

    // return back the original read stream
    if (dup2(original_read, files[PIPE::READ]))
    {
      perror("smash error: dup2 failed");
      return;
    }*/
}

// * Special Commands 3 (ChmodCommand) , actually inherits from BuiltInCommand

ChmodCommand::ChmodCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "chmod")
  {
    throw std::logic_error("wrong name");
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
  setGround(GroundType::Foreground);
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
    throw std::logic_error("wrong name");
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
    throw std::logic_error("wrong name");
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
    throw std::logic_error("wrong name");
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
std::string ChangeDirCommand::CD_PATH_HISTORY; // default c'tor will be called

ChangeDirCommand::ChangeDirCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "cd")
  {
    throw std::logic_error("wrong name");
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
  if ("-" == getArgs().front())
  {
    if (CD_PATH_HISTORY.size() == 0)
    {
      std::cerr << "smash error: cd: OLDPWD not set\n";
      throw std::logic_error("ChangeDirCommand::ChangeDirCommand");
    }
    else
    {
      // CD_PATH_HISTORY has paths in it
      if (chdir(CD_PATH_HISTORY.c_str()) == -1) // failure
      {
        perror("smash error: chdir failed");
        return;
      }
      else
      {
        CD_PATH_HISTORY = curr_dir;
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
      CD_PATH_HISTORY = curr_dir;
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
      CD_PATH_HISTORY = curr_dir;
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
    throw std::logic_error("wrong name");
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
    throw std::logic_error("wrong name");
  }
  JobsList &jobslist = SmallShell::getInstance().getJobsList();

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
  JobsList &jobslist = SmallShell::getInstance().getJobsList();
  JobsList::JobEntry *job = jobslist.getJobById(m_id);
  if (!job)
  {
    jobslist.removeJobById(m_id);
    return;
  }
  pid_t pid = job->getJobPid();
  SmallShell::getInstance().setCurrForegroundPID(pid);
  std::cout << job->getCMDLine() << " " << pid << "\n";
  // job->getCommand()->setGround(GroundType::Foreground);
  jobslist.removeJobById(m_id);
  if (waitpid(pid, nullptr, WUNTRACED) == -1) // options == 0 will wait for the process to finish
  {
    perror("smash error: waitpid failed");
  }
  SmallShell::getInstance().setCurrForegroundPID(-1);
}

// * BuiltInCommand 7 (QuitCommand)

QuitCommand::QuitCommand(const char *cmd_line)
    : BuiltInCommand(cmd_line)
{
  if (getName() != "quit")
  {
    throw std::logic_error("wrong name");
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
      JobsList &jobs = SmallShell::getInstance().getJobsList();
      std::cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:\n";
      if (jobs.size() > 0)
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
    throw std::logic_error("wrong name");
  }

  try
  {
    m_job_id = std::stoi(getArgs().at(1));
    // check if the job id actually exists
    if (SmallShell::getInstance().getJobsList().getJobById(m_job_id) == nullptr)
    {
      std::cerr << "smash error: kill: job-id " << m_job_id << " does not exist\n";
      throw std::logic_error("KillCommand::KillCommand");
    }

    if (getArgs().size() != 2)
    {
      std::cerr << "smash error: kill: invalid arguments\n";
      throw std::logic_error("KillCommand::KillCommand");
    }

    // should remove the - before the signal number before std::stoi
    m_signal_number = std::stoi(getArgs().front()); // kill -9 3 this will give -9 (int)
    if (m_signal_number > 0)
    {
      std::cerr << "smash error: kill: invalid arguments\n";
      throw std::logic_error("KillCommand::KillCommand");
    }
    m_signal_number = -m_signal_number; // this will make the -9 to 9
  }
  catch (const std::invalid_argument &e)
  {
    std::cerr << "smash error: kill: invalid arguments\n";
    throw std::logic_error("KillCommand::KillCommand");
  }
  catch (const std::out_of_range &e)
  {
    std::cerr << "smash error: kill: invalid arguments\n";
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
  JobsList::JobEntry *job = job_list.getJobById(m_job_id);
  if (job != nullptr) // job exists
  {
    if (kill(job->getJobPid(), m_signal_number) != 0) // failure
    {
      perror("smash error: kill failed");
      return;
    }
    std::cout << "signal number " << m_signal_number << " was sent to pid " << job->getJobPid() << "\n";
  }
}

/* *
 * The JobsList class
 */

/* The JobEntry class methods */
JobsList::JobEntry::JobEntry(const std::string &cmd, pid_t job_pid, int job_id)
    : m_command(cmd),
      m_job_pid(job_pid),
      m_job_id(job_id)
{
}

JobsList::JobEntry::~JobEntry()
{
}

const std::string &JobsList::JobEntry::getCMDLine()
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
  if (cmd)
  {
    m_jobs.push_back(JobEntry(cmd->getCMDLine(), pid, (m_jobs.size() ? getLastJob()->getJobID() + 1 : 1)));
  }
}

void JobsList::printJobsList()
{
  for (JobsList::JobEntry &job : m_jobs)
  {
    std::cout << "[" << job.getJobID() << "] " << job.getCMDLine() << "\n";
  }
}

void JobsList::killAllJobs()
{
  for (JobsList::JobEntry &job : m_jobs)
  {
    std::cout << job.getJobPid() << ": " << job.getCMDLine() << "\n";

    if (kill(job.getJobPid(), SIGKILL) != 0) // failure
    {
      perror("smash error: kill failed");
    }
  }
  m_jobs.clear();
}

void JobsList::removeFinishedJobs()
{
  std::vector<int> ids;

  for (JobsList::JobEntry &job : m_jobs)
  {
    // TODO what happens when its zombie?
    int res = waitpid(job.getJobPid(), nullptr, WNOHANG);
    if (res == -1 || res == job.getJobPid())
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
  for (JobsList::JobEntry &job : m_jobs)
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
  for (std::vector<JobEntry>::iterator it = m_jobs.begin(); it != m_jobs.end(); ++it)
  {
    if ((*it).getJobID() == jobId)
    {
      m_jobs.erase(it);
      return;
    }
  }
}

JobsList::JobEntry *JobsList::getLastJob()
{
  return m_jobs.size() ? &m_jobs.back() : nullptr;
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
  m_background_jobs.removeFinishedJobs();
  Command *cmd = CreateCommand(cmd_line);
  if (cmd)
  {
    try
    {
      cmd->execute();
    }
    catch (const std::exception &e)
    {
      // std::cerr << e.what() << '\n';
    }
    delete cmd;
  }
  setCurrForegroundPID(-1);
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
  if (*cmd_line == '\0')
  {
    return nullptr;
  }
  try
  {
    return new RedirectionCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    if (std::string(e.what()) == "RedirectionCommand::RedirectionCommand")
    {
      return nullptr;
    }
  }

  try
  {
    return new PipeCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    if (std::string(e.what()) == "PipeCommand::PipeCommand")
    {
      return nullptr;
    }
  }

  try
  {
    return new ChangePromptCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    if (std::string(e.what()) == "ChangePromptCommand::ChangePromptCommand")
    {
      return nullptr;
    }
  }

  try
  {
    return new ShowPidCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    if (std::string(e.what()) == "ShowPidCommand::ShowPidCommand")
    {
      return nullptr;
    }
  }

  try
  {
    return new GetCurrDirCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    if (std::string(e.what()) == "GetCurrDirCommand::GetCurrDirCommand")
    {
      return nullptr;
    }
  }

  try
  {
    return new ChangeDirCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    if (std::string(e.what()) == "ChangeDirCommand::ChangeDirCommand")
    {
      return nullptr;
    }
  }

  try
  {
    return new JobsCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    if (std::string(e.what()) == "JobsCommand::JobsCommand")
    {
      return nullptr;
    }
  }

  try
  {
    return new ForegroundCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    if (std::string(e.what()) == "ForegroundCommand::ForegroundCommand")
    {
      return nullptr;
    }
  }

  try
  {
    return new QuitCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    if (std::string(e.what()) == "QuitCommand::QuitCommand")
    {
      return nullptr;
    }
  }

  try
  {
    return new KillCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    if (std::string(e.what()) == "KillCommand::KillCommand")
    {
      return nullptr;
    }
  }

  try
  {
    return new ChmodCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    if (std::string(e.what()) == "ChmodCommand::ChmodCommand")
    {
      return nullptr;
    }
  }

  try
  {
    return new ExternalCommand(cmd_line);
  }
  catch (const std::exception &e)
  {
    // std::cout << e.what() << '\n';
  }

  return nullptr;
}
