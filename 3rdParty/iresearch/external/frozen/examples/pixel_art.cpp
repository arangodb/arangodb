#if 0
g++ $0 -std=c++14 -Iinclude && ./a.out && rm -f a.out && qiv panda.ppm 1up.ppm
exit
#else

#include <array>
#include <frozen/map.h>
#include <fstream>

constexpr frozen::map<char, std::array<char, 3>, 5> Tans{
    {'R', {(char)0xFF, (char)0x00, (char)0x00}},
    {'G', {(char)0x00, (char)0xFF, (char)0x00}},
    {'B', {(char)0x00, (char)0x00, (char)0xFF}},
    {'#', {(char)0x00, (char)0x00, (char)0x00}},
    {' ', {(char)0xFF, (char)0xFF, (char)0xFF}},
};

constexpr unsigned itoa(unsigned char * start, unsigned i) {
  constexpr unsigned step = sizeof(unsigned) * 3;
    for(unsigned k = 0; k < step; ++k)
      *start++ = ' ';
    do {
      *--start = '0' + i % 10;
      i /= 10;
    } while(i);
    return step;
}

template <unsigned H, unsigned W> struct ppm {
  unsigned char bytes[9 /* fixed header*/ + sizeof(unsigned) * 3 * 2 /* to hold sizes */ + 3 * H * W];

  constexpr ppm(unsigned char const *data) : bytes{0} {
    unsigned j = 0;
    bytes[j++] = 'P';
    bytes[j++] = '6';
    bytes[j++] = ' ';

    j += itoa(bytes + j, H);

    bytes[j++] = ' ';

    j += itoa(bytes + j, W);

    bytes[j++] = ' ';
    bytes[j++] = '2';
    bytes[j++] = '5';
    bytes[j++] = '5';
    bytes[j++] = '\n';
    for (unsigned i = 0; i < H * W; ++i) {
      auto const code = Tans.find(data[i])->second;
      bytes[j + 3 * i + 0] = code[0];
      bytes[j + 3 * i + 1] = code[1];
      bytes[j + 3 * i + 2] = code[2];
    }
  }

  void save(char const path[]) const {
    std::ofstream out{path, std::ios::binary};
    out.write((char *)bytes, sizeof bytes);
  }
};

void make_panda() {
  constexpr unsigned char bytes[] = "   ###              ###   "
                                    "  ##### ########## #####  "
                                    " #######          ####### "
                                    " ####                #### "
                                    " ###                  ### "
                                    "  #                    #  "
                                    "  #                    #  "
                                    " #    ###        ###    # "
                                    " #   #####      #####   # "
                                    " #  #### #      # ####  # "
                                    " #  #### #      # ####  # "
                                    " #  #####        #####  # "
                                    " #   ###    ##    ###   # "
                                    "  #         ##         #  "
                                    "  ####  RRR    RRR  ####  "
                                    " ######RRRRR  RRRRR###### "
                                    " ######RRRRRRRRRRRR###### "
                                    " ######RRRRRRRRRRRR###### "
                                    " ######RRRRRRRRRRRR###### "
                                    " #######RRRRRRRRRR####### "
                                    "  #####  RRRRRRRR  ####   "
                                    "   ###    RRRRRR    ##    "
                                    "           RRRR           ";
  constexpr ppm<26, 22> some{bytes};
  some.save("panda.ppm");
}

void make_1up() {
  constexpr unsigned char bytes[] = "      ######      "
                                    "    ##GGGG  ##    "
                                    "   #  GGGG    #   "
                                    "  #  GGGGGG    #  "
                                    "  # GG    GG   #  "
                                    " #GGG      GGGGG# "
                                    " #GGG      GG  G# "
                                    " # GG      G    # "
                                    " #  GG    GG    # "
                                    " #  GGGGGGGGG  G# "
                                    " # GG########GGG# "
                                    "  ###  #  #  ###  "
                                    "   #   #  #   #   "
                                    "   #          #   "
                                    "    #        #    "
                                    "     ########     "

      ;
  constexpr ppm<18, 16> some{bytes};
  some.save("1up.ppm");
}

int main() {
  make_panda();
  make_1up();
  return 0;
}

#endif
