#include "CommandWindow.h"
#include "RegisterWindow.h"

extern registerWindow * rw;
#include <algorithm>

extern bool running;

void commandWindow::doHelp(std::vector<Param> params) {
  wprintw(innerWin, "All commands can be shorted until they becaome ambigous. \nFor example A for ATTACH.\n");
  wprintw(innerWin, "Some commands take parameters. Also parameters may be shortened.\n");
  wprintw(innerWin, "An equal sign delimits the parameter and the parameter value.\n");
  wprintw(innerWin, "Example AT F=file - Attach file <file> to drive 0.\n");
  for (std::vector<Cmd>::const_iterator it = commands.begin();
        it != commands.end(); it++) {
    wprintw(innerWin, "%s - %s\n", it->command.c_str(), it->help.c_str());
    printLog("INFO", "%s - %s\n", it->command.c_str(), it->help.c_str());
  }
}

void commandWindow::doStep(std::vector<Param> params) { 
  cpu->interruptPending = 0;
  cpu->execute(); 
}

void commandWindow::doExit(std::vector<Param> params) {
  endwin();
  exit(0);
}

void commandWindow::doOct(std::vector<Param> params) {
  rw->octal = true;
  rw->setOctal(rw->octal);
  cpu->octal=true;
}

void commandWindow::doHex(std::vector<Param> params) {
  rw->octal=false;
  rw->setOctal(rw->octal); 
  cpu->octal=false; 
}

void commandWindow::doYield(std::vector<Param> params) { 
  int value=0;
  for (auto it = params.begin(); it < params.end(); it++) {
    if (it->paramId == VALUE) {
      value = it->paramValue.i;
    }
  }
  if (value <0 || value >100) {
    wprintw(innerWin, "Value out of range %d. Should be between 0 and 100.\n", value);
  } else {
    yield =(float) value;
  };   
}

void commandWindow::doLoadBoot(std::vector<Param> params) {
  if (!cpu->ioCtrl->cassetteDevice->loadBoot([memory=cpu->memory](int address, unsigned char data)->void { memory->physicalMemoryWrite(address, data);})) {
    wprintw(innerWin, "Unable to load bootstrap into memory.\n");
  }
}
void commandWindow::doClear(std::vector<Param> params) {
  cpu->clear();
}
void commandWindow::doRun(std::vector<Param> params) {
  cpu->totalInstructionTime.tv_nsec=0;
  cpu->totalInstructionTime.tv_sec=0;
  running = true;
}

void commandWindow::doContinue(std::vector<Param> params) {
  running = true;
}

void commandWindow::doReset(std::vector<Param> params) {
  cpu->reset();
  running=false;
}

void commandWindow::doTrace(std::vector<Param> params) {
  cpu->traceEnabled=true;
}

void commandWindow::doRim(std::vector<Param> params) {
  int address=0234, node=1;
  for (const auto & param : params) {
    if (param.paramId==ADDRESS) address=param.paramValue.i;
    if (param.paramId==NODE) node=param.paramValue.i;
  }
  if (!cpu->is5500 || cpu->is6600 || !cpu->ioCtrl->attachRim(address,node)) {
    wprintw(innerWin,"RIM requires CPU 5500, a free address mask 0..255 with exactly four set bits and unique node 1..255.\n");
    return;
  }
  wprintw(innerWin,"9483 RIM at %03o, node %d attached (register/buffer model; no link).\n",address,node);
}

void commandWindow::doSet(std::vector<Param> params) {
  for (auto it = params.begin(); it < params.end(); it++) {
    if (it->paramId == CPU) {
      if (it->paramValue.i == 5500) {
        cpu->setCPUtype5500();
        rw->set2200Mode(false); 
      } else if (it->paramValue.i == 2200) {
        cpu->setCPUtype2200(); 
        rw->set2200Mode(true); 
      } else {
        wprintw(innerWin, "Invalid CPU type: %d\n", it->paramValue.i);  
      }
    }
    if (it->paramId == MEMORY) {
      if (it->paramValue.i >= 2 && it->paramValue.i <= 64) {
        cpu->memorySize = it->paramValue.i;
      } else {
        wprintw(innerWin, "Invalid memory size: %d \n", it->paramValue.i); 
      }
    }
    if (it->paramId == AUTORESTART) {
      cpu->setAutorestart(it->paramValue.b);
    }
  }  
}
void commandWindow::doNoTrace(std::vector<Param> params) {
  cpu->traceEnabled=false; 
}

