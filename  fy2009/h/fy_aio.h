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
#ifndef __FENGYI2009_AIO_DREAMFREELANCER_20080616_H__
#define __FENGYI2009_AIO_DREAMFREELANCER_20080616_H__

/*[tip]
 *[desc] offer a generic asynchronous I/O framework for socket and other file descriptor based I/O operations
 * e.g. socket, file, pipe and stuff like that
 * 
 *[history] 
 * Initilaize: 2008-6-16
 */
#include "fy_msg.h"

#ifdef POSIX

#include <signal.h>
#include <fcntl.h>

#endif //POSIX
#include <list>

DECL_FY_NAME_SPACE_BEGIN

/*[tip]
 *[declare]
 */
class aio_event_handler_it;
typedef smart_pointer_lu_tt<aio_event_handler_it> sp_aioeh_t;

class aio_provider_t;
typedef smart_pointer_tt<aio_provider_t> sp_aiop_t;

class aio_stub_t;
typedef smart_pointer_tt<aio_stub_t> sp_aio_stub_t;

class aio_proxy_t;
typedef smart_pointer_tt<aio_proxy_t> sp_aio_proxy_t;

/*[tip]
 *[desc] aio events marcros
 */
#ifdef LINUX

#if defined(__ENABLE_EPOLL__)
#	include <sys/epoll.h>
#	define AIO_POLLIN   EPOLLIN  /* There is data to read.  */
#	define AIO_POLLOUT  EPOLLOUT /* Writing now will not block.  */
#	define AIO_POLLERR  EPOLLERR /* Error condition.  */
#	define AIO_POLLHUP  EPOLLHUP /* Hung up.--if peer call ::shutdown(fd), this side may receive this event*/
#else
//real-time signal
#	include <sys/poll.h>
#	define AIO_POLLIN   POLLIN  /* There is data to read.  */
#	define AIO_POLLOUT  POLLOUT /* Writing now will not block.  */
#	define AIO_POLLERR  POLLERR /* Error condition.  */
#	define AIO_POLLHUP  POLLHUP /* Hung up.peer shutdown connection */

#	define AIO_RTS_NUM (SIGRTMIN+20)
#endif //__ENABLE_EPOLL__

#elif defined(WIN32)

#	define AIO_POLLIN   1  /*accept or receiv  */
#	define AIO_POLLOUT  2  /* send  */
#	define AIO_POLLERR  4  /* Error condition.  */
#	define AIO_POLLHUP  8  /* Hung up.peer shutdown connection */

#ifdef __ENABLE_COMPLETION_PORT__

typedef struct{
	OVERLAPPED overlapped;
	int32 fd;
	//set to AIO_POLLIN by WSAAccept, WSARecv or set to AIO_POLLOUT by WSASend, or set to AIO_POLLERR by aio_provider
	uint32 aio_events; 	
	WSABUF wsa_buf;
	volatile uint32 transferred_bytes; //set by aio_provider
} FY_OVERLAPPED, *LPFY_OVERLAPPED;

#endif
#endif //LINUX

const int INVALID_FD=-1;

//deliver aio events via message
//para_0(int32):socket fd
//para_1(uint32):aio events
//para_2(pointer_box_t):ex_para
const uint32 MSG_AIO_EVENTS=MSG_USER;
const uint32 MSG_PIN_AIO_EVENTS=0x89acd299;

//define a specific trace level for IO related detail trace service, 
//it often implies a great number of trace outputs, so it should be disabled by default
const uint8 TRACE_LEVEL_IO=TRACE_LEVEL_MAX_PREDEFINED + 1;

#if defined(FY_TRACE_ENABLE_IO)

#       define FY_TRACE_IO(desc) do{ __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_IO, desc) }while(0)
#       define FY_XTRACE_IO(desc) do{ __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_IO, get_object_id()<<": "<<desc) }while(0)

#else

#       define FY_TRACE_IO(desc) 
#       define FY_XTRACE_IO(desc) 

#endif

/*[tip]
 *[desc] aio service access point interface, it will be implemted by aio_provider_t and aio_proxy_t, it offers 
 *       a same profile to caller no matter concrete implementation
 *[history] 
 * Initialize: 2008-10-8
 */
class aio_sap_it : public lookup_it
{
public:
        //register file descriptor to aio service, then, aio events occurred on fd will be monitored and delivered
        //to event hander eh
	//if implementation is aio_provider_t, dest_sap will be ignored, i.e. it can be null in this case
	//,otherwise, if implementatior is aio_proxy_t, it points to aio_provider_t
        virtual bool register_fd(aio_sap_it *dest_sap, int32 fd, sp_aioeh_t& eh)=0;

