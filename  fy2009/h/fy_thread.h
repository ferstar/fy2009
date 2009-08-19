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
#ifndef __FENGYI2009_THREAD_DREAMFREELANCER_20080610_H__
#define __FENGYI2009_THREAD_DREAMFREELANCER_20080610_H__

#include "fy_msg.h"
#include "fy_aio.h"

DECL_FY_NAME_SPACE_BEGIN

/*[tip]
 *[declare]
 */
class thread_t;
typedef smart_pointer_lu_tt<thread_t> sp_thd_t;

class thread_pool_t;
typedef smart_pointer_tt<thread_pool_t> sp_tpool_t;

/*[tip]
 *[desc] manage a thread which can be enabled trace service and/or message service and/or aio serverice and can 
 * prvovide a graceful thread terminating mechanism.
 *
 *[note that] it's just an abstract base class, concrete class must implement run(), additionally, it can realize
 *            ref_cnt_it if needed
 *[history] 
 * Initialize 2008-6-16
 * add aio proxy support,2008-10-10
 * remove thread cancellation logic, because it can't be supported by current exception macro(it requires catch(...)
 * clause must be terminated with throw statement,on the other hand, windows doesn't support thread cancellation. it's
 * not a necessary logic, to simplify many things, decide to remove it, 2009-8-17
 */
//default time out to wait for thread exit(unit:ms)
#define THD_DEF_TIMEOUT 1000

uint16 const THREAD_EVENT_SLOT_COUNT=3; //++it has to be changed as adding new Event slot index
//->
uint16 const THD_ESI_TRACE=0; //event slot index for trace service, when trace pipe isn't full, it will be signalled 
uint16 const THD_ESI_MSG=1; //event slot index for msg service, if msg comes to this thread, it will be signalled
uint16 const THD_ESI_AIO=2; //event slot index for aio service, if aio event comes to this thread, it will be signalled
//<-

class thread_t : public object_id_impl_t
{
public:
#ifdef POSIX

	static void *s_t_f(void *para);//universal thread function,specific logic is in run().

#elif defined(WIN32)

	static DWORD WINAPI s_t_f(LPVOID para);

#endif

public:
	thread_t();
	virtual ~thread_t();

	//specific thread logic,should be overwritten by descendant
	virtual void run()=0;

	//create a detached thread,return the return value of pthread_create
	int32 start();//start to run thread

	//stop thread. set _stop_flag=true to notify thread exit,if no response,pthread_cancel will be called 
	//and wait for at most another timeout, then return directly.
	//para:timeout:milliseconds of time out waiting for thread exit
	void stop(uint32 timeout = THD_DEF_TIMEOUT); 
	
	inline void enable_trace(uint32 pipe_size=TRACE_DEF_NLP_SIZE,
			uint32 max_queued_size=TRACE_DEF_MAX_QUEUED_SIZE) throw()
	{
		if(_thd) return;
		_trace_flag=true;
		_trace_pipe_size=pipe_size;
		_trace_max_queued_size = max_queued_size;
	}
	inline void disable_trace() throw()
	{
		if(_thd) return;
		_trace_flag=false;
	}
	
	//this thread doesn't handle not-full event, it can be handled by main thread if need
	void enable_msg(uint32 msg_pipe_size=MPXY_DEF_MP_SIZE, event_slot_t *es_notfull=0, uint16 esi_notfull=0);
	inline void disable_msg() throw()
	{
		if(_thd) return;
		_msg_flag=false;
	}

	void enable_aio(uint32 aioep_size=AIOEHPXY_DEF_EP_SIZE, uint16 max_fd_count=AIO_DEF_MAX_FD_COUNT);
	inline void disable_aio() throw()
	{
		if(_thd) return;
		_aio_flag=false;
	}
	
	//lookup_it
	void *lookup(uint32 iid, uint32 pin) throw();
	
	inline fy_thd_t get_thd() const throw() { return _thd; }
	inline uint32 get_thd_id() const throw() { return _thd_id; }
	
	inline msg_proxy_t *get_msg_proxy() throw() { return (msg_proxy_t *)_msg_proxy; }

	inline aio_proxy_t *get_aio_proxy() throw() { return (aio_proxy_t *)_aio_proxy; }

	//true means thread is doing user logic within run() 
	inline bool is_running() const throw() { return _is_running; }	
protected:
	//when run() completed initialization, it should be called to unlock start(),
	//start() then return
	inline void _unlock_start() throw() { _cs.unlock(); _is_running=true; }
	
	//implementation of run should call it frequently within while loop near nonsystemic cancel-point
	//to check if receiving cancellation request from other thread
	void _test_cancel();

	//descendant must ofen checked stop flag to exit on demand
	inline bool _is_stopping() const throw() { return _stop_flag; }

	//if thread is idle, run() should call it to handle trace or message service etc.
	//this call may be blocked until event occurred (return true) or time out expired (return false)
	//if there exist any periodical message, time_out should not be greater than its interval
	bool _on_idle(uint32 time_out); //time-out unit:ms
private:
	void _lazy_init_object_id() throw();

	void _handle_trace_on_idle(); //try to write pending trace info to trace service from _on_idle()
	void _handle_msg_on_idle();   //try to handle msg reaching this thread from _on_idle()
	void _handle_aio_on_idle();  //try to handle aio event reaching this thread from _on_idle()
private:
	fy_thd_t _thd;
	uint32 _thd_id;
	critical_section_t _cs;//indicate start() calling has been completed

