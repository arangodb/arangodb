#include <windows.h>

#include <Basics/TerminalUtils.h>

namespace triagens {
  namespace basics {
    namespace TerminalUtils {

      int columnsWidth () {
        HANDLE hOut;
        CONSOLE_SCREEN_BUFFER_INFO SBInfo;

        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) {
          return DEFAULT_COLUMNS;
        }
        if (GetConsoleScreenBufferInfo(hOut, &SBInfo) == 0) {
          return DEFAULT_COLUMNS;
        }
        return SBInfo.dwSize.X;
      }
    }
  }
}
