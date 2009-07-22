/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2009-6-20
 * ====================================================================
 */
#include "fy_aio.h"

USING_FY_NAME_SPACE

//aio_provider_t
sp_aiop_t aio_provider_t::s_create(uint16 max_fd_count, bool rcts_flag)
{
	aio_provider_t *raw_prvd=new aio_provider_t(max_fd_count);
	if(rcts_flag) raw_prvd->set_lock(&(raw_prvd->_cs));

#ifdef LINUX
#if defined(__ENABLE_EPOLL__)
	
	raw_prvd->_epoll_h=::epoll_create(raw_prvd->_max_fd_count);
	if(raw_prvd->_epoll_h < 0)
	{
		FY_ERROR("aio_provider_t::s_create, epoll_create fails:errno="<<(int32)errno);
		delete raw_prvd;

		return sp_aiop_t();
	}
#endif
#elif defined(WIN32)
	if(max_fd_count < MAX_FD_COUNT_LOWER_BOUND_WIN32)
	{
		FY_ERROR("aio_provider_t::s_create, max_fd_count<MAX_FD_COUNT_LOWER_BOUND_WIN32");
		delete raw_prvd;

		return sp_aiop_t();
	}
#if defined(__ENABLE_COMPLETION_PORT__)

	raw_prvd->_iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if(raw_prvd->_iocp == NULL)
	{
		FY_ERROR("aio_provider_t::s_create, CreateIoCompletionPort fails,errno:"
			<<(int32)GetLastError());
		delete raw_prvd;

		return sp_aiop_t();
	}
#endif
#endif
	return sp_aiop_t(raw_prvd, true); 
}

aio_provider_t::aio_provider_t(uint16 max_fd_count) : _cs(true)
{
	_max_slice=AIOP_HB_MAX_SLICE;
	_hb_thread=0;

	user_clock_t *uclk=user_clock_t::instance();

#ifdef LINUX	
#if defined(__ENABLE_EPOLL__)
	_epoll_h=0;

	_epoll_wait_timeout=_max_slice * get_tick_count_res(uclk);	
#else
	_hb_tid=0;
	uint32 ms_slice=_max_slice * get_tick_count_res(uclk);
	_sigtimedwait_timeout.tv_sec=ms_slice/1000;		
	_sigtimedwait_timeout.tv_nsec=(ms_slice%1000) * 1000000;

        sigemptyset(&_sigset);
#endif
#elif defined(WIN32)
#ifdef __ENABLE_COMPLETION_PORT__
	_iocp_wait_timeout=_max_slice * get_tick_count_res(uclk);
#endif
#endif
	_max_fd_count=max_fd_count;
	if(_max_fd_count)
	{
		_ehs=new sp_aioeh_t[_max_fd_count];
	}
	else
	{
		_ehs=0;
	}
}

aio_provider_t::~aio_provider_t()
{
	if(_ehs) delete [] _ehs;

#ifdef LINUX
#if defined(__ENABLE_EPOLL__)
	if(_epoll_h > 0) ::close(_epoll_h);
#endif
#elif defined(WIN32)
#if defined(__ENABLE_COMPLETION_PORT__)
	if(_iocp) ::CloseHandle(_iocp);
#endif
#endif
}

void aio_provider_t::set_max_slice(uint32 max_slice)
{
	_max_slice=max_slice;

        user_clock_t *uclk=user_clock_t::instance();

#ifdef LINUX        
#if defined(__ENABLE_EPOLL__)
        _epoll_wait_timeout=_max_slice * get_tick_count_res(uclk);       
#else
        uint32 ms_slice=_max_slice * get_tick_count_res(uclk);
        _sigtimedwait_timeout.tv_sec=ms_slice/1000;             
        _sigtimedwait_timeout.tv_nsec=(ms_slice%1000) * 1000000;
#endif
#elif defined(WIN32)
#ifdef __ENABLE_COMPLETION_PORT__
	_iocp_wait_timeout=_max_slice * get_tick_count_res(uclk);
#endif
#endif  
}

void aio_provider_t::init_hb_thd()
{
	_hb_thread = fy_thread_self();

#ifdef LINUX
#if defined(__ENABLE_EPOLL__)
#else
	_hb_tid=fy_gettid();
	sigaddset(&_sigset, AIO_RTS_NUM);
	sigaddset(&_sigset, SIGIO);
	sigprocmask(SIG_BLOCK, &_sigset, NULL);
	signal(SIGPIPE, SIG_IGN);
#endif
#elif defined(WIN32)
#endif
}

