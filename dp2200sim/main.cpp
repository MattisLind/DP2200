#include "cassetteTape.h"
#include "dp2200_cpu_sim.h"
#include <algorithm>
#include <assert.h>
#include <ctype.h>
#include <form.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/time.h>
#include <time.h>
#include <vector>

void printLog(const char *level, const char *fmt, ...);

FILE *logfile;



class Window {
public:
  virtual void hightlightWindow() = 0;
  virtual void normalWindow() = 0;
  virtual void handleKey(int) = 0;
  virtual void resetCursor() = 0;
};

class dp2200Window : public virtual Window {
  int cursorX, cursorY;
  WINDOW *win;
  bool activeWindow;

public:
  dp2200Window(class dp2200_cpu *) {
    cursorX = 1;
    cursorY = 1;
    win = newwin(14, 82, 0, 0);
    normalWindow();
    wrefresh(win);
    activeWindow = false;
  }
  void hightlightWindow() {
    wattrset(win, A_STANDOUT);
    box(win, 0, 0);
    mvwprintw(win, 0, 1, "DATAPOINT 2200 SCREEN");
    wattrset(win, 0);
    wmove(win, cursorY, cursorX);
    wrefresh(win);
    activeWindow = true;
  }
  void normalWindow() {
    curs_set(0);
    box(win, 0, 0);
    mvwprintw(win, 0, 1, "DATAPOINT 2200 SCREEN");
    wrefresh(win);
    activeWindow = false;
  }
  void handleKey(int key) {}
  void resetCursor() {
    if (activeWindow) {
      curs_set(2);
      wmove(win, cursorY, cursorX);
      wrefresh(win);
    }
  }
};

class commandWindow;

typedef enum { STRING, NUMBER, BOOL } Type;
typedef enum { DRIVE, FILENAME } ParamId;

#define PARAM_VALUE_SIZE 32

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

Cmd cmds[] = {
    {"HELP", "Show help information"}, "STEP", "Step one instruction"};
class commandWindow : public virtual Window {
  int cursorX, cursorY;
  WINDOW *win, *innerWin;
  std::string commandLine;
  std::vector<Cmd> commands;
  dp2200_cpu *cpu;
  bool activeWindow;
  void doHelp(std::vector<Param> params) {
    for (std::vector<Cmd>::const_iterator it = commands.begin();
         it != commands.end(); it++) {
      wprintw(innerWin, "%s - %s\n", it->command.c_str(), it->help.c_str());
      printLog("INFO", "%s - %s\n", it->command.c_str(), it->help.c_str());
    }
  }

  void doStep(std::vector<Param> params) { wprintw(innerWin, "Do STEP\n"); }

  void doExit(std::vector<Param> params) {
    endwin();
    exit(0);
  }


  void doLoadBoot(std::vector<Param> params) {
    cpu->tapeDrive[0]->loadBoot(cpu->memory);
  }
  void doClear(std::vector<Param> params) {
    cpu->clear();
  }
  void doRun(std::vector<Param> params) {}
  void doReset(std::vector<Param> params) {
    cpu->reset();
  }
  void doRestart(std::vector<Param> params) {}
  void doHalt(std::vector<Param> params) {}
  void doDetach(std::vector<Param> params) {
    int drive;
    for (auto it = params.begin(); it < params.end(); it++) {
      if (it->paramId == DRIVE) {
        drive = it->paramValue.i;
      }
    }
    cpu->tapeDrive[drive]->closeFile();
    wprintw(innerWin, "Detaching file %s to drive %d\n",
            cpu->tapeDrive[drive]->getFileName().c_str(), drive);
  }

