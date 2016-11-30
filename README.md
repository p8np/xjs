# xjs
A simple JSON parser for C/C++

**xjs** parses a JSON stream and signals the caller of items parsed via
a callback function. 
Input can be ASCII or UTF-8

# Usage

Parsing JSON is accomplished with a single call to `xjs_parse`. The function
takes 7 parameters including 3 callback functions:

1. `const char *json` - (optional) can provide the entire json to be parsed,
the beginning json to be parsed and the input callback will provide the rest, 
or `NULL` and the input callback will provide all the input. 

2. `XJSNodeCB` - gets called once for every item parsed, and twice for
containers, once when the container is opened, and again when it is closed. 

3. `void *node_arg` - to be passed to the `XJSNodeCB` function.

4. `XJSInputCB` - (optional) gets called when the parser encounters `0` byte
(null terminator) in the input before the end of the highest level
object or array. This callback allows the caller to provide the parser
with more input. 

5. `void *inp_arg` - to be passed to the `XJSInputCB` function.

6. `XJSMemCB` - (optional) gets called when the parser needs storage for an
element. If `NULL` the parser will fallback to `malloc/free`. You can
use the callback to implement static minimal storage (see example).

7. `const char **errpos` - sets `errpos` to the position in the input where
an error occurred.

**xjs** does not provide any hierarchical structure to the stream.
The caller can associate memory with parsed objects and arrays within
the callback. These addressed will passed to the callback for all the
items parsed while the object or array is active. This feature facilitates
creating a hierarchical representation of the nodes parsed.

**xjs** also supports an input callback that allows the caller to append input
data when a null terminator is encountered. This is useful for parsing a
stream whose size is unknown, or whose content is impractical to store in a
single memory allocation. The input can be broken anywhere so that reading
a fixed size buffer of input can be done in a loop without concern for
element boundaries.

Each buffer returned by the input callback must be null terminated. 

## Blocks (not standard JSON)

Blocks provides a more compact method of representing an array
of homogeneous objects (each with the same number of fields). 

Every array in the block must contain **N** elements. The first array
in a block is the header containing the member names as string values. 
All the subsequent arrays are data records, and the fields must be
in the same order as the fields in the header.

Blocks are disabled by default. You must define `XJS_ENABLE_BLOCKS`
in your project, makefile, command line or source code prior to 
including `xjs.h`

### Block example

```
(["Rank", "Team", "2016", "Last 3", "Last 1", "Home", "Away", "2015"], 
 [ 1, "Chi Cubs", 3.12, 3.00, 2.00, 2.65, 3.62, 3.41 ], 
 [ 2, "Washington", 3.53, 4.50, 4.00, 3.42, 3.64, 3.62 ], 
 [ 3, "NY Mets", 3.57, 3.12, 3.00, 3.40, 3.74, 3.42 ], 
 [ 4, "SF Giants", 3.64, 4.20, 4.00, 3.51, 3.76, 3.72 ], 
 [ 5, "Cleveland", 3.69, 1.73, 3.38, 3.71, 3.67, 3.67 ] 
)
```

## Compile-time Preprocessor Flags

**xjs** watches for two compiler flags:

`XJS_CFLAG_DISABLE_MEM_FALLBACK` - By default the parser will fallback
to using libc `malloc/free` to allocate storage when the `XJSMemCB`
parameter to parse is `NULL`. When defined, a memory manager must be
provided to parse and `malloc/free/realloc` will **NOT** be linked.

`XJS_CFLAG_ENABLE_BLOCKS` - When defined, the parser will handle Blocks as 
described above otherwise blocks in JSON will generate an error.

`XJS_CFLAG_DISABLE_DESCRIPTIONS` - When defined, the library will provide
ascii english descriptions for Node Types and Error Codes.

`XJS_CFLAG_DISABLE_ARRAY_POS_AS_NAME` - **xjs** sends the ordinal number
of array elements as ascii text in the name field for all array elements.
When defined, the name field will be ```NULL``` for array elements and 
`snprintf` will **NOT** be linked.

`XJS_CFLAG_DISABLE_SIZE_T` - When defined, the API will not use `size_t`
from libc, instead it will use `unsigned long`.
