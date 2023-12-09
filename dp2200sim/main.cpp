
#include "dp2200_cpu_sim.h"
#include <algorithm>
#include <assert.h>
#include <ctype.h>
#include <form.h>
#include <ncurses.h>
#include <stdarg.h>
#include <cstdio>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <sys/time.h>
#include <time.h>
#include <vector>
#include "dp2200_io_sim.h"
#include "cassetteTape.h"
#include "dp2200Window.h"
#include "Window.h"
#include "CommandWindow.h"
#include "RegisterWindow.h"
#include <sys/ioctl.h>
#include <unistd.h>

void printLog(const char *level, const char *fmt, ...);
float yield=100.0;
FILE *logfile;

class dp2200_cpu cpu;

class dp2200Window *dpw;
class registerWindow *rw;
class registerWindow *r;
class commandWindow *cw;

int pollKeyboard(void);
int cpuRunner();

class commandWindow;



void form_hook_proxy(formnode * f) {
  class hookExecutor * hE;
  FIELD *field = current_field(f);
  printLog("INFO", "form_hook_proxy ENTRY\n");
  hE = (class hookExecutor * ) field_userptr(field);
  if (hE!= NULL) hE->exec(field);
}

void registerWindow::Form::hexMemoryAddressHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  int value = strtol(bufferString, NULL, 16);
  printLog("INFO", "memoryAddressHookExecutor string=%s value=%d\n", bufferString, value);
  cpu.startAddress = (value - address * 16) & 0x3ff0;
  rwf->updateForm();
  wrefresh(r->win);
}

void registerWindow::Form::hexMemoryDataHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  int value = strtol(bufferString, NULL, 16);
  cpu.memory[cpu.startAddress + data] = value;
  rwf->updateForm();
  wrefresh(r->getWin());
}


void registerWindow::Form::octalMemoryAddressHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  int value = strtol(bufferString, NULL, 8);
  printLog("INFO", "memoryAddressHookExecutor string=%s value=%d\n", bufferString, value);
  cpu.startAddress = (value - address * 16) & 0x3ff0;
  rwf->updateForm();
  wrefresh(r->win);
}

void registerWindow::Form::octalMemoryDataHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  int value = strtol(bufferString, NULL, 8);
  cpu.memory[cpu.startAddress + data] = value;
  rwf->updateForm();
  wrefresh(r->getWin());
}

void registerWindow::Form::hexCharPointerHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  unsigned char value = strtol(bufferString, NULL, 16);
  *address = value;
}

void registerWindow::Form::hexShortPointerHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  unsigned short value = strtol(bufferString, NULL, 16);
  *address = value;
}

void registerWindow::Form::octalCharPointerHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  unsigned char value = strtol(bufferString, NULL, 8);
  *address = value;
}

void registerWindow::Form::octalShortPointerHookExecutor::exec(FIELD *field) {
  char *bufferString = field_buffer(field, 0);
  unsigned short value = strtol(bufferString, NULL, 8);
  *address = value;
}



class callbackRecord {
  public:
  std::function<int(class callbackRecord *)> cb;
  struct timespec deadline;
};
class Window *windows[3];
int activeWindow = 0;

std::vector<callbackRecord *> timerqueue;



void timeoutInNanosecs (struct timespec * t, long nanos) {
  *t = cpu.totalInstructionTime;
  t->tv_nsec += nanos;
  if (t->tv_nsec >= 1000000000) {
    t->tv_nsec -= 1000000000;
    t->tv_sec++;
  }
}


int pollKeyboard() {
  int ch;
  struct winsize w;
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
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if ((w.ws_col < 179) || (w.ws_row < 46)) {
      endwin();
      fprintf(stderr, "Too small screen. Increase terminal window to be bigger than 179 x 46. Current screen size is %d x %d\n", w.ws_col, w.ws_row);
      exit(1);
    }
    for (int i=0; i<3; i++) windows[i]->resize();
    break;
  case ERR:
    break;
  default:
    windows[activeWindow]->handleKey(ch);
    break;
  }
  return 0;
}


struct timespec subtractTimeSpec (struct timespec a, struct timespec b, bool * negative=NULL) {
  struct timespec diff; 
  if ((b.tv_sec > a.tv_sec) || ((b.tv_sec == a.tv_sec) && (b.tv_nsec > a.tv_nsec) )) {
    // b is larger do reverse subtracton and return negative tv_sec
    if (negative!=NULL) *negative=true;
    diff.tv_nsec = b.tv_nsec - a.tv_nsec;
    diff.tv_sec = b.tv_sec - a.tv_sec;
  } else {
    // a is larger than b 
    if (negative!=NULL) *negative=false;
    diff.tv_nsec = a.tv_nsec - b.tv_nsec;
    diff.tv_sec = a.tv_sec - b.tv_sec;
  }
  if (diff.tv_nsec < 0) {
    diff.tv_sec--;
    diff.tv_nsec +=1000000000;
  }
  return diff;
}

