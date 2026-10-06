#include "dp2200_cpu_sim.h"
#include "HeadlessConsole.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdarg>
#include <cstring>

// Same callbacks as the interactive runner, driven solely by CPU time.
class callbackRecord {
public:
  std::function<int(callbackRecord *)> cb;
  timespec deadline;
};
static dp2200_cpu cpu;
bool running = false;
static std::vector<callbackRecord *> timers;
static uint64_t nanoseconds(timespec t) {
  return uint64_t(t.tv_sec)*1000000000 + t.tv_nsec;
}
static FILE * traceFile=nullptr;
static FILE * ioTraceFile=nullptr;
void printLog(const char * level, const char * format, ...) {
  FILE * output=nullptr;
  if (ioTraceFile && (!std::strcmp(level,"DISK") || !std::strcmp(level,"TAPE"))) output=ioTraceFile;
  else if (traceFile && !std::strcmp(level,"TRACE") && std::strstr(format,"->")) output=traceFile;
  if (!output) return;
  va_list args; va_start(args,format); vfprintf(output,format,args); va_end(args);
  fflush(output);
}
void timeoutInNanosecs(timespec * t, long delay) {
  uint64_t deadline = nanoseconds(cpu.totalInstructionTime) + delay;
  t->tv_sec = deadline/1000000000;
  t->tv_nsec = deadline%1000000000;
}
callbackRecord * addToTimerQueue(std::function<int(callbackRecord *)> cb, timespec t) {
  auto * record = new callbackRecord{cb, t};
  timers.push_back(record);
  std::stable_sort(timers.begin(), timers.end(), [](auto * a, auto * b) {
    return nanoseconds(a->deadline)<nanoseconds(b->deadline);
  });
  return record;
}
void removeTimerCallback(callbackRecord * record) {
  auto it = std::find(timers.begin(), timers.end(), record);
  if (it!=timers.end()) { timers.erase(it); delete record; }
}
static void dispatchTimers() {
  while (!timers.empty() && nanoseconds(timers.front()->deadline)<=nanoseconds(cpu.totalInstructionTime)) {
    auto * record=timers.front();
    timers.erase(timers.begin());
    record->cb(record);
    delete record;
  }
}
static std::string quote(const std::string & value) {
  std::string result="\"";
  const char * hex="0123456789abcdef";
  for (unsigned char c : value) {
    if (c=='"' || c=='\\') { result+='\\'; result+=c; }
    else if (c<32 || c>=127) { result+="\\u00"; result+=hex[c>>4]; result+=hex[c&15]; }
    else result+=c;
  }
  return result+'"';
}

