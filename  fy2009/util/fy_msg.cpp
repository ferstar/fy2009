/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2009-4-29
 * ====================================================================
 */
#include "fy_msg.h"
#include "fy_trace.h"
#include "fy_base.h"

USING_FY_NAME_SPACE

//msg_t
sp_msg_t msg_t::s_create(uint32 msg, uint32 pin, uint16 para_count, bool rcts_flag)
{
	msg_t *raw_msg=new msg_t(msg, pin, para_count);
	if(rcts_flag) raw_msg->set_lock(&(raw_msg->_cs));

	return sp_msg_t(raw_msg,true);	
}

msg_t::msg_t() : _cs(true)
{
	_flag=0; 
	_msg=MSG_NULL;
	_pin = 0;
	_utc_posted=0;
	_utc_interval=0;
	_repeat=0;
	_pv_para=0;
	_pv_len=0; 
}

msg_t::msg_t(uint32 msg, uint32 pin, uint16 para_count) : _cs(true)
{
	_flag=0;
	_msg=msg; 
	_pin = pin;
	_utc_posted=0;
	_utc_interval=0;
	_repeat=0;
	_pv_len=para_count;
	if(_pv_len) 
		_pv_para=new variant_t[_pv_len];
	else
		_pv_para=0;
}

msg_t::~msg_t()
{
	if(!_smt_on_destroy.is_null())
	{
		_smt_on_destroy->on_msg_destroy(_msg, _pv_para, _pv_len);
	}
	if(_pv_para) delete [] _pv_para;
}

void msg_t::set_para(uint16 para_index, const variant_t& para)
{
	if((_flag & MSG_FLAG_POSTED) == MSG_FLAG_POSTED)
	{
		FY_XERROR("set_para, it is posted, not allow change");
		return;	
	}
	if(para_index >= _pv_len) 
	{
		FY_DECL_EXCEPTION_CTX_EX("set_para");
		FY_THROW_EX("sp", "invalid para index:"<<para_index<<",valid para len:"<<_pv_len);
	}
	_pv_para[para_index] = para;
}

const variant_t& msg_t::get_para(uint16 para_index)
{
	if(para_index >= _pv_len)
	{
		FY_DECL_EXCEPTION_CTX_EX("get_para");
		FY_THROW_EX("gp", "invalid para index:"<<para_index<<",valid para len:"<<_pv_len);
	}
	return _pv_para[para_index];
}

void msg_t::set_tc_interval(uint32 tc_interval) throw()
{
	if((_flag & MSG_FLAG_POSTED) == MSG_FLAG_POSTED)
	{
		FY_XERROR("set_tc_interval, it is posted, not allow change");
                return;
	}
 
        user_clock_t *uclk=user_clock_t::instance();
        FY_ASSERT(uclk);

        uint32 utc_res=get_tick_count_res(uclk);
        if(utc_res)       
		_utc_interval = tc_interval/utc_res;
	else
		_utc_interval = tc_interval; 
}

uint32 msg_t::get_tc_interval() const throw()
{
        user_clock_t *uclk=user_clock_t::instance();
        FY_ASSERT(uclk);
        
        uint32 utc_res=get_tick_count_res(uclk);
        if(utc_res)
		return _utc_interval * utc_res;
	else
		return _utc_interval;	
}

//lookup_i
void *msg_t::lookup(uint32 iid, uint32 pin) throw()
{       
        switch(iid)
        {
        case IID_self:
		if(pin != PIN_self) return 0;
        case IID_lookup:
		if(pin != PIN_lookup) return 0;
                return this;
        
        case IID_object_id:
                return object_id_impl_t::lookup(iid, pin); 
        
        default:
                return ref_cnt_impl_t::lookup(iid, pin);
        }
}

//msg_proxy_t
fy_thread_key_t msg_proxy_t::_s_tls_key=fy_thread_key_t();

