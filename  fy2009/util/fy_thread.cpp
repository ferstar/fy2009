/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2009-8-7
 * ====================================================================
 */   
#include "fy_thread.h"

USING_FY_NAME_SPACE

//thread_t
#ifdef POSIX

void thread_t::s_cleanup_f(void *para)
{
	thread_t *p_this=(thread_t *)para;
	FY_ASSERT(p_this);

	FY_TRY

	p_this->on_cancel();//specific thread cancellation logic

	__INTERNAL_FY_EXCEPTION_TERMINATOR(;);

	//2007-4-6
        //->	
	if(p_this->_trace_flag)//trace service is enabled
	{
                trace_provider_t *trp=trace_provider_t::instance();
                trp->unregister_tracer();
	}
	//<-

	if(p_this->_msg_flag)//msg service is enabled
	{
		msg_proxy_t::s_delete_tls_instance();
		(p_this->_msg_proxy).attach(0);	
	}

	//aio support,2008-10-10
	if(p_this->_aio_flag) //aio proxy is enabled
	{
		aio_proxy_t::s_delete_tls_instance();
		(p_this->_aio_proxy).attach(0);
	}

	p_this->_e_stopped.signal();//notify stop() calling thread that this thread has exited	
}

void *thread_t::s_t_f(void *para)

#elif defined(WIN32)

DWORD WINAPI thread_t::s_t_f(LPVOID para)

#endif
{
	thread_t *p_this=(thread_t *)para;
	FY_ASSERT(p_this);

	p_this->_thd_id=fy_gettid();

#ifdef POSIX
	
	pthread_cleanup_push(s_cleanup_f, para);
#endif
	if(p_this->_trace_flag)//enable trace service
	{
        	trace_provider_t *trp=trace_provider_t::instance();
        	trp->register_tracer(	p_this->_trace_pipe_size,
					p_this->_trace_max_queued_size,
					&(p_this->_es),
					THD_ESI_TRACE);
	}

	if(p_this->_msg_flag) //enable message service
	{
		msg_proxy_t *raw_msg_proxy= msg_proxy_t::s_tls_instance(p_this->_msg_pipe_size, 
							p_this->_es_msg_notfull, p_this->_esi_msg_notfull,
							&(p_this->_es), THD_ESI_MSG); 
		p_this->_msg_proxy= sp_msg_proxy_t(raw_msg_proxy, true);

	}

	//aio support, 2008-10-10
	if(p_this->_aio_flag) //enable aio proxy service
	{
		aio_proxy_t *raw_aio_proxy=aio_proxy_t::s_tls_instance(p_this->_aioep_size, p_this->_max_fd_count,
							&(p_this->_es), THD_ESI_AIO);
		p_this->_aio_proxy = sp_aio_proxy_t(raw_aio_proxy, true);
	}

	FY_TRY

	p_this->run();//run specific thread logic,this virtual fuction should be overwritten by descendant

	}catch(std::exception& e){
		if(e.what()){
			__INTERNAL_FY_TRACE(e.what());
			__INTERNAL_FY_TRACE("\r\n");
		}else{
			__INTERNAL_FY_TRACE("Caught a std::exception but what() is empty\r\n");}
	}catch(...){
		//it's important for pthread cancel to throw again, otherwise, pthread cleanup logic will not be called,
		//and process will be aborted
FY_INFOD("==caugth unspecified exception");
		throw; 
  	}

	p_this->_is_running=false;
        if(p_this->_trace_flag)
        {
                trace_provider_t *trp=trace_provider_t::instance();	
		trp->unregister_tracer();
	}

        if(p_this->_msg_flag)//msg service is enabled
        {
                msg_proxy_t::s_delete_tls_instance();
                (p_this->_msg_proxy).attach(0);
        }

	if(p_this->_aio_flag) //aio proxy is enabled
	{
		aio_proxy_t::s_delete_tls_instance();
		(p_this->_aio_proxy).attach(0);	
	}

#ifdef POSIX	

	pthread_cleanup_pop(0);	
#endif
	p_this->_e_stopped.signal();//notify stop() requester that this thread has stopped

	return 0;
}

