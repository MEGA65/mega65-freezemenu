#include <stdio.h>

int main(int argc, char** argv)
{
  FILE* f = fopen("ascii.h", "wt");
  fprintf(f, "#ifndef __ASCII_H__\n#define __ASCII_H__\n\n");
  for (int i = 0; i < 256; i++)
    fprintf(f, "#pragma charmap (0x%02x,0x%02x)\n", i, i);
  fprintf(f, "\n#endif /* __ASCII_H__ */");
  fclose(f);
}
