/*
 * build-tests/__builtin_ctzll.c
 *
 * test if the compiler has __builtin_ctzll().
 */

int main(void) {
  return (__builtin_ctzll(0x100000000LL) == 32) ? 1 : 0;
}
