/* ---------------------------------------------------------------------------
** xjs is a small and simple JSON reader/parser for C/C++
** ---------------------------------------------------------------------------
** Copyright (c) 2016 by Payton Bissell, payton.bissell@gmail.com
** ---------------------------------------------------------------------------
** Permission to use, copy, modify, and/or distribute this software for any
** purpose with or without fee is hereby granted. 
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
** OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
** THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
**
** Also offered with MIT License. 
**
** INTENTION: I, Payton Bissell, the sole author of this source code, dedicate
** any and all copyright interest in this code to the public domain. I make
** this dedication for the benefit of the public at large and to the detriment
** of my heirs and successors. I intend this dedication to be an overt act of
** relinquishment in perpetuity of all present and future rights to this code
** under copyright law.
**
** Copyleft is the antithesis of freedom. Please resist it.
** ---------------------------------------------------------------------------
*/

#include "xjs.h"

#ifndef XJS_CFLAG_DISABLE_MEM_FALLBACK 
#include <stdlib.h>
#endif

#ifndef XJS_CFLAG_DISABLE_ARRAY_POS_AS_NAME  
#include <stdio.h>
#endif

#if !defined(NULL)
#define NULL ((void*)0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
** ---------------------------------------------------------------------------
** These are the allocation suggested sizes when calling the memory manager.
** IXJS_MEM_STR_BLOCK_SIZE - Suggested 256 bytes for String/Value size.
** IXJS_MEM_HEADER_BLOCK_SIZE - Suggested 256 block array elements (Alloc 256xptr).
** ---------------------------------------------------------------------------
*/
#define IXJS_MEM_STR_BLOCK_SIZE  256  

#ifdef XJS_CFLAG_ENABLE_BLOCKS
#define IXJS_MEM_HEADER_BLOCK_SIZE  256
#endif

/*
** ---------------------------------------------------------------------------
** USER FUNCTIONS
** Description functions use static storage. Can be disabled at compile time
** with XJS_CFLAG_DISABLE_DESCRIPTIONS
** ---------------------------------------------------------------------------
*/
#ifndef XJS_CFLAG_DISABLE_DESCRIPTIONS
const char *xjs_desc_type(XJSType nodetype)
{ switch(nodetype)
  { case XJS_unknown          : return "unknown";
    case XJS_array            : return "array";
    case XJS_object           : return "object";
    case XJS_number           : return "number";
    case XJS_string           : return "string";
    case XJS_true             : return "true";
    case XJS_false            : return "false";
    case XJS_null             : return "null";
    case XJS_name             : return "name";
    case XJS_array_end        : return "array_end";
    case XJS_object_end       : return "object_end";
#ifdef XJS_CFLAG_ENABLE_BLOCKS
    case XJS_block            : return "block";
    case XJS_block_end        : return "block_end";
    case XJS_block_header     : return "block_header";
    case XJS_block_header_end : return "block_header_end";
    case XJS_block_array      : return "block_array";
    case XJS_block_array_end  : return "block_array_end";
#endif
  }
  return "invalid type";
}
const char *xjs_desc_error(int code) 
{ switch(code)
  { case XJS_OK                   : return "all is well";
    case XJS_NOPE                 : return "input does not match a kind";
    case XJS_END                  : return "end of the input was reached";
    case XJS_ERR                  : return "something went wrong (callback returned)";
    case XJS_ERR_NO_INPUT         : return "there was no input provided";
    case XJS_ERR_BAD_INPUT        : return "the input is not valid";
    case XJS_ERR_EXP_COLON        : return "expected an object member separator";
    case XJS_ERR_EXP_COMMA        : return "expected a comma";
    case XJS_ERR_EXP_END_STRING   : return "expected a string marker";
    case XJS_ERR_EXP_ESCAPE       : return "expected valid escape characters";
    case XJS_ERR_EXP_ESCAPE_HEX   : return "expected escaped hex code";
    case XJS_ERR_EXP_DIGIT        : return "expected a digit for a number";
    case XJS_ERR_USAGE            : return "broken contract, unexpected behavior from callback";
    case XJS_ERR_MEM_MISSING      : return "there is no way to get storage for output";
    case XJS_ERR_MEM_ALLOC        : return "the memory manager failed to return required storage";
    case XJS_ERR_BAD_LITERAL      : return "invalid input regarding a literal";
    case XJS_ERR_CONTROL_CHAR     : return "ascii control characters not allowed in strings or values";
    case XJS_ERR_MORE_INPUT       : return "input remaining after first level object/array complete";
#ifdef XJS_CFLAG_ENABLE_BLOCKS
    case XJS_ERR_BLOCK_ARRAY_SIZE : return "block arrays must all be the same size (including header)";
#endif
  }
  return "invalid error code";
}
#endif

