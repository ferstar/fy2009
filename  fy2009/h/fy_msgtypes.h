/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2009-6-1
 * ====================================================================
 */
#ifndef __FENGYI2009_MESSAGETYPE_DREAMFREELANCER_20080604_H__
#define __FENGYI2009_MESSAGETYPE_DREAMFREELANCER_20080604_H__

#include "fy_primitives.h"

DECL_FY_NAME_SPACE_BEGIN

//message macros
//->
uint32 const MSG_NULL=0;
uint32 const MSG_PIN_NULL=0xbe44fa8f;

//remove before pending msg with specific reciever
//para_0(int32):destination msg type, zero means removing all
//msgs to para receiver
uint32 const MSG_REMOVE_MSG=1;
uint32 const MSG_PIN_REMOVE_MSG=0x7da5a2fa;

uint32 const MSG_USER=1000;
uint32 const MSG_PIN_USER=0x402f1606;

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_MESSAGETYPE_DREAMFREELANCER_20080604_H__