  void doAttach(std::vector<Param> params) {
    std::string fileName;
    int drive;
    FILE *tmp;
    for (auto it = params.begin(); it < params.end(); it++) {
      printLog("INFO", "paramName=%s paramType=%d paramId=%d\n",
               it->paramName.c_str(), it->type, it->paramId);
      switch (it->type) {
      case NUMBER:
        printLog("INFO", "type=NUMBER paramValue=%d\n", it->paramValue.i);
        break;
      case STRING:
        printLog("INFO", "type STRING paramValue=%s\n", it->paramValue.s);
        break;
      case BOOL:
        printLog("INFO", "type=BOOL paramValue=%s\n",
                 it->paramValue.b ? "TRUE" : "FALSE");
        break;
      }
      if (it->paramId == FILENAME) {
        fileName = it->paramValue.s;
      }
      if (it->paramId == DRIVE) {
        drive = it->paramValue.i;
      }
    }

    if (cpu->tapeDrive[0]->openFile(fileName)) {
      wprintw(innerWin, "Attaching file %s to drive %d\n", fileName.c_str(),drive);
    } else {
      wprintw(innerWin, "Failed to open file %s\n", fileName.c_str());      
    }
  }
  void doStop(std::vector<Param> params) { wprintw(innerWin, "Stopping!\n"); }
  void processCommand(char ch) {
    std::vector<Cmd> filtered;
    std::vector<std::string> paramStrings;
    std::size_t found, prev;
    std::string commandWord, tmp;
    bool failed = false;

    // Trim end of string by removing any trailing spaces.
    found = commandLine.find_last_not_of(' ');
    if (found != std::string::npos) {
      commandLine.erase(found + 1);
    }
    // chop up space delimited command word and params
    found = commandLine.find_first_of(" ");
    printLog("INFO", "found=%lu\n", found);
    if (found == std::string::npos) {
      commandWord = commandLine;
    } else {
      commandWord = commandLine.substr(0, found);
      prev = found + 1;
      found = commandLine.find_first_of(' ', found + 1);
      while (found != std::string::npos) {
        tmp = commandLine.substr(prev, found - prev);
        if (tmp.size() > 0) {
          paramStrings.push_back(tmp);
        }
        prev = found + 1;
        found = commandLine.find_first_of(' ', found + 1);
      }
      tmp = commandLine.substr(prev);
      if (tmp.size() > 0) {
        paramStrings.push_back(tmp);
      }
    }
    std::transform(commandWord.begin(), commandWord.end(), commandWord.begin(),
                   ::toupper);

    for (std::vector<std::string>::const_iterator it = paramStrings.begin();
         it != paramStrings.end(); it++) {
      printLog("INFO", "paramString=%s**\n ", it->c_str());
    }

    printLog("INFO", "commandWord=%s commandLine=%s**\n", commandWord.c_str(),
             commandLine.c_str());
    for (std::vector<Cmd>::const_iterator it = commands.begin();
         it != commands.end(); it++) {
      if (commandLine.size() == 0 || it->command.find(commandWord) == 0) {
        filtered.push_back(*it);
      }
    }
    if (ch == '?') {
      if (filtered.size() == 1) {
        int y, x;
        getyx(innerWin, y, x);
        wmove(innerWin, y, 1);
        wprintw(innerWin, filtered[0].command.c_str());
        commandLine = filtered[0].command;
      } else if (filtered.size() > 1) {
        wprintw(innerWin, "\n");
        for (std::vector<Cmd>::const_iterator it = filtered.begin();
             it != filtered.end(); it++) {
          wprintw(innerWin, "%s ", it->command.c_str());
        }
        wprintw(innerWin, "\n>%s", commandLine.c_str());
      }
    } else if (ch == '\n') {
      if (filtered.size() == 0) {
        wprintw(innerWin, "No command matching %s\n", commandLine.c_str());
      } else if (filtered.size() == 1) {
        std::string paramName;
        std::vector<Param *> filteredParams;
        // process all parameters
        for (auto it = paramStrings.begin(); it != paramStrings.end(); it++) {
          printLog("INFO", "paramStrings=%s\n", it->c_str());
          // split param at =
          found = it->find_first_of('=');
          auto s = it->substr(0, found);
          std::transform(s.begin(), s.end(), s.begin(), ::toupper);
          auto v = it->substr(found + 1);
          printLog("INFO", "s=%s v=%s\n", s.c_str(), v.c_str());
          // Find matching in command params.
          for (auto i = filtered[0].params.begin();
               i != filtered[0].params.end(); i++) {
            if (i->paramName.find(s) == 0) {
              filteredParams.push_back(&(*i));
              printLog("INFO", "Pushing onto filteredParams=%s\n",
                       i->paramName.c_str());
            }
          }
          if (filteredParams.size() == 0) {
            wprintw(innerWin, "Invalid parameter given: %s \n", s.c_str());
            failed = true;
          } else if (filteredParams.size() == 1) {
            // OK parse the value.
            if (filteredParams[0]->type == NUMBER) {
              filteredParams[0]->paramValue.i = atoi(v.c_str());
              printLog("INFO", "number = %d\n",
                       filteredParams[0]->paramValue.i);
            } else if (filteredParams[0]->type == STRING) {
              strncpy(filteredParams[0]->paramValue.s, v.c_str(),
                      PARAM_VALUE_SIZE);
              printLog("INFO", "string=%s\n", filteredParams[0]->paramValue.s);
              filteredParams[0]->paramValue.s[PARAM_VALUE_SIZE - 1] = 0;
            } else {
              if (v == "TRUE") {
                filteredParams[0]->paramValue.b = true;
              } else {
                filteredParams[0]->paramValue.b = false;
              }
            }
          } else {
            // Ambiguous param given.
            failed = true;
            wprintw(innerWin, "Ambiguous parameter given: %s, can match",
                    s.c_str());
            printLog("INFO", "Ambiguous parameter given: %s, can match\n",
                     s.c_str());
            for (auto i = filteredParams.begin(); i < filteredParams.end();
                 i++) {
              wprintw(innerWin, "%s", (*i)->paramName.c_str());
              printLog("INFO", "%s\n", (*i)->paramName.c_str());
            }
            wprintw(innerWin, "\n");
          }
          filteredParams.clear();
        }
        if (!failed)
          ((*this).*(filtered[0].func))(filtered[0].params);
      } else if (filtered.size() > 1) {
        wprintw(innerWin, "Ambiguous command given. Did you mean: ");
        for (std::vector<Cmd>::const_iterator it = filtered.begin();
             it != filtered.end(); it++) {
          wprintw(innerWin, "%s ", it->command.c_str());
        }
        wprintw(innerWin, "\n");
      }
    }
  }

public:
  commandWindow(class dp2200_cpu * c) {
    cursorX = 1;
    cursorY = 0;
    cpu = c;
    activeWindow = false;
    commands.push_back(
        {"HELP", "Show help information", {}, &commandWindow::doHelp});
    commands.push_back(
        {"STEP", "Step one instruction", {}, &commandWindow::doStep});
    commands.push_back({"ATTACH",
                        "Attach file to cassette drive",
                        {{"DRIVE", DRIVE, NUMBER, {.i = 0}},
                         {"FILENAME", FILENAME, STRING, {.s = {'\0'}}}},
                        &commandWindow::doAttach});
    commands.push_back({"DETACH",
                        "Detach file from cassette drive",
                        {{"DRIVE", DRIVE, NUMBER, {.i = 0}}},
                        &commandWindow::doDetach});
    commands.push_back({"STOP", "Stop execution", {}, &commandWindow::doStop});
    commands.push_back(
        {"EXIT", "Exit the simulator", {}, &commandWindow::doExit});
    commands.push_back({"LOADBOOT",
                        "Load the bootstrap from cassette into memory",
                        {},
                        &commandWindow::doLoadBoot});
    commands.push_back({"RESTART",
                        "Load bootstrap and restart CPU",
                        {},
                        &commandWindow::doRestart});
    commands.push_back({"RESET", "Reset the CPU", {}, &commandWindow::doReset});
    commands.push_back({"STOP", "Stop the CPU", {}, &commandWindow::doHalt});
    commands.push_back(
        {"RUN", "Run CPU from current location", {}, &commandWindow::doRun});
    commands.push_back(
        {"CLEAR", "Clear memory", {}, &commandWindow::doClear});    
    win = newwin(LINES - 14, 82, 14, 0);
    innerWin = newwin(LINES - 16, 80, 15, 1);
    normalWindow();
    scrollok(innerWin, TRUE);
    wmove(innerWin, 0, 0);
    waddch(innerWin, '>');
    wrefresh(innerWin);
  }
  void hightlightWindow() {
    curs_set(1);
    wattrset(win, A_STANDOUT);
    box(win, 0, 0);
    // draw_borders(win);
    mvwprintw(win, 0, 1, "COMMAND WINDOW");
    wmove(innerWin, cursorY, cursorX);
    wattrset(win, 0);
    wrefresh(win);
    redrawwin(innerWin);
    wrefresh(innerWin);
    // wrefresh(win);
    activeWindow = true;
  }
  void normalWindow() {
    // getyx(innerWin,cursorY,cursorX);
    // curs_set(0);
    box(win, 0, 0);
    // draw_borders(win);
    mvwprintw(win, 0, 1, "COMMAND WINDOW");
    wrefresh(win);
    redrawwin(innerWin);
    wrefresh(innerWin);
    activeWindow = false;
    // wrefresh(win);
  }
  void handleKey(int ch) {
    printLog("INFO", "Got %c %02X\n", ch, ch);
    if (ch == '?') {
      processCommand(ch);
    } else if ((ch >= 32) && (ch < 127)) {
      waddch(innerWin, ch);
      commandLine += ch;
    } else if (ch == 10) {
      waddch(innerWin, '\n');
      processCommand(ch);

      commandLine.clear();
      waddch(innerWin, '>');
    } else if (ch == 0x107 || ch == 127) {
      int y, x;
      getyx(innerWin, y, x);
      wmove(innerWin, y, x > 1 ? x - 1 : x);
      wdelch(innerWin);
      commandLine.erase(commandLine.size() - 1, 1);
    }
    wrefresh(innerWin);
    getyx(innerWin, cursorY, cursorX);
  }
  void resetCursor() {
    if (activeWindow) {
      curs_set(2);
      wmove(innerWin, cursorY, cursorX);
      wrefresh(innerWin);
    }
  }
};