thread_t::thread_t() : _cs(false), _es(THREAD_EVENT_SLOT_COUNT), _e_stopped(false) 
{
	_thd=0;
	_thd_id=0;

	_trace_flag=true;
	_trace_pipe_size= TRACE_DEF_NLP_SIZE;
	_trace_max_queued_size= TRACE_DEF_MAX_QUEUED_SIZE; 

	_msg_flag=false;
	_msg_pipe_size=0;
	_es_msg_notfull=0;
	_esi_msg_notfull=0;

	_aio_flag=false;
	_aioep_size=0;
	_max_fd_count=0;

	_stop_flag=false;
	_is_running=false;
}

thread_t::~thread_t()
{
	stop();
} 

int32 thread_t::start()
{
	if(_thd) return 0; //has started
	_stop_flag=false;

#ifdef POSIX

    	pthread_attr_t attr;
    	pthread_attr_init(&attr);
	//because pthread_timedjoin cann't be supported so far, to avoid infinitely block when requesting a thread to stop
	//,thread is created as detached type.
    	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
#endif   
	_cs.lock();

#ifdef POSIX

    	int32 ret=pthread_create(&_thd, &attr, thread_t::s_t_f, this);

#elif defined(WIN32)

	_thd=::CreateThread(NULL, 0, s_t_f, (void*)this, 0, NULL);
	int32 ret = (_thd? 0:-1);
#endif
	if(ret) 
	{
		_cs.unlock();
	}
	else
	{
		_cs.lock(); //thread function will unlock it by calling unlock_start() after it has initialized
		_cs.unlock();
	}

#ifdef POSIX

    	pthread_attr_destroy(&attr);

#endif
	return ret;
}

void thread_t::stop(uint32 timeout)
{
	if(!_thd) return;

	//notify run() to exit,the later should often check this variable by calling is_stopping() 
	_stop_flag=true;
	int32 ret=_e_stopped.wait(timeout);//wait for thread exiting
	if(ret)//thread doesn't exit before timeout expired
	{
#ifdef POSIX

		pthread_cancel(_thd); //maybe thread was blocked at cancel-point
		_e_stopped.wait(timeout);

#elif defined(WIN32)

		//wait for another timeout
		if(_e_stopped.wait(timeout)) 
		{
			::TerminateThread(_thd, 0);
			FY_XERROR("stop, thread is terminated abnormally, may fail to free some resources");
		}

#endif
	}

#ifdef WIN32

	::CloseHandle(_thd);	
#endif
	_thd=0;
	_thd_id=0;
} 

bool thread_t::_on_idle(uint32 time_out)
{
	FY_DECL_EXCEPTION_CTX_EX("_on_idle");

	event_slot_t::slot_vec_t es_vec;
	_es.wait(es_vec, time_out);
	int32 ecnt=es_vec.size();

	FY_TRY

	for(int32 i=0; i<ecnt; ++i)
	{
		switch(es_vec[i])
		{
		case THD_ESI_TRACE:
			if(_trace_flag) _handle_trace_on_idle();
			return true;

		case THD_ESI_MSG:
			if(_msg_flag) _handle_msg_on_idle();
			return true;

		case THD_ESI_AIO:
			if(_aio_flag) _handle_aio_on_idle();
			return true;

		default:
			FY_XWARNING("on_idle,unknown event slot index:"<<es_vec[i]);
			return false;
		}
	}

	FY_CATCH_N_THROW_AGAIN_EX("thdidl","cnta",;)
	
	return false;		
}
       
void thread_t::enable_msg(uint32 msg_pipe_size, event_slot_t *es_notfull, uint16 esi_notfull)
{
	if(_thd) return;
	
	_msg_flag=true;

	_msg_pipe_size= msg_pipe_size;
	_es_msg_notfull=es_notfull;
	_esi_msg_notfull=esi_notfull;
}

void thread_t::enable_aio(uint32 aioep_size, uint16 max_fd_count)
{
	if(_thd) return;

	_aio_flag=true;

	_aioep_size = aioep_size;
	_max_fd_count = max_fd_count;	
}

//lookup_i
void *thread_t::lookup(uint32 iid, uint32 pin) throw()
{
        switch(iid)
        {
        case IID_self:
		if(pin != PIN_self) return 0;
        case IID_lookup:
		if(pin != PIN_lookup) return 0;
                return this;

        default:
                return object_id_impl_t::lookup(iid, pin);
        }
}

