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
#ifndef __FENGYI2009_IID_DREAMFREELANCER_20090513_H__
#define __FENGYI2009_IID_DREAMFREELANCER_20090513_H__

#include "fy_primitives.h"

DECL_FY_NAME_SPACE_BEGIN

//[bullet] generic interfaces
const uint32 IID_self=0;
const uint32 IID_lookup=1;
const uint32 IID_object_id=2;
const uint32 IID_ref_cnt=3;
const uint32 IID_stream=4;
const uint32 IID_random_stream=5;
const uint32 IID_persist=6;
const uint32 IID_oneway_pipe_sink=7;
const uint32 IID_trace_stream=8;
const uint32 IID_clone = 9;
const uint32 IID_heart_beat=10;
const uint32 IID_msg_sink=11;
const uint32 IID_msg_receiver=12;
const uint32 IID_aio_event_handler=13;
const uint32 IID_dyna_prop=14;
const uint32 IID_tpdu=15;
const uint32 IID_aio_sap=16;
const uint32 IID_iovec=17;
const uint32 IID_tp_connection = 18;
const uint32 IID_tp_connection_sink = 19;
const uint32 IID_tp_driver = 20;
const uint32 IID_tpkt_pack_sink = 21;
const uint32 IID_tpkt_aload_sink =22;
const uint32 IID_tp_connection_indication_sink =23;
const uint32 IID_crypto =24;

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_IID_DREAMFREELANCER_20090513_H__