void form_hook_proxy(formnode *);


class hookExecutor {
  public:
  virtual void exec(FIELD *field) = 0;  
};

class registerWindow : public virtual Window {
  int cursorX, cursorY;
  WINDOW *win;
  dp2200_cpu *cpu;
  bool octal;
  bool activeWindow;
  FORM *form;


  int base = 0;
  typedef struct m {
    int data;
    int address;
    int ascii;
  } M;
  std::vector<FIELD *> fields;
  std::vector<FIELD *> addressFields;
  std::vector<FIELD *> dataFields;
  std::vector<M> indexToAddress;
  std::vector<FIELD *> asciiFields;
  std::vector<FIELD *> registerViewFields;
  FIELD * regs[2][7];
  FIELD * stack[16];
  FIELD * flagParity[2];
  FIELD * flagSign[2];
  FIELD * flagCarry[2];
  FIELD * flagZero[2];
  FIELD * pc;
  FIELD * interruptEnabled;
  FIELD * interruptPending; 

  class memoryDataHookExecutor : hookExecutor {
    int data;
    class registerWindow * rw;
    public:
    memoryDataHookExecutor(class registerWindow * r, int d) {
      data=d;
      rw = r;
    }
    void exec (FIELD *field);
  };

  class memoryAddressHookExecutor : hookExecutor {
    int address;
    class registerWindow * rw;
    public:
    memoryAddressHookExecutor(class registerWindow * r, int a) { 
      address = a; 
      rw = r;
    }
    void exec(FIELD *field);
  };