// returns true if a >= b false otherwise
bool compareTimeSpec (struct timespec a, struct timespec b) {
  
  if ((b.tv_sec > a.tv_sec) || ((b.tv_sec == a.tv_sec) && (b.tv_nsec > a.tv_nsec) )) {
    return false;
  } else {
    return true;
  }
}

bool compareCallbackRecord (class callbackRecord * a,class callbackRecord * b) {
  return !compareTimeSpec(a->deadline, b->deadline);
}

class callbackRecord * addToTimerQueue(std::function<int(class callbackRecord *)> cb, struct timespec t) {
  class callbackRecord * c = new callbackRecord;
  c->cb = cb;
  c->deadline = t;
  timerqueue.push_back(c);
  std::sort (timerqueue.begin(), timerqueue.end(), compareCallbackRecord); 
  return c;
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
  
  va_list args;
  va_start(args, fmt);
  updateTimeBuf();
  charsPrinted = snprintf(buffer, sizeof level + sizeof timeBuf + 2, "%s %s ",
                          level, timeBuf);
  vsnprintf(buffer + charsPrinted, 256 - charsPrinted, fmt, args);
  fwrite(buffer, strlen(buffer), 1, logfile);
  va_end(args);
}

void removeTimerCallback(class callbackRecord * c) {
  auto it = std::find(timerqueue.begin(), timerqueue.end(), c);
  if (it != timerqueue.end()) {
    timerqueue.erase(it);
  }
}

void addTimeSpec(struct timespec * after, struct timespec * before, long increment ) {
  after->tv_nsec = before->tv_nsec + increment;
  after->tv_sec = before->tv_sec;
  if (after->tv_nsec>1000000000) {
    after->tv_sec++;
    after->tv_nsec -= 1000000000;
  }
}

bool nowIsLessThan(struct timespec * after) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return !compareTimeSpec(now, *after);
}

char * getCpuTimeStr (char * buffer, int size) {
  snprintf(buffer, size, "%10ld.%10ld", cpu.totalInstructionTime.tv_sec, cpu.totalInstructionTime.tv_nsec);
  return buffer;
}

int main(int argc, char *argv[]) {
  struct timespec now,before, after, diff;
  //char buffer[100];
  struct winsize w;
  bool negative;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  logfile = fopen("dp2200.log", "w");
  printLog("INFO", "Starting up %d\n", 10);
  if ((w.ws_col < 179) || (w.ws_row < 46)) {
    fprintf(stderr, "Too small screen. Increase terminal window to be bigger than 179 x 46. Current screen size is %d x %d\n", w.ws_col, w.ws_row);
    exit(1);
  }
  initscr(); /* Start curses mode 		*/
  raw();     /* Line buffering disabled, Pass on everything to me 		*/
  keypad(stdscr, TRUE); /* I need that nifty F1 	*/
  noecho();
  nodelay(stdscr, true);
  set_escdelay(100);
  timeout(0);
  refresh();
  dpw = new dp2200Window(&cpu);
  r = rw = new registerWindow(&cpu);
  cw = new commandWindow(&cpu);
  windows[0] = cw;
  windows[1] = rw;
  windows[2] = dpw;
  windows[activeWindow]->hightlightWindow();
  //cpuRunner();
  while (1) { // event loop
    cpu.interruptPending = 1;
    clock_gettime(CLOCK_MONOTONIC, &before);
    addTimeSpec(&after, &before, (long) (yield/100 * 1000000));
    while (cpu.running && nowIsLessThan(&after)) {
      // Run instructions
      if (cpu.execute()) {
        cpu.running = false;
      } 
      //printLog("INFO", "%s\n", getCpuTimeStr(buffer, 100));
      if (std::find(cpu.breakpoints.begin(), cpu.breakpoints.end(), cpu.P)!=cpu.breakpoints.end()) {
        cpu.running = false;
      } 
      //printLog("INFO", "%s size=%d \n", getCpuTimeStr(buffer, 100), timerqueue.size());
      if (timerqueue.size()>0 && compareTimeSpec(timerqueue.front()->deadline, cpu.totalInstructionTime)) {
        //printLog("INFO", "Not executing callback %s top=%10ld.%10ld \n", getCpuTimeStr(buffer, 100), timerqueue.front()->deadline.tv_sec, timerqueue.front()->deadline.tv_nsec);
        continue;
      }
      if (timerqueue.size() == 0) continue;
      //printLog("INFO", "Executing callback %s top=%10ld.%10ld \n", getCpuTimeStr(buffer, 100), timerqueue.front()->deadline.tv_sec, timerqueue.front()->deadline.tv_nsec);
      auto callBack = timerqueue.front()->cb;
      class callbackRecord * timerRecord = timerqueue.front();
      delete timerqueue.front();
      timerqueue.erase(timerqueue.begin());
      callBack(timerRecord);
    }
    pollKeyboard();
    addTimeSpec(&after, &before, 1000000); // 1 ms

    // calculate time between after and now 
    clock_gettime(CLOCK_MONOTONIC, &now);
    diff = subtractTimeSpec(after, now, &negative); 
    // call nanosleep to sleep this amount
    if (!negative) nanosleep(&diff, NULL);
  }
  return 0;
}
