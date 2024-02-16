#include <stdio.h>

int main () {
  int i,j;
  printf("unsigned char firmware[] = {\n");
  for (j = 0; j < 240; j++) {
    printf("             0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o,\n", getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar());
  }
  printf("             0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o, 0%03o\n", getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar(), getchar());
  printf("};\n");
}