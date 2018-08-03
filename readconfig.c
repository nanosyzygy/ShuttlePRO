
/*

  Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)

  Read and process the configuration file ~/.shuttlepro

  Lines starting with # are comments.

  Sequence of sections defining translation classes, each section is:

  [name] regex
  K<1..15> output
  S<-7..7> output
  I<LR> output
  J<LR> output

  When focus is on a window whose title matches regex, the following
  translation class is in effect.  An empty regex for the last class
  will always match, allowing default translations.  Any output
  sequences not bound in a matched section will be loaded from the
  default section if they are bound there.

  Each "[name] regex" line introduces the list of key and shuttle
  translations for the named translation class.  The name is only used
  for debugging output, and needn't be unique.  The following lines
  with K, S, and J labels indicate what output should be produced for
  the given keypress, shuttle position, shuttle direction, or jog direction.

  output is a sequence of one or more key codes with optional up/down
  indicators, or strings of printable characters enclosed in double
  quotes, separated by whitespace.  Sequences bound to keys may have
  separate press and release sequences, separated by the word RELEASE.

  Examples:

  K1 "qwer"
  K2 XK_Right
  K3 XK_Alt_L/D XK_Right
  K4 "V" XK_Left XK_Page_Up "v"
  K5 XK_Alt_L/D "v" XK_Alt_L/U "x" RELEASE "q"

  Any keycode can be followed by an optional /D, /U, or /H, indicating
  that the key is just going down (without being released), going up,
  or going down and being held until the shuttlepro key is released.

  So, in general, modifier key codes will be followed by /D, and
  precede the keycodes they are intended to modify.  If a sequence
  requires different sets of modifiers for different keycodes, /U can
  be used to release a modifier that was previously pressed with /D.

  At the end of shuttle and jog sequences, all down keys will be
  released.

  Keypresses translate to separate press and release sequences.

  At the end of the press sequence for key sequences, all down keys
  marked by /D will be released, and the last key not marked by /D,
  /U, or /H will remain pressed.  The release sequence will begin by
  releasing the last held key.  If keys are to be pressed as part of
  the release sequence, then any keys marked with /D will be repressed
  before continuing the sequence.  Keycodes marked with /H remain held
  between the press and release sequences.

 */

#include "shuttle.h"

int debug_regex = 0;
int debug_strokes = 0;

char *
allocate(size_t len)
{
  char *ret = (char *)malloc(len);
  if (ret == NULL) {
    fprintf(stderr, "Out of memory!\n");
    exit(1);
  }
  return ret;
}

char *
alloc_strcat(char *a, char *b)
{
  size_t len = 0;
  char *result;

  if (a != NULL) {
    len += strlen(a);
  }
  if (b != NULL) {
    len += strlen(b);
  }
  result = allocate(len+1);
  result[0] = '\0';
  if (a != NULL) {
    strcpy(result, a);
  }
  if (b != NULL) {
    strcat(result, b);
  }
  return result;
}

static char *read_line_buffer = NULL;
static int read_line_buffer_length = 0;

#define BUF_GROWTH_STEP 1024


// read a line of text from the given file into a managed buffer.
// returns a partial line at EOF if the file does not end with \n.
// exits with error message on read error.
char *
read_line(FILE *f, char *name)
{
  int pos = 0;
  char *new_buffer;
  int new_buffer_length;

  if (read_line_buffer == NULL) {
    read_line_buffer_length = BUF_GROWTH_STEP;
    read_line_buffer = allocate(read_line_buffer_length);
    read_line_buffer[0] = '\0';
  }

  while (1) {
    read_line_buffer[read_line_buffer_length-1] = '\377';
    if (fgets(read_line_buffer+pos, read_line_buffer_length-pos, f) == NULL) {
      if (feof(f)) {
	if (pos > 0) {
	  // partial line at EOF
	  return read_line_buffer;
	} else {
	  return NULL;
	}
      }
      perror(name);
      exit(1);
    }
    if (read_line_buffer[read_line_buffer_length-1] != '\0') {
      return read_line_buffer;
    }
    if (read_line_buffer[read_line_buffer_length-2] == '\n') {
      return read_line_buffer;
    }
    new_buffer_length = read_line_buffer_length + BUF_GROWTH_STEP;
    new_buffer = allocate(new_buffer_length);
    memcpy(new_buffer, read_line_buffer, read_line_buffer_length);
    free(read_line_buffer);
    pos = read_line_buffer_length-1;
    read_line_buffer = new_buffer;
    read_line_buffer_length = new_buffer_length;
  }
}