/*
** Internal funcntion prototypes
** ---------------------------------------------------------------------------
*/
static int r_parse_any(int top, const char **jsp, void *parent, const char *name, XJSNodeCB node_cb, void *node_arg, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb);
static int i_parse_array(char ***hdr, XJSType kind, int *cnt, const char **jsp, void *parent, const char *name, XJSNodeCB node_cb, void *node_arg, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb);
static int i_parse_object(const char **jsp, void *parent, const char *name, XJSNodeCB node_cb, void *node_arg, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb);
static int i_parse_value(const char **jsp, XJSType *kind, char **value, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb);
static int i_parse_string(const char **jsp, char **value, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb, XJSType context);
static int i_parse_literal(const char **jsp, const char *lit, char **value, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg);
static int i_parse_number(const char **jsp, char **value, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb);
static int i_parse_escape(const char **jsp, char **escape, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb, XJSType context);
static int i_eatwhite(const char **jsp, XJSInputCB inp_cb, void *inp_arg);
static int i_assign(const char **jsp, char **to, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb, XJSType context);
static int i_advance(int ew, const char **jsp, XJSInputCB inp_cb, void *inp_arg);
static int i_noend(int r) { if (r==XJS_END) return XJS_ERR_BAD_INPUT; return r; }

#ifdef XJS_CFLAG_ENABLE_BLOCKS
static int i_parse_block(const char **jsp, void *parent, const char *name, XJSNodeCB node_cb, void *node_arg, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb);
#endif

/*
** Default memory management using libc malloc/free/realloc. 
** ---------------------------------------------------------------------------
*/
#ifdef XJS_CFLAG_DISABLE_MEM_FALLBACK
static XJSMemCB i_mem_fallback = NULL;
#else
static void *i_mem_fallback(XJSMemReq action, XJSType context, void *prev, XJSSize size, XJSSize *actsz)
{ void *ret=NULL; if (actsz) *actsz=0; // on realloc, prev!=NULL and actsz will be the last size of prev.
  if (action==XJS_free) free(prev);
  else if (action==XJS_alloc)
  { if (prev) ret=realloc(prev, size);
    else ret=malloc(size);
  }
  if ((ret)&&(actsz)) *actsz=size;
  return ret;
}
#endif

/* ---------------------------------------------------------------------------
** USER FUNCTION
** returns XJS_OK or an error code
** ---------------------------------------------------------------------------
*/
int xjs_parse(const char *json, XJSNodeCB node_cb, void *node_arg, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb, const char **errpos)
{ int r; *errpos=NULL; const char *js=json;
  if (mem_cb==NULL) mem_cb=i_mem_fallback;
  if (mem_cb==NULL) return XJS_ERR_MEM_MISSING;
  if (js==NULL)
  { if (inp_cb==NULL) return XJS_ERR_NO_INPUT;
    if ((r=inp_cb(&js, inp_arg))!=XJS_OK) return r;
  }
  if (js==NULL) return XJS_ERR_BAD_INPUT;  
  r=r_parse_any(1, &js, NULL, NULL, node_cb, node_arg, inp_cb, inp_arg, mem_cb);
  if (*js) i_eatwhite(&js, inp_cb, inp_arg);
  if (*js) r=XJS_ERR_MORE_INPUT;
  *errpos=js; 
  return (r==XJS_END)?XJS_OK:r;
}

