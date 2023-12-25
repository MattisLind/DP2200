#ifndef _COMMAND_WINDOW_
#define _COMMAND_WINDOW_

#include "Window.h"
#include <form.h>
#include <ncurses.h>
#include <string>
#include <cstring>
#include "dp2200_cpu_sim.h"

typedef enum { STRING, NUMBER, BOOL } Type;
typedef enum { DRIVE, FILENAME, ADDRESS, ENABLED, VALUE, TYPE, WRITEBACK, WRITEPROTECT } ParamId;
class commandWindow;
void printLog(const char *level, const char *fmt, ...);
extern float yield;

#define PARAM_VALUE_SIZE 256

typedef union p {
  char s[PARAM_VALUE_SIZE];
  bool b;
  int i;
} ParamValue;

typedef struct param {
  std::string paramName;
  ParamId paramId;
  Type type;
  ParamValue paramValue;
} Param;

typedef struct cmd {
  std::string command;
  std::string help;
  std::vector<Param> params;
  void (commandWindow::*func)(std::vector<Param>);
} Cmd;


class commandWindow : public virtual Window {
  int cursorX, cursorY;
  WINDOW *win, *innerWin;
  std::string commandLine;
  std::vector<Cmd> commands;
  dp2200_cpu *cpu;
  bool activeWindow;
  std::vector<int> test;
  std::vector<std::string> commandHistory;
  int commandHistoryIndex;
  
  void doHelp(std::vector<Param> params);

  void doStep(std::vector<Param> params);

  void doExit(std::vector<Param> params);


  void doLoadBoot(std::vector<Param> params);
  void doClear(std::vector<Param> params);
  void doRun(std::vector<Param> params);
  void doReset(std::vector<Param> params);
  void doRestart(std::vector<Param> params);
  void doHalt(std::vector<Param> params);
  void doAddBreakpoint(std::vector<Param> params);
  void doRemoveBreakpoint(std::vector<Param> params);
  void doDetach(std::vector<Param> params);
  void doAttach(std::vector<Param> params);
  void doTrace(std::vector<Param> params);
  void doNoTrace(std::vector<Param> params);
  void doYield(std::vector<Param> params);
  void doHex(std::vector<Param> params);
  void doOct(std::vector<Param> params);  
  void processCommand(char ch);

public:
  commandWindow(class dp2200_cpu * c);
  void hightlightWindow();
  void normalWindow();
  void handleKey(int ch);
  void resetCursor();
  void resize();
};


#endif