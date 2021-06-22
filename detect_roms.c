#include <stdio.h>
#include <string.h>
#include <strings.h>

#define MAX_ROMS 256
char* rom_names[MAX_ROMS] = { 0 };
unsigned char roms[MAX_ROMS][8192];
int roms_detected[MAX_ROMS];
int rom_count = 0;

int main(int argc, char** argv)
{
  for (int i = 1; i < argc; i++) {
    FILE* f = fopen(argv[i], "r");
    if (!f) {
      fprintf(stderr, "Could not open ROM file '%s'\n", argv[i]);
    }
    printf("Looking at ROM file '%s'\n", argv[i]);
    // Read twice, in case it is a combined BASIC + KERNAL ROM
    fread(roms[rom_count], 8192, 1, f);
    fread(roms[rom_count], 8192, 1, f);
    fclose(f);

    int j;
    for (j = 0; j < rom_count; j++)
      if (!bcmp(roms[rom_count], roms[j], 8192)) {
        printf("ROM '%s' is a duplicate of '%s' -- ignoring.\n", argv[j], rom_names[j]);
        break;
      }
    if (j == rom_count) {
      rom_names[rom_count] = strdup(argv[i]);
      rom_count++;
    }
  }

  printf("Read %d roms\n", rom_count);

  // Start with all ROMs needing detecting
  for (int r = 0; r < rom_count; r++) {
    roms_detected[r] = 0;
  }

  int detected_count = 0;

  while (detected_count < rom_count) {
    for (int r = 0; r < rom_count; r++) {
      int o;

      if (roms_detected[r])
        continue;

      for (o = 0; o < 8192; o++) {
        int matches = 0;
        for (int j = 0; j < rom_count; j++) {
          // Only check for uniqueness among ROMs not yet detected.
          if (!roms_detected[j]) {
            if (roms[r][o] == roms[j][o]) {
              matches++;
            }
          }
        }
        if (matches == 1) {
          printf("if (freeze_peek(0x%xL)==0x%02x) return \"%s\";\n", 0x2e000 + o, roms[r][o], rom_names[r]);
          roms_detected[r] = 1;
          detected_count++;
          break;
        }
      }
      // if (o==8192) printf("// No single byte identifies ROM '%s' yet\n",rom_names[r]);
    }
  }

  return 0;
}
