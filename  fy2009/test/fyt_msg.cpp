/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@hotmail.com
 *
 * [History]
 * initialize: 2009-5-21
 * ====================================================================
 */
#ifdef LINUX
#define FY_TEST_MSG
#endif //LINUX

#ifdef FY_TEST_MSG

#include "fy_msg.h"

USING_FY_NAME_SPACE

#ifdef LINUX

struct timeval last_tv={0,0};

#elif defined(WIN32)

uint32 last_tc=0;

#endif
class stub_msg_recver_t : public msg_receiver_it,
                          public ref_cnt_impl_t
{
public:
        stub_msg_recver_t():_cs(true),ref_cnt_impl_t(&_cs){ _cnt=0; }
        void on_msg(msg_t *msg)
        {
		if(msg->get_msg() == 888)
		{
#ifdef LINUX
			struct timeval tv;
			gettimeofday(&tv,0);
			printf("==msg 888 interval:%d\n", timeval_util_t::diff_of_timeval_tc(last_tv, tv));
			last_tv = tv;
#elif defined(WIN32)
			uint32 tc = GetTickCount();
			printf("==msg 888 interval:%d\n", tc - last_tc);
			last_tc = tc;
#endif
		}
                ++_cnt;
                printf("handle a msg[%d]:%d,tc_posted:%d,utc_interval:%d,repeat:%d\n",
                        _cnt,msg->get_msg(), msg->get_utc_posted(), msg->get_utc_interval(), msg->get_repeat());
//                fy_msleep(100); //determine msg proxy heart_beat return busy or interrupted
/*
                if(_cnt == 2)
                {
                        sp_msg_proxy_t msg_proxy=sp_msg_proxy_t(msg_proxy_t::s_tls_instance(),true);
                        sp_msg_t msg=msg_t::s_create(MSG_REMOVE_MSG,MSG_PIN_REMOVE_MSG,1);
                        msg->set_receiver(sp_msg_rcver_t(this,true));
                        msg->set_para(0,variant_t((int32)0));

                        printf("post a REMOVE_MSG\n");
                        msg_proxy->post_msg(msg);
                }
*/
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

stub_msg_recver_t *g_rcver=0;

#ifdef LINUX
void *tf_msg(void *arg)
#elif defined(WIN32)
DWORD WINAPI tf_msg(void *arg)
#endif
{
        msg_proxy_t *msg_proxy=(msg_proxy_t*)arg;
        msg_proxy->add_reference();
        sp_msg_t msg=msg_t::s_create((uint32)fy_thread_self(),0, 0);
        msg->set_receiver(sp_msg_rcver_t(g_rcver,true));
        msg->set_tc_interval(2000);
        msg_proxy->post_msg(msg);
        msg_proxy->release_reference();

	return 0;
}

void test_msg()
{
        sp_msg_proxy_t msg_proxy=sp_msg_proxy_t(msg_proxy_t::s_tls_instance(),true);
        sp_msg_t msg=msg_t::s_create(888,0,0);
        msg->set_repeat(-1);
        msg->set_tc_interval(1000);
        g_rcver=new stub_msg_recver_t();
        g_rcver->add_reference();
        msg->set_receiver(sp_msg_rcver_t(g_rcver,true));
        msg_proxy->post_msg(msg);

        //add msg 2
        msg=msg_t::s_create(999,0,0);
        msg->set_repeat(4);
        msg->set_tc_interval(2000);
        msg->set_receiver(sp_msg_rcver_t(g_rcver,true));
        msg_proxy->post_msg(msg);


	//test cross-thread message
        fy_thread_t thd;
        msg_proxy_t *raw_msg_proxy=(msg_proxy_t*)msg_proxy;
        for(int i=0;i<10;++i) 
	{
#ifdef LINUX
		pthread_create(&thd,0,tf_msg,(void *)(raw_msg_proxy));
#elif defined(WIN32)
		thd = ::CreateThread(NULL, 0, tf_msg, (void*)raw_msg_proxy, 0, NULL); 
#endif
	}
	uint32 deta_sleep=0;
        while(1)
        {
                int8 ret=msg_proxy->heart_beat();
                switch(ret)
                {
                case RET_HB_IDLE:
			deta_sleep=msg_proxy->get_min_delay_interval();
                        printf("heart_beat ret:RET_HB_IDLE,min_delay:%d\n",deta_sleep);
			fy_msleep(deta_sleep/4);
                        break;
                case RET_HB_BUSY:
                        printf("heart_beat ret:RET_HB_BUSY\n");
                        break;
                case RET_HB_INT:
                        printf("heart_beat ret:RET_HB_INT\n");
                        break;
                default:
                        printf("heart_beat ret:UNKNOWN\n");
                        break;
                }
        }

        msg_proxy_t::s_delete_tls_instance();
}

int main(int argc, char **argv)
{
	FY_TRY

	user_clock_t::instance();
	//pre-heat user_clock 
	fy_msleep(200);

	test_msg();

	__INTERNAL_FY_EXCEPTION_TERMINATOR(;);

	return 0;
}

#endif //FY_TEST_MSG