void thread_t::_lazy_init_object_id() throw()
{
        FY_TRY

        string_builder_t sb;
        sb<<"thread_"<<(void*)this<<"_thd"<<(uint32)_thd;
        sb.build(_object_id);

        __INTERNAL_FY_EXCEPTION_TERMINATOR(;);
}
 
void thread_t::_handle_trace_on_idle()
{
	trace_provider_t* prvd=trace_provider_t::instance();
	trace_provider_t::tracer_t *tcer=prvd->get_thd_tracer();
	FY_ASSERT(tcer);

	tcer->write_trace(0); //try to write pending trace if any	
}

void thread_t::_handle_msg_on_idle()
{
	//ensure _msg_proxy isn't null
	_msg_proxy->heart_beat();
}
   
void thread_t::_handle_aio_on_idle()
{
	//ensure _aio_proxy isn't null
	_aio_proxy->heart_beat();
}
 
//thread_pool_t
//_thd_t
void thread_pool_t::_thd_t::run()
{
	msg_proxy_t *msg_proxy=get_msg_proxy();
	aio_proxy_t *aio_proxy=get_aio_proxy();

	_unlock_start();

	FY_TRY
	
	int8 hb_ret_msg=RET_HB_IDLE;
	int8 hb_ret_aio=RET_HB_IDLE;
	_busy=true;
	while(1)
	{
		if(msg_proxy) hb_ret_msg=msg_proxy->heart_beat();//enabled msg service 
		if(aio_proxy) hb_ret_aio=aio_proxy->heart_beat();//enabled aio service

		if(RET_HB_IDLE == hb_ret_msg && RET_HB_IDLE == hb_ret_aio)
			_busy = false;
		else
			_busy = true;

		//wait for signal and try to write pending trace and receive msg and receiving aio events
		//as both msg_proxy and aio_proxy are idle
		if(!_busy) _busy=_on_idle(400); //tim-out: 400ms, should less than thread stop time-out(1000ms)

		if(_is_stopping()) break;
	}

	FY_EXCEPTION_XTERMINATOR(;);
}

void thread_pool_t::_thd_t::_lazy_init_object_id() throw()
{
        FY_TRY

        string_builder_t sb;
        sb<<"thread_pool_t::_thd_"<<(void*)this<<"_thd"<<(uint32)get_thd();
        sb.build(_object_id);

        __INTERNAL_FY_EXCEPTION_TERMINATOR(;);
}

//lookup_i
void *thread_pool_t::_thd_t::lookup(uint32 iid, uint32 pin) throw()
{
        switch(iid)
        {
        case IID_self:
		if(pin != PIN_self) return 0;
        case IID_lookup:
		if(pin != PIN_lookup) return 0; 
                return this;

        case IID_ref_cnt:
                return ref_cnt_impl_t::lookup(iid, pin);

	default:
                return thread_t::lookup(iid, pin);
        }
}

//thread_pool_t
sp_tpool_t thread_pool_t::s_create(uint16 pool_size, 
                                event_slot_t *msg_es_notfull, uint16 msg_esi_notfull,
                                uint32 msg_pipe_size, 
                                uint32 trace_pipe_size, 
                                uint32 trace_max_queued_size,
				uint32 aioep_size,
				uint16 max_fd_count)
{
        thread_pool_t *tmp_inst=new thread_pool_t(pool_size, 
				msg_es_notfull, msg_esi_notfull, msg_pipe_size, 
				trace_pipe_size, trace_max_queued_size, aioep_size, max_fd_count);

        return sp_tpool_t(tmp_inst, true);
}

thread_pool_t::thread_pool_t():_cs(true),ref_cnt_impl_t(&_cs)
{
	_pp_thds=0;
	_pool_size=0;
        _msg_es_notfull=0;
        _msg_esi_notfull=0; 
        _msg_pipe_size=MPXY_DEF_MP_SIZE; 
        _trace_pipe_size=TRACE_DEF_NLP_SIZE; 
        _trace_max_queued_size=TRACE_DEF_MAX_QUEUED_SIZE;  
	_aioep_size=AIOEHPXY_DEF_EP_SIZE;
	_max_fd_count=AIO_DEF_MAX_FD_COUNT;
	_next_rr_index=0;
}