bool aio_provider_t::register_fd(aio_sap_it *dest_sap, int32 fd, sp_aioeh_t& eh)
{
	FY_ASSERT(fd != INVALID_FD);

	uint32 fd_key=AIO_SOCKET_TO_KEY(fd);
	if(fd_key >= _max_fd_count)
	{
		FY_XERROR("register_fd,too big fd, can't be registered to aio service,fd="<<(uint32)fd\
			<<",fd_key="<<fd_key<<",_max_fd_count:"<<_max_fd_count);

		return false;
	}
#ifdef LINUX
	int flags = ::fcntl(fd, F_GETFL, NULL);

#if defined(__ENABLE_EPOLL__)

	flags |= O_NONBLOCK;
	::fcntl(fd, F_SETFL, flags);

	struct epoll_event ev;
	ev.data.fd=fd;
	ev.events=AIO_POLLIN | AIO_POLLOUT | AIO_POLLERR | AIO_POLLHUP | EPOLLET;		
	::epoll_ctl(_epoll_h, EPOLL_CTL_ADD, fd, &ev);
#else
	flags |= O_NONBLOCK|O_ASYNC;
	::fcntl(fd, F_SETFL, flags);

	if(!_hb_tid)
	{
		FY_XERROR("register_fd, no valid heart_beat thread ID");

		return false;
	}
	::fcntl(fd, F_SETSIG, AIO_RTS_NUM);
	::fcntl(fd, F_SETOWN, _hb_tid); //real-time signal will be routed to heart-beat thread	
#endif
#elif defined(WIN32)
#if defined(__ENABLE_COMPLETION_PORT__)
	if(::CreateIoCompletionPort((HANDLE)fd, _iocp, 0, 0) == NULL)
	{
		FY_XERROR("register_fd, CreateIoCompletionPort fails,error:"<<(int32)GetLastError());

		return false;
	}
#endif
#endif
	smart_lock_t slock(&_cs);

	//fix a re-connecting bug, fd has been re-used by another connection,
	//as socket is half-closed,e.g. ::shutdown() it from this side--2009-1-12
	if(!_ehs[ fd_key ].is_null()) 
	{
		FY_XWARNING("register_fd, duplicate fd has been registered,fd:"<<(uint32)fd);

		return false; 
 	}

	_ehs[ fd_key ]=eh; //map aio event handler to file descriptor

	return true;		
}

//test shows that in real-time signal mode, after this call, there may still be signal is polled 
void aio_provider_t::unregister_fd(int32 fd)
{
	FY_ASSERT(fd != INVALID_FD);

	uint32 fd_key=AIO_SOCKET_TO_KEY(fd);
	if(fd_key>= _max_fd_count) return;

#ifdef LINUX
#if defined(__ENABLE_EPOLL__)
	FY_ASSERT(_epoll_h);
	
	::epoll_ctl(_epoll_h, EPOLL_CTL_DEL, fd, 0);
#else
	int flags = fcntl(fd, F_GETFL, 0);
	flags &= ~O_ASYNC;
	::fcntl(fd, F_SETFL, flags);//os will not report rts on this socket afterward	
#endif
#elif defined(WIN32)
#endif
	smart_lock_t slock(&_cs);

	_ehs[fd_key]=sp_aioeh_t();
}

