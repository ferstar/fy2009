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

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_PRIMITIVES_DREAMFREELANCER_20090513_H__

