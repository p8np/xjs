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
** # xjs
** A simple JSON parser for C/C++
** 
** **xjs** parses a JSON stream and signals the caller of items parsed via
** a callback function. Input can be ASCII or UTF-8
** 
** **xjs** does not provide any hierarchical structure to the stream.
** The caller can assosicate memory with parsed objects and arrays within
** the callback. These addressed will passed to the callback for all the
** items parsed while the object or array is active. This feature facilitates
** creating a hierarchical representation of the nodes parsed.
** 
** **xjs** also supports an input callback that allows the caller to append input
** data when a null terminator is encountered. This is useful for parsing a
** stream whose size is unknown, or whose content is impractical to store in a
** single memory allocation. The input can be broken anywhere so that reading
** a fixed size buffer of input can be done in a loop without concern for
** element boundaries.
** 
** Each buffer returned by the input callback must be null terminated. 
** 
** ## Blocks (not standard JSON)
** 
** Blocks provides a more compact method of representing an array
** of homogeneous objects (each with the same number of fields). 
** 
** Every array in the block must contain **N** elements. The first array
** in a block is the header containing the member names as string values. 
** All the subsequent arrays are data records, and the fields must be
** in the same order as the fields in the header.
** 
** Blocks are disabled by default. You must define `XJS_ENABLE_BLOCKS`
** in your project, makefile, command line or source code prior to 
** including `xjs.h`
** 
** ### Block example
** 
** ```
** (["Rank", "Team", "2016", "Last 3", "Last 1", "Home", "Away", "2015"], 
**  [ 1, "Chi Cubs", 3.12, 3.00, 2.00, 2.65, 3.62, 3.41 ], 
**  [ 2, "Washington", 3.53, 4.50, 4.00, 3.42, 3.64, 3.62 ], 
**  [ 3, "NY Mets", 3.57, 3.12, 3.00, 3.40, 3.74, 3.42 ], 
**  [ 4, "SF Giants", 3.64, 4.20, 4.00, 3.51, 3.76, 3.72 ], 
**  [ 5, "Cleveland", 3.69, 1.73, 3.38, 3.71, 3.67, 3.67 ] 
** )
** ```
** 
** ## Compile-time Preprocessor Flags
** 
** **xjs** watches for two compiler flags:
** 
** `XJS_CFLAG_DISABLE_MEM_FALLBACK` - if defined the parser will NOT use
** libc `malloc/free` to allocate storage, and `XJSMemCB` must be passed to
** parse. Otherwise passing NULL to xjs_parse for `XJSMemCB` will default to 
** `malloc/free`.
** 
** `XJS_CFLAG_ENABLE_BLOCKS` - if defined the parser will handle Blocks as 
** described above otherwise blocks in JSON will generate an error.
** 
** `XJS_CFLAG_DISABLE_DESCRIPTIONS` - if defined the library will provide
** ascii english descriptions for Node Types and Error Codes.
** 
** `XJS_CFLAG_DISABLE_ARRAY_POS_AS_NAME` - **xjs** sends the ordinal number
** of array elements as ascii text in the name field for all array elements.
** If defined the name field will be ```NULL``` for array elements. 
** *Otherwise `snprintf` from libc will be linked.*
** 
** `XJS_CFLAG_DISABLE_SIZE_T` - if defined the API will not use `size_t`
** from libc, instead it will use `unsigned long`.
** 
** ---------------------------------------------------------------------------
*/

#ifndef __XJS_H__
#define __XJS_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
** ---------------------------------------------------------------------------
** Regret including a header... Need a platform specific pointer size for
** size type. I vote size_t be included in the language, and __SIZEOF_POINTER__
** would be nice at compile time.
** ---------------------------------------------------------------------------
*/
#ifdef XJS_CFLAG_DISABLE_SIZE_T
typedef unsigned long XJSSize; /* fallback */
#else
#include <stddef.h> 
typedef size_t XJSSize;
#endif

/*
** Error codes returned by the API
** ---------------------------------------------------------------------------
*/
#define XJS_OK                       0    /* all is well */
#define XJS_NOPE                     1    /* input does not match a kind */
#define XJS_END                      2    /* end of the input was reached */
#define XJS_ERR                      3    /* something went wrong */
#define XJS_ERR_NO_INPUT             4    /* there was no input provided */
#define XJS_ERR_BAD_INPUT            5    /* the input is not valid */
#define XJS_ERR_EXP_COLON            6    /* expected an object member marker */
#define XJS_ERR_EXP_COMMA            7    /* expected the end of an object marker '}' */
#define XJS_ERR_EXP_END_STRING       8    /* expected a close double quote */
#define XJS_ERR_EXP_ESCAPE           9    /* expected valid escape characters */
#define XJS_ERR_EXP_ESCAPE_HEX       10   /* expected escaped hex code */
#define XJS_ERR_EXP_DIGIT            11   /* expected a digit for a number */
#define XJS_ERR_USAGE                12   /* broken contract, unexpected behavior from callback */
#define XJS_ERR_MEM_MISSING          13   /* there is no way to get storage for output */
#define XJS_ERR_MEM_ALLOC            14   /* the memory manager failed to return required storage */
#define XJS_ERR_BAD_LITERAL          15   /* invalid input regarding a literal */
#define XJS_ERR_CONTROL_CHAR         16   /* bad ascii */
#define XJS_ERR_MORE_INPUT           17   /* input remaining after completion */
#ifdef XJS_CFLAG_ENABLE_BLOCKS
#define XJS_ERR_BLOCK_ARRAY_SIZE    100   /* block arrays must all be the same size (including header) */
#endif