int8 aio_provider_t::heart_beat()
{
	user_clock_t *usr_clk=user_clock_t::instance();
	uint32 tc_start=get_tick_count(usr_clk);

#ifdef LINUX
#if defined(__ENABLE_EPOLL__)
	struct epoll_event evs[EPOLL_WAIT_SIZE]={0};	
	int ep_cnt=0;
	aio_event_handler_it *raw_eh=0;
#else
	FY_ASSERT(fy_gettid() == _hb_tid);

	int rts_num=0;
#endif
#elif defined(WIN32)
#endif
	int8 hb_ret=RET_HB_IDLE;

	FY_TRY

	while(true)
	{
#ifdef LINUX
#if defined(__ENABLE_EPOLL__)

		ep_cnt=::epoll_wait(_epoll_h, evs, EPOLL_WAIT_SIZE, _epoll_wait_timeout);
		if(0 == ep_cnt) return hb_ret; //timeout expire
		if(ep_cnt < 0)
		{
			FY_XERROR("heart_beat,epoll_wait fail,errno:"<<(int32)errno);
			
			return hb_ret;
		}
		hb_ret=RET_HB_BUSY;
		for(int i=0; i<ep_cnt; ++i) 
		{
			if(!_ehs[evs[i].data.fd].is_null())
				_ehs[evs[i].data.fd]->on_aio_events(evs[i].data.fd, evs[i].events);

		}
#else
		//wait for real time signal
        	rts_num = ::sigtimedwait(&_sigset, &_sig_info, &_sigtimedwait_timeout);
        	switch(rts_num)
                {
                case -1:
                        if(errno == EAGAIN || errno == EINTR) return hb_ret;//timeout expire

                        FY_XERROR("heart_beat,sigtimedwait error,errno="<<(int32)errno);

                        return hb_ret;

                case SIGIO: //overflow of rts queue,poll all sockets and reset rts queue
                        {
							FY_XWARNING("heart_beat, system real time signal queue is overflow");

							//reset rts queue
							signal(AIO_RTS_NUM, SIG_IGN);
							signal(AIO_RTS_NUM, SIG_DFL);

                            //notify all sockets that system real time signal queue is overflow
							//this logic should seldom occur, as a exception logic, it's inefficient
							for(int i=0; i<_max_fd_count; ++i)
							{
								if(_ehs[i].is_null()) continue;
								_ehs[i]->on_aio_events(i, AIO_POLLIN | AIO_POLLOUT);
							}	
                        }
                        break;

                default: //rts triggered
                        {
				if(!_ehs[_sig_info.si_fd].is_null())
					_ehs[_sig_info.si_fd]->on_aio_events(_sig_info.si_fd, _sig_info.si_band);	
                        }
                        break;
                }//switch(rts_num)
		hb_ret = RET_HB_BUSY;
#endif
#elif defined(WIN32)
#ifdef __ENABLE_COMPLETION_PORT__
	
		uint32 bytes_transferred=0;
		uint32 cp_key=0;
		LPFY_OVERLAPPED p_op=NULL;
		if (::GetQueuedCompletionStatus(_iocp, &bytes_transferred, &cp_key, 
			(LPOVERLAPPED *)(&p_op), _iocp_wait_timeout) == 0)
		{
			uint32 last_error=GetLastError();
			if(WAIT_TIMEOUT != last_error)
			{
				FY_XERROR("heart_beat,GetQueuedCompletionStatus fail, error:"<<last_error);
				return hb_ret;
			}
      	}
		if(p_op)
		{
			hb_ret=RET_HB_BUSY;
			p_op->transferred_bytes = bytes_transferred;
			uint32 fd_key=AIO_SOCKET_TO_KEY(p_op->fd);
//88
FY_INFOD("88transfer bytes:"<<bytes_transferred<<",fd:"<<p_op->fd<<",fd_key:"<<fd_key<<",ehs is null?"<<(int8)_ehs[fd_key].is_null());
			if(!_ehs[fd_key].is_null())
				_ehs[fd_key]->on_aio_events(p_op->fd, p_op->aio_events, (pointer_box_t)p_op);			
FY_INFOD("88on_aio_events is called");
		}	
#endif
#endif
		//check time slice
		if(tc_util_t::is_over_tc_end(tc_start, _max_slice, get_tick_count(usr_clk)))
			break;
	}//while(true)

	FY_EXCEPTION_XTERMINATOR(;);
	
	return hb_ret;
}

//lookup_it
void *aio_provider_t::lookup(uint32 iid, uint32 pin) throw()
{
        switch(iid)
        {
        case IID_self:
		if(pin != PIN_self) return 0;
        case IID_lookup:
		if(pin != PIN_lookup) return 0;
                return this;

	case IID_aio_sap:
		if(pin != PIN_aio_sap) return 0;
		return static_cast<aio_sap_it*>(this);

        case IID_heart_beat:
		if(pin != PIN_heart_beat) return 0;
                return static_cast<heart_beat_it*>(this);

        case IID_object_id:
                return object_id_impl_t::lookup(iid, pin);

        default:
                return ref_cnt_impl_t::lookup(iid, pin);
        }
}

