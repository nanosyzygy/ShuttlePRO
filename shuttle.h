
// Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <linux/input.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <regex.h>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>


// delay in ms before processing each XTest event
// CurrentTime means no delay
#define DELAY CurrentTime

// protocol for events from the shuttlepro HUD device
//
// ev.type values:
#define EVENT_TYPE_DONE 0
#define EVENT_TYPE_KEY 1
#define EVENT_TYPE_JOGSHUTTLE 2
#define EVENT_TYPE_ACTIVE_KEY 4

// ev.code when ev.type == KEY
#define EVENT_CODE_KEY1 256
// KEY2 257, etc...

// ev.value when ev.type == KEY
// 1 -> PRESS; 0 -> RELEASE

// ev.code when ev.type == JOGSHUTTLE
#define EVENT_CODE_JOG 7
#define EVENT_CODE_SHUTTLE 8

// ev.value when ev.code == JOG
// 8 bit value changing by one for each jog step

// ev.value when ev.code == SHUTTLE
// -7 .. 7 encoding shuttle position

// we define these as extra KeySyms to represent mouse events
#define XK_Button_0 0x2000000 // just an offset, not a real button
#define XK_Button_1 0x2000001
#define XK_Button_2 0x2000002
#define XK_Button_3 0x2000003
#define XK_Scroll_Up 0x2000004
#define XK_Scroll_Down 0x2000005

#define PRESS 1
#define RELEASE 2
#define PRESS_RELEASE 3
#define HOLD 4

#define NUM_KEYS 15
#define NUM_SHUTTLES 15
#define NUM_SHUTTLE_INCRS 2
#define NUM_JOGS 2

typedef struct _stroke {
  struct _stroke *next;
  KeySym keysym;
  int press; // zero -> release, non-zero -> press
} stroke;

#define KJS_KEY_DOWN 1
#define KJS_KEY_UP 2
#define KJS_SHUTTLE 3
#define KJS_SHUTTLE_INCR 4
#define KJS_JOG 5

typedef struct _translation {
  struct _translation *next;
  char *name;
  int is_default;
  regex_t regex;
  stroke *key_down[NUM_KEYS];
  stroke *key_up[NUM_KEYS];
  stroke *shuttle[NUM_SHUTTLES];
  stroke *shuttle_incr[NUM_SHUTTLE_INCRS];
  stroke *jog[NUM_JOGS];
} translation;

extern translation *get_translation(char *win_title, char *win_class);
extern void print_stroke_sequence(char *name, char *up_or_down, stroke *s);
extern int debug_regex, debug_strokes, debug_keys;
extern int default_debug_regex, default_debug_strokes, default_debug_keys;
extern char *config_file_name;
