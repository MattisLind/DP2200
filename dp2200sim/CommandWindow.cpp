#include "CommandWindow.h"


void commandWindow::doHelp(std::vector<Param> params) {
  for (std::vector<Cmd>::const_iterator it = commands.begin();
        it != commands.end(); it++) {
    wprintw(innerWin, "%s - %s\n", it->command.c_str(), it->help.c_str());
    printLog("INFO", "%s - %s\n", it->command.c_str(), it->help.c_str());
  }
}

void commandWindow::doStep(std::vector<Param> params) { 
  cpu->execute(); 
}

void commandWindow::doExit(std::vector<Param> params) {
  endwin();
  exit(0);
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
  if (!cpu->ioCtrl->cassetteDevice->loadBoot(cpu->memory)) {
    wprintw(innerWin, "Unable to load bootstrap into memory.\n");
  }
}
void commandWindow::doClear(std::vector<Param> params) {
  cpu->clear();
}
void commandWindow::doRun(std::vector<Param> params) {
  cpu->totalInstructionTime.tv_nsec=0;
  cpu->totalInstructionTime.tv_sec=0;
  cpu->running = true;
}
void commandWindow::doReset(std::vector<Param> params) {
  cpu->reset();
  cpu->running=false;
}

void commandWindow::doTrace(std::vector<Param> params) {
  cpu->traceEnabled=true;
}
void commandWindow::doNoTrace(std::vector<Param> params) {
  cpu->traceEnabled=false; 
}

void commandWindow::doRestart(std::vector<Param> params) {}
void commandWindow::doHalt(std::vector<Param> params) {
  cpu->running = false;
}
void commandWindow::doAddBreakpoint(std::vector<Param> params) {
  unsigned short address=0;
  for (auto it = params.begin(); it < params.end(); it++) {
    if (it->paramId == ADDRESS) {
      address = strtol(it->paramValue.s, NULL, 16);
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
      address = strtol(it->paramValue.s, NULL, 16);
    }
  }
  if (cpu->removeBreakpoint(address)) {
    wprintw(innerWin, "Failed to remove breakpoint at address %04X\n", address);
  };   
}

void commandWindow::doDetach(std::vector<Param> params) {
  int drive=0;
  for (auto it = params.begin(); it < params.end(); it++) {
    if (it->paramId == DRIVE) {
      drive = it->paramValue.i;
    }
  }
  cpu->ioCtrl->cassetteDevice->closeFile(drive);
  wprintw(innerWin, "Detaching file %s to drive %d\n",
          cpu->ioCtrl->cassetteDevice->getFileName(drive).c_str(), drive);
}

void commandWindow::doAttach(std::vector<Param> params) {
  std::string fileName;
  int drive=0;
  
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

  if (cpu->ioCtrl->cassetteDevice->openFile(drive, fileName)) {
    wprintw(innerWin, "Attaching file %s to drive %d\n", fileName.c_str(),drive);
  } else {
    wprintw(innerWin, "Failed to open file %s\n", fileName.c_str());      
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

commandWindow::commandWindow(class dp2200_cpu * c) {
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
  commands.push_back({"STOP", "Stop execution", {}, &commandWindow::doHalt});
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
      {"BREAK", "Add breakpoint", {{"ADDRESS", ADDRESS, STRING, {.s = {'\0'}}}}, &commandWindow::doAddBreakpoint});
  commands.push_back(
      {"NOBREAK", "Remove breakpoint", {{"ADDRESS", ADDRESS, STRING, {.s = {'\0'}}}}, &commandWindow::doRemoveBreakpoint});  
  commands.push_back({"TRACE", "Enable trace logging", {}, &commandWindow::doTrace});    
  commands.push_back({"NOTRACE", "Disable trace logging", {}, &commandWindow::doNoTrace});  
  commands.push_back(
      {"YIELD", "The amount of CPU time consumed byt the simulator.", {{"VALUE", VALUE, NUMBER, {.i = 100}}}, &commandWindow::doYield});         
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
void commandWindow::handleKey(int ch) {
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
    if (commandLine.size()>0) {
    wdelch(innerWin);
    commandLine.erase(commandLine.size() - 1, 1);
    }

  }
  wrefresh(innerWin);
  getyx(innerWin, cursorY, cursorX);
}
void commandWindow::resetCursor() {
  if (activeWindow) {
    curs_set(2);
    wmove(innerWin, cursorY, cursorX);
    wrefresh(innerWin);
  }
}