#include <stdio.h>

int main(void) {
  int byte;
  unsigned int count = 0;
  puts("const unsigned char firmware[] = {");
  while ((byte = getchar()) != EOF) {
    if (count % 16 == 0) printf("  ");
    printf("0%03o,", byte);
    ++count;
    if (count % 16 == 0) putchar('\n');
    else putchar(' ');
  }
  if (count % 16 != 0) putchar('\n');
  puts("};");
  return ferror(stdin) || ferror(stdout) ? 1 : 0;
}