bool msg_proxy_t::_s_key_created=false;
critical_section_t msg_proxy_t::_s_cs=critical_section_t(true);
uint32 msg_proxy_t::_s_local_mq_bound[MPXY_LOCAL_MQ_CNT]={8,64,512,4096,32768};//unit:user tick-count

msg_proxy_t *msg_proxy_t::s_tls_instance(uint32 mp_size,
					event_slot_t *es_notfull, uint16 esi_notfull,
                                        event_slot_t *es_notempty, uint16 esi_notempty)
{       
        if(!_s_key_created)//lazy create tls key
        {
                _s_cs.lock();

                if(!_s_key_created)
                {
                        if(fy_thread_key_create(&_s_tls_key,0))
                        {
                                FY_ERROR("msg_proxy_t::s_tls_instance, create TLS key error");
                                _s_cs.unlock(); 
 
                                return 0;
                        }
                        _s_key_created=true;
                }

                _s_cs.unlock();
        }
        void *ret=fy_thread_getspecific(_s_tls_key);//read msg_proxy_t pointer from Thread Local Storage
        if(ret) return (msg_proxy_t *)ret; //current thread ever has one msg proxy instance
        
        msg_proxy_t *msg_proxy=new msg_proxy_t(mp_size, es_notfull, esi_notfull, es_notempty, esi_notempty);
        
        //register read thread for _mp
        msg_proxy->_mp->register_read();

        //register msg proxy to TLS
	msg_proxy->add_reference();
        int32 ret_set=fy_thread_setspecific(_s_tls_key,(void *)msg_proxy);
        FY_ASSERT(ret_set == 0);
	msg_proxy->_thd=fy_thread_self();

        return msg_proxy;
}

void msg_proxy_t::s_delete_tls_instance()
{
        if(!_s_key_created) return;

        msg_proxy_t *msg_proxy=(msg_proxy_t*)fy_thread_getspecific(_s_tls_key);
        if(msg_proxy) msg_proxy->release_reference();
        fy_thread_setspecific(_s_tls_key,0);//reset TLS
}

msg_proxy_t::msg_proxy_t(uint32 mp_size, 
				event_slot_t *es_notfull, uint16 esi_notfull,
                        	event_slot_t *es_notempty, uint16 esi_notempty
			) : _cs(true), ref_cnt_impl_t(&_cs) 
{  
        _mp=oneway_pipe_t::s_create(sizeof(msg_t*)*mp_size);

	_mp->register_sink(this);

	memset(_utc_lastpoll,0,sizeof(uint32)*MPXY_LOCAL_MQ_CNT);
	_idx_nextpoll=0;
	_max_slice=MPXY_HB_MAX_SLICE;

	_es_notfull = es_notfull;
	_esi_notfull = esi_notfull;
	
	_es_notempty = es_notempty;
	_esi_notempty = esi_notempty;	
}

//it's forbidden to call IMsg_Sink.OnMsg with this function,otherwise,
//post_msg may be called within OnMsg, which will cause _local_mq to be destroyed 
#define SLICE_CHECK_POINT 128

