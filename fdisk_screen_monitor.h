#ifndef __FDISK_SCREEN_MONITOR_H__
#define __FDISK_SCREEN_MONITOR_H__

void setup_screen(void);
void display_footer(unsigned char index);
void write_line(char *s, char col);
void write_line_len(char *s, char col, char length);
void write_line_raw(char *s, char col, char length);
void recolour_last_line(char colour);
void screen_colour_line(unsigned char line, unsigned char colour);
#define screen_colour_line_segment(LA, W, C) lfill(LA + (0x1f800 - SCREEN_ADDRESS), C, W)
void set_screen_attributes(long p, unsigned char count, unsigned char attr);
char read_line(char *buffer, unsigned char maxlen);
void fatal_error(unsigned char *filename, unsigned int line_number);
#define FATAL_ERROR fatal_error(__FILE__, __LINE__)

#endif /* __FDISK_SCREEN_MONITOR_H__ */