/* ---------------------------------------------------------------------------
** Parse a JSON array, return XJS_NOPE is the input does not start correctly
** <white> '[' <white> (',' <white> <any> <white>)* ']' 
** COMMA only after first item.
** ---------------------------------------------------------------------------
*/
static int i_parse_array(char ***hdr, XJSType kind, int *cnt, const char **jsp, void *parent, const char *name, XJSNodeCB node_cb, void *node_arg, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb)
{ XJSSize actsz=0; int i=0, tcnt=0, r=XJS_OK; void *np=parent;
  char *pos=NULL;
#ifndef XJS_CFLAG_DISABLE_ARRAY_POS_AS_NAME  
  char p_pos[40]; pos=p_pos; // the position in the array as text, passed in the item name field.
#endif  
  if (cnt) *cnt=0; // cnt can be null for non block arrays!
  if ((r=i_eatwhite(jsp, inp_cb, inp_arg))!=XJS_OK) return r;
  if ((**jsp)!='[') return XJS_NOPE; // this is not an array!
  if ((r=node_cb(&np, kind, name, NULL, node_arg))!=XJS_OK) return r;
  if ((r=i_advance(1, jsp, inp_cb, inp_arg))!=XJS_OK) return i_noend(r); 
#ifdef XJS_CFLAG_ENABLE_BLOCKS
    if (kind==XJS_block_array) if (hdr) if (*hdr==NULL) hdr=NULL;
#endif
  while ((**jsp)!=']')  
  { if (i++!=0) if ((**jsp)!=',') return XJS_ERR_EXP_COMMA; else if ((r=i_advance(0, jsp, inp_cb, inp_arg))!=XJS_OK) return i_noend(r); 
#ifndef XJS_CFLAG_DISABLE_ARRAY_POS_AS_NAME  
      snprintf(pos, 40, "%d", i);
#endif
#ifdef XJS_CFLAG_ENABLE_BLOCKS
    if (kind==XJS_block_header) 
    { if ((hdr) && (((*cnt)==0)||(((*cnt)+1)>=tcnt))) 
      { *hdr=(char**)mem_cb(XJS_alloc, XJS_block_header, (void*)(*hdr), ((*cnt)+IXJS_MEM_HEADER_BLOCK_SIZE)*sizeof(char*), &actsz);
        tcnt=((*hdr)==NULL)?0:(int)(actsz/(sizeof(char*))); // max(tcnt) is probably less than max(actsz/ptrsz)
        if ((*hdr)==NULL) hdr=NULL; // short circut any more header storage requests if the memory manager rejected the first request.
      }
      XJSSize mxsz=0, used=0; char *value=NULL; // value will be freed by the caller as part of hdr.
      if ((r=i_parse_string(jsp, &value, &mxsz, &used, inp_cb, inp_arg, mem_cb, XJS_name))!=XJS_OK) return i_noend(r);
      r=node_cb(&np, XJS_name, pos, value, node_arg);
      if ((hdr)&&(tcnt>(*cnt))) (*hdr)[*cnt] = value;  // remember the name
      else mem_cb(XJS_free, XJS_name, value, 0, NULL); // forget the name
      // caller of parse_array must clean up the header, even on error
    }
    else
#endif /* BLOCKS */
    r=r_parse_any(0, jsp, np, ((hdr==NULL)||(kind==XJS_array))?pos:((*hdr)[*cnt]), node_cb, node_arg, inp_cb, inp_arg, mem_cb);
    if (r!=XJS_OK) return i_noend(r);    
    if (cnt) (*cnt)++;
    if ((r=i_eatwhite(jsp, inp_cb, inp_arg))!=XJS_OK) return i_noend(r);
  }
  r=i_advance(0, jsp, inp_cb, inp_arg);
  if ((r!=XJS_OK)&&(r!=XJS_END)) return i_noend(r); // eat the close bracket, END IS OKAY!
#ifdef XJS_CFLAG_ENABLE_BLOCKS
  if (kind==XJS_block_header) kind=XJS_block_header_end;
  else if (kind==XJS_block_array) kind=XJS_block_array_end;
  else
#endif
  kind=XJS_array_end;
  return node_cb(&np, kind, name, NULL, node_arg);
}