  class charPointerHookExecutor : hookExecutor {
    unsigned char *  address;
    class registerWindow * rw;
    public:
    charPointerHookExecutor(class registerWindow * r, unsigned char * a) { 
      address = a; 
      rw = r;
    }
    void exec(FIELD *field);
  };

   class shortPointerHookExecutor : hookExecutor {
    unsigned short *  address;
    class registerWindow * rw;
    public:
    shortPointerHookExecutor(class registerWindow * r, unsigned short * a) { 
      address = a; 
      rw = r;
    }
    void exec(FIELD *field);
  };

  void updateForm(int startAddress) {
    int i, k = 0, j;
    char b[5];
    unsigned char t;
    char asciiB[17], fieldB[19];
    for (i = 0; i < 16 && startAddress < 0x4000; startAddress += 16, i++) {
      snprintf(b, 5, "%04X", startAddress);
      set_field_buffer(addressFields[i], 0, b);
      for (j = 0; j < 16; j++) {
        t = cpu->memory[startAddress + j];
        snprintf(b, 3, "%02X", t);
        set_field_buffer(dataFields[k], 0, b);
        k++;
        asciiB[j] = (t >= 0x20 && t < 127) ? t : '.';
      }
      asciiB[j] = 0;
      snprintf(fieldB, 19, "|%s|", asciiB);
      set_field_buffer(asciiFields[i], 0, fieldB);
    }
    for (; i < 16; startAddress += 16, i++) {
      snprintf(b, 5, "%04X", startAddress);
      set_field_buffer(addressFields[i], 0, b);
      for (int j = 0; j < 16; j++) {
        t = cpu->memory[startAddress + j];
        snprintf(b, 3, "%02X", t);
        set_field_buffer(dataFields[k], 0, b);
        k++;
        asciiB[j] = (t >= 0x20 && t < 127) ? t : '.';
      }
      asciiB[j] = 0;
      snprintf(fieldB, 19, "|%s|", asciiB);
      set_field_buffer(asciiFields[i], 0, fieldB);
    }
    // update registers.
    for (auto regset =0; regset<2; regset++) {
      for (auto i=0; i<7; i++) {
        auto f = regs[regset][i];
        auto r = cpu->regSets[regset].regs[i];
        snprintf(b, 3, "%02X", r);
        set_field_buffer(f, 0, b);
      }
      snprintf(b, 2, "%01X", cpu->flagCarry[regset]);
      set_field_buffer(flagCarry[regset], 0, b);
      snprintf(b, 2, "%01X", cpu->flagZero[regset]);
      set_field_buffer(flagZero[regset], 0, b);
      snprintf(b, 2, "%01X", cpu->flagParity[regset]);
      set_field_buffer(flagParity[regset], 0, b);
      snprintf(b, 2, "%01X", cpu->flagSign[regset]);
      set_field_buffer(flagSign[regset], 0, b);                  
    }
    snprintf(b, 5, "%04X", cpu->P);
    set_field_buffer(pc, 0, b);
  }
public:

