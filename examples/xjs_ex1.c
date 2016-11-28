/* ---------------------------------------------------------------------------
** xjsf example : parses a file containing JSON
** ---------------------------------------------------------------------------
** Copyright (c) 2016 by Payton Bissell, payton.bissell@gmail.com
** ---------------------------------------------------------------------------
*/

#include "xjs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ---------------------------------------------------------------------------
** A Static Memory Manager. It will fail if the allocations are too small,
** but if the nature of the JSON is known upfront, this approach will improve
** performance. 
** ---------------------------------------------------------------------------
*/
#define NAME_BUF_SIZE  256 // maximum name field length
#define VALUE_BUF_SIZE 512 // maximum value field length
void *memcb_static(XJSMemReq action, XJSType context, void *prev, XJSSize size, XJSSize *actsz)
{ static char s_name[NAME_BUF_SIZE], s_value[VALUE_BUF_SIZE];
  if (action==XJS_free) return NULL;   // nothing to free.
  if (prev) { *actsz=0; return NULL; } // no reallocation allowed.
  if (context==XJS_name) { *actsz=NAME_BUF_SIZE; return s_name; } // require two buffers for object members (name:value)
  *actsz=VALUE_BUF_SIZE; 
  return s_value;
}

/* ---------------------------------------------------------------------------
** Input will read TEST_INPUT_BUFFER_SIZE bytes from the file passed as arg.
** The parser will request the next buffer when it reaches the null terminator
** ---------------------------------------------------------------------------
*/
#define TEST_INPUT_BUFFER_SIZE 1024
int inpcb(const char **input, void *arg)
{ static char b[TEST_INPUT_BUFFER_SIZE+1]; b[0]=0; *input=b; size_t r=0;
  if (arg==NULL) return XJS_END; // make sure input file is set
  if ((r=fread(b, 1, TEST_INPUT_BUFFER_SIZE-1, ((FILE*)arg)))==0) 
    return XJS_END; // signal the end of input if there is nothing to read.
  b[r]=0; // null terminate all inputs!
  return XJS_OK; 
}

/* ---------------------------------------------------------------------------
** Print nodes encountered, and demonstrate simple hierarchy by tracking level
** ---------------------------------------------------------------------------
*/
int nodecb(void **parent, XJSType kind, const char *name, const char *value, void *arg)
{ static int g_parent_level=0;
  if ((kind==XJS_object_end)||(kind==XJS_array_end)) g_parent_level--;
  int i; for (i=0;i<g_parent_level;i++) printf(" "); printf("+");
  printf("%s ", xjs_desc_type(kind));
  if (name) printf("<%s> : ", name);
  if (value) printf("<%s>", value);
  printf("\n");
  if ((kind==XJS_object)||(kind==XJS_array)) g_parent_level++;
  return XJS_OK;
}

/* ---------------------------------------------------------------------------
**  xjsf <json-file>
** ---------------------------------------------------------------------------
*/
int main(int argc, char *argv[])
{ const char *err=NULL;
  if (argc<2) { printf("%s <json-file>\n", argv[0]); return 1; }
  FILE *f=fopen(argv[1], "rb");
  if (f==NULL) { printf("%s is not a valid file\n", argv[1]); return 1; }
  int r=xjs_parse(NULL, nodecb, NULL, inpcb, (void*)f, memcb_static, &err);
  if (r==XJS_OK) printf("SUCCESS.\n");
  else 
  { // if there was an error, truncate the input from the error position (more usefule)
    char errcutoff[60];
    strncpy(errcutoff, err, 60);
    printf("ERROR: r=%d, %s, : <%s>\n", r, xjs_desc_error(r), errcutoff);
  }
  fclose(f);
  return 0;
}

/* EOF */