static translation *first_translation_section = NULL;
static translation *last_translation_section = NULL;

translation *default_translation;

translation *
new_translation_section(char *name, char *regex)
{
  translation *ret = (translation *)allocate(sizeof(translation));
  int err;
  int i;

  if (debug_strokes) {
    printf("------------------------\n[%s] %s\n\n", name, regex);
  }
  ret->next = NULL;
  ret->name = alloc_strcat(name, NULL);
  if (regex == NULL || *regex == '\0') {
    ret->is_default = 1;
    default_translation = ret;
  } else {
    ret->is_default = 0;
    err = regcomp(&ret->regex, regex, REG_NOSUB);
    if (err != 0) {
      regerror(err, &ret->regex, read_line_buffer, read_line_buffer_length);
      fprintf(stderr, "error compiling regex for [%s]: %s\n", name, read_line_buffer);
      regfree(&ret->regex);
      free(ret->name);
      free(ret);
      return NULL;
    }
  }
  for (i=0; i<NUM_KEYS; i++) {
    ret->key_down[i] = NULL;
    ret->key_up[i] = NULL;
  }
  for (i=0; i<NUM_SHUTTLES; i++) {
    ret->shuttle[i] = NULL;
  }
  for (i=0; i<NUM_SHUTTLE_INCRS; i++) {
    ret->shuttle_incr[i] = NULL;
  }
  for (i=0; i<NUM_JOGS; i++) {
    ret->jog[i] = NULL;
  }
  if (first_translation_section == NULL) {
    first_translation_section = ret;
    last_translation_section = ret;
  } else {
    last_translation_section->next = ret;
    last_translation_section = ret;
  }
  return ret;
}

void
free_strokes(stroke *s)
{
  stroke *next;
  while (s != NULL) {
    next = s->next;
    free(s);
    s = next;
  }
}

void
free_translation_section(translation *tr)
{
  int i;

  if (tr != NULL) {
    free(tr->name);
    if (!tr->is_default) {
      regfree(&tr->regex);
    }
    for (i=0; i<NUM_KEYS; i++) {
      free_strokes(tr->key_down[i]);
      free_strokes(tr->key_up[i]);
    }
    for (i=0; i<NUM_SHUTTLES; i++) {
      free_strokes(tr->shuttle[i]);
    }
    for (i=0; i<NUM_SHUTTLE_INCRS; i++) {
      free_strokes(tr->shuttle_incr[i]);
    }
    for (i=0; i<NUM_JOGS; i++) {
      free_strokes(tr->jog[i]);
    }
    free(tr);
  }
}

void
free_all_translations(void)
{
  translation *tr = first_translation_section;
  translation *next;

  while (tr != NULL) {
    next = tr->next;
    free_translation_section(tr);
    tr = next;
  }
  first_translation_section = NULL;
  last_translation_section = NULL;
}

static char *config_file_name = NULL;
static time_t config_file_modification_time;

static char *token_src = NULL;

// similar to strtok, but it tells us what delimiter was found at the
// end of the token, handles double quoted strings specially, and
// hardcodes the delimiter set.
char *
token(char *src, char *delim_found)
{
  char *delims = " \t\n/\"";
  char *d;
  char *token_start;

  if (src == NULL) {
    src = token_src;
  }
  if (src == NULL) {
    *delim_found = '\0';
    return NULL;
  }
  token_start = src;
  while (*src) {
    d = delims;
    while (*d && *src != *d) {
      d++;
    }
    if (*d) {
      if (src == token_start) {
	src++;
	token_start = src;
	if (*d == '"') {
	  while (*src && *src != '"' && *src != '\n') {
	    src++;
	  }
	} else {
	  continue;
	}
      }
      *delim_found = *d;
      if (*src) {
	*src = '\0';
	token_src = src+1;
      } else {
	token_src = NULL;
      }
      return token_start;
    }
    src++;
  }
  token_src = NULL;
  *delim_found = '\0';
  if (src == token_start) {
    return NULL;
  }
  return token_start;
}