  FIELD * createAField(int length, int y, int x, const char * str) {
    FIELD * t;
    t = new_field(1, length, y, x, 0, 0); // HEADER - REGISTERS TEXT
    registerViewFields.push_back(t);
    set_field_buffer(t, 0, str);
    set_field_opts(t, O_VISIBLE | O_PUBLIC  | O_STATIC);
    return t;
  }

  FIELD * createAField(int length, int y, int x, const char * str, Field_Options f, const char * regexp, int just, char * h) {
    FIELD * t = createAField(length, y, x, str);
    set_field_opts(t, O_VISIBLE | O_PUBLIC  | O_STATIC | f);
    set_field_userptr(t, h);
    if (regexp != NULL) set_field_type(t, TYPE_REGEXP, regexp);
    return t;
  }

  registerWindow(class dp2200_cpu *c) {
    cursorX = 4;  
    cursorY = 1;
    win = newwin(LINES, COLS - 82, 0, 82);
    normalWindow();
    wrefresh(win);
    cpu = c;
    octal = false;
    activeWindow = false;
    int ch;
    FIELD **f;
    int i;
    int offset=19;
    
    
    FIELD * t;
    int j = 0;
    const char * rName[]={"A:","B:","C:","D:","E:","H:","L:"};

    // Registers.
    createAField(10,2,6, "REGISTERS");
    createAField(20,3,3,"ALPHA          BETA");

    for (auto regset=0;regset < 2; regset++) {
      for (auto reg=0; reg<7; reg++) {
        createAField(2,4+reg, 3+15*regset,rName[reg]);
        regs[regset][reg]=createAField(2,4+reg, 5+15*regset, "00", O_EDIT | O_ACTIVE, "[0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new charPointerHookExecutor(this, &cpu->regSets[regset].regs[reg]));
      }
      // flags
      createAField(2, 12,3+15*regset, "P:" );
      flagParity[regset]=createAField(1,12,5+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu->flagParity[regset]));
      createAField(2, 12,8+15*regset, "S:" );
      flagSign[regset]=createAField(1,12,10+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu->flagSign[regset]));
      createAField(2, 13,3+15*regset, "C:" );
      flagCarry[regset]=createAField(1,13,5+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu->flagCarry[regset]));
      createAField(2, 13,8+15*regset, "Z:" );
      flagZero[regset]=createAField(1,13,10+15*regset, "0", O_EDIT | O_ACTIVE, "[0-1]", NO_JUSTIFICATION, (char *) new charPointerHookExecutor(this, &cpu->flagZero[regset]));
    }

    createAField(2, 15,3, "P:" );
    pc=createAField(4,15,5, "0000", O_EDIT | O_ACTIVE, "[0-3][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]", JUSTIFY_LEFT, (char *) new shortPointerHookExecutor(this, &cpu->P));
    
    for (auto line = 0; line < 16; line++) {
      // Add one address field
      auto tmp = new_field(1, 4, line+offset, 3, 0, 0);
      fields.push_back(tmp);
      set_field_userptr(tmp, (char *) new memoryAddressHookExecutor(this, line));
      addressFields.push_back(tmp);
      for (auto column = 0; column < 16; column++) {
        tmp = new_field(1, 2, line+offset, 8 + 3 * column, 0, 0);
        set_field_userptr(tmp, (char *) new memoryDataHookExecutor(this, line * 16 + column));
        fields.push_back(tmp);
        dataFields.push_back(tmp);
      }
      tmp = new_field(1, 18, line+offset, 57, 0, 0);
      asciiFields.push_back(tmp);
      fields.push_back(tmp);
    }

    for (auto it = addressFields.begin(); it < addressFields.end(); it++) {
      set_field_buffer(*it, 0, "0000");
      set_field_opts(*it, O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE | O_STATIC);
      set_field_type(*it, TYPE_REGEXP,
                     "[0-3][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]");
      set_field_just(*it, JUSTIFY_LEFT);
    }

    for (auto it = dataFields.begin(); it < dataFields.end(); it++) {
      set_field_buffer(*it, 0, "00");
      set_field_opts(*it, O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE | O_STATIC);
      set_field_type(*it, TYPE_REGEXP, "[0-9A-Fa-f][0-9A-Fa-f]");
      set_field_just(*it, JUSTIFY_LEFT);
    }

    for (auto it = asciiFields.begin(); it < asciiFields.end(); it++) {
      set_field_buffer(*it, 0, "|................|");
      set_field_opts(*it, O_VISIBLE | O_PUBLIC | O_STATIC);
    }

    f = (FIELD **)malloc(
        (sizeof(FIELD *)) *
        (addressFields.size() + dataFields.size() + asciiFields.size() + registerViewFields.size()+ 1));
    i = 0;
    for (auto it = fields.begin(); it < fields.end(); it++) {
      f[i] = *it;
      i++;
    }
    for (auto it = registerViewFields.begin(); it < registerViewFields.end(); it++) {
      f[i] = *it;
      i++;
    }
    f[i] = NULL;

    form = new_form(f);
    assert(form != NULL);
    set_field_term(form, form_hook_proxy);
    set_form_win(form, win);
    set_form_sub(form, derwin(win, 40, 76, 1, 1));
    post_form(form);
    pos_form_cursor(form);
    updateForm(base);  
    pos_form_cursor(form);
    wmove(win, cursorY, cursorX);
    pos_form_cursor(form);
    refresh();
    wrefresh(win);
    form_driver(form, REQ_OVL_MODE);
    hightlightWindow();
    normalWindow();
  }
  ~registerWindow() {
    unpost_form(form);
    free_form(form);
    for (auto it = fields.begin(); it < fields.end(); it++) {
      free_field(*it);
    }
  }

  void updateWindow() {
    // curs_set(0);
    if (!activeWindow)  {
      updateForm(base);
      wrefresh(win);
    }
    //pos_form_cursor(form);
    // leaveok(win, true);
    //wrefresh(win);
    //refresh();
  }

  void hightlightWindow() {
    curs_set(2);
  
    wattrset(win, A_STANDOUT);
    box(win, 0, 0);
    mvwprintw(win, 0, 1, "REGISTER WINDOW");
    wattrset(win, 0);
    pos_form_cursor(form);
    wmove(win, cursorY, cursorX);
    printLog("INFO", "cursorY=%d cursorX=%d\n", cursorY, cursorX);
    refresh();  
    wrefresh(win);
    activeWindow = true;
    pos_form_cursor(form);
    updateForm(base);
  }
  void normalWindow() {
    printLog("INFO", "registerWindow::normalWindow()");
    getyx(win,cursorY,cursorX);
    curs_set(0);
    box(win, 0, 0);
    mvwprintw(win, 0, 1, "REGISTER WINDOW");
    wrefresh(win);
    activeWindow = false;
  }
  void resetCursor() {
    if (activeWindow) {
    //  curs_set(2);
    //  wmove(win, cursorY, cursorX);
    //  wrefresh(win);
    }
  }
  void handleKey(int key) {
    /*
    '?' toggle a help screen for the register window

    */
    int i;

    switch (key) {
    case 27: // ESC
      updateForm(base);
      break;
    case KEY_DOWN:
      form_driver(form, REQ_DOWN_FIELD);
      form_driver(form, REQ_BEG_FIELD);
      break;

    case KEY_UP:
      form_driver(form, REQ_UP_FIELD);
      form_driver(form, REQ_BEG_FIELD);
      break;

    case KEY_LEFT:
      form_driver(form, REQ_LEFT_FIELD);
      break;

    case KEY_RIGHT:
      form_driver(form, REQ_RIGHT_FIELD);
      break;

    // Delete the char before cursor
    case KEY_BACKSPACE:
    case 127:
      form_driver(form, REQ_DEL_PREV);
      break;

    // Delete the char under the cursor
    case KEY_DC:
      form_driver(form, REQ_DEL_CHAR);
      break;

    case '\n':
      form_driver(form, REQ_NEXT_FIELD);
      form_driver(form, REQ_PREV_FIELD);
      break;

    default:
      if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'F') ||
          (key >= 'a' && key <= 'f')) {
        form_driver(form, toupper(key));
      }
      break;
    }

    wrefresh(win);
  }
};