//aio_stub_t
aio_stub_t::aio_stub_t(int32 fd, sp_owp_t& ep, aio_proxy_t *aio_proxy, sp_msg_proxy_t& msg_proxy, 
			event_slot_t *es_notempty, uint16 esi_notempty)
	 : _cs(true), ref_cnt_impl_t(&_cs) 
{
	FY_ASSERT(fd != INVALID_FD);
	FY_ASSERT(!ep.is_null());
	FY_ASSERT(aio_proxy);

	_fd=fd;
	_ep=ep;
	_proxy=sp_aio_proxy_t(aio_proxy, true);
	_msg_rcver= sp_msg_rcver_t((msg_receiver_it*)aio_proxy, true);
	_msg_proxy = msg_proxy;
	_es_notempty = es_notempty;
	_esi_notempty = esi_notempty;
}

void aio_stub_t::on_aio_events(int32 fd, uint32 aio_events, pointer_box_t ex_para)
{
	FY_XFUNC("on_aio_events,fd:"<<(uint32)fd<<",aio_events:"<<aio_events);

	if(!_ep->is_write_registered()) _ep->register_write(); //lazy register writer

	//write event to _ep		
	if(_ep->write((const int8*)&fd, sizeof(fd), false) != sizeof(fd))
	{
		_ep->rollback_w();

		//send event via generic message service
		_send_aio_events_as_msg(fd, aio_events, ex_para);

		return;
	}
	if(_ep->write((const int8*)&aio_events, sizeof(aio_events), false) != sizeof(aio_events))
	{
		_ep->rollback_w();

                //send event via generic message service
                _send_aio_events_as_msg(fd, aio_events, ex_para);

		return;
	}
	if(_ep->write((const int8*)&ex_para, sizeof(ex_para), false) != sizeof(ex_para))
	{
		_ep->rollback_w();
		
		//send event via generic message service
		_send_aio_events_as_msg(fd, aio_events, ex_para);

		return;		
	}
	_ep->commit_w();
	if(_es_notempty) _es_notempty->signal(_esi_notempty);//notify aio_proxy that event pipe isn't empty
}

//lookup_it
void *aio_stub_t::lookup(uint32 iid, uint32 pin) throw()
{
        switch(iid)
        {
        case IID_self:
		if(pin != PIN_self) return 0;

        case IID_lookup:
		if(pin != PIN_lookup) return 0;
                return this;
                     
        case IID_aio_event_handler:
		if(pin != PIN_aio_event_handler) return 0;
                return static_cast<aio_event_handler_it*>(this);
        
        case IID_object_id:
                return object_id_impl_t::lookup(iid, pin);
        
        default:
                return ref_cnt_impl_t::lookup(iid, pin);
        }
}

void aio_stub_t::_lazy_init_object_id() throw()
{
	FY_TRY

	string_builder_t sb;
	sb<<"aio_stub_fd"<<(int32)_fd<<"_"<<(void*)this;
	sb.build(_object_id); 

	__INTERNAL_FY_EXCEPTION_TERMINATOR(;);
}

void aio_stub_t::_send_aio_events_as_msg(int32 fd, uint32 aio_events, pointer_box_t ex_para)
{
	FY_XFUNC("_send_aio_events_as_msg,fd:"<<(int32)fd<<",aio_events:"<<aio_events);

	FY_ASSERT(!_msg_proxy.is_null());
	FY_ASSERT(!_msg_rcver.is_null());

	sp_msg_t msg=msg_t::s_create(MSG_AIO_EVENTS, 3, true);
	
	msg->set_receiver(_msg_rcver);
	msg->set_para(0, variant_t((int32)fd));
	msg->set_para(1, variant_t((int32)aio_events));
	msg->set_para(2, variant_t(ex_para));

	_msg_proxy->post_msg(msg);
}

//aio_proxy_t
fy_thread_key_t aio_proxy_t::_s_tls_key=fy_thread_key_t();
bool aio_proxy_t::_s_key_created=false;
critical_section_t aio_proxy_t::_s_cs=critical_section_t(true);

