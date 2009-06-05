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
const uint32 PIN_self=0xd51489ac;

const uint32 IID_lookup=1;
const uint32 PIN_lookup=0xef117f76;

const uint32 IID_object_id=2;
const uint32 PIN_object_id=0x4a7be0d8;

const uint32 IID_ref_cnt=3;
const uint32 PIN_ref_cnt=0xd06e12f0;

const uint32 IID_stream=4;
const uint32 PIN_stream=0xb107a7d9;

const uint32 IID_random_stream=5;
const uint32 PIN_random_stream=0x9e3ce6fe;

const uint32 IID_persist=6;
const uint32 PIN_persist=0x97de0f80;

const uint32 IID_oneway_pipe_sink=7;
const uint32 PIN_oneway_pipe_sink=0x4f203931;

const uint32 IID_trace_stream=8;
const uint32 PIN_trace_stream=0x059d6519;

const uint32 IID_clone = 9;
const uint32 PIN_clone = 0xfaf93ee3;

const uint32 IID_heart_beat=10;
const uint32 PIN_heart_beat=0x7f766298;

const uint32 IID_msg_sink=11;
const uint32 PIN_msg_sink=0xa99336e1;

const uint32 IID_msg_receiver=12;
const uint32 PIN_msg_receiver=0x128583bf;

const uint32 IID_aio_event_handler=13;
const uint32 PIN_aio_event_handler=0x8499a48b;

const uint32 IID_dyna_prop=14;
const uint32 PIN_dyna_prop=0x7691f437;

const uint32 IID_tpdu=15;
const uint32 PIN_tpdu=0x117c48b1;

const uint32 IID_aio_sap=16;
const uint32 PIN_aio_sap=0x58b7ac0b;

const uint32 IID_iovec=17;
const uint32 PIN_iovec=0x7516a373;

const uint32 IID_tp_connection = 18;
const uint32 PIN_tp_connection =0x79e01937;

const uint32 IID_tp_connection_sink = 19;
const uint32 PIN_tp_connection_sink = 0xb1fcd0e5;

const uint32 IID_tp_driver = 20;
const uint32 PIN_tp_driver = 0x5f9f340e;

const uint32 IID_tpkt_pack_sink = 21;
const uint32 PIN_tpkt_pack_sink = 0xe3bbddc5;

const uint32 IID_tpkt_aload_sink =22;
const uint32 PIN_tpkt_aload_sink =0x9d46cbb4;

const uint32 IID_tp_connection_indication_sink =23;
const uint32 PIN_tp_connection_indication_sink=0xfcea534a;

const uint32 IID_crypto =24;
const uint32 PIN_crypto = 0x8e647b1f;

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_IID_DREAMFREELANCER_20090513_H__