typedef struct _keysymmapping {
  char *str;
  KeySym sym;
} keysymmapping;

static keysymmapping key_sym_mapping[] = {
#include "keys.h"
  { "XK_Button_1", XK_Button_1 },
  { "XK_Button_2", XK_Button_2 },
  { "XK_Button_3", XK_Button_3 },
  { "XK_Scroll_Up", XK_Scroll_Up },
  { "XK_Scroll_Down", XK_Scroll_Down },
  { NULL, 0 }
};

KeySym
string_to_KeySym(char *str)
{
  size_t len = strlen(str) + 1;
  int i = 0;

  while (key_sym_mapping[i].str != NULL) {
    if (!strncmp(str, key_sym_mapping[i].str, len)) {
      return key_sym_mapping[i].sym;
    }
    i++;
  }
  return 0;
}

char *
KeySym_to_string(KeySym ks)
{
  int i = 0;

  while (key_sym_mapping[i].sym != 0) {
    if (key_sym_mapping[i].sym == ks) {
      return key_sym_mapping[i].str;
    }
    i++;
  }
  return NULL;
}

void
print_stroke(stroke *s)
{
  char *str;

  if (s != NULL) {
    str = KeySym_to_string(s->keysym);
    if (str == NULL) {
      printf("0x%x", (int)s->keysym);
      str = "???";
    }
    printf("%s/%c ", str, s->press ? 'D' : 'U');
  }
}

void
print_stroke_sequence(char *name, char *up_or_down, stroke *s)
{
  printf("%s[%s]: ", name, up_or_down);
  while (s) {
    print_stroke(s);
    s = s->next;
  }
  printf("\n");
}

stroke **first_stroke;
stroke *last_stroke;
stroke **press_first_stroke;
stroke **release_first_stroke;
int is_keystroke;
char *current_translation;
char *key_name;
int first_release_stroke; // is this the first stroke of a release?
KeySym regular_key_down;

#define NUM_MODIFIERS 64

stroke modifiers_down[NUM_MODIFIERS];
int modifier_count;

void
append_stroke(KeySym sym, int press)
{
  stroke *s = (stroke *)allocate(sizeof(stroke));

  s->next = NULL;
  s->keysym = sym;
  s->press = press;
  if (*first_stroke) {
    last_stroke->next = s;
  } else {
    *first_stroke = s;
  }
  last_stroke = s;
}

// s->press values in modifiers_down:
// PRESS -> down
// HOLD -> held
// PRESS_RELEASE -> released, but to be re-pressed if necessary
// RELEASE -> up

void
mark_as_down(KeySym sym, int hold)
{
  int i;

  for (i=0; i<modifier_count; i++) {
    if (modifiers_down[i].keysym == sym) {
      modifiers_down[i].press = hold ? HOLD : PRESS;
      return;
    }
  }
  if (modifier_count > NUM_MODIFIERS) {
    fprintf(stderr, "too many modifiers down in [%s]%s\n", current_translation, key_name);
    return;
  }
  modifiers_down[modifier_count].keysym = sym;
  modifiers_down[modifier_count].press = hold ? HOLD : PRESS;
  modifier_count++;
}

void
mark_as_up(KeySym sym)
{
  int i;

  for (i=0; i<modifier_count; i++) {
    if (modifiers_down[i].keysym == sym) {
      modifiers_down[i].press = RELEASE;
      return;
    }
  }
}

void
release_modifiers(int allkeys)
{
  int i;

  for (i=0; i<modifier_count; i++) {
    if (modifiers_down[i].press == PRESS) {
      append_stroke(modifiers_down[i].keysym, 0);
      modifiers_down[i].press = PRESS_RELEASE;
    } else if (allkeys && modifiers_down[i].press == HOLD) {
      append_stroke(modifiers_down[i].keysym, 0);
      modifiers_down[i].press = RELEASE;
    }
  }
}

void
re_press_temp_modifiers(void)
{
  int i;

  for (i=0; i<modifier_count; i++) {
    if (modifiers_down[i].press == PRESS_RELEASE) {
      append_stroke(modifiers_down[i].keysym, 1);
      modifiers_down[i].press = PRESS;
    }
  }
}