void commandWindow::doRestart(std::vector<Param> params) {
  running = false;
  cpu->reset();
  if (cpu->cpuIs2200()) {
    cpu->ioCtrl->cassetteDevice->loadBoot([memory=cpu->memory](int address, unsigned char data)->void { return memory->physicalMemoryWrite(address,data);});
  }
  cpu->totalInstructionTime.tv_nsec=0;
  cpu->totalInstructionTime.tv_sec=0;
  running = true;  
}
void commandWindow::doHalt(std::vector<Param> params) {
  running = false;
}
void commandWindow::doAddBreakpoint(std::vector<Param> params) {
  unsigned short address=0;
  for (auto it = params.begin(); it < params.end(); it++) {
    if (it->paramId == ADDRESS) {
      if (rw->octal) {
        address = strtol(it->paramValue.s, NULL, 8);
      } else {
        address = strtol(it->paramValue.s, NULL, 16);  
      }
    }
  }
  if (cpu->addBreakpoint(address)) {
    wprintw(innerWin, "Failed to add breakpoint at address %04X\n", address);
  };   
}
void commandWindow::doRemoveBreakpoint(std::vector<Param> params) {
  unsigned short address=0;
  for (auto it = params.begin(); it < params.end(); it++) {
    if (it->paramId == ADDRESS) {
      if (rw->octal) {
        address = strtol(it->paramValue.s, NULL, 8);
      } else {
        address = strtol(it->paramValue.s, NULL, 16);  
      }
    }
  }
  if (cpu->removeBreakpoint(address)) {
    wprintw(innerWin, "Failed to remove breakpoint at address %04X\n", address);
  };   
}

void commandWindow::doAddWatch(std::vector<Param> params) {
  unsigned short address=0;
  for (auto it = params.begin(); it < params.end(); it++) {
    if (it->paramId == ADDRESS) {
      if (rw->octal) {
        address = strtol(it->paramValue.s, NULL, 8);
      } else {
        address = strtol(it->paramValue.s, NULL, 16);  
      }
    }
  }
  if (cpu->memory->addWatch(address)) {
    wprintw(innerWin, "Failed to add memory watch at address %04X\n", address);
  };   
}
void commandWindow::doRemoveWatch(std::vector<Param> params) {
  unsigned short address=0;
  for (auto it = params.begin(); it < params.end(); it++) {
    if (it->paramId == ADDRESS) {
      if (rw->octal) {
        address = strtol(it->paramValue.s, NULL, 8);
      } else {
        address = strtol(it->paramValue.s, NULL, 16);  
      }
    }
  }
  if (cpu->memory->removeWatch(address)) {
    wprintw(innerWin, "Failed to remove memory watch at address %04X\n", address);
  };   
}


void commandWindow::doDetach(std::vector<Param> params) {
  int drive=0; 
  std::string type;
  for (auto it = params.begin(); it < params.end(); it++) {
    if (it->paramId == DRIVE) {
      drive = it->paramValue.i;
    }
    if (it->paramId == TYPE) {
      type = it->paramValue.s;
    }
  }
  std::transform(type.begin(), type.end(), type.begin(),::toupper);
  if (type == "CASSETTE") {
    cpu->ioCtrl->cassetteDevice->closeFile(drive);
    wprintw(innerWin, "Detaching file %s to drive %d\n",cpu->ioCtrl->cassetteDevice->getFileName(drive).c_str(), drive);
  } else if (type == "FLOPPY") {
    cpu->ioCtrl->floppyDevice->closeFile(drive); 
  } else if (type == "PRINTER") {
    cpu->ioCtrl->localPrinterDevice->closeFile(drive); 
  } else if (type == "9350") {
    cpu->ioCtrl->disk9350Device->closeFile(drive);
  } else if (type == "9370" || type == "9374") {
    cpu->ioCtrl->disk9370Device->closeFile(drive);
  }else {
    wprintw(innerWin, "Unrecognized type %s\n", type.c_str());
  }
}

