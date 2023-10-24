#include <ncurses.h>
#include <string>
#include <vector>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <ctype.h>
#include <algorithm>
#include "dp2200_cpu_sim.h"

void printLog (const char * level, const char * fmt,  ...);

class Window {
  public: 
  virtual void hightlightWindow() = 0;
  virtual void normalWindow() = 0;
  virtual void handleKey(int) = 0;
};

class dp2200Window : public virtual Window {
  int cursorX, cursorY;
  WINDOW *win;
  public:
  dp2200Window(class dp2200_cpu *) {
    cursorX=0;
    cursorY=0;
    win = newwin(14, 82, 0, 0);
    normalWindow();
    wrefresh(win);
  } 
  void hightlightWindow() {
    wattrset(win, A_STANDOUT);
    box(win, 0 , 0); 
    mvwprintw(win, 0, 1, "DATAPOINT 2200 SCREEN"); 
    wattrset(win, 0);
    wrefresh(win);
  }
  void normalWindow() {
    box(win, 0 , 0); 
    mvwprintw(win, 0, 1, "DATAPOINT 2200 SCREEN"); 
    wrefresh(win);  
  }
  void handleKey(int key) {
    
  }

};

class commandWindow;

typedef enum {STRING, NUMBER, BOOL} Type;
typedef enum {DRIVE, FILENAME} ParamId;

#define PARAM_VALUE_SIZE 32