int
start_translation(translation *tr, char *which_key)
{
  char c;
  int k;
  int n;

  //printf("start_translation(%s)\n", which_key);

  if (tr == NULL) {
    fprintf(stderr, "need to start translation section before defining key: %s\n", which_key);
    return 1;
  }
  current_translation = tr->name;
  key_name = which_key;
  is_keystroke = 0;
  first_release_stroke = 0;
  regular_key_down = 0;
  modifier_count = 0;
  if (tolower(which_key[0]) == 'j' &&
      (tolower(which_key[1]) == 'l' || tolower(which_key[1]) == 'r') &&
      which_key[2] == '\0') {
    // JL, JR
    k = tolower(which_key[1]) == 'l' ? 0 : 1;
    first_stroke = &(tr->jog[k]);
  } else if (tolower(which_key[0]) == 'i' &&
      (tolower(which_key[1]) == 'l' || tolower(which_key[1]) == 'r') &&
      which_key[2] == '\0') {
    // IL, IR
    k = tolower(which_key[1]) == 'l' ? 0 : 1;
    first_stroke = &(tr->shuttle_incr[k]);
  } else {
    n = 0;
    sscanf(which_key, "%c%d%n", &c, &k, &n);
    if (n != (int)strlen(which_key)) {
      fprintf(stderr, "bad key name: [%s]%s\n", current_translation, which_key);
      return 1;
    }
    switch (c) {
    case 'k':
    case 'K':
      // K1 .. K15
      k = k - 1;
      if (k < 0 || k >= NUM_KEYS) {
	fprintf(stderr, "bad key name: [%s]%s\n", current_translation, which_key);
	return 1;
      }
      first_stroke = &(tr->key_down[k]);
      release_first_stroke = &(tr->key_up[k]);
      is_keystroke = 1;
      break;
    case 's':
    case 'S':
      // S-7 .. S7
      if (k < -7 || k > 7) {
	fprintf(stderr, "bad key name: [%s]%s\n", current_translation, which_key);
	return 1;
      }
      first_stroke = &(tr->shuttle[k+7]);
      break;
    default:
      fprintf(stderr, "bad key name: [%s]%s\n", current_translation, which_key);
      return 1;
    }
  }
  if (*first_stroke != NULL) {
    fprintf(stderr, "can't redefine key: [%s]%s\n", current_translation, which_key);
    return 1;
  }
  press_first_stroke = first_stroke;
  return 0;
}

void
add_keysym(KeySym sym, int press_release)
{
  //printf("add_keysym(0x%x, %d)\n", (int)sym, press_release);
  switch (press_release) {
  case PRESS:
    append_stroke(sym, 1);
    mark_as_down(sym, 0);
    break;
  case RELEASE:
    append_stroke(sym, 0);
    mark_as_up(sym);
    break;
  case HOLD:
    append_stroke(sym, 1);
    mark_as_down(sym, 1);
    break;
  case PRESS_RELEASE:
  default:
    if (first_release_stroke) {
      re_press_temp_modifiers();
    }
    if (regular_key_down != 0) {
      append_stroke(regular_key_down, 0);
    }
    append_stroke(sym, 1);
    regular_key_down = sym;
    first_release_stroke = 0;
    break;
  }
}

void
add_release(int all_keys)
{
  //printf("add_release(%d)\n", all_keys);
  release_modifiers(all_keys);
  if (!all_keys) {
    first_stroke = release_first_stroke;
  }
  if (regular_key_down != 0) {
    append_stroke(regular_key_down, 0);
  }
  regular_key_down = 0;
  first_release_stroke = 1;
}

void
add_keystroke(char *keySymName, int press_release)
{
  KeySym sym;

  if (is_keystroke && !strncmp(keySymName, "RELEASE", 8)) {
    add_release(0);
    return;
  }
  sym = string_to_KeySym(keySymName);
  if (sym != 0) {
    add_keysym(sym, press_release);
  } else {
    fprintf(stderr, "unrecognized KeySym: %s\n", keySymName);
  }
}

void
add_string(char *str)
{
  while (str && *str) {
    if (*str >= ' ' && *str <= '~') {
      add_keysym((KeySym)(*str), PRESS_RELEASE);
    }
    str++;
  }
}