int8 msg_proxy_t::_poll_local_mq(uint32 utc_start, uint8 start_idx, uint8 end_idx)
{
	FY_XFUNC("_poll_local_mq,utc_start:"<<utc_start<<",start_idx:"<<start_idx<<",end_idx:"<<end_idx);
	
	FY_DECL_EXCEPTION_CTX_EX("_poll_local_mq");

	FY_ASSERT(start_idx<=end_idx);
	FY_ASSERT(start_idx<MPXY_LOCAL_MQ_CNT && end_idx<MPXY_LOCAL_MQ_CNT);

        user_clock_t *usr_clk=user_clock_t::instance();
        FY_ASSERT(usr_clk);

	int8 hb_ret=RET_HB_IDLE;

	FY_TRY

        //poll local messages
        for(uint8 i=start_idx;i<=end_idx;++i)
        {
                //always poll _local_mq[0] as often as possible
                if(i && !tc_util_t::is_over_tc_end(_utc_lastpoll[i],
                                msg_proxy_t::_s_local_mq_bound[i-1],
                                get_tick_count(usr_clk))) continue;

                //poll _local_mq[i]
                for(mq_t::iterator it=_local_mq[i].begin(); it!=_local_mq[i].end(); )
                {
			FY_TRY //2008-12-25, avoid dead-running caused by failed msg

                        uint32 utc_cur=get_tick_count(usr_clk);
			sp_msg_t& smt_msg=*it;
			uint32 utc_interval=smt_msg->get_utc_interval();
			if(utc_interval) //delay running
                        {
				//meet running condition
                                if(tc_util_t::is_over_tc_end(smt_msg->get_utc_posted(), utc_interval, utc_cur))
                                {
					hb_ret=RET_HB_BUSY;
					sp_msg_rcver_t smt_receiver=smt_msg->get_receiver();
					//slice usage is determined by receiver.on_msg 
					if(!smt_receiver.is_null()) smt_receiver->on_msg(smt_msg);

					//update _repeat
                                        int32 repeat=smt_msg->get_repeat();
                                        if(!repeat)
                                        {
                                                it=_local_mq[i].erase(it);
                                        }
                                        else 
                                        {
                                                smt_msg->_set_utc_posted(utc_cur); //update posted utc
						//_repeat < 0 means repeat forever
                                                if(repeat > 0) smt_msg->_repeat=repeat - 1;
                                                ++it;
                                        }
                                }
                                else //not meet running condition
                                {
                                        ++it;
                                }
                        }
                        else //_utc_interval == 0 means running as soon as possible 
                        {
				hb_ret=RET_HB_BUSY;
				sp_msg_rcver_t smt_receiver=smt_msg->get_receiver();
				//slice usage is determined by receiver.on_msg 
                                if(!smt_receiver.is_null()) smt_receiver->on_msg(smt_msg);
                                it=_local_mq[i].erase(it);
                        }
			//2008-12-25,avoid dead-running caused by failed msg	
			FY_CATCH_N_THROW_AGAIN_EX("mpxylmf","cnta", _local_mq[i].erase(it); return hb_ret;);
                }

                _utc_lastpoll[i]=get_tick_count(usr_clk);
		_idx_nextpoll=i+1;
		if(_idx_nextpoll == MPXY_LOCAL_MQ_CNT) _idx_nextpoll=0;

                //exceeds max slice
                if(tc_util_t::is_over_tc_end(utc_start, _max_slice,get_tick_count(usr_clk))) return RET_HB_INT;
        }
	
	FY_CATCH_N_THROW_AGAIN_EX("mpxyplm","cnta",;);

	return hb_ret;
}

void msg_proxy_t::_remove_local_msg(const sp_msg_rcver_t& rcver, uint32 msg)
{
	FY_XFUNC("_remove_local_msg");

	for(uint8 i=0; i<MPXY_LOCAL_MQ_CNT; ++i)
	{
		for(mq_t::iterator it=_local_mq[i].begin(); it!=_local_mq[i].end();)
		{
			if(!rcver.is_same_as((*it)->get_receiver()))
			{
				++it;
				continue;
			}
			if(msg && (msg != (*it)->get_msg())) 
			{
				++it;
				continue;
			}
			it=_local_mq[i].erase(it);
		}
	}	
}

void msg_proxy_t::_push_to_local_mq(sp_msg_t& msg)
{
	FY_XFUNC("_push_to_local_mq,msg:"<<msg->get_msg());

	if(msg->get_msg() == MSG_REMOVE_MSG)
	{
		_remove_local_msg(msg->get_receiver(), (msg->get_para(0)).get_i32());
		return;
	}

	uint32 utc_interval=msg->get_utc_interval();
        if(utc_interval) //delay msg
        {
                for(int i=0; i< MPXY_LOCAL_MQ_CNT - 1 ; ++i)
                {
                	if(utc_interval < msg_proxy_t::_s_local_mq_bound[i])
                        {
                        	_local_mq[i].push_back(msg);
                                return;
                        }
                }

                //no match
                _local_mq[MPXY_LOCAL_MQ_CNT -1].push_back(msg);
        }
        else
        {
                _local_mq[0].push_back(msg);
        }
}