        //unregister file descriptor from aio service
        virtual void unregister_fd(int32 fd)=0;	
};
typedef smart_pointer_lu_tt<aio_sap_it> sp_aiosap_t;

/*[tip]
 *[desc] aio_provider_t poll OS aio events and dispatch them to related aio_event_handler_it, this interface will draw a
 *       uniform aio image for above layer and don't care underlayer aio implementation(real-time signal or epoll). 
 *[history] 
 * Initialize: 2008-6-16
 *[note that] implementation of this interface should be efficient enough, because it will be run within hear_beat(),
 *            if it's inefficient, it will take bad effect on aio events monitoring on other file descriptors
 */
class aio_event_handler_it : public lookup_it
{
public:
	//once call can deliver multi events, aio_events can be one or more AIO_POLLXXX events
	//add ex_para to pass overlapped data pointer in windwos completion port
	virtual void on_aio_events(int32 fd, uint32 aio_events, pointer_box_t ex_para=0)=0;
};

/*[tip]
 *[desc] monitor and dispatch OS aio event occurred on registered file descriptors, isolate AIO event mechanism
 *       between upper layer and OS implementation(e.g. real-time signal or epoll, etc.)
 *[history]
 * Initialize: 2008-6-16
 *[note that]
 *1.On OS RedHat8.0, pass real-time signal test, and On OS CentOS, pass both real-time signal and epoll test,2008-6-20
 */
//default concurrent open file descriptor count can be monitored by aio service,
//it refers to fd key exactly, for linux, it's same as fd, for windows, it isn't
uint16 const AIO_DEF_MAX_FD_COUNT=1024;

//default max slice of heart_beat,100ms
uint32 const AIOP_HB_MAX_SLICE=100; //unit:ms

//epoll_wait can polled max events count for each call
int const EPOLL_WAIT_SIZE=512;

#ifdef POSIX

#define AIO_SOCKET_TO_KEY(fd) (fd)
#define AIO_KEY_TO_SOCKET(key) (key)

#elif defined(WIN32)

//max_fd_count should not be less than 1952/4=488, otherwise, aio_provider will not work due to socket handle is out of bound
uint32 const MAX_FD_COUNT_LOWER_BOUND_WIN32=512;

//under windows socket always increases/decreases for each 4, under 64bit OS, it may be different
#define AIO_SOCKET_TO_KEY(fd) ((uint32)fd>>2)
#define AIO_KEY_TO_SOCKET(key) ((int32)(key<<2))

#endif

class aio_provider_t : public aio_sap_it,
		       public heart_beat_it,
		       public object_id_impl_t,
			public ref_cnt_impl_t
{
public:
	//max_fd_count available max value is limited by OS allowed open files,
	//can call getrlimit(RLIMIT_NOFILE,) to retrieve it. maximum is 65535
	static sp_aiop_t s_create(uint16 max_fd_count=AIO_DEF_MAX_FD_COUNT, bool rcts_flag=false);

#ifdef LINUX
#if defined(__ENABLE_EPOLL__)
#else
	//real-time signal

	//if SIGIO is often triggered under real-time signal mode, it indicates the length of real-time signal queue
	// is too short or server is too busy,2009-8-6
	int32 s_get_sigio_count(){ return _s_sigio_count; }
#endif
#endif //LINUX

public:
	~aio_provider_t();
	//aio_sap_it
	bool register_fd(aio_sap_it *dest_sap, int32 fd, sp_aioeh_t& eh);
	void unregister_fd(int32 fd);

	//if epoll isn't enabled, it shoulde be called from heart_beat thread within thread initialization,
	//on the other hand, set heart beat thread
	void init_hb_thd();

	//get its owner thread id, i.e. heart beat thread
	uint32 get_hb_tid() { return _hb_tid; }

	//heart_beat_i,ensure it's only called by one thread
        virtual int8 heart_beat(); //drive aio service and monitor aio events

        //expected max slice(milli-seccond) for heart_beat once call
        virtual void set_max_slice(uint32 max_slice); 
        virtual uint32 get_max_slice() const throw(); 

        //indicate callee expected called frequency: 0 means expected to be called as frequent as possible,
        //otherwise, means expected interval(unit:ms) between two calling
        virtual uint32 get_expected_interval() const throw() { return 0; }

	//lookup_it
	void *lookup(uint32 iid, uint32 pin) throw();
protected:
	aio_provider_t(uint16 max_fd_count=AIO_DEF_MAX_FD_COUNT);
	void _lazy_init_object_id() throw(){ OID_DEF_IMP("aio_provider"); }	
private:
	uint32 _utc_max_slice; //max slice length expected once heart_beat calling,unit:ms
	sp_aioeh_t *_ehs; //registered event handler, which subfix is file descriptor
	uint32 _max_fd_count; //size of _ehs
	critical_section_t _cs;
	uint32 _hb_tid; //heart beat thread id

#ifdef LINUX
#if defined(__ENABLE_EPOLL__)
	int32 _epoll_h; //epoll handle, it's only valid if enable epoll
	uint32 _epoll_wait_timeout; //unit: ms
#else //real-time signal
	//under linux 2.6,SIGIO can not be blocked, and it will not be queued, 2009-8-6
	//->
	static critical_section_t _s_cs; //sync lazy SIGIO catching
	static bool _s_sigio_is_hooked;
	static bool _s_sigio_triggered; //indicate SIGIO is triggered or not due to real-time signal queue is full
	static uint32 _s_sigio_count; //total times of SIGIO is triggered over whole operation system
	static void _s_catch_sigio(int sig_num);		
	//<-
	uint32 _hb_tid; //id of thread in which heart_beat will run, this thread will receive real-time signal
	struct timespec _sigtimedwait_timeout; //used as sigtimedwait para in heart_beat
	siginfo_t _sig_info; //used as sigtimedwait para in heart_beat
	sigset_t _sigset;
#endif
#elif defined(WIN32)
#if defined(__ENABLE_COMPLETION_PORT__)
	HANDLE _iocp; //completion port
	uint32 _iocp_wait_timeout; //unit: ms
#else
#endif
#endif
};

