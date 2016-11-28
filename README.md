# xjs
A simple JSON parser for C/C++

**xjs** parses a JSON stream and signals the caller of items parsed via
a callback function. Input can be ASCII or UTF-8

**xjs** does not provide any hierarchical structure to the stream.
The caller can assosicate memory with parsed objects and arrays within
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
parameter to parse is `NULL`. If defined a memory manager must be
provided to parse.

`XJS_CFLAG_ENABLE_BLOCKS` - if defined the parser will handle Blocks as 
described above otherwise blocks in JSON will generate an error.

`XJS_CFLAG_DISABLE_DESCRIPTIONS` - if defined the library will provide
ascii english descriptions for Node Types and Error Codes.

`XJS_CFLAG_DISABLE_ARRAY_POS_AS_NAME` - **xjs** sends the ordinal number
of array elements as ascii text in the name field for all array elements.
If defined the name field will be ```NULL``` for array elements. 
*Otherwise `snprintf` from libc will be linked.*

`XJS_CFLAG_DISABLE_SIZE_T` - if defined the API will not use `size_t`
from libc, instead it will use `unsigned long`.