void commandWindow::doAttach(std::vector<Param> params) {
  std::string fileName, type;
  int drive=0, ret;
  bool writeProtect=true, writeBack=false;
  
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
      printLog("INFO", "type=BOOL paramValue=%s\n",it->paramValue.b ? "TRUE" : "FALSE");
      break;
    }
    if (it->paramId == FILENAME) {
      fileName = it->paramValue.s;
    }
    if (it->paramId == TYPE) {
      type = it->paramValue.s;
    }
    if (it->paramId == DRIVE) {
      drive = it->paramValue.i;
    }
    if (it->paramId == WRITEBACK) {
      writeBack = it->paramValue.b;
    }
    if (it->paramId == WRITEPROTECT) {
      writeProtect = it->paramValue.b;
    }
  }
  std::transform(type.begin(), type.end(), type.begin(),
                  ::toupper);

  if (type == "CASSETTE") {
    if (cpu->ioCtrl->cassetteDevice->openFile(drive, fileName, writeProtect)) {
      wprintw(innerWin, "Attaching file %s to drive %d\n", fileName.c_str(),drive);
    } else {
      wprintw(innerWin, "Failed to open file %s\n", fileName.c_str());      
    }
  } else if (type == "FLOPPY") {
    if ((ret = cpu->ioCtrl->floppyDevice->openFile(drive, fileName, writeProtect, writeBack))==0) {
      wprintw(innerWin, "Attaching file %s to floppy drive %d\n", fileName.c_str(),drive);
    } else {
      if (ret == FILE_HAS_BAD_BLOCKS) {
        wprintw(innerWin, "Warning: This IMD image was successfully mounted but contains one or more bad blocks stored when originally reading it. You may encounter problems when accessing it.\n");
      } else {
        wprintw(innerWin, "Failed to open file %s code %d. The file is not present or the format is invalid. \n", fileName.c_str(), ret); 
      }    
    }
  } else if (type == "PRINTER") {
    if ((ret = cpu->ioCtrl->localPrinterDevice->openFile(drive, fileName))==0) {
      wprintw(innerWin, "Attaching file %s to printer\n", fileName.c_str());
    } else {
      wprintw(innerWin, "Failed to open file %s code %d \n", fileName.c_str(), ret);      
    }    
  } else if (type == "9350") {
    if ((ret = cpu->ioCtrl->disk9350Device->openFile(drive, fileName, writeProtect))==0) {
      wprintw(innerWin, "Attaching file %s to 9350 disk drive %d\n", fileName.c_str(), drive );
    } else {
      wprintw(innerWin, "Failed to open file %s code %d \n", fileName.c_str(), ret);
    }
  } else if (type == "9370" || type == "9374") {
    if (!cpu->ioCtrl->disk9370Device->setModel(type=="9374" ? 9374 : 9370)) {
      wprintw(innerWin, "Detach disk images before changing controller type\n");
      return;
    }
    if ((ret = cpu->ioCtrl->disk9370Device->openFile(drive, fileName, writeProtect))==0) {
      wprintw(innerWin, "Attaching file %s to %s disk unit %d\n", fileName.c_str(), type.c_str(), drive );
    } else {
      wprintw(innerWin, "Failed to open file %s code %d \n", fileName.c_str(), ret);
    }
  }
}
void commandWindow::processCommand(char ch) {
  std::vector<Cmd> filtered;
  std::vector<std::string> paramStrings;
  std::size_t found, prev;
  std::string commandWord, tmp;
  bool failed = false;
  printLog("INFO", "commandLine=%s\n", commandLine.c_str());
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
      wprintw(innerWin, "%s", filtered[0].command.c_str());
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
            strncpy(filteredParams[0]->paramValue.s, v.c_str(),PARAM_VALUE_SIZE);
            printLog("INFO", "string=%s\n", filteredParams[0]->paramValue.s);
            filteredParams[0]->paramValue.s[PARAM_VALUE_SIZE - 1] = 0;
          } else {
            std::transform(v.begin(), v.end(), v.begin(), ::toupper);
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

commandWindow::commandWindow(class dp2200_cpu * c) {
  cursorX = 1;
  cursorY = 0;
  cpu = c;
  activeWindow = false;
  commandHistoryIndex = -1;
  commands.push_back(
      {"HELP", "Show help information", {}, &commandWindow::doHelp});
  commands.push_back(
      {"STEP", "Step one instruction", {}, &commandWindow::doStep});
  commands.push_back({"ATTACH",
                      "Attach file to cassette drive. \n  Parameter FILE specify the file to attach.\nParameter DRIVE= specify the drive used. Default drive is 0.\nTYPE specify either CASSETTE, FLOPPY or PRINTER. CASSETTE is default.",
                      {{"DRIVE", DRIVE, NUMBER, {.i = 0}},
                        {"FILENAME", FILENAME, STRING, {.s = {'\0'}}},
                        {"TYPE", TYPE, STRING, {.s = {'C','A','S','S','E','T','T','E','\0'}}},
                        {"WRITEBACK", WRITEBACK, BOOL, {.b = false } },
                        {"WRITEPROTECT", WRITEPROTECT, BOOL, {.b = true}}                        
                      },
                      &commandWindow::doAttach});
  commands.push_back({"DETACH",
                      "Detach file from cassette drive. \n  Parameter DRIVE specify the drive used. Default drive is 0.\nTYPE specify either CASSETTE, FLOPPY or PRINTER. CASSETTE is default.",
                      {{"DRIVE", DRIVE, NUMBER, {.i = 0}}, 
                      {"TYPE", TYPE, STRING, {.s = {'C','A','S','S','E','T','T','E','\0'}}}},
                      &commandWindow::doDetach});
  commands.push_back({"STOP", "Stop execution", {}, &commandWindow::doHalt});
  commands.push_back({"CONTINUE", "Continue execution", {}, &commandWindow::doContinue});
  commands.push_back(
      {"EXIT", "Exit the simulator", {}, &commandWindow::doExit});
  commands.push_back(
      {"QUIT", "Quit the simulator", {}, &commandWindow::doExit});        
  commands.push_back({"LOADBOOT",
                      "Load the bootstrap from cassette into memory",
                      {},
                      &commandWindow::doLoadBoot});
  commands.push_back({"RESTART",
                      "Load bootstrap and restart CPU",
                      {},
                      &commandWindow::doRestart});
  commands.push_back({"RESET", "Reset the CPU", {}, &commandWindow::doReset});
  commands.push_back({"HALT", "Stop the CPU", {}, &commandWindow::doHalt});
  commands.push_back(
      {"RUN", "Run CPU from current location", {}, &commandWindow::doRun});
  commands.push_back(
      {"CLEAR", "Clear memory", {}, &commandWindow::doClear});   
  commands.push_back(
      {"BREAK", "Add breakpoint. \n  Parameter ADDRESS is used for specifying the address of the breakpoint.", {{"ADDRESS", ADDRESS, STRING, {.s = {'\0'}}}}, &commandWindow::doAddBreakpoint});
  commands.push_back(
      {"NOBREAK", "Remove breakpoint. \n  Parameter ADDRESS is used for specifying the address of the breakpoint.", {{"ADDRESS", ADDRESS, STRING, {.s = {'\0'}}}}, &commandWindow::doRemoveBreakpoint});  
  commands.push_back(
      {"WATCH", "Add memory watch. \n  Parameter ADDRESS is used for specifying the address of the memory watch.", {{"ADDRESS", ADDRESS, STRING, {.s = {'\0'}}}}, &commandWindow::doAddWatch});
  commands.push_back(
      {"NOWATCH", "Remove memory watch. \n  Parameter ADDRESS is used for specifying the address of the memory watch.", {{"ADDRESS", ADDRESS, STRING, {.s = {'\0'}}}}, &commandWindow::doRemoveWatch});  
  commands.push_back({"TRACE", "Enable trace logging", {}, &commandWindow::doTrace});
  commands.push_back({"RIM", "Attach a 9483 register/buffer device (CPU 5500). ADDRESS and NODE are decimal; defaults 156 and 1. No network link yet.", {{"ADDRESS", ADDRESS, NUMBER, {.i=156}}, {"NODE", NODE, NUMBER, {.i=1}}}, &commandWindow::doRim});
  commands.push_back({"NOTRACE", "Disable trace logging", {}, &commandWindow::doNoTrace}); 
  commands.push_back({"HEXADECIMAL", "Show in hexadecimal notation.\nAlso possible to toggle in the register view by pressing 'o'.", {}, &commandWindow::doHex});  
  commands.push_back({"OCTAL", "Show in Octal notation.\nAlso possible to toggle in the register view by pressing 'o'.", {}, &commandWindow::doOct});  
  commands.push_back({"SET", "Set various system parameters like cpu type and memory amount.\nCPU=2200 or CPU=5500 specify architecture. MEMORY=nn where nn=2 .. 64 (k) Memory.\n AUTORESTART is a boolean used on the 5500. TRUE or FALSE", {{"CPU", CPU, NUMBER, {.i=2200}}, {"MEMORY", MEMORY, NUMBER, {.i=16}}, {"AUTORESTART", AUTORESTART, BOOL, {.i=2200}}}, &commandWindow::doSet});
  commands.push_back({"YIELD", "The amount of CPU time consumed byt the simulator. \n  VALUE parameter specify the amount. Value between 0 and 100.", {{"VALUE", VALUE, NUMBER, {.i = 100}}}, &commandWindow::doYield});         
  win = newwin(LINES - 14, 82, 14, 0);
  innerWin = newwin(LINES - 16, 80, 15, 1);
  normalWindow();
  scrollok(innerWin, TRUE);
  wmove(innerWin, 0, 0);
  waddch(innerWin, '>');
  wrefresh(innerWin);
}
void commandWindow::hightlightWindow() {
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
void commandWindow::normalWindow() {
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
// The curses cursor is a screen coordinate, not an index into commandLine.
// Keep the edit position separately and scroll long commands horizontally.
void commandWindow::redrawCommandLine() {
  int rows, columns;
  getmaxyx(innerWin, rows, columns);
  const std::size_t visible = columns>2 ? columns-2 : 0;
  editCursor = std::min(editCursor, commandLine.size());
  viewOffset = std::min(viewOffset, commandLine.size());
  if (editCursor<viewOffset) viewOffset=editCursor;
  if (editCursor-viewOffset>visible) viewOffset=editCursor-visible;
  cursorY = std::max(0,std::min(cursorY,rows-1));
  wmove(innerWin,cursorY,0);
  wclrtoeol(innerWin);
  waddch(innerWin,'>');
  if (visible) waddnstr(innerWin,commandLine.c_str()+viewOffset,visible);
  cursorX = 1+static_cast<int>(editCursor-viewOffset);
  wmove(innerWin,cursorY,cursorX);
  wrefresh(innerWin);
}

void commandWindow::handleKey(int ch) {
  printLog("INFO", "Got %c %02X\n", ch, ch);
  if (ch=='?') {
    processCommand(ch);
    editCursor=commandLine.size();
    getyx(innerWin,cursorY,cursorX);
  } else if (ch>=32 && ch<127) {
    commandLine.insert(editCursor,1,static_cast<char>(ch));
    ++editCursor;
  } else {
    switch (ch) {
      case 10:
      case KEY_ENTER:
        // Start command output on the next line, including at the bottom
        // of the scrolling window. Never use the string length as a column.
        wmove(innerWin,cursorY,getmaxx(innerWin)-1);
        waddch(innerWin,'\n');
        if (!commandLine.empty()) commandHistory.push_back(commandLine);
        processCommand('\n');
        commandLine.clear();
        editCursor=0; viewOffset=0;
        commandHistoryIndex=-1;
        getyx(innerWin,cursorY,cursorX);
        break;
      case KEY_BACKSPACE:
      case 127:
      case 8:
        if (editCursor) commandLine.erase(--editCursor,1);
        break;
      case KEY_DC:
        if (editCursor<commandLine.size()) commandLine.erase(editCursor,1);
        break;
      case KEY_LEFT:
        if (editCursor) --editCursor;
        break;
      case KEY_RIGHT:
        if (editCursor<commandLine.size()) ++editCursor;
        break;
      case KEY_UP:
        if (!commandHistory.empty()) {
          if (commandHistoryIndex==-1) commandHistoryIndex=commandHistory.size()-1;
          else if (commandHistoryIndex>0) --commandHistoryIndex;
          commandLine=commandHistory[commandHistoryIndex];
          editCursor=commandLine.size(); viewOffset=0;
        }
        break;
      case KEY_DOWN:
        if (commandHistoryIndex!=-1) {
          if (++commandHistoryIndex>=static_cast<int>(commandHistory.size()))
            commandHistoryIndex=-1;
          commandLine=commandHistoryIndex==-1 ? "" : commandHistory[commandHistoryIndex];
          editCursor=commandLine.size(); viewOffset=0;
        }
        break;
      case 0x01: // Ctrl-A.
      case KEY_HOME:
        editCursor=0;
        break;
      case 0x05: // Ctrl-E.
      case KEY_END:
        editCursor=commandLine.size();
        break;
    }
  }
  redrawCommandLine();
}
void commandWindow::resetCursor() {
  if (activeWindow) {
    curs_set(2);
    wmove(innerWin, cursorY, cursorX);
    wrefresh(innerWin);
  }
}

void commandWindow::resize() {
  wresize(win, LINES - 14, 82);
  wresize(innerWin, LINES - 16, 80);
  if (activeWindow) {
    hightlightWindow();
  } else {
    normalWindow();
  }
  wrefresh(win);
  redrawwin(innerWin);
  redrawCommandLine();
}