aio_proxy_t *aio_proxy_t::s_tls_instance(uint32 ep_size, uint16 max_fd_count, event_slot_t *es_notempty, uint16 esi_notempty)
{
#ifdef WIN32
	if(max_fd_count < MAX_FD_COUNT_LOWER_BOUND_WIN32)
	{
		FY_ERROR("aio_proxy_t::s_tls_instance,max_fd_count < MAX_FD_COUNT_LOWER_BOUND_WIN32");

		return 0;
	}
#endif
	if(!_s_key_created)//lazy create tls key
	{
		_s_cs.lock();

		if(!_s_key_created)
		{
			if(fy_thread_key_create(&_s_tls_key,0))
			{
				FY_ERROR("aio_proxy_t::s_tls_instance, create TLS key error");
				_s_cs.unlock();

				return 0;
			}
			_s_key_created=true;
		}

		_s_cs.unlock();
	}
	void *ret=fy_thread_getspecific(_s_tls_key);//read aio_proxy_t pointer from Thread Local Storage
	if(ret) return (aio_proxy_t *)ret; //current thread ever has one aio_proxy instance

	aio_proxy_t *aio_proxy=new aio_proxy_t(ep_size, max_fd_count);

	//register read thread for _ep
	aio_proxy->_ep->register_read();

	//register aio_proxy to TLS
	aio_proxy->add_reference();
	int ret_set=fy_thread_setspecific(_s_tls_key,(void *)aio_proxy);
	FY_ASSERT(ret_set == 0);
	aio_proxy->_thd=fy_thread_self();
	aio_proxy->_msg_proxy = sp_msg_proxy_t(msg_proxy_t::s_tls_instance(), true);
	aio_proxy->_es_notempty = es_notempty;
	aio_proxy->_esi_notempty = esi_notempty;

	return aio_proxy;
}

void aio_proxy_t::s_delete_tls_instance()
{
        if(!_s_key_created) return;

        aio_proxy_t *aio_proxy=(aio_proxy_t*)fy_thread_getspecific(_s_tls_key);
        if(aio_proxy) aio_proxy->release_reference();

        fy_thread_setspecific(_s_tls_key,0);//reset TLS
}

aio_proxy_t::aio_proxy_t(uint32 ep_size, uint16 max_fd_count) : _cs(true), ref_cnt_impl_t(&_cs)
{
	_overflow_cnt=0;
	_ep=oneway_pipe_t::s_create((sizeof(int32)+sizeof(uint32))*ep_size);
	_max_slice=AIOEHPXY_HB_MAX_SLICE;	

	_max_fd_count=max_fd_count;
	if(_max_fd_count)
	{
			_items=new _reg_item_t[_max_fd_count];
	}
	else
	{
			_items=0;
	}
}

aio_proxy_t::~aio_proxy_t()
{
	if(_items) delete [] _items;	
}

bool aio_proxy_t::register_fd(aio_sap_it *dest_sap, int32 fd, sp_aioeh_t& eh)
{
    FY_ASSERT(fd != INVALID_FD);
	FY_ASSERT(dest_sap);

	uint32 fd_key = AIO_SOCKET_TO_KEY(fd);
	if(fd_key >= _max_fd_count)
	{
		FY_XERROR("register_fd,too big fd, can't be registered to aio service,fd="<<(uint32)fd\
				<<",fd_key="<<fd_key<<",_max_fd_count:"<<_max_fd_count);

		return false;
	}

	smart_lock_t slock(&_cs);

	_items[fd_key]=_reg_item_t(sp_aiosap_t(dest_sap,true), eh); //map aio event handler to file descriptor
	aio_stub_t *raw_stub=new aio_stub_t(fd, _ep, this, _msg_proxy, _es_notempty, _esi_notempty);	
	sp_aioeh_t smt_eh((aio_event_handler_it*)raw_stub, true);//add_reference raw_stub

	return dest_sap->register_fd(0, fd, smt_eh); 
}

void aio_proxy_t::unregister_fd(int32 fd)
{
	FY_ASSERT(fd != INVALID_FD);

	uint32 fd_key = AIO_SOCKET_TO_KEY(fd);
	if(fd_key >= _max_fd_count) return;

	smart_lock_t slock(&_cs);

	if(_items[fd_key].dest_sap.is_null()) return;

	_items[fd_key].dest_sap->unregister_fd(fd);
	_items[fd_key]= _reg_item_t();
}

//lookup_it
void *aio_proxy_t::lookup(uint32 iid, uint32 pin) throw()
{
	switch(iid)
	{
	case IID_self:
		if(pin!=PIN_self) return 0;
	case IID_lookup:
		if(pin != PIN_lookup) return 0;
		return this;

	case IID_aio_sap:
		if(pin != PIN_aio_sap) return 0;
		return static_cast<aio_sap_it*>(this);

	case IID_heart_beat:
		if(pin != PIN_heart_beat) return 0;
		return static_cast<heart_beat_it*>(this);

	case IID_object_id:
		return object_id_impl_t::lookup(iid, pin);

	default:
		return ref_cnt_impl_t::lookup(iid, pin);
	}
}

