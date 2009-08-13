/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@hotmail.com
 *
 * [History]
 * initialize: 2009-6-22
 * ====================================================================
 */
#ifdef LINUX
#define FY_TEST_THD
#endif

#ifdef FY_TEST_THD
 
#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <vector>
#include <queue>
#include <string>

#ifdef LINUX

#include <unistd.h>

#endif
#include "fy_thread.h"
#include "fy_trace.h"

USING_FY_NAME_SPACE

class stub_msg_recver_t : public msg_receiver_it,
                          public ref_cnt_impl_t
{
public:
        stub_msg_recver_t():_cs(true),ref_cnt_impl_t(&_cs){ _cnt=0; }
        void on_msg(msg_t *msg)
        {
                ++_cnt;
                FY_INFOD("handle a msg["<<_cnt<<"]:"<<msg->get_msg()<<",utc_posted:"<<msg->get_utc_posted()
			<<",tc_interval:"<<msg->get_tc_interval()<<",repeat:"<<msg->get_repeat());

		fy_msleep(100);

                if(_cnt == 2)
                {
                        sp_msg_proxy_t msg_proxy=sp_msg_proxy_t(msg_proxy_t::s_tls_instance(),true);
                        sp_msg_t msg=msg_t::s_create(MSG_REMOVE_MSG, MSG_PIN_REMOVE_MSG, 1);
                        msg->set_receiver(sp_msg_rcver_t(this,true));
                        msg->set_para(0,variant_t((int32)99));

                        FY_INFOD("post a REMOVE_MSG");
                        msg_proxy->post_msg(msg);
                }

        }
        void *lookup(uint32 iid, uint32 pin) throw()
        {
                switch(iid)
                {
                case IID_self:
			if(pin != PIN_self) return 0;
                        return this;
                case IID_msg_receiver:
			if(pin != PIN_msg_receiver) return 0;
                        return static_cast<msg_receiver_it*>(this);
                default:
                        return ref_cnt_impl_t::lookup(iid, pin);
                }
        }
private:
        uint32 _cnt;
        critical_section_t _cs;
};

//pass test 2008-6-12
//test thread_t
uint32 g_last_utc=0;
class test_thread_t : public thread_t
{
public:
	void run()
	{
		FY_INFOD(">>>>start initializing test thread");

		//initialize here
		msg_proxy_t *msg_proxy=get_msg_proxy();	

		FY_INFOD("<<<<end initializing test thread");

		_unlock_start();

		//send a msg to itself.
        	sp_msg_t msg=msg_t::s_create(999, 0, 0);
        	msg->set_repeat(0);
        	msg->set_tc_interval(100);	
		msg->set_receiver(sp_msg_rcver_t(new stub_msg_recver_t(),true));
		if(msg_proxy) msg_proxy->post_msg(msg);
		
		while(true)
		{
			if(_is_stopping())
			{ 
				FY_INFOD("!!!got stop command, no hurry, wait,wait...");

				//fy_msleep(5000);

				FY_INFOD("ok, I stop on my own");
				break;
			}
			//do something
			user_clock_t *usr_clk=user_clock_t::instance(); 
			uint32 utc_cur = get_tick_count(usr_clk);
			if(tc_util_t::is_over_tc_end(g_last_utc, 100, utc_cur))
			{
				g_last_utc = utc_cur;

				FY_INFOD("do something from thread");
				FY_INFOD("trace detail from thread");
				FY_INFOI("trace important from thread");
				FY_WARNING("trace warning from thread");
				FY_ERROR("trace error from thread");
			}
			//receive msg
			if(msg_proxy) 
			{
				if(msg_proxy->heart_beat() == RET_HB_IDLE) 
				{
					_on_idle(1000);
				}
			}
			else
			{
				_on_idle(1000);
			}

		}
	}
	void on_cancel()
	{
		FY_INFOD("thread has been canceled, on_cancel is called");
	}
};