thread_pool_t::thread_pool_t(uint16 pool_size, 
                      event_slot_t *msg_es_notfull, 
		      uint16 msg_esi_notfull,
                      uint32 msg_pipe_size,
                      uint32 trace_pipe_size, 
                      uint32 trace_max_queued_size,
		      uint32 aioep_size,
		      uint16 max_fd_count):_cs(true),ref_cnt_impl_t(&_cs)
{
	_pool_size=pool_size;
        _msg_es_notfull=msg_es_notfull;
        _msg_esi_notfull=msg_esi_notfull;
        _msg_pipe_size=msg_pipe_size;
        _trace_pipe_size=trace_pipe_size;
        _trace_max_queued_size=trace_max_queued_size;
	_aioep_size=aioep_size;
	_max_fd_count=max_fd_count;

	if(_pool_size)
	{
		_pp_thds=new _thd_t*[_pool_size];
		memset((void*)_pp_thds, 0, sizeof(_thd_t*)*_pool_size);
	}
	else
		_pp_thds=0;

	_next_rr_index=0; 
}

thread_pool_t::~thread_pool_t()
{
	if(!_pp_thds) return;

	for(uint16 i=0; i<_pool_size; ++i)
	{
		if(!_pp_thds[i]) continue;
		_stop(i);
	}
	delete [] _pp_thds;
}

void thread_pool_t::_start(uint16 idx)
{
	FY_ASSERT(idx<_pool_size);

	if(_pp_thds[idx]) return; //has started

        _pp_thds[idx]=new _thd_t(); //lazy create thread
	_pp_thds[idx]->add_reference();

        if(_trace_pipe_size) _pp_thds[idx]->enable_trace(_trace_pipe_size, _trace_max_queued_size);
	if(_msg_pipe_size) _pp_thds[idx]->enable_msg(_msg_pipe_size, _msg_es_notfull, _msg_esi_notfull);
	if(_aioep_size) _pp_thds[idx]->enable_aio(_aioep_size, _max_fd_count);

        _pp_thds[idx]->start();	//start thread	
}

void thread_pool_t::_stop(uint16 idx)
{
	FY_ASSERT(idx<_pool_size);

	if(!_pp_thds[idx]) return; //has stopped or never started

	_pp_thds[idx]->stop();//request to stop thread
	_pp_thds[idx]->release_reference();
	_pp_thds[idx]=0;
}

sp_thd_t thread_pool_t::assign_thd(uint16 *p_idx) 
{
	smart_lock_t slock(&_cs);

	if(!_pool_size) return sp_thd_t(); //no thread available
	uint16 i=0;
	for(i=_next_rr_index; i<_pool_size; ++i)
	{
		if(!_pp_thds[i])//empty thread slot
		{
			_start(i);//lazy create and start thread
			if(p_idx) *p_idx=i;
			if(++_next_rr_index >= _pool_size) _next_rr_index=0;

			return sp_thd_t((thread_t*)(_pp_thds[i]), true);
		}

		if(!(_pp_thds[i]->get_busy()))
		{
			if(p_idx) *p_idx=i;
			if(++_next_rr_index >= _pool_size) _next_rr_index=0;

			return sp_thd_t((thread_t*)(_pp_thds[i]), true);
		}
	} //for

	for(i=0; i<_next_rr_index; ++i)
	{
                if(!_pp_thds[i])//empty thread slot
                {
                        _start(i);//lazy create and start thread
                        if(p_idx) *p_idx=i;
			if(++_next_rr_index >= _pool_size) _next_rr_index=0;
                      
                        return sp_thd_t((thread_t*)(_pp_thds[i]), true);
                }     
                      
                if(!(_pp_thds[i]->get_busy()))
                {
                        if(p_idx) *p_idx=i;
			if(++_next_rr_index >= _pool_size) _next_rr_index=0;

                        return sp_thd_t((thread_t*)(_pp_thds[i]), true);
                }
	}

	if(p_idx) *p_idx=_next_rr_index;
        thread_t *thd=static_cast<thread_t*>(_pp_thds[_next_rr_index]);
        if(++_next_rr_index >= _pool_size) _next_rr_index=0;

	return sp_thd_t(thd,true);
}

void thread_pool_t::stop_all()
{
	smart_lock_t slock(&_cs);

        for(uint16 i=0; i<_pool_size; ++i)
        {
                if(!_pp_thds[i]) continue;
                _stop(i);
        }
}

//lookup_i
void *thread_pool_t::lookup(uint32 iid, uint32 pin) throw()
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

