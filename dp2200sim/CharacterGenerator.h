#ifndef DP2200_CHARACTER_GENERATOR_H
#define DP2200_CHARACTER_GENERATOR_H
#include <array>
#include <cstdint>

// The RAM display stores 128 glyphs, five columns of seven bits each.
// Bit 6 is the top pixel. Power-up contents are undefined; ROM/software loads them.
class CharacterGenerator {
  unsigned character=0, column=0;
public:
  std::array<std::array<unsigned char,5>,128> glyphs{};
  std::array<uint64_t,128> writes{};
  uint64_t selections=0, totalWrites=0;
  void select(int value) { character=value & 0177; column=0; ++selections; }
  void write(int value) {
    glyphs[character][column]=value & 0177;
    ++writes[character]; ++totalWrites;
    if (++column==5) { column=0; character=(character+1)&0177; }
  }
  bool pixel(unsigned code,unsigned x,unsigned y) const {
    return x<5 && y<7 && (glyphs[code&0177][x] & (0100 >> y));
  }
};
#endif