	bool _trace_flag;//enable ccp trace service for thread or not
	uint32 _trace_pipe_size;
	uint32 _trace_max_queued_size;

	bool _msg_flag; //enable message service for thread or not
	uint32 _msg_pipe_size;
	event_slot_t *_es_msg_notfull; //it must be ensured valid during whole lifecycle of this thread
	uint16 _esi_msg_notfull;
	sp_msg_proxy_t _msg_proxy;

	//aio support,2008-10-10
	//->
	bool _aio_flag; //enable aio proxy service for thread or not
	uint32 _aioep_size; //aio events one-way pipe size
	uint16 _max_fd_count; //max fd_key upperbound which can be managed by aio service 
	sp_aio_proxy_t _aio_proxy;	
	//<-

	bool _stop_flag;//notify thread to exit
	event_t _e_stopped;//notify stop() caller thread this thread has exited
	event_slot_t _es; //wait for notifications from other thread when thread is idle
	bool _is_running; //true means run() is ready; false means run() isn't ready or has exitted
};

/*[tip]
 *[desc] message-driven thread pool
 *[note that]
 *1.for independant message, you always can call assign_thd to get a thread from pool to handle it, otherwise, caller has to
 * be responsible for sending subsequent message to proper assigned thread
 *[history]
 * Initialize, 2007-4-4
 * change to non-singleton,2008-6-16, it makes sense that one process want to run more than one specific thread pools
 */
class thread_pool_t : public object_id_impl_t,
              	      public ref_cnt_impl_t //thread-safe
{
public:
	class _thd_t : public thread_t,
		       public ref_cnt_impl_t //thread-safe
	{
		friend class thread_pool_t;
	public:
		//describe if a thread is busy now
		inline bool get_busy() const throw() { return _busy; }

		//thread_t
		virtual void run();
		
		//lookup_i
		void *lookup(uint32 iid, uint32 pin) throw();
	protected:
		_thd_t() : _cs(true), ref_cnt_impl_t(&_cs) { _busy=false; }
		 void _lazy_init_object_id() throw();	
	private:
		bool _busy; //to describe thread is busy or not
		critical_section_t _cs; //syn ref_cnt_i
	};
public:
	//msg_pipe_size is zero will disable message service, trace_pipe_size is zero will disable trace service,
	//aioep_size is zero will disable aio service
        static sp_tpool_t s_create(uint16 pool_size, //total threads count in pool
 				event_slot_t *msg_es_notfull=0, uint16 msg_esi_notfull=0,//message pipe notfull notification
				uint32 msg_pipe_size=MPXY_DEF_MP_SIZE, //message pipe size for each thread
				uint32 trace_pipe_size=TRACE_DEF_NLP_SIZE, //trace pipe size for each thread 
				uint32 trace_max_queued_size=TRACE_DEF_MAX_QUEUED_SIZE,//max queued trace size for each thread
				uint32 aioep_size= AIOEHPXY_DEF_EP_SIZE, //aio event pipe size for each thread
				uint16 max_fd_count=AIO_DEF_MAX_FD_COUNT); //max fd count for each thread-related aio_proxy
public:
	~thread_pool_t();

	//assign a thread from pool for caller, if existing idle thread, it will be selected, otherwise, round-robin will be
	//applied
	sp_thd_t assign_thd(uint16 *p_idx);

	//stop all threads
	void stop_all();

	//lookup_it
	void *lookup(uint32 iid, uint32 pin) throw();
protected:
	void _lazy_init_object_id() throw(){ OID_DEF_IMP("thread_pool"); }	
private:
	thread_pool_t();
	thread_pool_t(uint16 pool_size, //total threads count in pool
		      event_slot_t *msg_es_notfull=0, uint16 msg_esi_notfull=0,//message pipe notfull notification
                      uint32 msg_pipe_size=MPXY_DEF_MP_SIZE, //message pipe size for each thread
                      uint32 trace_pipe_size=TRACE_DEF_NLP_SIZE, //trace pipe size for each thread
                      uint32 trace_max_queued_size=TRACE_DEF_MAX_QUEUED_SIZE, //max queued trace size for each thread
                      uint32 aioep_size= AIOEHPXY_DEF_EP_SIZE, //aio event pipe size for each thread
                      uint16 max_fd_count=AIO_DEF_MAX_FD_COUNT); //max fd count for each thread-related aio_proxy

	//start thread with para index
	void _start(uint16 idx);

	//stop thread with para index
	void _stop(uint16 idx);
private:
	_thd_t **_pp_thds; //thread pool
	uint16 _pool_size; //size of thread pool
	event_slot_t *_msg_es_notfull;//message pipe notfull notification event
	uint16 _msg_esi_notfull; //message pipe notfull notification event index
	uint32 _msg_pipe_size; //message pipe size for each thread
	uint32 _trace_pipe_size; //trace pipe size for each thread
	uint32 _trace_max_queued_size; //max queued trace size for each thread
	uint32 _aioep_size; //aio event pipe size for each thread
	uint16 _max_fd_count; //max fd count for each thread-related aio_proxy
	uint16 _next_rr_index; //assign_thd next round-robin thread index
	critical_section_t _cs; //syn ref_cnt_i
};

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_THREAD_DREAMFREELANCER_20080610_H__
