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
** Print nodes encountered, and demonstrate simple hierarchy with parent ID
** Using parent as an integer ID!
** ---------------------------------------------------------------------------
*/
int nodecb(void **parent, XJSType kind, const char *name, const char *value, void *arg)
{ static char *g_pos=NULL; 
  if (*parent) printf("(%p) ", *parent); // print parent ID
  printf("%s ", xjs_desc_type(kind));    // print description of node type
  if (name) printf("<%s> : ", name);     // print name (if present)
  if (value) printf("<%s>", value);      // print value (if present)
  printf("\n");
  
  // if this is a container, assign it a unique ID. Not thread safe.
  if ((kind==XJS_object)||(kind==XJS_array)) *parent = (void*)(++g_pos);
  return XJS_OK; // required to continue parsing
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
  int r=xjs_parse(NULL, nodecb, NULL, inpcb, (void*)f, NULL, &err);
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