/*[tip]
 *[desc] aio event stub, it is used to dispatch aio event to specific thread which is different from aio provider heat beat
 *       thread, it looks like a aio_event_handler_it implementation from aio provider side.
 *[note that] when aio_event_handler_it implementation should run in a different thread from aio_provider_t heart_beat,
 *       aio event will have to be dispatched to proper thread, though generic message service is optinal,
 *       but it isn't the most efficient method for this case because it needs lock/unlock operation at writer side.
 *       Considering aio service invloves massive event dilivery, for efficiency, this special aio event dilivery service
 *       is adopted.
 *2. multi aio providers are allowed but they must run in a same thread, otherwise, generic message service has to be adopted     
 *[history]
 * Initialize: 2008-10-6
 */
class aio_stub_t : public aio_event_handler_it,
                     public object_id_impl_t,
                     public ref_cnt_impl_t //thread-safe
{
	friend class aio_proxy_t;
public:
	//aio_event_handler_it
	void on_aio_events(int32 fd, uint32 aio_events, pointer_box_t ex_para=0);	

        //lookup_it
        void *lookup(uint32 iid, uint32 pin) throw();
protected:
	//it only can be instantiated by aio_proxy_t
	aio_stub_t(int32 fd, sp_owp_t& ep, aio_proxy_t *aio_proxy, sp_msg_proxy_t& msg_proxy, 
			event_slot_t *es_notempty, uint16 esi_notempty);
	void _lazy_init_object_id() throw();

	//if event pipe is full, aio events will be delivered to destination proxy via generic message service
	void _send_aio_events_as_msg(int32 fd, uint32 aio_events, pointer_box_t ex_para);
private:
	//aio event pipe,refer to  TLS one-way pipe associated with aio_proxy_t
	sp_owp_t _ep;
	int32 _fd;
	sp_aio_proxy_t _proxy; //associated aio_proxy

	//notify aio_proxy aio events pipe isn't empty to release possible waiting thread
	//->
	event_slot_t *_es_notempty; 	
	uint16 _esi_notempty;
	//<-

	sp_msg_rcver_t _msg_rcver; //it's just aioeh proxy itself to handle aio events from message service
	sp_msg_proxy_t _msg_proxy; //if _ep is full, aio event will be delivered via message service

	critical_section_t _cs;	
};
 
