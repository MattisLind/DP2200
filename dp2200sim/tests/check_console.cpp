// Exercise the real curses display, including the bottom-right ROM monitor cells.
#include "dp2200Window.h"
#include "HeadlessConsole.h"
#include "dp2200_io_sim.h"
#include <cstdio>
#include <stdexcept>
#include <cstdlib>
#include <fstream>

// These UI/keyboard collaborators are not used by display operations.
registerWindow *rw=nullptr;
void printLog(const char *, const char *, ...) {}
int registerWindow::setKeyboardButton(bool) { return 0; }
int registerWindow::setDisplayButton(bool) { return 0; }
int registerWindow::setKeyboardLight(bool) { return 0; }
int registerWindow::setDisplayLight(bool) { return 0; }
bool registerWindow::getKeyboardButton() { return false; }
bool registerWindow::getDisplayButton() { return false; }
void IOController::ScreenKeyboardDevice::updateKbd(int) {}

int main() {
  initscr();
  try {
    dp2200Window screen(nullptr);
    HeadlessConsole headless;
    Console *consoles[]={&screen, &headless};
    auto at=[&](int x,int y,int expected) {
      int actual=mvwinch(curscr,y+1,x+1) & A_CHARTEXT;
      if (actual!=expected || (unsigned char)headless.snapshot()[y*81+x]!=expected)
        throw std::runtime_error("wrong character at "+std::to_string(x)+","+
                                 std::to_string(y)+": "+std::to_string(actual));
    };
    auto put=[&](int x,int y,int ch) {
      for (auto c:consoles) { c->setCursorX(x); c->setCursorY(y); c->writeCharacter(ch); }
    };
    put(0,0,'A'); put(0,11,'Z'); put(79,11,'9');
    at(0,0,'A'); at(0,11,'Z'); at(79,11,'9'); // Must not auto-scroll.
    for (auto c:consoles) { c->incrementXPos(); c->writeCharacter('X'); }
    at(79,11,'9'); // Column 80 is off-screen, not clamped or wrapped.
    put(2,2,'B'); put(4,2,'C'); put(0,3,'D');
    for (auto c:consoles) { c->setCursorX(3); c->setCursorY(2); c->eraseFromCursorToEndOfLine(); }
    at(2,2,'B'); at(4,2,' '); at(0,3,'D'); at(79,11,'9');
    for (auto c:consoles) c->eraseFromCursorToEndOfFrame();
    at(2,2,'B'); at(0,3,' '); at(0,11,' '); at(79,11,' ');
    put(0,0,'A'); put(0,10,'Y'); put(0,11,'Z');
    for (auto c:consoles) c->scrollUp();
    at(0,9,'Y'); at(0,10,'Z'); at(0,11,' ');
    for (auto c:consoles) c->scrollDown();
    at(0,0,' '); at(0,10,'Y'); at(0,11,'Z');
    put(20,5,'Q'); put(21,5,0); put(22,5,'R');
    if ((mvwinch(curscr,6,22)&A_CHARTEXT)!=' ')
      throw std::runtime_error("NUL rendered as multiple terminal characters");
    at(20,5,'Q'); at(22,5,'R');
    put(255,255,'X');
    for (auto c:consoles) c->eraseFromCursorToEndOfFrame();
    at(0,11,'Z'); at(22,5,'R');
#if DP2200_WITH_SDL
    // Redefine an already displayed glyph, with asymmetric pixels to catch
    // transposition and reversed bit order. No screen write may be required.
    put(0,0,'A');
    screen.setCharGenChar('A');
    for (int value:{0100,0,0,0,1}) screen.updateCharGen(value);
    screen.updateScreen();
    SDL_Renderer *renderer=SDL_GetRenderer(SDL_GetWindowFromID(1));
    if (!renderer) throw std::runtime_error(SDL_GetError());
    SDL_Surface *pixels=SDL_CreateRGBSurfaceWithFormat(0,WINDOW_W*2,WINDOW_H*2,32,SDL_PIXELFORMAT_ARGB8888);
    auto pixel=[&](int x,int y,bool green) {
      if (SDL_RenderReadPixels(renderer,nullptr,pixels->format->format,pixels->pixels,pixels->pitch)!=0)
        throw std::runtime_error(SDL_GetError());
      auto row=reinterpret_cast<Uint32 *>(static_cast<char *>(pixels->pixels)+y*2*pixels->pitch);
      Uint8 r,g,b; SDL_GetRGB(row[x*2],pixels->format,&r,&g,&b);
      if (r!=0 || b!=0 || g!=(green?255:0)) throw std::runtime_error("wrong SDL glyph pixel");
    };
    pixel(PADDING+1,PADDING+1,true);
    pixel(PADDING+5,PADDING+7,true);
    pixel(PADDING+1,PADDING+7,false);
    screen.setCharGenChar('A');
    for (int value:{0,0,0100,0,0}) screen.updateCharGen(value);
    screen.updateScreen();
    pixel(PADDING+1,PADDING+1,false);
    pixel(PADDING+3,PADDING+1,true);
    if (const char *frame=std::getenv("DP_FONT_FRAME")) {
      std::ifstream input(frame,std::ios::binary);
      unsigned char data[1600];
      if (!input.read(reinterpret_cast<char *>(data),sizeof data))
        throw std::runtime_error("font frame must contain 640 font bytes and 960 screen codes");
      screen.setCharGenChar(0);
      for (unsigned i=0; i<640; ++i) screen.updateCharGen(data[i]);
      for (int y=0; y<12; ++y)
        for (int x=0; x<80; ++x) put(x,y,data[640+y*80+x]);
      screen.updateScreen();
      if (SDL_RenderReadPixels(renderer,nullptr,pixels->format->format,pixels->pixels,pixels->pitch)!=0)
        throw std::runtime_error(SDL_GetError());
      if (const char *capture=std::getenv("DP_FONT_CAPTURE"))
        if (SDL_SaveBMP(pixels,capture)!=0) throw std::runtime_error(SDL_GetError());
      // Allow an optional real-window smoke test to show the captured DOS font.
      for (int i=0; i<25; ++i) { screen.updateScreen(); SDL_Delay(20); }
    }
    SDL_FreeSurface(pixels);
#endif
    endwin();
    puts("PASS: curses/headless positioning, erase, scroll, control glyphs and off-screen writes");
#if DP2200_WITH_SDL
    puts("PASS: SDL glyph orientation and repaint after live font redefinition");
#endif
    return 0;
  } catch (const std::exception &e) {
    endwin(); fprintf(stderr,"FAIL: %s\n",e.what()); return 1;
  }
}
