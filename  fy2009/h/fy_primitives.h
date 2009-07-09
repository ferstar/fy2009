/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2009-5-13
 * ====================================================================
 */
#ifndef __FENGYI2009_PRIMITIVES_DREAMFREELANCER_20090513_H__
#define __FENGYI2009_PRIMITIVES_DREAMFREELANCER_20090513_H__

#include <string>
#include <vector>

//macros about fy2009 namespace declaration and using

#define DECL_FY_NAME_SPACE_BEGIN namespace fy2009{
#define DECL_FY_NAME_SPACE_END   } 

#define USING_FY_NAME_SPACE using namespace fy2009;

DECL_FY_NAME_SPACE_BEGIN

/*[tip] primitive typedefs
 *[desc] which indicates length of type. it makes it easy to calculate the serialized length
 *
 *[history]
 * passed test on win32 and linux,2006-5-23
 */
typedef char int8; //size is 8 bits
typedef unsigned char uint8; //size is 8 bits

typedef short int16; //size is 16 bits
typedef unsigned short uint16; //size is 16 bits

typedef long int32; //size is 32 bits 
typedef unsigned long uint32; //size is 32 bits

#ifdef LINUX

typedef long long int64; //size is 64 bits
typedef unsigned long long uint64; //size is 64 bits

#elif defined(WIN32)

typedef LONGLONG int64;
typedef ULONGLONG uint64;

#endif

//pointer_box_t is used to contain or transfer pointer type, it should be portable between 32bit OS and 64bit OS
#ifdef __OS64__

typedef uint64 pointer_box_t;

#else

typedef uint32 pointer_box_t; 

#endif

/*[tip] typedef std::string
 *[desc] in almost any occuasion, std::string can replace plain c string, meanwhile, you still can use pain c function 
 * if you like just by extracting plain c string from std::string by calling c_str()
 */
typedef std::string string_t;

/*[tip] int8 vector type.
 *[desc] communication application often needs a buffer to serialize/deserialize data, but plain c array isn't flexible
 * to be used as function output parameter, so c style api function always asks for caller to pre-allocate a big enough buffer
 * ,unfortunately, caller often can't predicate how many bytes are proper. stl vector is a better alternative, both caller and
 * callee can adjust its size as needed by calling reserve(), you also can use it just like a c array.
 *[memo]
 * only push_back() or resize() can change its size(), operator[] doesn't and also doesn't check memroy boundary
 */
typedef std::vector<int8> int8v_t;

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_PRIMITIVES_DREAMFREELANCER_20090513_H__