/*[tip]
 *[desc] aio event proxy, tls singleton. it provides aio event delivery service for specific thread which is different from
 *       aio provider heart beat thread. from even handler side, it looks like an aio provider
 *[note that] when aio_event_handler_it implementation should run in a different thread from aio_provider_t heart_beat, 
 *       aio event will have to be dispatched to proper thread, though generic message service is optinal, 
 *       but it isn't the most efficient method for this case because it needs lock/unlock operation at writer side. 
 *       Considering aio service invloves massive event dilivery, for efficiency, this special aio event dilivery service 
 *       is adopted.
 *2. multi aio providers are allowed but they must run in a same thread, otherwise, generic message service has to be adopted
 *3. on event pipe overflow, generic message service will be adopted, i.e. use this service, message service must be enabled 
 *   at the same time
 *[history]
 * Initialize: 2008-10-6
 */
uint32 const AIOEHPXY_DEF_EP_SIZE=1024;

//max slice of once heart_beat,unit:ms
uint32 const AIOEHPXY_HB_MAX_SLICE=80;

class aio_proxy_t : public aio_sap_it,
		    public heart_beat_it,
		    public msg_receiver_it, //handle overflow aio event
		    public object_id_impl_t,
		    public ref_cnt_impl_t //thread-safe
{
public:               
        //ep_size: aio event pipe size,unit: event piece
        //only first request takes effect whithin a thread
        //return same instance for same thread
	//--aio_stub_t will notify proxy that event pipe isn't empty via es_noempty and esi_notempty
        static aio_proxy_t *s_tls_instance(uint32 ep_size=AIOEHPXY_DEF_EP_SIZE, uint16 max_fd_count=AIO_DEF_MAX_FD_COUNT,
						event_slot_t *es_notempty=0, uint16 esi_notempty=0);
        static void s_delete_tls_instance(); //destroyed instance attached to current thread
public: 
	~aio_proxy_t();

        inline uint32 get_owner_tid() const throw() { return _tid; }

	//for dignosis
	//->
	inline uint32 get_overflow_cnt() const throw() { return _overflow_cnt; }
	inline void reset_overflow_cnt() throw() { _overflow_cnt = 0; }
	//<-

	//aio_sap_it
	//more than one aio provider can be supported, but caller should ensure all providers will run in same thread
       	bool register_fd(aio_sap_it *dest_sap, int32 fd, sp_aioeh_t& eh);
        void unregister_fd(int32 fd);
	
        //lookup_it
        void *lookup(uint32 iid, uint32 pin) throw();

        //heart_beat_it
	//ensure it's only called by owner thread
        virtual int8 heart_beat(); //receive aio event from _ep and dispatch to proper destination aio event handler 

        //expected max slice(unit:ms) for heart_beat once call
        virtual void set_max_slice(uint32 max_slice); 
        virtual uint32 get_max_slice() const throw(); 

        //indicate callee expected called frequency: 0 means expected to be called as frequent as possible,
        //otherwise, means expected interval(uint:ms) between two calling
        virtual uint32 get_expected_interval() const throw() { return 0; }

	//msg_receiver_it
	virtual void on_msg(msg_t *msg);
private:
	class _reg_item_t
	{
	public:
		_reg_item_t(){}
		_reg_item_t(sp_aiosap_t dest_sap, sp_aioeh_t aioeh)
		{
			this->dest_sap=dest_sap;
			this->aioeh = aioeh;
		}
		_reg_item_t(const _reg_item_t& itm)
		{
			this->dest_sap = itm.dest_sap;
			this->aioeh = itm.aioeh;
		}
		const _reg_item_t& operator =(const _reg_item_t& itm)
		{
			this->dest_sap = itm.dest_sap;
			this->aioeh = itm.aioeh;
			
			return *this;
		}
	public:
		sp_aiosap_t dest_sap;
		sp_aioeh_t aioeh;
	};

protected:
	aio_proxy_t(uint32 ep_size=AIOEHPXY_DEF_EP_SIZE, uint16 max_fd_count=AIO_DEF_MAX_FD_COUNT);
	void _lazy_init_object_id() throw();
private:
        static fy_thread_key_t _s_tls_key;
        static bool _s_key_created;
        static critical_section_t _s_cs;
private:
	sp_owp_t _ep; //event format in pipe: fd(int32)+aio events(uint32)
	uint32 _tid; //owner thread id
	uint32 _utc_max_slice; //max slice length expected once heart_beat calling,uint:ms
        _reg_item_t *_items; //registered item, whose subfix is file descriptor
        uint32 _max_fd_count; //size of _ehs	
	critical_section_t _cs;
	uint32 _overflow_cnt; //for dignosis
	sp_msg_proxy_t _msg_proxy; //owner thread TLS msg proxy
	event_slot_t *_es_notempty;
	uint16 _esi_notempty;
}; 

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_AIO_DREAMFREELANCER_20080616_H__