int main() {
  HeadlessConsole console;
  cpu.ioCtrl->screenKeyboardDevice->setConsole(&console);
  cpu.clear();
  cpu.reset();
  cpu.memorySize=16;
  uint64_t nextInterrupt=1000000;
  bool halted=false, loaded=false;
  uint64_t executed=0;
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream input(line);
    std::string command, error;
    bool inspect=false;
    std::string extra;
    input>>command;
    try {
      if (command=="quit") break;
      if (command=="cpu") {
        int model=0; input>>model;
        if (loaded || executed) throw std::runtime_error("select CPU before loading");
        if (model==2200) cpu.setCPUtype2200();
        else if (model==5500) cpu.setCPUtype5500();
        else if (model==6600) cpu.setCPUtype6600();
        else throw std::runtime_error("CPU must be 2200, 5500 or 6600");
        cpu.reset();
        cpu.P=0;
        for (int page=0; page<16; ++page) {
          cpu.memory->sectorTable[page].physicalPage=page;
          cpu.memory->sectorTable[page].accessEnable=true;
          cpu.memory->sectorTable[page].writeEnable=page!=15;
        }
      } else if (command=="disk-model") {
        int model=0; input>>model;
        if (!input || !cpu.ioCtrl->disk9370Device->setModel(model))
          throw std::runtime_error("disk-model requires 9370 or 9374, before attaching images");
      } else if (command=="disk-protect") {
        int drive=-1, value=-1; input>>drive>>value;
        int count=cpu.ioCtrl->disk9370Device->getModel()==9374 ? 16 : 8;
        if (!input || drive<0 || drive>=count || (value!=0 && value!=1))
          throw std::runtime_error("disk-protect requires a valid drive and 0|1");
        cpu.ioCtrl->disk9370Device->setWriteProtected(drive,value);
      } else if (command=="disk-state") {
        auto * disk=cpu.ioCtrl->disk9370Device;
        const auto & stats=disk->statistics;
        extra=",\"disk\":{\"model\":"+std::to_string(disk->getModel())
          +",\"reads\":"+std::to_string(stats.reads)+",\"writes\":"+std::to_string(stats.writes)
          +",\"formats\":"+std::to_string(stats.formats)+",\"seeks\":"+std::to_string(stats.seeks)
          +",\"errors\":"+std::to_string(stats.errors)+",\"max_cylinder\":"+std::to_string(stats.maxCylinder)
          +",\"max_head\":"+std::to_string(stats.maxHead)+",\"max_sector\":"+std::to_string(stats.maxSector)+"}";
      } else if (command=="cassette-state") {
        auto * cassette=cpu.ioCtrl->cassetteDevice;
        extra=",\"cassette\":{\"deck\":"+std::to_string(cassette->selectedDeck())
          +",\"status\":"+std::to_string(cassette->cassetteStatus())
          +",\"running\":"+(cassette->transportRunning()?"true":"false")+",\"decks\":[";
        for (int deck=0; deck<2; ++deck) {
          auto * tape=cassette->tapeDrive[deck];
          if (deck) extra+=",";
          extra+="{\"position\":"+std::to_string(tape->position())
            +",\"open\":"+(tape->isOpen()?"true":"false")
            +",\"write_protected\":"+(tape->isWriteProtected()?"true":"false")+"}";
        }
        extra+="]}";
      } else if (command=="floppy") {
        int drive=-1; input>>drive;
        std::string path; std::getline(input>>std::ws,path);
        if (!input || drive<0 || drive>3 || path.empty())
          throw std::runtime_error("floppy requires drive 0..3 and path");
        int result=cpu.ioCtrl->floppyDevice->openFile(drive,path,true,false);
        if (result!=0 && result!=FILE_HAS_BAD_BLOCKS)
          throw std::runtime_error("unable to open floppy image");
      } else if (command=="disk") {
        int drive=-1, writeProtect=-1;
        input>>drive>>writeProtect;
        std::string path; std::getline(input>>std::ws,path);
        if (drive<0 || drive>15 || (writeProtect!=0 && writeProtect!=1) || path.empty())
          throw std::runtime_error("disk requires logical drive, write protection (0|1), and path");
        if (cpu.ioCtrl->disk9370Device->openFile(drive,path,writeProtect)!=0)
          throw std::runtime_error("unable to open disk image for this controller");
      } else if (command=="printer") {
        std::string path; std::getline(input>>std::ws,path);
        if (path.empty() || cpu.ioCtrl->localPrinterDevice->openFile(0,path)!=0)
          throw std::runtime_error("unable to open printer output");
      } else if (command=="tape-stop") {
        if (cpu.ioCtrl->cassetteDevice->exTStop()!=0)
          throw std::runtime_error("unable to finish cassette write");
      } else if (command=="tape" || command=="tape-create") {
        int drive=-1; input>>drive;
        std::string path; std::getline(input>>std::ws,path);
        if (!input || drive<0 || drive>1 || path.empty())
          throw std::runtime_error("tape requires deck 0|1 and path");
        if (cpu.ioCtrl->cassetteDevice->transportRunning())
          throw std::runtime_error("stop the cassette transport before changing media");
        bool opened=command=="tape-create"
          ? cpu.ioCtrl->cassetteDevice->tapeDrive[drive]->createFile(path)
          : cpu.ioCtrl->cassetteDevice->openFile(drive,path,true);
        if (!opened)
          throw std::runtime_error("unable to open cassette image");
        cpu.ioCtrl->cassetteDevice->tapeDrive[drive]->rewind();
      } else if (command=="load") {
        if (loaded) throw std::runtime_error("start a new process to load another tape");
        std::string path; std::getline(input>>std::ws, path);
        auto * tape=cpu.ioCtrl->cassetteDevice->tapeDrive[0];
        if (!cpu.ioCtrl->cassetteDevice->openFile(0,path,true) || !tape->loadBoot([](int address, unsigned char data) { cpu.memory->physicalMemoryWrite(address, data); }))
          throw std::runtime_error("unable to load tape bootstrap");
        loaded=true;
        cpu.P=0;
        halted=false;
      } else if (command=="run") {
        uint64_t count=0; input>>count;
        if (!input || count==0 || count>100000000) throw std::runtime_error("run requires 1..100000000 instructions");
        if (!loaded && cpu.is2200) throw std::runtime_error("load a tape first");
        running=true;
        for (uint64_t i=0; i<count && running && !halted; ++i) {
          halted=cpu.execute()!=0;
          ++executed;
          dispatchTimers();
          if (nanoseconds(cpu.totalInstructionTime)>=nextInterrupt) {
            cpu.interruptPending=1;
            nextInterrupt=(nanoseconds(cpu.totalInstructionTime)/1000000+1)*1000000;
          }
        }
        running=false;
      } else if (command=="io-trace") {
        std::string path; std::getline(input>>std::ws,path);
        if (ioTraceFile) fclose(ioTraceFile);
        ioTraceFile=fopen(path.c_str(),"w");
        if (!ioTraceFile) throw std::runtime_error("cannot open I/O trace");
      } else if (command=="trace") {
        std::string path; std::getline(input>>std::ws,path);
        if (traceFile) fclose(traceFile);
        traceFile=fopen(path.c_str(),"w");
        if (!traceFile) throw std::runtime_error("cannot open trace");
        cpu.traceEnabled=true; cpu.octal=true;
      } else if (command=="pc") {
        int address=-1; input>>address;
        if (!input || address<0 || address>cpu.pMask) throw std::runtime_error("invalid PC");
        cpu.P=address;
        halted=false;
      } else if (command=="memory") {
        int address=-1, count=0; input>>address>>count;
        if (!input || address<0 || count<1 || count>256 || address+count>cpu.memory->size()) throw std::runtime_error("memory requires address and count (1..256)");
        extra=",\"memory\":[";
        for (int i=0; i<count; ++i) {
          if (i) extra+=",";
          extra+=std::to_string(cpu.memory->physicalMemoryRead(address+i));
        }
        extra+="]";
      } else if (command=="font-state") {
        const auto & font=console.characterGenerator;
        extra=",\"font\":{\"selections\":"+std::to_string(font.selections)
          +",\"writes\":"+std::to_string(font.totalWrites)+",\"glyphs\":[";
        for (unsigned code=0; code<128; ++code) {
          if (code) extra+=",";
          extra+="[";
          for (unsigned col=0; col<5; ++col) {
            if (col) extra+=",";
            extra+=std::to_string(font.glyphs[code][col]);
          }
          extra+="]";
        }
        extra+="],\"glyph_writes\":[";
        for (unsigned code=0; code<128; ++code) {
          if (code) extra+=",";
          extra+=std::to_string(font.writes[code]);
        }
        extra+="]}";
      } else if (command=="inspect") {
        inspect=true;
      } else if (command=="continue") {
        halted=false;
      } else if (command=="key") {
        int value=-1; input>>value;
        if (!input || value<0 || value>255) throw std::runtime_error("key requires a native byte 0..255");
        if (cpu.ioCtrl->screenKeyboardDevice->keyboardReady()) throw std::runtime_error("keyboard latch still occupied");
        cpu.ioCtrl->screenKeyboardDevice->updateKbd(value);
      } else if (command=="button") {
        std::string name; int value=-1; input>>name>>value;
        if (!input || (value!=0 && value!=1)) throw std::runtime_error("button requires keyboard|display and 0|1");
        if (name=="keyboard") console.keyboardButton=value;
        else if (name=="display") console.displayButton=value;
        else throw std::runtime_error("unknown button");
      } else if (command!="screen" && command!="state") throw std::runtime_error("unknown command");
    } catch (const std::exception & e) { error=e.what(); }
    std::string debug;
    if (inspect) {
      for (int bank=0; bank<2; ++bank) {
        debug += "bank="+std::to_string(bank)+" ";
        for (int r=0; r<8; ++r) debug += std::to_string(cpu.regSets[bank].regs[r])+" ";
        debug += "C="+std::to_string(cpu.flagCarry[bank])+" Z="+std::to_string(cpu.flagZero[bank])+" S="+std::to_string(cpu.flagSign[bank])+" P="+std::to_string(cpu.flagParity[bank])+"\n";
      }
      debug += "selected="+std::to_string(cpu.setSel)+" stackptr="+std::to_string(cpu.stackptr)+"\n";
      for (const auto & instruction : cpu.instructionTrace) {
        char buffer[128];
        int prefix=0;
        for (int candidate : {0022, 0062, 0111, 0113, 0115, 0117, 0174, 0176}) if (instruction.data[0]==candidate) prefix=candidate;
        cpu.disassembleLine(buffer, sizeof buffer, true, instruction.address, [instruction,prefix](int address) { return instruction.data[(address-instruction.address+(prefix?1:0)) % 5]; }, prefix);
        debug += std::to_string(instruction.address)+": "+buffer+"\n";
      }
    }
    std::cout<<"{\"ok\":"<<(error.empty()?"true":"false")<<",\"error\":"<<quote(error)
      <<",\"halted\":"<<(halted?"true":"false")<<",\"pc\":"<<cpu.P
      <<",\"instructions\":"<<executed<<",\"time_ns\":"<<nanoseconds(cpu.totalInstructionTime)
      <<",\"keyboard_ready\":"<<(cpu.ioCtrl->screenKeyboardDevice->keyboardReady()?"true":"false")
      <<",\"keyboard_light\":"<<(console.keyboardLight?"true":"false")
      <<",\"display_light\":"<<(console.displayLight?"true":"false")
      <<",\"beeps\":"<<console.beeps
      <<",\"debug\":"<<quote(debug)<<",\"screen\":"<<quote(console.snapshot())<<extra<<"}"<<std::endl;
  }
  if (traceFile) fclose(traceFile);
  if (ioTraceFile) fclose(ioTraceFile);
  for (auto * timer : timers) delete timer;
}
