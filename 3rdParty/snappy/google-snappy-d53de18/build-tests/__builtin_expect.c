/*
 * build-tests/__builtin_expect.c
 *
 * test if the compiler has __builtin_expect().
 */

int main(void) {
  return __builtin_expect(1, 1) ? 1 : 0;
}