void msg_proxy_t::_lazy_init_object_id() throw()
{
	FY_TRY

 	string_builder_t sb;
 	sb<<"msg_proxy_"<<(void*)this<<"_thd"<<(uint32)_thd;
 	sb.build(_object_id);
 
	__INTERNAL_FY_EXCEPTION_TERMINATOR(;);
}

uint32 const MPXY_MAX_TO_LOCAL_CNT_PER_HB=256;

int8 msg_proxy_t::heart_beat()
{
	FY_XFUNC("heart_beat");

        user_clock_t *usr_clk=user_clock_t::instance();
        FY_ASSERT(usr_clk);
        uint32 utc_start=get_tick_count(usr_clk);
	int8 hb_ret=RET_HB_IDLE;

	FY_TRY

	//batch transfer message in _post_owner_q to _local_mq
	if(!_post_owner_q.empty()) hb_ret=RET_HB_BUSY;
	uint32 to_local_cnt=0;
	while(!_post_owner_q.empty()) //never call post_msg from this loop
	{
		sp_msg_t tmp_msg=_post_owner_q.front();
		_post_owner_q.pop_front();
		_push_to_local_mq(tmp_msg);
		if(++to_local_cnt >= MPXY_MAX_TO_LOCAL_CNT_PER_HB) break;
	}

	//check and run local message
	uint8 old_idx_nextpoll=_idx_nextpoll;

	int8 tmp_ret=_poll_local_mq(utc_start, _idx_nextpoll, MPXY_LOCAL_MQ_CNT - 1);
	if(tmp_ret > hb_ret) hb_ret=tmp_ret;
	if(RET_HB_INT == hb_ret) return hb_ret;

	if(old_idx_nextpoll) tmp_ret=_poll_local_mq(utc_start, 0, old_idx_nextpoll - 1);
	if(tmp_ret > hb_ret) hb_ret=tmp_ret;
	if(RET_HB_INT == hb_ret) return hb_ret; 

	//try to receive remote msg from _mp
	msg_t *raw_msg=0;
	while(1)
	{
		uint32 read_len=_mp->read((int8*)&raw_msg, sizeof(msg_t*), false);
		if(read_len == 0) break; //no remote msg

		if(read_len == sizeof(msg_t*))
		{
			_mp->commit_r();
			//notify any one of writer threads the message pipe isn't full now
			if(_es_notfull) _es_notfull->signal(_esi_notfull);
		}
		else
		{
			_mp->rollback_r();

			FY_ERROR("msg_proxy_t::heart_beat, read partial message from _mq");
			break;
		}
		FY_ASSERT(raw_msg);
		sp_msg_t smt_msg(raw_msg); //attach raw_msg, can't add reference here,it got it from _mp
	
		hb_ret=RET_HB_BUSY;
		if(smt_msg->get_msg() == MSG_REMOVE_MSG)
		{
			variant_t para_msg=smt_msg->get_para(0);
			FY_ASSERT(para_msg.get_type() == VT_I32);
			
			_remove_local_msg(smt_msg->get_receiver(),(uint32)(para_msg.get_i32()));
		}
		else
		{
			uint32 utc_interval=smt_msg->get_utc_interval();
			if(utc_interval)
			{
				if(tc_util_t::is_over_tc_end(smt_msg->get_utc_posted(), utc_interval, get_tick_count(usr_clk)))
				{
					sp_msg_rcver_t smt_receiver=smt_msg->get_receiver();
					//slice usage is determined by receiver.on_msg
					if(!smt_receiver.is_null()) smt_receiver->on_msg(smt_msg); 
					int32 repeat=smt_msg->get_repeat();
					if(repeat)
					{
						if(repeat > 0) smt_msg->_repeat=repeat - 1;
						_push_to_local_mq(smt_msg); 
					}
				}
				else //not meet running condition
				{
					_push_to_local_mq(smt_msg);
				}
			}
			else //utc_interval == 0
			{
				sp_msg_rcver_t smt_receiver=smt_msg->get_receiver();
				//slice usage is determined by receiver.on_msg
				if(!smt_receiver.is_null()) smt_receiver->on_msg(smt_msg); 
			}
		}
		
		//check time slice
		if(tc_util_t::is_over_tc_end(utc_start, _max_slice, get_tick_count(usr_clk)))
		{
			hb_ret=RET_HB_INT; 
			break;
		}
	}

	FY_EXCEPTION_XTERMINATOR(;);

	return hb_ret;		
}