/* ---------------------------------------------------------------------------
** Parse a JSON object, return XJS_NOPE if the input does not start correctly
** <white> '{' <white> (',' <white> <string> <white> ':' <white> <any> <white>)* '}'
** COMMA only after first member
** ---------------------------------------------------------------------------
*/
static int i_parse_object(const char **jsp, void *parent, const char *name, XJSNodeCB node_cb, void *node_arg, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb)
{ int i=0, r=XJS_OK; void *np=parent;
  if ((**jsp)!='{') return XJS_NOPE;
  if ((r=node_cb(&np, XJS_object, name, NULL, node_arg))!=XJS_OK) return r;
  if ((r=i_advance(1, jsp, inp_cb, inp_arg))!=XJS_OK) return r; 
  while ((**jsp)!='}')
  {  if (i++!=0) if ((**jsp)!=',') return XJS_ERR_EXP_COMMA; else if ((r=i_advance(0, jsp, inp_cb, inp_arg))!=XJS_OK) return i_noend(r);
    XJSSize used=0, mxsz=0; char *new_name=NULL;
    r=i_parse_string(jsp, &new_name, &mxsz, &used, inp_cb, inp_arg, mem_cb, XJS_name);
    if (r==XJS_OK) r=i_eatwhite(jsp, inp_cb, inp_arg); // eat up to colon
    if (r==XJS_OK) if ((**jsp)!=':') r=XJS_ERR_EXP_COLON; 
    if (r==XJS_OK) r=i_advance(0, jsp, inp_cb, inp_arg); // eat colon
    if (r==XJS_OK) r=r_parse_any(0, jsp, np, new_name, node_cb, node_arg, inp_cb, inp_arg, mem_cb); // get value(name)
    if (new_name) mem_cb(XJS_free, XJS_name, new_name, 0, NULL); // done with new_name
    if (r!=XJS_OK) return i_noend(r);
    if ((r=i_eatwhite(jsp, inp_cb, inp_arg))!=XJS_OK) return i_noend(r); // eat until comma or end
  }
  r=i_advance(0, jsp, inp_cb, inp_arg);
  if ((r!=XJS_OK)&&(r!=XJS_END)) return r; // eat the close bracket, END IS OKAY!
  return node_cb(&np, XJS_object_end, name, NULL, node_arg);
}