//heart_beat_it
int8 aio_proxy_t::heart_beat()
{
	FY_XFUNC("heart_beat");

	FY_ASSERT(!_ep.is_null());

    user_clock_t *usr_clk=user_clock_t::instance();
    uint32 tc_start=get_tick_count(usr_clk);

	int8 hb_ret=RET_HB_IDLE;
	int32 fd=0;
	uint32 aio_events=0;
	pointer_box_t ex_para=0;
	int32 read_len=0;

	FY_TRY

	while(true)	
	{
		read_len=_ep->read((int8*)&fd, sizeof(fd));
		if(0 == read_len) break; //_ep is empty
 		
		if(read_len != sizeof(fd))
		{
			FY_XERROR("heart_beat, fail to read entire fd from _ep");

			return hb_ret;
		}
		if(_ep->read((int8*)&aio_events, sizeof(aio_events)) != sizeof(aio_events))
		{
			FY_XERROR("heart_beat, fail to read entire aio_events from _ep");
		
			return hb_ret;
		}
		if(_ep->read((int8*)&ex_para, sizeof(ex_para)) != sizeof(ex_para))
		{
			FY_XERROR("heart_beat, fail to read entire ex_para from _ep");
			
			return hb_ret;
		}
 		hb_ret = RET_HB_BUSY;

		uint32 fd_key=AIO_SOCKET_TO_KEY(fd);
		if(fd_key >= _max_fd_count)
		{
			FY_XERROR("heart_beat,too big fd:"<<(uint32)fd<<",fd_key="<<fd_key<<",_max_fd_count:"<<_max_fd_count);

			continue;
		}
		if(!_items[fd_key].aioeh.is_null())
		{
			//it should be efficient enought, otherwise, it will block heart_beat
			_items[fd_key].aioeh->on_aio_events(fd, aio_events, ex_para); //handle aio events
		}
		else
		{
			FY_XWARNING("heart_beat, no matched aio event handler for fd:"<<(uint32)fd);
		}

		//check time slice
		if(tc_util_t::is_over_tc_end(tc_start, _max_slice, get_tick_count(usr_clk)))
		{
			hb_ret=RET_HB_INT;
			break;
		}	
	}

	FY_EXCEPTION_XTERMINATOR(;);
	
	return hb_ret;				
}

//msg_receiver_it
void aio_proxy_t::on_msg(msg_t *msg)
{
	FY_XFUNC("on_msg,msg:"<<msg->get_msg());

	FY_ASSERT(msg);

	switch(msg->get_msg())
	{
	case MSG_AIO_EVENTS: //handle overflow aio events
		{
			++_overflow_cnt;

			variant_t v_fd=msg->get_para(0);
			variant_t v_events=msg->get_para(1);
			variant_t v_expara=msg->get_para(2);

			FY_ASSERT(v_fd.get_type() == VT_I32 && v_events.get_type() == VT_I32);

			int32 fd=v_fd.get_i32();
			uint32 fd_key=AIO_SOCKET_TO_KEY(fd);
			if(fd_key >= _max_fd_count)
			{
				FY_XERROR("on_msg,too big fd:"<<(uint32)fd<<",fd_key="<<fd_key<<",_max_fd_count:"<<_max_fd_count);
				return;
			}

			uint32 aio_events=(uint32)v_events.get_i32();
			pointer_box_t ex_para=v_expara.get_ptb();

			if(!_items[fd_key].aioeh.is_null())
			{
				_items[fd_key].aioeh->on_aio_events(fd, aio_events, ex_para); //handle aio events
			}
			else
			{
				FY_XWARNING("on_msg, no matched aio event handler for fd:"<<(uint32)fd);
			}
		}
		break;	
	default:
		break;
	}
}

void aio_proxy_t::_lazy_init_object_id() throw()
{
        FY_TRY

        string_builder_t sb;
        sb<<"aio_proxy_thd"<<(uint32)_thd<<"_"<<(void*)this;
        sb.build(_object_id);

        __INTERNAL_FY_EXCEPTION_TERMINATOR(;);
}