void msg_proxy_t::post_msg(sp_msg_t& msg)
{
	FY_XFUNC("post_msg,msg:"<<msg->get_msg());

	FY_TRY
	if(_thd == fy_thread_self()) //owner thread is destination
	{
		if(msg.is_null()) return; 

		msg->_set_utc_posted(get_tick_count(user_clock_t::instance()));
		msg->_flag |= MSG_FLAG_POSTED;
		_post_owner_q.push_back(msg);
		
		return;
	}
	
	//owner thread isn't destination
	smart_lock_t slock(&_cs);

	//write waiting msg
	while(!_post_wait_q.empty())
	{
		msg_t *wait_msg=(msg_t *)(_post_wait_q.front());
		if(_mp->write((const int8*)&wait_msg, sizeof(msg_t*), false) == sizeof(msg_t*))
		{
			wait_msg->add_reference();
			_mp->commit_w();

			_post_wait_q.pop_front();
			//notify owner thread message pipe isn't empty now
			if(_es_notempty) _es_notempty->signal(_esi_notempty);
		}
		else
		{
			_mp->rollback_w();
			if(!msg.is_null())
			{
				msg->_set_utc_posted(get_tick_count(user_clock_t::instance()));
				msg->_flag |= MSG_FLAG_POSTED;
			 	_post_wait_q.push_back(msg);
			}
			return;
		}	
	}
	//write para msg
	if(msg.is_null()) return;
	msg->_set_utc_posted(get_tick_count(user_clock_t::instance()));
	msg->_flag |= MSG_FLAG_POSTED;
	msg_t *raw_msg=(msg_t*)msg;
	if(_mp->write((const int8*)&raw_msg, sizeof(msg_t*), false) == sizeof(msg_t*))
	{
		raw_msg->add_reference();
		_mp->commit_w();
	}
	else
	{
		_mp->rollback_w();
		_post_wait_q.push_back(msg);
	}

	FY_EXCEPTION_XTERMINATOR(;);		
}

void msg_proxy_t::on_destroy(const int8 *buf, uint32 buf_len)
{
	FY_XFUNC("on_destroy");

	FY_TRY

        msg_t *raw_msg=0;
	for(uint32 i=0; i<buf_len; i+=sizeof(msg_t*))
	{
                memcpy(&raw_msg, buf+i, sizeof(msg_t*));
                if(raw_msg) delete raw_msg;
		raw_msg=0;
	}

	FY_EXCEPTION_XTERMINATOR(;);
}

//lookup_it
void *msg_proxy_t::lookup(uint32 iid, uint32 pin) throw()
{
        switch(iid)
        {
        case IID_self:
		if(pin != PIN_self) return 0;
        case IID_lookup:
		if(pin != PIN_lookup) return 0;
                return this;

	case IID_heart_beat:
		if(pin != PIN_heart_beat) return 0;
		return static_cast<heart_beat_it*>(this);

	case IID_oneway_pipe_sink:
		if(pin != PIN_oneway_pipe_sink) return 0;
		return static_cast<oneway_pipe_sink_it *>(this);
  
        case IID_object_id:
                return object_id_impl_t::lookup(iid, pin);

        default:
                return ref_cnt_impl_t::lookup(iid, pin);
        }
}