/*
** Memory Manager Action Request Codes 
** ---------------------------------------------------------------------------
*/
typedef enum { XJS_free, XJS_alloc } XJSMemReq;

/*
** Node type values determined by the API
** ---------------------------------------------------------------------------
*/
typedef enum 
{ XJS_unknown          =  0,  /* initialized value */
  XJS_array            =  1,  /* an array has been found, the CB can set a parent that will be used for all its elements */
  XJS_object           =  2,  /* an object has been found, the CB can set a parent that will be used for all its members */
  XJS_number           =  3,  /* a valid number field was parsed */
  XJS_string           =  4,  /* a valid string field was parsed */
  XJS_true             =  5,  /* a "true" literal was parsed */
  XJS_false            =  6,  /* a "false" literal was parsed */
  XJS_null             =  7,  /* a "null" literal was parsed */
  XJS_name             =  8,  /* NOT SIGNALED! only for memory management */
#ifdef XJS_CFLAG_ENABLE_BLOCKS 
  XJS_block            =  100,  /* a data block has been found, the CB can set a parent that will be used for all its records */
  XJS_block_end        = -100,  /* a data block is completed (parent is set to the data block) */
  XJS_block_header     =  101,  /* the first record of a block contains the header, mark start of header */ 
  XJS_block_header_end = -101,  /* the end of the header record has been found */ 
  XJS_block_array      =  102,  /* the next record in the block has started */
  XJS_block_array_end  = -102,  /* the current record in the block is complete */
#endif
  XJS_array_end        = -1,  /* signals that an array is completed (parent is set to the array) */
  XJS_object_end       = -2   /* signals that an object is completed (parent is set to the object) */
} XJSType; 

/*
** ---------------------------------------------------------------------------
** CALLBACK: XJSNodeCB
** Structure of JSON Node (item) Callback. This function is passed to the 
** parser and called whenever an item is encountered in the stream. 
**
** Returns XJS_OK, any other value will halt processing 
** ---------------------------------------------------------------------------
*/
typedef int (*XJSNodeCB)(
  void **parent,      /* IN/OUT : Callback should set on new array or object, and use on others to create hierarchy */
  XJSType kind,       /* IN : Specifies the kind of node that was parsed. */
  const char *name,   /* IN : Name for an object member */
  const char *value,  /* IN : Value field */
  void *node_arg);    /* IN : argument from parse call */
    
/*
** ---------------------------------------------------------------------------
** CALLBACK: XJSInputCB
** Structure of JSON Input Callback. This function is called whenever a NULL
** terminator is encountered in the input stream. This function should provide
** a pointer to a null terminated stream of JSON that continues where the last
** ended. 
**
** Returns XJS_OK, any other value will halt processing
** ---------------------------------------------------------------------------
*/
typedef int (*XJSInputCB)(
  const char **input, /* OUT : Pointer to new input buffer */
  void *inp_arg);     /* IN : argument from parse call */

/*
** ---------------------------------------------------------------------------
** CALLBACK: XJSMemCB
** Structure of Memory Manager Callback. This function is passed to the 
** parser and called whenever memory is required, or to free memory
** previously allocated, or to reallocate if an allocation is too small.
**
** Returns the pointer to memory, or NULL.
** ---------------------------------------------------------------------------
*/
typedef void *(*XJSMemCB)(
  XJSMemReq action,  /* IN : Memory service requested */
  XJSType context,   /* IN : The thing we are allocating for */
  void *prev,        /* IN : NULL, or pointer to free or realloc */
  XJSSize size,      /* IN : Storage requested, but can be overridden */
  XJSSize *actsz);   /* OUT : The actual size of the allocation */
    
/*
** ---------------------------------------------------------------------------
** Public API: xjs_parse function performs the parsing returns after the
** stream is completed or an error is encountered.
**
** Returns XJS_OK or an error code defined above.
** ---------------------------------------------------------------------------
*/
int xjs_parse(
  const char *json,      /* IN : NULL terminated input, or NULL to use inp_cb prior to parsing */
  XJSNodeCB node_cb,     /* IN : Node parsed callback function, can't be NULL */
  void *node_arg,        /* IN : Parameter passed to node_cb */
  XJSInputCB inp_cb,     /* IN : Request for input, can be NULL if json contains all the input */
  void *inp_arg,         /* IN : Parameter passed to inp_cb */
  XJSMemCB mem_cb,       /* IN : Optional memory manager callback (NULL for libc)   */
  const char **errpos);  /* OUT: NULL if successful, otherwise pointer to input when an error occurred */

/*
** ---------------------------------------------------------------------------
** Public API: Description functions return english descriptions. 
** If XJS_CFLAG_DISABLE_DESCRIPTIONS is defined no static storage will be
** used for the descriptions, and these functions will not be available.
** ---------------------------------------------------------------------------
*/
#ifndef XJS_CFLAG_DISABLE_DESCRIPTIONS
const char *xjs_desc_type(XJSType nodetype); /* returns an english description of the node type */
const char *xjs_desc_error(int code);        /* returns an english description of the error code */  
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __XJS_H__

/* EOF */