#ifdef XJS_CFLAG_ENABLE_BLOCKS
/* ---------------------------------------------------------------------------
** Parse a Block. NOT STANDARD JSON!
** <white> '(' <white> (',' <white> <array> <white>)* ')'
** COMMA only after first array, first array is strings only (header)
** ---------------------------------------------------------------------------
*/
static int i_parse_block(const char **jsp, void *parent, const char *name, XJSNodeCB node_cb, void *node_arg, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb)
{ int i=0, r, cnt=0, tmpcnt=0; void *np=parent; char **hdr=NULL;
  if ((**jsp)!='(') return XJS_NOPE;
  if ((r=node_cb(&np, XJS_block, name, NULL, node_arg))!=XJS_OK) return r;
  if ((r=i_advance(1, jsp, inp_cb, inp_arg))!=XJS_OK) return i_noend(r); 
  if (**jsp!=')') r=i_parse_array(&hdr, XJS_block_header, &cnt, jsp, np, NULL, node_cb, node_arg, inp_cb, inp_arg, mem_cb);
  while ((r==XJS_OK)&&(**jsp!=')'))
  { if (i++!=0) if ((**jsp)!=',') return XJS_ERR_EXP_COMMA; else r=i_advance(0, jsp, inp_cb, inp_arg);
    if (r==XJS_OK) r=i_parse_array(&hdr, XJS_block_array, &tmpcnt, jsp, np, NULL, node_cb, node_arg, inp_cb, inp_arg, mem_cb);
    if ((r==XJS_OK)&&(tmpcnt!=cnt)) r=XJS_ERR_BLOCK_ARRAY_SIZE; // keep first error
  }
  if (hdr) // clean up the header storage.
  { for (i=0;i<cnt;i++) mem_cb(XJS_free, XJS_name, hdr[i], 0, NULL); 
    mem_cb(XJS_free, XJS_block_header, hdr, 0, NULL);
  }
  if (r!=XJS_OK) return i_noend(r);
  r=i_advance(0, jsp, inp_cb, inp_arg);
  if ((r!=XJS_OK)&&(r!=XJS_END)) return r; // eat the close bracket, END IS OKAY!
  return node_cb(&np, XJS_block_end, name, NULL, node_arg);
}
#endif

/* ---------------------------------------------------------------------------
** Determine the next node in the input stream and parse it accordingly.
** ---------------------------------------------------------------------------
*/
static int r_parse_any(int top, const char **jsp, void *parent, const char *name, XJSNodeCB node_cb, void *node_arg, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb)
{ void *np=parent;
  int r=i_eatwhite(jsp, inp_cb, inp_arg); if ((r!=XJS_OK)&&(r!=XJS_END)) return r;
  if (r==XJS_END) return r;
  if ((**jsp)=='{') r=i_parse_object(jsp, parent, name, node_cb, node_arg, inp_cb, inp_arg, mem_cb);
  else if ((**jsp)=='[') r=i_parse_array(NULL, XJS_array, NULL, jsp, parent, name, node_cb, node_arg, inp_cb, inp_arg, mem_cb);
#ifdef XJS_CFLAG_ENABLE_BLOCKS 
  else if ((**jsp)=='(') r=i_parse_block(jsp, parent, name, node_cb, node_arg, inp_cb, inp_arg, mem_cb);
#endif
  else if (top==1) r=XJS_ERR_BAD_INPUT;  // can only be one the above at the top level. 
  else
  { XJSType kind; XJSSize used=0, mxsz=0; char *value=NULL;
    r=i_parse_value(jsp, &kind, &value, &mxsz, &used, inp_cb, inp_arg, mem_cb);
    if (r==XJS_OK) r=node_cb(&np, kind, name, value, node_arg); 
    mem_cb(XJS_free, kind, value, 0, NULL);
  }
  return r;
}

/* ---------------------------------------------------------------------------
** Parse a value node (literal, string or number).
** ---------------------------------------------------------------------------
*/
static int i_parse_value(const char **jsp, int *kind, char **value, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb)
{ int r; *kind=XJS_unknown; 
  if ((r=i_eatwhite(jsp, inp_cb, inp_arg))!=XJS_OK) return r;
  r=XJS_NOPE;
  if (r==XJS_NOPE) if ((r=i_parse_literal(jsp, "true",  value, mxsz, used, inp_cb, inp_arg))==XJS_OK) *kind=XJS_true; 
  if (r==XJS_NOPE) if ((r=i_parse_literal(jsp, "false", value, mxsz, used, inp_cb, inp_arg))==XJS_OK) *kind=XJS_false;
  if (r==XJS_NOPE) if ((r=i_parse_literal(jsp, "null",  value, mxsz, used, inp_cb, inp_arg))==XJS_OK) *kind=XJS_null;
  if (r==XJS_NOPE) if ((r=i_parse_number(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb))==XJS_OK) *kind=XJS_number;
  if (r==XJS_NOPE) if ((r=i_parse_string(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb, XJS_string))==XJS_OK) *kind=XJS_string;
  if (r!=XJS_OK) return r;
  return XJS_OK;
}