typedef union p {
  char s[PARAM_VALUE_SIZE];
  bool b;
  int  i;
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

Cmd cmds[] = {{ "HELP", "Show help information"}, "STEP", "Step one instruction"};
class commandWindow : public virtual Window {
  int cursorX, cursorY;
  WINDOW *win, * innerWin;
  std::string commandLine;
  std::vector<Cmd> commands;
  void doHelp (std::vector<Param> params) {
    for (std::vector<Cmd>::const_iterator it=commands.begin(); it != commands.end(); it++) {
      wprintw(innerWin, "%s - %s\n", it->command.c_str(), it->help.c_str());
      printLog("INFO", "%s - %s\n", it->command.c_str(), it->help.c_str());
    }
  }

  void doStep (std::vector<Param> params) {
    wprintw(innerWin, "Do STEP\n"); 
  }

  void doExit(std::vector<Param> params) {
    endwin();
    exit(0);  
  }

  void doAttach (std::vector<Param> params) { 
    std::string fileName;
    int drive;
    for (auto it = params.begin(); it < params.end(); it++) {
      printLog("INFO", "paramName=%s paramType=%d paramId=%d\n", it->paramName.c_str(), it->type, it->paramId);
      switch(it->type) {
        case NUMBER:
        printLog("INFO", "type=NUMBER paramValue=%d\n", it->paramValue.i);
        break;
        case STRING:
        printLog("INFO", "type STRING paramValue=%s\n", it->paramValue.s);
        break;
        case BOOL:
        printLog("INFO", "type=BOOL paramValue=%s\n", it->paramValue.b?"TRUE":"FALSE");
        break;
      }
      if (it->paramId == FILENAME) {
        fileName = it->paramValue.s;
      }
      if (it->paramId == DRIVE) {
        drive = it->paramValue.i;
      }
    }
    wprintw (innerWin, "Attaching file %s to drive %d\n", fileName.c_str(), drive);
    
  }
  void doStop (std::vector<Param> params) {
    wprintw(innerWin, "Stopping!\n"); 
  }
  void processCommand(char ch) {
    std::vector<Cmd> filtered;
    std::vector<std::string> paramStrings;
    std::size_t found,prev;
    std::string commandWord, tmp;
    bool failed=false;

    // Trim end of string by removing any trailing spaces.
    found = commandLine.find_last_not_of(' ');
    if (found!=std::string::npos) {
      commandLine.erase(found+1);
    } 
    // chop up space delimited command word and params
    found=commandLine.find_first_of(" ");
    printLog("INFO", "found=%lu\n", found);
    if (found == std::string::npos) {
      commandWord=commandLine;
    } else {
      commandWord=commandLine.substr(0,found);
      prev = found + 1;
      found = commandLine.find_first_of(' ', found + 1);
      while (found != std::string::npos) {
        tmp = commandLine.substr(prev, found - prev);
        if (tmp.size()>0) {
          paramStrings.push_back(tmp);
        }
        prev = found + 1;
        found = commandLine.find_first_of(' ', found + 1);
      }
      tmp = commandLine.substr(prev);
      if (tmp.size()>0) {
        paramStrings.push_back(tmp);
      }
    }
    std::transform(commandWord.begin(), commandWord.end(), commandWord.begin(), ::toupper);

    for (std::vector<std::string>::const_iterator it=paramStrings.begin(); it != paramStrings.end();it++) {
      printLog("INFO", "paramString=%s**\n ", it->c_str());
    }
  
    printLog("INFO", "commandWord=%s commandLine=%s**\n", commandWord.c_str(), commandLine.c_str());
    for (std::vector<Cmd>::const_iterator it=commands.begin(); it != commands.end(); it++) {
      if (commandLine.size() == 0 || it->command.find(commandWord) == 0 ) {
        filtered.push_back(*it);
      }
    }
    if (ch == '?') {
      if (filtered.size()==1) {
        int y,x;
        getyx(innerWin, y, x);
        wmove(innerWin, y, 1);
        wprintw(innerWin, filtered[0].command.c_str());
        commandLine = filtered[0].command;
      } else if (filtered.size()>1) {
        wprintw(innerWin, "\n");
        for (std::vector<Cmd>::const_iterator it=filtered.begin(); it != filtered.end(); it++) {
          wprintw(innerWin, "%s ", it->command.c_str());
        }
        wprintw(innerWin, "\n>%s", commandLine.c_str());
      }
    } else if (ch == '\n') {
      if (filtered.size()==0) {
        wprintw(innerWin, "No command matching %s\n", commandLine.c_str());
      } else if (filtered.size()==1) {
        std::string paramName;
        std::vector<Param*> filteredParams;
        // process all parameters 
        for (auto it = paramStrings.begin(); it != paramStrings.end(); it++) {
          printLog("INFO", "paramStrings=%s\n", it->c_str());
          // split param at =
          found = it->find_first_of('=');
          auto s = it->substr(0,found);
          std::transform(s.begin(), s.end(), s.begin(), ::toupper);
          auto v = it->substr(found+1);
          printLog("INFO", "s=%s v=%s\n", s.c_str(), v.c_str());
          // Find matching in command params.
          for (auto i = filtered[0].params.begin(); i != filtered[0].params.end(); i++) {
            if (i->paramName.find(s) == 0) {
              filteredParams.push_back(&(*i));
              printLog("INFO", "Pushing onto filteredParams=%s\n", i->paramName.c_str());
            }
          }
          if (filteredParams.size()==0) {
            wprintw(innerWin, "Invalid parameter given: %s \n", s.c_str());
            failed=true;
          } else if (filteredParams.size() == 1) {
            // OK parse the value.
            if (filteredParams[0]->type==NUMBER) {
              filteredParams[0]->paramValue.i  = atoi(v.c_str());
              printLog("INFO", "number = %d\n", filteredParams[0]->paramValue.i);
            } else if (filteredParams[0]->type==STRING) {
              strncpy(filteredParams[0]->paramValue.s , v.c_str(), PARAM_VALUE_SIZE);
              printLog("INFO", "string=%s\n", filteredParams[0]->paramValue.s);
              filteredParams[0]->paramValue.s[PARAM_VALUE_SIZE-1]=0; 
            } else {
              if (v == "TRUE") {
                filteredParams[0]->paramValue.b=true;
              } else {
                filteredParams[0]->paramValue.b=false; 
              }
            }
          } else {
            // Ambiguous param given.
            failed=true;
            wprintw(innerWin, "Ambiguous parameter given: %s, can match", s.c_str());
            printLog("INFO", "Ambiguous parameter given: %s, can match\n", s.c_str());
            for (auto i=filteredParams.begin(); i< filteredParams.end(); i++) {
              wprintw(innerWin, "%s", (*i)->paramName.c_str());
              printLog("INFO", "%s\n", (*i)->paramName.c_str());
            }
            wprintw(innerWin, "\n");
          }
          filteredParams.clear();
        }
        if (!failed) ((*this).*(filtered[0].func))(filtered[0].params);
      } else if (filtered.size()>1) {
        wprintw(innerWin, "Ambiguous command given. Did you mean: ");
        for (std::vector<Cmd>::const_iterator it=filtered.begin(); it != filtered.end(); it++) {
          wprintw(innerWin, "%s ", it->command.c_str());
        }
        wprintw(innerWin, "\n");
      }  
    }

  }
  public:
  commandWindow(class dp2200_cpu *) {
    cursorX=0;
    cursorY=0;
    commands.push_back({ "HELP", "Show help information", {}, &commandWindow::doHelp});
    commands.push_back({ "STEP", "Step one instruction", {}, &commandWindow::doStep});
    commands.push_back({ "ATTACH", "Step one instruction", {{"DRIVE",DRIVE, NUMBER,{.i=0}}, {"FILENAME", FILENAME, STRING, {.s={'\0'}}}}, &commandWindow::doAttach});
    commands.push_back({ "STOP", "Stop execution", {}, &commandWindow::doStop});
    commands.push_back({ "EXIT", "Exit the simulator", {}, &commandWindow::doExit});
    win = newwin(LINES-14, 82, 14, 0);
    innerWin = newwin(LINES-16, 80, 15,1);
    normalWindow();
    scrollok(innerWin,TRUE);
    wmove(innerWin,0,0);
    waddch(innerWin, '>');
    wrefresh(innerWin);    
  } 
  void hightlightWindow() {
    wattrset(win, A_STANDOUT);
    box(win, 0 , 0); 
    //draw_borders(win);
    mvwprintw(win, 0, 1, "COMMAND WINDOW");
    wattrset(win, 0);
    wrefresh(win);
    redrawwin(innerWin);
    wrefresh(innerWin); 
    //wrefresh(win);
  }
  void normalWindow() {
    box(win, 0 , 0); 
    //draw_borders(win);
    mvwprintw(win, 0, 1, "COMMAND WINDOW");
    wrefresh(win);
    redrawwin(innerWin);
    wrefresh(innerWin);  
    //wrefresh(win); 
  }
  void handleKey(int ch) {
    printLog("INFO", "Got %c %02X\n", ch, ch);
    if (ch=='?') {
      processCommand(ch);
    } else if ((ch>=32) && (ch <127)) {
      waddch(innerWin, ch);
      commandLine += ch;
    } else if (ch==10) {
      waddch(innerWin,'\n');
      processCommand(ch);

      commandLine.clear();
      waddch(innerWin,'>');
    } else if (ch == 0x107 || ch == 127) {
      int y,x;
      getyx(innerWin, y, x);
      wmove(innerWin, y, x>1?x-1:x);
      wdelch(innerWin);
      commandLine.erase(commandLine.size()-1,1);
    }
    wrefresh(innerWin);
  }
};

class registerWindow : public virtual Window {
  int cursorX, cursorY;
  WINDOW *win;
  public:
  registerWindow(class dp2200_cpu *) {
    cursorX=0;
    cursorY=0;
    win = newwin(LINES, COLS-82, 0, 82);
    normalWindow();
    wrefresh(win);

  } 
  void hightlightWindow() {
    wattrset(win, A_STANDOUT);
    box(win, 0 , 0); 
    mvwprintw(win, 0, 1, "REGISTER WINDOW");
    wattrset(win, 0);
    wrefresh(win);
  }
  void normalWindow() {
    box(win, 0 , 0); 
    mvwprintw(win, 0, 1, "REGISTER WINDOW");
    wrefresh(win);  
  }
  void handleKey(int key) {
    
  }
};

int pollKeyboard(void);
struct callbackRecord {
  int (* cb) ();
  struct timespec deadline;
};
class Window * windows [3];
int activeWindow = 0;

std::vector<callbackRecord> timerqueue;

int pollKeyboard () {
  int ch;
  struct callbackRecord cbr;
  ch = getch();
  switch(ch) {
    case KEY_BTAB:
      windows[activeWindow++]->normalWindow();
      activeWindow = activeWindow>2?0:activeWindow;
      windows[activeWindow]->hightlightWindow(); 
      break;	
    case '\t':
      windows[activeWindow--]->normalWindow();
      activeWindow = activeWindow<0?2:activeWindow;
      windows[activeWindow]->hightlightWindow();
      break; 
    case KEY_RESIZE:    
      break;  
    case ERR:
      break;  
    default:
      windows[activeWindow]->handleKey(ch);
      break;
    
  }
  
  clock_gettime(CLOCK_MONOTONIC, &cbr.deadline);
  cbr.deadline.tv_nsec += 10000000; // 10 ms
  if(cbr.deadline.tv_nsec >= 1000000000) {
    cbr.deadline.tv_nsec -= 1000000000;
    cbr.deadline.tv_sec++;
  }
  cbr.cb = pollKeyboard;
  timerqueue.push_back(cbr);

  return 0;
}

FILE * logfile;

char timeBuf[sizeof "2011-10-08T07:07:09.000Z"];

void updateTimeBuf(){
  timeval curTime;
  gettimeofday(&curTime, NULL);
  int milli = curTime.tv_usec / 1000;
  char *p = timeBuf + strftime(timeBuf, sizeof timeBuf, "%FT%T", gmtime(&curTime.tv_sec));
  snprintf(p, 6,".%03dZ", milli);
}

void printLog (const char * level, const char * fmt,  ...) {
  char buffer[256];
  int charsPrinted;
  struct timeval tv;
  va_list args;
  va_start (args, fmt);
  updateTimeBuf();
  charsPrinted = snprintf(buffer, sizeof level + sizeof timeBuf + 2, "%s %s ", level, timeBuf);
  vsnprintf (buffer+charsPrinted,256-charsPrinted,fmt, args);
  fwrite(buffer, strlen(buffer), 1, logfile);
  va_end (args);
}

int main(int argc, char *argv[]) {	
  WINDOW *my_win, * win;
	int startx, starty, width, height;
  struct timespec now, timeout;
  class dp2200Window * dpw;
  class registerWindow * rw;
  class commandWindow * cw;
  class dp2200_cpu cpu;

  logfile = fopen("dp2200.log", "w");
  printLog("INFO", "Starting up %d\n", 10);
	initscr();			        /* Start curses mode 		*/
	raw();			            /* Line buffering disabled, Pass on everything to me 		*/
	keypad(stdscr, TRUE);		/* I need that nifty F1 	*/
  noecho();
  nodelay(stdscr, true);
  timeout(0);
  refresh();
  dpw = new dp2200Window(&cpu);
  rw = new registerWindow(&cpu);
  cw = new commandWindow(&cpu);
  windows[0] = cw;
  windows[1] = rw;
  windows[2] = dpw;
  windows[activeWindow]->hightlightWindow();
  pollKeyboard();
	while (1) { // event loop
    clock_gettime(CLOCK_MONOTONIC, &now);
    timeout.tv_nsec = timerqueue.front().deadline.tv_nsec -now.tv_nsec;
    timeout.tv_sec = timerqueue.front().deadline.tv_sec -now.tv_sec;
    if (timeout.tv_nsec < 0) {
      timeout.tv_nsec += 1000000000;
      timeout.tv_sec--;
    }  
    nanosleep(&timeout, NULL);
    timerqueue.front().cb();
    timerqueue.erase(timerqueue.begin());
   

  }		
	return 0;
}