void
finish_translation(void)
{
  //printf("finish_translation()\n");
  if (is_keystroke) {
    add_release(0);
  }
  add_release(1);
  if (debug_strokes) {
    if (is_keystroke) {
      print_stroke_sequence(key_name, "D", *press_first_stroke);
      print_stroke_sequence(key_name, "U", *release_first_stroke);
    } else {
      print_stroke_sequence(key_name, "", *first_stroke);
    }
    printf("\n");
  }
}

void
read_config_file(void)
{
  struct stat buf;
  char *home;
  char *line;
  char *s;
  char *name = NULL;
  char *regex;
  char *tok;
  char *which_key;
  char *updown;
  char delim;
  translation *tr = NULL;
  FILE *f;

  if (config_file_name == NULL) {
    config_file_name = getenv("SHUTTLE_CONFIG_FILE");
    if (config_file_name == NULL) {
      home = getenv("HOME");
      config_file_name = alloc_strcat(home, "/.shuttlerc");
    } else {
      config_file_name = alloc_strcat(config_file_name, NULL);
    }
    config_file_modification_time = 0;
  }
  if (stat(config_file_name, &buf) < 0) {
    perror(config_file_name);
    return;
  }
  if (buf.st_mtime == 0) {
    buf.st_mtime = 1;
  }
  if (buf.st_mtime > config_file_modification_time) {
    config_file_modification_time = buf.st_mtime;

    f = fopen(config_file_name, "r");
    if (f == NULL) {
      perror(config_file_name);
      return;
    }

    free_all_translations();
    debug_regex = 0;
    debug_strokes = 0;

    while ((line=read_line(f, config_file_name)) != NULL) {
      //printf("line: %s", line);
      
      s = line;
      while (*s && isspace(*s)) {
	s++;
      }
      if (*s == '#') {
	continue;
      }
      if (*s == '[') {
	//  [name] regex\n
	name = ++s;
	while (*s && *s != ']') {
	  s++;
	}
	regex = NULL;
	if (*s) {
	  *s = '\0';
	  s++;
	  while (*s && isspace(*s)) {
	    s++;
	  }
	  regex = s;
	  while (*s) {
	    s++;
	  }
	  s--;
	  while (s > regex && isspace(*s)) {
	    s--;
	  }
	  s[1] = '\0';
	}
	tr = new_translation_section(name, regex);
	continue;
      }

      tok = token(s, &delim);
      if (tok == NULL) {
	continue;
      }
      if (!strcmp(tok, "DEBUG_REGEX")) {
	debug_regex = 1;
	continue;
      }
      if (!strcmp(tok, "DEBUG_STROKES")) {
	debug_strokes = 1;
	continue;
      }
      which_key = tok;
      if (start_translation(tr, which_key)) {
	continue;
      }
      tok = token(NULL, &delim);
      while (tok != NULL) {
	if (delim != '"' && tok[0] == '#') {
	  break; // skip rest as comment
	}
	//printf("token: [%s] delim [%d]\n", tok, delim);
	switch (delim) {
	case ' ':
	case '\t':
	case '\n':
	  add_keystroke(tok, PRESS_RELEASE);
	  break;
	case '"':
	  add_string(tok);
	  break;
	default: // should be slash
	  updown = token(NULL, &delim);
	  if (updown != NULL) {
	    switch (updown[0]) {
	    case 'U':
	      add_keystroke(tok, RELEASE);
	      break;
	    case 'D':
	      add_keystroke(tok, PRESS);
	      break;
	    case 'H':
	      add_keystroke(tok, HOLD);
	      break;
	    default:
	      fprintf(stderr, "invalid up/down modifier [%s]%s: %s\n", name, which_key, updown);
	      add_keystroke(tok, PRESS);
	      break;
	    }
	  }
	}
	tok = token(NULL, &delim);
      }
      finish_translation();
    }

    fclose(f);

  }
}

translation *
get_translation(char *win_title)
{
  translation *tr;

  read_config_file();
  tr = first_translation_section;
  while (tr != NULL) {
    if (tr->is_default) {
      return tr;
    }
    if (regexec(&tr->regex, win_title, 0, NULL, 0) == 0) {
      return tr;
    }
    tr = tr->next;
  }
  return NULL;
}