/* ---------------------------------------------------------------------------
** Parse a string node. JSON strings are surrounded by double quotes.
** ---------------------------------------------------------------------------
*/
static int i_parse_string(const char **jsp, char **value, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb, XJSType context)
{ int r; if ((r=i_eatwhite(jsp, inp_cb, inp_arg))!=XJS_OK) return r;
  if ((**jsp)!='"') return XJS_NOPE; if ((r=i_advance(0, jsp, inp_cb, inp_arg))!=XJS_OK) return r; 
  while ((**jsp)&&((**jsp)!='"')) 
  { if ((**jsp)!='\\') r=i_assign(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb, context); 
    else r=i_parse_escape(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb, context);
    if (r!=XJS_OK) return r; 
  }
  if ((**jsp)!='"') return XJS_ERR_EXP_END_STRING; if ((r=i_advance(0, jsp, inp_cb, inp_arg))!=XJS_OK) return r; 
  if (*value) (*value)[(*used)++]=0; 
  return XJS_OK;
}

/* ---------------------------------------------------------------------------
** Determine if a literal is the next node and parse if it is. 
** This function can be called to test if the next token is a literal.
** So don't move the cursor unless we have it.
** this call is internal, it will only be called for true, false and null
** We know by the first character, and deviation is an error, so we can
** advance the input while we look. 
** ---------------------------------------------------------------------------
*/
static int i_parse_literal(const char **jsp, const char *lit, char **value, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg)
{ int r; if ((r=i_eatwhite(jsp, inp_cb, inp_arg))!=XJS_OK) return r;
  if ((**jsp)!=*lit) return XJS_NOPE;
  int i; for (i=0;(lit[i]!=0);i++)
  { if ((**jsp)!=lit[i]) return XJS_ERR_BAD_LITERAL; // check 0 twice, that's ok.
    if ((r=i_advance(0, jsp, inp_cb, inp_arg))!=XJS_OK) return r; 
  }
  if (isalnum(**jsp)) return XJS_ERR_BAD_LITERAL;
  return XJS_OK;
}

/* ---------------------------------------------------------------------------
** Parse a JSON escaped character sequence. 
** Do not NULL terminate because its part of a string.
** ---------------------------------------------------------------------------
*/
static int i_parse_escape(const char **jsp, char **escape, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb, XJSType context)
{ int r; 
  if ((**jsp)!='\\') return XJS_NOPE; 
  if ((r=i_assign(jsp, escape, mxsz, used, inp_cb, inp_arg, mem_cb, context))!=XJS_OK) return r; 
  if (((**jsp)!='"') && ((**jsp)!='\\') && ((**jsp)!='/') && ((**jsp)!='b') && ((**jsp)!='f') && 
      ((**jsp)!='n') && ((**jsp)!='r')  && ((**jsp)!='t') && ((**jsp)!='u')) return XJS_ERR_EXP_ESCAPE;
  int i=0; if ((**jsp)=='u') i=1;
  if ((r=i_assign(jsp, escape, mxsz, used, inp_cb, inp_arg, mem_cb, context))!=XJS_OK) return r; 
  if (i) for (i=0;i<4;i++) 
  { if (!isxdigit(**jsp)) return XJS_ERR_EXP_ESCAPE_HEX;
    if ((r=i_assign(jsp, escape, mxsz, used, inp_cb, inp_arg, mem_cb, context))!=XJS_OK) return r; 
  }
  return XJS_OK;
}