void test_thd()
{
	FY_INFOD("main test info detail");
	FY_INFOI("main test info important");

	test_thread_t thd;
	thd.enable_trace();
	thd.enable_msg();

	FY_INFOD("...start a thread");
	thd.start();
	FY_INFOD("thread is running...");
	
	//send msg to thread
        sp_msg_t msg=msg_t::s_create(888,0,0);
        msg->set_repeat(2);
        msg->set_tc_interval(100);

        stub_msg_recver_t *rcver=new stub_msg_recver_t();
        rcver->add_reference();

        msg->set_receiver(sp_msg_rcver_t(rcver,true));
	msg_proxy_t *msg_proxy=thd.get_msg_proxy();       
 	if(msg_proxy) 
		msg_proxy->post_msg(msg);
	else
		FY_INFOD("can't get valid msg_proxy linked to thread");
	
	fy_msleep(3000); //wait for 3 seconds before exiting

	FY_INFOD("...stop a thread");
	thd.stop();
	FY_INFOD("thread is stopped");	
	
	rcver->release_reference();
}
/*
//test thread pool
void test_thd_pool()
{
        trace_provider_t *trace_prvd=trace_provider_t::instance();
        trace_prvd->register_trace_stream(0, sp_trace_stream_t(), REG_TRACE_STM_OPT_ALL); //remove default trace stream(STDOUT)

        sp_trace_stream_t tr_stm_detail;
        for(uint8 i=0; i<MAX_TRACE_LEVEL_COUNT; ++i)
        {
                sp_trace_stream_t tr_stm=trace_file_t::s_create(i);
                trace_prvd->register_trace_stream(i, tr_stm, REG_TRACE_STM_OPT_EQ);
                if(i == TRACE_LEVEL_INFOD) tr_stm_detail=tr_stm;
        }
        trace_prvd->register_trace_stream(TRACE_LEVEL_INFOD, tr_stm_detail,
                                REG_TRACE_STM_OPT_LT|REG_TRACE_STM_OPT_GT);

        trace_prvd->open();

        trace_prvd->register_tracer();

	sp_tpool_t thd_pool=thread_pool_t::s_create(3);//pool_size=3
	uint16 thd_idx=0; 
	for(int i=0;i<10;++i)
	{

	sp_thd_t sp_thd=thd_pool->assign_thd(&thd_idx);
	if(!sp_thd.is_null())
		printf("assigned a thread from pool,idx=%d\n",thd_idx);
	else
	{
		printf("assign thread failed\n");
		continue;
	}
	//send a message to assigned thread
        sp_msg_t msg=msg_t::s_create(thd_idx+100,0);
        msg->set_repeat(i);
        msg->set_tc_interval(10);
        msg->set_receiver(sp_msg_rcver_t(new stub_msg_recver_t(),true));
	msg_proxy_t *msg_proxy=sp_thd->get_msg_proxy();
        if(msg_proxy) msg_proxy->post_msg(msg);	
	}

	usleep(10000000);//10s
	thd_pool->stop_all();

	trace_prvd->unregister_tracer();
        trace_prvd->close();
}
*/

int main(int argc,char **argv)
{
        trace_provider_t *trace_prvd=trace_provider_t::instance();
	//change trace output
	//->
/*        trace_prvd->register_trace_stream(0, sp_trace_stream_t(), REG_TRACE_STM_OPT_ALL); //remove default trace stream(STDOUT)

        sp_trace_stream_t tr_stm_detail;
        for(uint8 i=0; i<MAX_TRACE_LEVEL_COUNT; ++i)
        {
                sp_trace_stream_t tr_stm=trace_file_t::s_create(i);
                trace_prvd->register_trace_stream(i, tr_stm, REG_TRACE_STM_OPT_EQ);
                if(i == TRACE_LEVEL_INFOD) tr_stm_detail=tr_stm;
        }
        trace_prvd->register_trace_stream(TRACE_LEVEL_INFOD, tr_stm_detail,
                                REG_TRACE_STM_OPT_LT|REG_TRACE_STM_OPT_GT);
*/	//<-
        trace_prvd->open();
        //give enough time to initialize time service
        fy_msleep(100);
        trace_prvd->register_tracer();

	FY_TRY

        test_thd();
        //test_thd_pool();

	FY_EXCEPTION_TERMINATOR("thd-main",;);

        trace_prvd->unregister_tracer();
        trace_prvd->close();

        return 0;
}

#endif //FY_TEST_THD