class registerWindow *r;

void form_hook_proxy(formnode * f) {
  class hookExecutor * hE;
  FIELD *field = current_field(f);
  hE = (class hookExecutor * ) field_userptr(field);
  hE->exec(field);
}

void registerWindow::memoryAddressHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  int value = strtol(bufferString, NULL, 16);
  rw->base = (value - address * 16) & 0x3ff0;
  rw->updateForm(rw->base);
  wrefresh(rw->win);
}

void registerWindow::memoryDataHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  int value = strtol(bufferString, NULL, 16);
  rw->cpu->memory[rw->base + data] = value;
}

void registerWindow::charPointerHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  unsigned char value = strtol(bufferString, NULL, 16);
  *address = value;
}

void registerWindow::shortPointerHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  unsigned short value = strtol(bufferString, NULL, 16);
  *address = value;
}


class dp2200Window *dpw;
class registerWindow *rw;
class commandWindow *cw;

int pollKeyboard(void);
struct callbackRecord {
  int (*cb)();
  struct timespec deadline;
};
class Window *windows[3];
int activeWindow = 0;

std::vector<callbackRecord> timerqueue;

int pollKeyboard() {
  int ch;
  int savedY, savedX;
  struct callbackRecord cbr;
  rw->updateWindow();
  windows[activeWindow]->resetCursor();
  ch = getch();
  switch (ch) {
  case KEY_BTAB:
    windows[activeWindow++]->normalWindow();
    activeWindow = activeWindow > 2 ? 0 : activeWindow;
    windows[activeWindow]->hightlightWindow();
    break;
  case '\t':
    windows[activeWindow--]->normalWindow();
    activeWindow = activeWindow < 0 ? 2 : activeWindow;
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
  if (cbr.deadline.tv_nsec >= 1000000000) {
    cbr.deadline.tv_nsec -= 1000000000;
    cbr.deadline.tv_sec++;
  }
  cbr.cb = pollKeyboard;
  timerqueue.push_back(cbr);

  return 0;
}



char timeBuf[sizeof "2011-10-08T07:07:09.000Z"];

void updateTimeBuf() {
  timeval curTime;
  gettimeofday(&curTime, NULL);
  int milli = curTime.tv_usec / 1000;
  char *p = timeBuf +
            strftime(timeBuf, sizeof timeBuf, "%FT%T", gmtime(&curTime.tv_sec));
  snprintf(p, 6, ".%03dZ", milli);
}

void printLog(const char *level, const char *fmt, ...) {
  char buffer[256];
  int charsPrinted;
  struct timeval tv;
  va_list args;
  va_start(args, fmt);
  updateTimeBuf();
  charsPrinted = snprintf(buffer, sizeof level + sizeof timeBuf + 2, "%s %s ",
                          level, timeBuf);
  vsnprintf(buffer + charsPrinted, 256 - charsPrinted, fmt, args);
  fwrite(buffer, strlen(buffer), 1, logfile);
  va_end(args);
}

int main(int argc, char *argv[]) {
  WINDOW *my_win, *win;
  int startx, starty, width, height;
  struct timespec now, timeout;

  class dp2200_cpu * cpu = new dp2200_cpu;
  cpu->tapeDrive[0] = new CassetteTape();
  cpu->tapeDrive[1] = new CassetteTape();
  logfile = fopen("dp2200.log", "w");
  printLog("INFO", "Starting up %d\n", 10);
  initscr(); /* Start curses mode 		*/
  raw();     /* Line buffering disabled, Pass on everything to me 		*/
  keypad(stdscr, TRUE); /* I need that nifty F1 	*/
  noecho();
  nodelay(stdscr, true);
  set_escdelay(100);
  timeout(0);
  refresh();
  dpw = new dp2200Window(cpu);
  r = rw = new registerWindow(cpu);
  cw = new commandWindow(cpu);
  windows[0] = cw;
  windows[1] = rw;
  windows[2] = dpw;
  windows[activeWindow]->hightlightWindow();
  pollKeyboard();
  while (1) { // event loop
    clock_gettime(CLOCK_MONOTONIC, &now);
    timeout.tv_nsec = timerqueue.front().deadline.tv_nsec - now.tv_nsec;
    timeout.tv_sec = timerqueue.front().deadline.tv_sec - now.tv_sec;
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