/* ---------------------------------------------------------------------------
** Parse a JSON number node.
** ---------------------------------------------------------------------------
*/
static int i_parse_number(const char **jsp, char **value, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb)
{ int r; if ((r=i_eatwhite(jsp, inp_cb, inp_arg))!=XJS_OK) return r;
  if ((**jsp)=='-') if ((r=i_assign(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb, XJS_number))!=XJS_OK) return r; 
  if (!isdigit(**jsp)) return XJS_NOPE; 
  if ((**jsp)=='0') r=i_assign(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb, XJS_number); 
  else
  { if (!isdigit(**jsp)) return XJS_ERR_EXP_DIGIT;
    while (isdigit(**jsp)) if ((r=i_assign(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb, XJS_number))!=XJS_OK) return r; 
  }
  if (r!=XJS_OK) return r;
  if ((**jsp)=='.') 
  { if ((r=i_assign(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb, XJS_number))!=XJS_OK) return r; 
    if (!isdigit(**jsp)) return XJS_ERR_EXP_DIGIT;
    while (isdigit(**jsp)) if ((r=i_assign(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb, XJS_number))!=XJS_OK) return r; 
  }
  if (((**jsp)=='e') || ((**jsp)=='E'))
  { if ((r=i_assign(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb, XJS_number))!=XJS_OK) return r; 
    if (((**jsp)=='+') || ((**jsp)=='-')) if ((r=i_assign(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb, XJS_number))!=XJS_OK) return r; 
    if (!isdigit(**jsp)) return XJS_ERR_EXP_DIGIT;
    while (isdigit(**jsp)) if ((r=i_assign(jsp, value, mxsz, used, inp_cb, inp_arg, mem_cb, XJS_number))!=XJS_OK) return r; 
  }
  if (*value) (*value)[(*used)++]=0; 
  return XJS_OK;
}

/* ---------------------------------------------------------------------------
** Consume the white space in the input stream.
** ---------------------------------------------------------------------------
*/
static int i_eatwhite(const char **jsp, XJSInputCB inp_cb, void *inp_arg)
{ int r; while (isspace(**jsp)) if ((r=i_advance(0, jsp, inp_cb, inp_arg))!=XJS_OK) return r; 
  return XJS_OK;
}

/* ---------------------------------------------------------------------------
** Advance the input stream one character. Advance through white space if
** ew==1.
** ---------------------------------------------------------------------------
*/
static int i_advance(int ew, const char **jsp, XJSInputCB inp_cb, void *inp_arg)
{ int r; if ((**jsp)!=0) (*jsp)++; 
  if ((**jsp)==0) if (inp_cb!=NULL) if ((r=inp_cb(jsp, inp_arg))!=XJS_OK) return r;
  if ((*jsp)==NULL) return XJS_ERR_USAGE;
  if ((**jsp)==0) return XJS_END;
  return (ew)?i_eatwhite(jsp, inp_cb, inp_arg):XJS_OK;
}

/* ---------------------------------------------------------------------------
** Place the current input stream character into the output stream.
** Adjust the size of the output stream if it runs out of space.
** ---------------------------------------------------------------------------
*/
static int i_assign(const char **jsp, char **to, XJSSize *mxsz, XJSSize *used, XJSInputCB inp_cb, void *inp_arg, XJSMemCB mem_cb, XJSType context)
{ XJSSize actsz=*mxsz;
  if (((*to)==NULL) || ((*mxsz)==0) || ((1+(*used))>=(*mxsz)))
  { (*to)=mem_cb(XJS_alloc, context, (*to), (*mxsz)+IXJS_MEM_STR_BLOCK_SIZE, &actsz); 
    if (((*to)==NULL)||(actsz<=(*mxsz))) return XJS_ERR_MEM_ALLOC;
    (*mxsz)=actsz;
  }
  if (((**jsp)=='\b')||((**jsp)=='\f')||((**jsp)=='\n')||((**jsp)=='\r')||((**jsp)=='\t')) return XJS_ERR_CONTROL_CHAR;
  (*to)[(*used)++]=(**jsp);
  return i_advance(0, jsp, inp_cb, inp_arg);
}

#ifdef __cplusplus
} // extern "C"
#endif

/* EOF */
