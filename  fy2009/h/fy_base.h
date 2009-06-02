/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2009-4-28
 * ====================================================================
 */
#ifndef __FENGYI2009_BASE_DREAMFREELANCER_20080322_H__
#define __FENGYI2009_BASE_DREAMFREELANCER_20080322_H__ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exception>
#include <sys/types.h>
#include <vector>
#include <queue>
#include "fy_iid.h"

#ifdef POSIX

#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

#elif defined(WIN32)

#include <windows.h>
#include <time.h>

#endif //POSIX

DECL_FY_NAME_SPACE_BEGIN

/*[tip] buffer type template
 *[desc] 
 * buffer is very often used in communication software, plain c char* isn't good enough to used as out parameter
 * (without length info and indicator to tell where the buffer is allocated(stack or heap). std::string or 
 * std::vector<char> can meet above need, unfortunatedly, they often has an expensive constructor.
 * for performance of basic service, below buffer type is perferred to be used in basic service.
 * if buffer is allocated on heap, its lifecycle will be controlled by this class
 *
 *[memo]
 * spending of a million constructions:
 *            std::vector<char>: 80ms; std::string: 150ms; this type: 20ms--it's excellent,2008-4-7
 *[history]
 * initialize: 2006-7-28
 * revise 2008-4-7
 */
//reserved stack buffer size
const uint32 TBUF_RESERVED_SIZE_ON_STACK=128;

//fill_from position indicator
const int32 TBUF_FILL_POS_TAIL= -1;

template< typename T > class buffer_tt
{
public:
	typedef T ele_type;
	typedef T* ele_pointer;
	typedef T& ele_refer;
public:
	buffer_tt()
	{
		_buf=_buf_on_stack;
		_size = TBUF_RESERVED_SIZE_ON_STACK;
		_filled_len=0;
		_on_stack=true;
	}
	buffer_tt(T *buf, uint32 buf_size, bool on_stack, uint32 filled_len=0)
	{
		_buf=buf;
		_size = buf_size;
		_filled_len=filled_len;
		_on_stack=on_stack;
	}
	~buffer_tt()
	{
		if(_buf && !_on_stack) delete [] _buf;
	}

	void reset() //2008-11-26
	{
		if(_buf && !_on_stack) delete [] _buf;

                _buf=_buf_on_stack;
                _size = TBUF_RESERVED_SIZE_ON_STACK;
                _filled_len=0;
                _on_stack=true;
	}

	//attach a buffer to this class, which can be allocated on heap or stack,
	//if it's on heap, its lifecycle will be controlled by this object, if it's on stack,
	//you must take care of its lifecycle to avoid invalid access
	inline void attach(T *buf, uint32 buf_size, bool on_stack, uint32 filled_len=0)
	{
		if(_buf && !_on_stack) delete [] _buf;//remove old buffer on heap if any
		_buf=buf;
		_size=buf_size;
		_filled_len=filled_len;
		_on_stack=on_stack;
	}

	//if attached buffer is allocated on heap, after this call, its lifecycle will be freed from
	//this object, on the other hand, this object will be reset to reserved stack buffer	
	inline T *detach(uint32 *p_buf_size=0, uint32 *p_filled_len=0, bool *p_on_stack=0)
	{
		if(p_buf_size) *p_buf_size=_size;
		if(p_filled_len) *p_filled_len =_filled_len;
		if(p_on_stack) *p_on_stack=_on_stack;
		
		T *p=_buf;

                _buf=_buf_on_stack;
                _size = TBUF_RESERVED_SIZE_ON_STACK;
                _filled_len=0;
                _on_stack=true;

		return p;
	}

	//reserve buffer size if needed, new buffer is always allocated on heap,
	//valid content, indicated by _filled_len, will be copy to new buffer
	inline uint32 reserve(uint32 want_size)
	{
		if(_size >= want_size) return _size;

		uint32 new_size=_size<<1;
		if(want_size > new_size) new_size=want_size;

		T *tmp_buf=new T[new_size];
		if(_filled_len) memcpy(tmp_buf, _buf, _filled_len * sizeof(T));
		
		attach(tmp_buf, new_size, false, _filled_len);

		return _size;
	}

	//copy valid content in para buf to this object and ensure attached buffer is big enough
	//if para buf is empty, this object will become empty after operation
        inline void copy_from(const buffer_tt& buf)
        {
                _filled_len=0;
                if(0 == buf._filled_len) return;

                if(_size < buf._filled_len) reserve(buf._filled_len);

                memcpy(_buf, buf._buf, buf._filled_len*sizeof(T));
                _filled_len = buf._filled_len;
        }

	//take over para buf attached buffer,if it's allocated on heap, otherwise, it's same as copy_from
	inline void relay(buffer_tt& buf)
	{
		_filled_len=0;
		if(0 == buf._filled_len) return;

		if(buf._on_stack)
		{
                	if(_size < buf._size) reserve(buf._size);

                	memcpy(_buf, buf._buf, buf._filled_len*sizeof(T));
                	_filled_len = buf._filled_len;			
			return;
		}
		//buf's buffer is allocated on heap
		attach(buf._buf, buf._size, buf._on_stack, buf._filled_len);
		buf.detach();	
	}

	//copy valid content in para buf into this object from posisiton specified by 
	//para pos, if it's TBUF_FILL_POS_TAIL, indications new content will be appended to
	//this object, otherwise, pos should be an exact number which indicats the starting
	//position to fill
	//return filled length after operation 
	//if para buf is empty, it does nothing, it's different from copy_from
	inline uint32 fill_from(const buffer_tt& buf, int32 pos=TBUF_FILL_POS_TAIL)
	{
		if(0 == buf._filled_len) return _filled_len;

		uint32 cpy_filled_len=(TBUF_FILL_POS_TAIL == pos? _filled_len : pos);
		uint32 need_size=cpy_filled_len + buf._filled_len;
		if(need_size > _size) reserve(need_size);
		
		memcpy(_buf + cpy_filled_len, buf._buf, buf._filled_len*sizeof(T));
		_filled_len = cpy_filled_len + buf._filled_len;
			
		return _filled_len;
	}

	//another version of fill_from to fill copy raw buffer into this object
	inline uint32 fill_from(const T *raw_buf, uint32 len, int32 pos=TBUF_FILL_POS_TAIL)
	{
		if(!raw_buf || !len) return _filled_len;

		uint32 cpy_filled_len=(TBUF_FILL_POS_TAIL == pos? _filled_len : pos);
		uint32 need_size=cpy_filled_len + len;
		if(need_size > _size) reserve(need_size);

		memcpy(_buf + cpy_filled_len, raw_buf, len*sizeof(T));
		_filled_len = cpy_filled_len + len;

		return _filled_len;
	}

	//get attached raw buffer
	inline T *get_buf() const throw() { return _buf; }

	//get buffer size
	inline uint32 get_size() const throw() { return _size; }

	//set filled length manually, which is equal to length of valid content
	inline void set_filled_len(uint32 filled_len) throw() { _filled_len = filled_len; }

	inline uint32 get_filled_len() const throw() { return _filled_len; }

	//check if the attached buffer is upon stack or heap
	inline bool is_on_stack() const throw() { return _on_stack; }

	//forcely cast this object to raw T* type
	operator T *() const throw() { return _buf; } //equivalent of get_buf()
private:
	//forbiden implicitly copy constructor to eliminate lifecycle or thread related issue
	buffer_tt(const buffer_tt& buf){} 
private:
	T *_buf; //attached raw buffer
	uint32 _size; //the size of _buf
	uint32 _filled_len; //length of filled content
	bool _on_stack; //indicate _buf is on stack or heap
	T _buf_on_stack[ TBUF_RESERVED_SIZE_ON_STACK ]; //reserved stack buffer
};

typedef buffer_tt<int8> bb_t; //byte buffer type

#define FY_TRY try{

/*[tip] internal tracer
 *[desc] output trace info to stdandard error(you can redirect it if you want), it's used within basic FengYi2009 
 * modules which can't access higher layer FengYi2009 trace service
 *
 *[memo] 
 * test shows that trace info outputed by printf often loses tail info while redirect standard error device, 
 * but ::write will not.
 *
 *[history]
 * Initialize: 2008-4-2
 */
#ifdef POSIX

#	define __INTERNAL_FY_TRACE(sz_str) ::write(STDERR_FILENO, sz_str, ::strlen(sz_str))

#elif defined(WIN32) 

#	define __INTERNAL_FY_TRACE(sz_str) ::printf("%s",sz_str)

#endif //POSIX

/*[tip] internal exception handler 
 *[desc] some basic service functions are not allowed to throw exception, but you can't make sure if the functions 
 * are called, it will throw exceptions or not, end try block with below macro can catch potential exception 
 * and output exception message to stderr(you can redirect it if you want)
 * final_logic is a statements block to free allocated resources
 */
#define __INTERNAL_FY_EXCEPTION_TERMINATOR(final_logic) }catch(std::exception& e){ \
 if(e.what()){ \
     __INTERNAL_FY_TRACE(e.what()); \
     __INTERNAL_FY_TRACE("\r\n");}else{ \
     __INTERNAL_FY_TRACE("Caught a std::exception but what() is empty\r\n");} \
     final_logic;\
  }catch(...){ \
     __INTERNAL_FY_TRACE("Unspecified exception is caught\r\n"); \
     final_logic;\
  }

/*[tip] a kind of thread synchronization device
 *[desc] it has similar sematic with the counterpart in windows OS. 
 *
 *[memo]
 * according to ctor para,it can be recursive(default) or not, recursive is slight efficient than non-recursive.
 * it should be sealed
 *
 *[history] 
 *Initialize: 2006-8-4
 *Revise:     2009-5-10
*/
class critical_section_t
{
public:
        critical_section_t(bool recursive_flag=true) throw();//default critical section is recursive
        ~critical_section_t() throw();
        void lock() throw();
        void unlock() throw();
        bool try_lock() throw();

#ifdef POSIX

        //2008-3-28
        inline pthread_mutex_t& get_mutex() throw() { return _mtx; }

#endif //POSIX

private:

#ifdef POSIX

        pthread_mutex_t _mtx;

#elif defined(WIN32)

	CRITICAL_SECTION _cs;

#endif //POSIX
};

/*[tip]smart lock
 *[desc] it's often difficult to make sure matched calling lock/unlock. this class can meet this requirement automatically,
 * you still can manually call unlock if you want.
 *
 *[memo]
 * this object should be always used as stack object
 *[history]
 * Initialize: 2006-9-28
 */
class smart_lock_t
{
public:
        smart_lock_t(critical_section_t *p_cs) throw()
        {
                _p_cs=p_cs;
                if(_p_cs) _p_cs->lock(); //auto lock
        }

        ~smart_lock_t() throw() { unlock(); } //auto unlock to avoid dead lock

        //can call it to unlock on demand before destructor
        inline void unlock() throw()
        {
                if(_p_cs)
                {
                        _p_cs->unlock();
                        _p_cs=0;
                }
        }
private:
        critical_section_t* _p_cs;
};

/*[timp]event, inter-thread synchronization device 
 *[desc] suitable for "one waker, one waitor" mode. due to the limitation of Linux, you can't wait for multi 
 * event objects at the same time. to do it, you should use below event_slot_t or event_queue_t
 *
 *[history] 
 * Initialize: 2008-4-18
 * Revise:     2009-5-11
 */
const uint32 EVENT_WAIT_INFINITE=0xffffff;

class event_t
{
public:
        event_t(bool initial_signalled=true);
        ~event_t();

        //free a waiting thread(s),not support multi waiting threads
        //--for performance,if current state is signalled, this function will return immediately.
        void signal();

        //wait for state is changed to signalled, if current state is signalled, it return immediately,
        //otherwise, calling thread will be blocked until other thread call signal() to wake it up
	//return zero if got event, nonzero if timeout
        int32 wait(uint32 ms_timeout=EVENT_WAIT_INFINITE);//wait for other thread call signal() till ms_timeout expired
private:
        critical_section_t _cs;

#ifdef POSIX

        pthread_cond_t  _cnd;
        volatile bool _signalled;

#elif defined(WIN32)

	HANDLE _he;

#endif //POSIX
};

/*[tip]event slot, inter-thread synchronization device
 *[desc] suitable for "multi waker, one waitor" mode, can support waiting for multi-objects semantic
 *[memo] before waiting, if same slot is signalled more than once, waiter thread can only got signaled slot once,
 * that is, waitor thread don't know how many times a specified slot is signalled.
 * implemenation only can make sure either singal() or wait() with constant complexity,
 * here signal() are chosen, obviously, too big _slot_cnt is harmful with performance
 *[history] 
 *Initialize: 2008-4-17
 *Revise:     2009-5-12
 */
const uint16 DEF_EVENT_SLOT_COUNT=32;

class event_slot_t
{
public:
        typedef std::vector<uint16> slot_vec_t;
public:
        event_slot_t(uint16 slot_count=DEF_EVENT_SLOT_COUNT);
        ~event_slot_t();

        //signalled event slot identified by slot_index and wake up blocking wait()
        void signal(uint16 slot_index); //value range:0 to _slot_cnt

        //wait for any slot is signalled and push signalled slot index into signalled_vec
        //return the count of signaled slots 
        //timeout unit: tick-count
        int32 wait(slot_vec_t& signalled_vec, uint32 ms_timeout=EVENT_WAIT_INFINITE);
private:
        critical_section_t _cs;
	event_t _evt;
        int8 *_slot;
        uint16 _slot_cnt;
};

/*[tip] tick-count utility services
 *[desc] provide some common operations about tick-count
 *[history]
 *Initialize: 2007-3-12
 */
class tc_util_t
{
public:
    /*function: check if current tick-count has exeeded para tick-count end or not,it must 
                treat tick-count overflow properly
      usage:	use to treat time-out or other timespan logic
      remark:	you must ensure tc_end and tc_cur are later than tc_start, it doesn't mean
		tc_end and tc_cur are greater than tc_start because tick-count may overflow.
		on the other hand, the tc_deta must much less than the whole range of uint32
		and tc_deta must be non-zero

      history:	2007-3-12   
    */
    static bool is_over_tc_end(uint32 tc_start, uint32 tc_deta,
		uint32 tc_cur) throw(); 
		
    /*function: calculate difference between two tick-count
      history: 2007-3-15
    */
    static uint32 diff_of_tc(uint32 tc_start, uint32 tc_end) throw();
};

#ifdef POSIX

/*[tip]struct timeval utility services
 *[desc] provide some common operations about struct timeval
 *[history] 
 *Initialize: 2005-10-25
 */
class timeval_util_t
{
public:
    /*function: to calculate difference between of two struct timeval which often returned from gettimeofday()
      usage:    use to do performance test which need high accuracy
      remark:   it returns the differ timeval of para tv2 subtract para tv1. tv2 can be small than tv1,it makes sure
                the result timeval's tv_sec field always has same sign with tv_usec.
      test:     the overhead of this fuction self is less than 1 microsecond(1/1000000 second)
      history:  2005-10-25
    */
    static struct timeval diff_of_timeval(const struct timeval& tv1,
		const struct timeval& tv2) throw() __attribute__((const));

    /*function: change the return timeval of diff_of_timeval() to tick count(1/1000 second)
      usage:    use to performance test which need normal accuracy(tick count level)
      remark:   when the accuracy in tick count can meet need, it has simpler type of the 
                return value than diff_of_timeval()
      test: it work whether tv2 greater than tv1 or not
      history:  2005-10-27
    */
    static int32 diff_of_timeval_tc(const struct timeval& tv1,
		const struct timeval& tv2) throw() __attribute__((const));

};

/*[tip] efficient timestamp service, follows singleton dessign pattern
 *[desc] typical asynchronous application has to frequently access system time, but don't need very accurate,
 * general time service is a system service,which involve context switch,is slight expensive,not very suitable for frequent
 * call.this service implements a clock within user address space, so it's very efficient, but not very precious.
 * it's suitable for the most asynchronous applications, if you need accurate time service, you still have to access 
 * system time service, but I believe it seldom happens in fact.
 *
 *[memo]
 * test shows this clock is very excellent, it's much more efficient than its equivalent of system service, 2006-8-10 usa
 * GetTickCount under Windows is efficient enough, so this service isn't needed under windows,2009-5-5
 *[history] 
 * Initialize: 2006-8-4
 */
class user_clock_t
{
public:
        static user_clock_t *instance() throw();

        ~user_clock_t() throw();

        //this tick unit isn't ms(system tick-count), it's unit is get_resolution()
        inline uint32 get_usr_tick() const throw() {return _tick;}

        //return residual of milli-seconds
        //this function can be called by serveral thread simultaneously, but becuase only one thread(_thd_f) may
        //change related member variable, so lock isn't needed and must be forbiden
        uint32 get_localtime(struct tm *lt) throw();

        inline uint32 get_resolution() const throw() {return _resolution;}//unit is tick count
        inline bool is_running() const throw() { return _running_flag; }
private:
        user_clock_t() throw();//forbiden caller to call ctor
        static void *_thd_f(void *) throw();
        //compare two struct tiveval
        static int8 _compare_timeval(const struct timeval& tv1,const struct timeval& tv2) throw();
        void _start() throw();
        void _stop() throw();
private:
        const static uint32 UCLK_PRECISION=10;//unit:ms

        static user_clock_t *_s_inst;//singleton instance
        static critical_section_t _s_cs;
        uint32 _tick;
        uint32 _resolution;
        pthread_t _thd;
        bool _stop_flag;//notify _thd thread to stop
        bool _running_flag;
        uint32 _sleep_para;
        struct timeval _l_cp;//last check point
        struct timeval _c_cp; //current check point
        struct timeval _local_time;//for get_localtime
        struct tm _tm_ltb;//local time base(tm)
        uint32 _s_ltb;//second part of local time base(struct timeval)
        int32 _lt_ahead_cp;//_local_time go ahead check point(uint:ms)
        critical_section_t _cs;//make sure user_clock_t is ready get_localtime,2007-3-5
};

#	define get_tick_count(user_clock) user_clock->get_usr_tick()
//user-clock resolusion
#	define get_tick_count_res(user_clock) user_clock->get_resolution()
#	define get_local_time(lt,user_clock) user_clock->get_localtime(lt)

#elif defined(WIN32)

/*[tip] user clock stub
 *[desc]just a stub class, it does nothing under windows
 *[history]
 * Initialize: 2009-5-5
 */
class user_clock_t
{
public:
        static user_clock_t *instance() throw() { return 0; }
		//GetLocalTime of Windows is about as 5 times faster as localtime_r of Linux,
		//to simplify implementation, this function will be implemented by calling
		//::GetLocalTime directly
		inline uint32 get_localtime(struct tm *lt) throw()
		{
			SYSTEMTIME stm;
			::GetLocalTime(&stm);
			if(!lt) return 0;
			lt->tm_sec = stm.wSecond;
			lt->tm_min = stm.wMinute;
			lt->tm_hour = stm.wHour;
			lt->tm_mday = stm.wDay;
			lt->tm_mon = stm.wMonth;
			lt->tm_year = stm.wYear;
			lt->tm_wday = stm.wDayOfWeek;
			lt->tm_yday = 0; //unspecified
			lt->tm_isdst = 0; //unspecified

			return stm.wMilliseconds;
		}
};

#	define get_tick_count(user_clock) ::GetTickCount()
//clock resolsuion is one
#	define get_tick_count_res(user_clock) (1)
#	define get_local_time(lt,user_clock) user_clock->get_localtime(lt)

#endif //POSIX

/*[tip]root interface for dynamic type discovery
 *[desc] it's often important for serveral interfaces implemented by one class to dynamically discover each other.
 *       to support this feature, any interface MUST inherit from below interface and any class must overwrite it.
 *[memo] dynamic_cast or static_cast is another alternative,but they can't work in multi parents are inheritted 
 *       which have one or more same ancient
 *[history] 
 * Initialize: 2006-10-13
 */
class lookup_it
{
public:
        //any discoverable interface MUST be identified by an unique id
        virtual void *lookup(uint32 iid) throw()=0;
};

/*[tip]interface to support tracibility
 *[desc] any tracable object MUST be identified by a readable object id, this interface specifies this requirement,
 *       any tracable class should realize this interface
 *[history] 
 * Initialize: 2008-3-18
 */
class object_id_it : public lookup_it
{
public:
        //return an unique object id and ensure it's readable for tracibility
        virtual const int8 *get_object_id() throw()=0;
};

/*[tip] string utility
 *[desc] define some static string functionss
 *[history] 
 *Initialize: 2007-2-12
 */
class string_util_t
{
public:
        //trim both left and right white space of para str in place, return start position of result string in para str
        // and length of result string if para p_len isn't null
        //pass test 2007-2-12
        //--note that, it doesn't change the para str end position even if right white space exists, it only returns a
        //length indicator to reveal the right white space's existence.
        static const int8 *s_trim_inplace(const int8 *str,uint32 *p_len);

private:
        //check if a character is white space
        inline static bool _s_is_ws(int8 c);
};

/*[tip]merge multi string pieces to one
 *[desc] merging many string pieces to one is more useful, but often is low efficient, std::ostringstream is a good choice,
 * unfortunately, it spends too much on construction, so not good enough for basic services. this class will do better.
 * for performance, this class isn't responsible for lifecycle management for any string pieces, if uncpy_str is set
 *[memo] test reveals that pre-allocated _piece_v by former build operation can fairely accelerate later build operation
 * (pdp decreases from 0.70 to 0.25,and pre-allocated buffer for numeric piece by alloc_buffer_n can also accerlate
 * build performance(pdp decreases from 8.74 to 2.03--2006-8-1
 * this class is used in exception framework, so it is never allowed to throw any exceptions,2006-8-2
 * test proves it's also more efficient than std::ostringstream on formatting string,prealloc buffer and eUNCPY_STR
 * can improve performance obviously, 2008-4-7
 *[history] 
 *Initialize: 2006-7-28
 */
class string_builder_t
{
public:
        //indicate open/close copy string flag, which means sucessive string paramenter will be copied/attached by this object
        typedef enum
        {
                eCPY_STR,
                eUNCPY_STR
        } cpy_str_e;
public:
        string_builder_t();
        ~string_builder_t();

        string_builder_t& operator <<(int8 data);
        string_builder_t& operator <<(uint8 data);
        string_builder_t& operator <<(int16 data);
        string_builder_t& operator <<(uint16 data);
        string_builder_t& operator <<(int32 data);
        string_builder_t& operator <<(uint32 data);
        string_builder_t& operator <<(float data);//not recommend to use
        string_builder_t& operator <<(double data);//not recommend to use
        string_builder_t& operator <<(const void *data);//format pointer as hex string

        //if cpy_str_flag == eCPY_STR,life cycle of successive string para piece will be independent with para string,2008-4-10
        string_builder_t& operator <<(const cpy_str_e& cpy_str_flag);

        //if eCPY_STR is set,builder will be independent from sz_str, otherwise, sz_str shoulde be valid before calling build()
        string_builder_t& operator <<(const int8* sz_str);

        inline uint32 get_result_size() const throw(){ return _len_total; }//return length of result string,not byte length

        //build all string pieces to one,return length of result string
        uint32 build(bb_t& bb);

        //pre-allocated specified count of bytes space to cache cpy_str or numeric data,
        //can call it multi times if object is empty(eg. after reset),if alloc_size para is zero,_buf_n will be freed
        void prealloc(uint32 alloc_size);

        //similar to prealloc, but the para count_n means to alloc enough space to hold count_n pieces of numeric formatted string
        void prealloc_n(uint32 count_n);

        //reset object to 'empty',if bShrinkage is true,_piece_v will be freed, otherwise, _piece_v and _len_v will be kept.
        void reset(bool bShrinkage=false);

protected:
        //describe a piece of string
        class _piece_t
        {
        public:
                _piece_t()
                {
                        str=0;
                        len=0;
                        need_free=false;
                }
                _piece_t(const int8 *str,bool need_free)
                {
                        if(str)
                        {
                                this->str=str;
                                this->len=::strlen(str);
                                this->need_free=need_free;
                        }
                        else
                        {
                                this->str=0;
                                this->len=0;
                                this->need_free=false;
                        }
                }
        public:
                const int8 *str;//string piece buffer
                uint32 len;//length of string piece
                bool need_free;//need string builder free it on destruction or not
        };
protected:
        inline void _append_str(const int8 *str,bool bNeedFree);//append one piece string
protected:
        _piece_t *_piece_v;//vector for string pieces
        uint32 _len_v; //length of _str_v
        int8 *_buf_n;//preallocated buffer to accelerate numeric piece converting to string
        uint32 _len_buf_n;//length of _buf_n;
        uint32 _cur_pos_buf_n;//start position for next result string of numberic piece
        uint32 _cur_pos; //next index of string pieces
        uint32 _len_total;//total length of result string
        cpy_str_e _cpy_str_flag;
private:
        string_builder_t(const string_builder_t& sb){} //forbidden copy ctor
};

/*[tip] lower layer trace service with formatting output
 *[desc] output trace info to stdandard error, it's used within basic ccp service modules which
 *       can't access higher layer ccp trace service but can use string_builder_t
 *[memo] test shows that trace info outputed by printf often loses tail info while redirect standard error device, but
 *            ::write will not.
 *[history] 
 * Initialize: 2008-4-23
 * Revise:     2009-5-13
 */
#ifdef POSIX

#define __INTERNAL_FY_TRACE_EX(desc) do{ string_builder_t sb; sb<<desc; bb_t bb; sb.build(bb);\
           ::write(STDERR_FILENO, (int8*)bb, ::strlen((int8*)bb)); }while(false)

#elif defined(WIN32)

#define __INTERNAL_FY_TRACE_EX(desc) do{ string_builder_t sb; sb<<desc; bb_t bb; sb.build(bb);\
           ::printf("%s",(int8*)bb); }while(false)

#endif //POSIX

/*[tip] exception service
 *[desc] The effective exception process is always important to make program robust.
 *       The throw and try...catch mechanism has been proved to be an efficient method and has been becoming mainstream
 *       , but most exception class describes where the exception occurs rather detailly than the path.
 *       I think the path that exception passed is as important as where it occured for troubleshooting,
 *       The following exception class will catch both where exception occurs and the path it has walked through
 *[memo]
 *1.any member function of this class is disallowed to throw exception, this exception type is not remotable,
 *  to report exception to remote client, should wrapp it to a remotable exception object and transfer the wrapper
 *  to remote side.
 *2. a global error number is more manageable, but in practice, it's very difficult to realize,
 * so this class replaces global exception number with a local unique spot string, although, it's preferred to specify
 * a short and globally unique spot, but it's not necessary. a merged string of spots through exception path is often
 * globally unique,it's also not necessary,2008-4-10
 *
 *[history] 
 *Initialize: 2005-11-30 created
 *Revise:     2008-3-13  
 * change string_t to c-string for performance, because construction of string_t is a little expensive,2008-4-7
 */
class exception_t;

/*[tip]internal object for exception mechanism
 *[desc]holding exception detail data, it will not be copied by try...catch mechnism , avoid frequently call its
 * expendisve copy ctor,try ... catch will copy light object exception_t which manages the real exception
 * data object by reference mechanism
 */
class __exception_body_t
{
        friend class exception_t;
public:
        __exception_body_t() throw() { _ref_cnt=0; }
        //create exception with its occurence description
        __exception_body_t(const int8 *src_file, int32 line_num,
                        const int8 *object_id, const int8 *function_name, const int8 *spot, const int8 *desc) throw();

        virtual ~__exception_body_t() throw(){}

        //add an exception row to describe exception path or its occurence
        void push_back(const int8 *src_file, int32 line_num,
                        const int8 *object_id, const int8 *function_name, const int8 *spot, const int8 *desc) throw();

        //build excpetion description string, if verbose_flag is true, will build  a string to describe all exception path
        //in detail for technical guy, otherwise, only descibe the topmost excpetion row for end-user
        void to_string(bb_t& bb, bool verbose_flag=true) const throw();

        //the following is designed for programmer to iterate _exp_vec or other purpose
        //->
        inline int32 get_path_steps() const throw() { return _exp_vec.size(); } //return size of _exp_vec;

        //max step_index is get_path_steps() - 1, zero step_index is relevant to exception occurence
        //return true on success, false on failure
        bool iterate(int32 step_index, bb_t& source_file_ref, int32& line_num_ref, bb_t& object_id_ref, bb_t& function_name_ref,
                        bb_t& spot_ref, bb_t& desc_ref) throw();

        //<-

protected:
        class _exp_row_t
        {
        public:
                _exp_row_t() throw();
                _exp_row_t(const int8 *src_file, int32 line_num,
                                const int8 *object_id, const int8 *function_name, const int8 *spot,
                                const int8 *desc) throw();
                _exp_row_t(const _exp_row_t& er) throw(); //copy ctor

        public:
                //source file name where exceptions occurs or passes
                bb_t m_src_file;
                int32 m_line_num; //line number of source file

                //object id where exceptions occurs or passes
                //for global function it can be reserved string like "global" or other equivalent
                bb_t m_obj_id;
                bb_t m_fun;//function name where excpetion occurs or passes
                bb_t m_spot; //identify where the exception occurs within the function
                bb_t m_desc; //exception description
        };
protected:
        __exception_body_t(const __exception_body_t& exp) throw(){} //copy ctor
        void _add_reference();
        void _release_reference();
protected:
        std::vector<_exp_row_t> _exp_vec;//hold all exception path
        uint32 _ref_cnt;
};

/*it's a flightweight exception class, it only hold smart pointer to real exception data, so its copy ctor is efficient,
 *which can meet try...catch requirements to call copy ctor frequently
 */
class exception_t
{
public:
        exception_t() throw();
        exception_t(const int8 *src_file, int32 line_num,
                        const int8 *object_id, const int8 *function_name, const int8 *spot, const int8 *desc) throw();

        exception_t(const exception_t& e) throw();
        virtual ~exception_t();

        //exception is thrown out of each code scope, copy ctor will be called once, _cc_cnt will be increased
        //_cc_cnt == 1 means catch clause catchs an exception within same scope
        inline uint32 get_cc_count() const throw() { return _cc_cnt; }

        void to_string(bb_t& bb, bool verbose_flag=true) const throw() { _exp_body->to_string(bb, verbose_flag); }

        __exception_body_t *operator ->() throw() { return _exp_body; }
protected:
        uint32 _cc_cnt;//count copy constructor is called
        __exception_body_t *_exp_body;
};

/*[tip]
 *[desc] some macros to make ease exception handling
 *[history] 
 * Initialize: 2008-3-19
 * Revise:     2009-5-13
 */
//internal macro to throw an exception
#define __INTERNAL_FY_THROW(object_id, function_name, spot, desc) do{ string_builder_t sb; sb<<desc; bb_t bb;\
   sb.build(bb);\
   throw exception_t(__FILE__, __LINE__,\
             object_id, function_name, spot, (int8*)bb); }while(false)

//declare exception context, it should appears in the front of a function definition
#define FY_DECL_EXCEPTION_CTX(object_id, function_name) int8 __exp_object_id_klv_20080317[]=object_id;\
 int8 __exp_function_name_klv_20080317[]=function_name

//throw an exception, it always appears after FY_DECL_EXCEPTION_CTX
#define FY_THROW(spot, desc) __INTERNAL_FY_THROW(__exp_object_id_klv_20080317, \
 __exp_function_name_klv_20080317, spot, desc)

//internal catch and throw again an exception
#define __INTERNAL_FY_CATCH_N_THROW_AGAIN(object_id, function_name, spot, desc, final_code) }catch(exception_t& e){\
       final_code;\
       if(e.get_cc_count() == 1) throw e;\
       e->push_back(__FILE__, __LINE__, object_id, function_name, spot, desc);\
       throw e;}catch(std::exception& e){\
       final_code;\
       __INTERNAL_FY_THROW(object_id, function_name, spot, e.what());\
       }catch(...){\
       final_code; \
       __INTERNAL_FY_THROW(object_id, function_name, spot, "unknown exception");}

//catch an expcetion and throw it again, it always appears after FY_DECL_EXCEPTION_CTX
#define FY_CATCH_N_THROW_AGAIN(spot, desc, final_code) __INTERNAL_FY_CATCH_N_THROW_AGAIN(\
          __exp_object_id_klv_20080317, __exp_function_name_klv_20080317, spot, desc, final_code)

//declare exception context for class which support get_object_id() function
#define FY_DECL_EXCEPTION_CTX_EX(function_name) int8 __exp_function_name_ex_klv_20080317[]=function_name

//throw an exception, it always appears after FY_DECL_EXCEPTION_CTX_EX
#define FY_THROW_EX(spot, desc)  __INTERNAL_FY_THROW(this->get_object_id(), __exp_function_name_ex_klv_20080317, spot, desc)

//catch an expcetion and throw it again, it always appears after FY_DECL_EXCEPTION_CTX_EX
#define FY_CATCH_N_THROW_AGAIN_EX(spot, desc, final_code) __INTERNAL_FY_CATCH_N_THROW_AGAIN(\
         this->get_object_id(), __exp_function_name_ex_klv_20080317, spot, desc, final_code)

#if defined(FY_ENABLE_ASSERT)

#       define FY_ASSERT(exp) if(!(exp)) { __INTERNAL_FY_THROW("ASSERT","ASSERT", "ASSERT", #exp); }

#else

#       define FY_ASSERT(exp)

#endif

/*[tip]macro to build default object_id_it
 *[desc] the default format of object_id: <class_name>_<hex this pointer> 
 *[history] 
 * Initialize: 2006-12-1,usa
 */
#define OID_DEF_IMP(class_name) FY_TRY\
 string_builder_t sb;\
 sb<<(class_name)<<"_"<<(void*)this;\
 sb.build(_object_id); __INTERNAL_FY_EXCEPTION_TERMINATOR(;)

/*[tip]default implementation of object_id_it
 */
class object_id_impl_t : public object_id_it
{
public:
        object_id_impl_t(){}
        virtual ~object_id_impl_t(){}

        //lookup_it
        void *lookup(uint32 iid) throw()
	{
        	switch(iid)
        	{
        	case IID_self:
        	case IID_lookup:
                	return this;

        	case IID_object_id:
                	return static_cast<object_id_it *>(this);

        	default:
                	return 0;
        	}
	}

        //object_id_it
        //descendant should not overwrite it
        inline const int8 *get_object_id() throw()
        {
                if(_object_id.get_filled_len() == 0) _lazy_init_object_id();//lazy initialize object_id
                return (int8*)_object_id;
        }
protected:
        //it should be overwritten by descendant
        virtual void _lazy_init_object_id() throw() { OID_DEF_IMP("object_id_impl") }

protected:
        bb_t _object_id;
};

/*[tip]reference count interface
 *[desc] reference count based objects' life cycle management is proved useful. any reference-count-aware class should
 * realize this interface
 *[memo]
 * semantic convention: reference count mechanism is used to control heap objects' life cycle, It's add_reference() must
 * be called as soon as a heap object is constructed, and heap object will be destroyed just after its reference count is
 * released to zero.
 *[history] 
 *Initialize: 2006-6-19
*/
class ref_cnt_it : public lookup_it
{
public:
        virtual void add_reference()=0;
        virtual void release_reference()=0;
        virtual uint32 get_ref_cnt() const throw()=0; //for tracibility
};

/*[tip] default implementation of ref_cnt_it 
 *[desc] any class can inherit reference-count mechanism from this class,
 *       you can specify an outer critical_section_t object for thread-safe, or null for non-thread-safe
 *       to eliminate mess, all ref_cnt_it based object must always be allocated on heap, though this means tiny performance down
 *       than stack object in some occasions, but both support heap object and stack object will lead to much mess
 *       and crash risk, so ALWAYS CREATE RCO(ref_cnt_it-based object) ON HEAP is a good practice,2008-3-26
 *[history] 
 *Initialize: 2006-6-19
 */
class ref_cnt_impl_t : public ref_cnt_it
{
public:
       virtual ~ref_cnt_impl_t(){}

        //set_lock is called with a non-null para will enable thread-safe
        void set_lock(critical_section_t *cs) throw();

        bool is_thread_safe() const throw() { return _p_cs!=0; }

        //lookup_i
        void *lookup(uint32 iid) throw();

        //ref_cnt_i
        void add_reference();
        void release_reference();
        inline uint32 get_ref_cnt() const throw(){ return _ref_cnt; }
protected:
        //disable instantiate it directly
        ref_cnt_impl_t() throw();
        ref_cnt_impl_t(critical_section_t *cs) throw();

protected:
        uint32 _ref_cnt;
        critical_section_t *_p_cs;//set it to nonull,it will be thread-safe
};

/*[tip] smart pointer to wrapp ref_cnt_it
 *[desc] reference count based object's lifecycle management is proved useful. but manually add/release reference
 * is programmer's nightmare, and often cause memory leak, the following smart pointer can automatically add/release 
 * reference correctly.
 *
 *[memo] this smart pointer assumes that T implements ref_cnt_i or equivalent interface. 
 * It's forbidden to manually add or release reference passing by smart pointer
 *
 *[history] 
 * Initialize: 2006-9-14
 */
template < typename T> class _NoAddOrReleaseRefPtr : public T
{
private:
        //forbid  to call add_reference and release_reference
        void add_reference(){}
        void release_reference(){}
};

template < typename T> class smart_pointer_tt
{
public:
        smart_pointer_tt() throw() { _p=0; }
        smart_pointer_tt(T *p,bool shallow_copy_opt=false)//default:attach a raw pointer
        {
                _p=p;
                if(shallow_copy_opt && _p) _p->add_reference();
        }

        smart_pointer_tt(const smart_pointer_tt& sp)//shallow copy para pointer
        {
                _p=sp._p;
                if(_p) _p->add_reference();
        }
        ~smart_pointer_tt(){ if(_p) _p->release_reference(); }
        const smart_pointer_tt& operator =(const smart_pointer_tt& sp)//shallow copy para pointer
        {
                if(_p) _p->release_reference();
                _p=sp._p;
                if(_p) _p->add_reference();

                return sp;
        }

        bool is_null() const throw() { return _p == 0; }
        bool is_same_as(const smart_pointer_tt& sp) const throw() { return _p == sp._p; }
        bool is_same_as(T *raw_obj) const throw() { return _p == raw_obj; }
        //attach a zero pointer means to clear,2009-1-5
        void attach(T *p)//not add new pointer's reference
        {
                if(_p) _p->release_reference();//release old raw pointer
                _p=p;
        }
        T *detach() throw()
        {
                T *p=_p;
                _p=0;
                return p;
        }
        //copy_from a zero pointer is same as attach a zero pointer,2009-1-5
        void copy_from(T *p)//add reference
        {
                if(_p) _p->release_reference();
                _p=p;
                if(_p) _p->add_reference();
        }
        operator T*() throw() { return _p; }

        _NoAddOrReleaseRefPtr<T> *operator ->() throw() { return (_NoAddOrReleaseRefPtr<T> *)_p; }
        T& operator *() throw() { return *_p; }

        const T *get_p_const() const throw() { return _p; }
protected:
        T * _p;//raw pointer which implements ref_cnt_t or equivalent interface
};

/*[tip] smart pointer to support Dynamic Interface Discovery(DID) mechanism
 *[desc] type T can be an interface that doesn't inherit from ref_cnt_it, if only concrete class implements both T interface 
 * and ref_cnt_it, it will work
 *[history]
 * Initialize: 2007-2-15
 */
template < typename T> class smart_pointer_lu_tt
{
public:
        smart_pointer_lu_tt() throw(): _p(0),_pr(0) {}
        smart_pointer_lu_tt(T *p, bool shallow_copy_opt=false)//default:attach a raw pointer
        {
                _p=p;
                if(_p)
                {
                        _pr=(ref_cnt_it *)_p->lookup(IID_ref_cnt);
                        if(shallow_copy_opt && _pr) _pr->add_reference();
                }
                else
                {
                        _pr=0;
                }
        }

        smart_pointer_lu_tt(const smart_pointer_lu_tt& sp)//shallow copy para pointer
        {
                _p=sp._p;
                _pr=sp._pr;
                if(_pr) _pr->add_reference();
        }
        ~smart_pointer_lu_tt(){ if(_pr) _pr->release_reference(); }

        bool is_null() const throw() { return _p == 0; }
        bool is_same_as(const smart_pointer_lu_tt& sp) const throw()
        {
                if(_p)
                {
                        if(sp._p)
                                return _p->lookup(IID_self) == sp._p->lookup(IID_self);
                        else
                                return false;
                }
                else
                        return sp._p == 0;
        }

        bool is_same_as(T *raw_obj) const throw()
        {
                if(_p)
                {
                        if(raw_obj)
                                return _p->lookup(IID_self) == raw_obj->lookup(IID_self);
                        else
                                return false;
                }
                else
                        return raw_obj == 0;
        }

        const smart_pointer_lu_tt& operator =(const smart_pointer_lu_tt& sp)//shallow copy para pointer
        {
                if(_pr) _pr->release_reference();
                _p=sp._p;
                _pr=sp._pr;
                if(_pr) _pr->add_reference();

                return sp;
        }
        //attach a zero pointer means to clear,2009-1-5
        void attach(T *p)//not add new pointer's reference
        {
                if(_pr) _pr->release_reference();//release old raw pointer
                _p=p;
                if(_p)
                        _pr=(ref_cnt_it *)_p->lookup(IID_ref_cnt);
                else
                        _pr=0;
        }
        T *detach() throw()
        {
                T *p=_p;
                _p=0;
                _pr=0;
                return p;
        }
        //copy_from a zero pointer is same as attach a zero pointer,2009-1-5
        void copy_from(T *p)//add reference
        {
                if(_pr) _pr->release_reference();
                _p=p;
                if(_p)
                {
                        _pr=(ref_cnt_it *)_p->lookup(IID_ref_cnt);
                        if(_pr) _pr->add_reference();
                }
                else
                        _pr=0;
        }
        operator T*() throw() { return _p; }

        _NoAddOrReleaseRefPtr<T> *operator ->() throw() { return (_NoAddOrReleaseRefPtr<T> *)_p; }
        T& operator *() throw() { return *_p; }

        const T *get_p_const() const throw() { return _p; }
private:
        T * _p;//target interface pointer which doesn't inherit IRefCnt
        ref_cnt_it *_pr; //ref_cnt_it pointer which concrete class implements
};

typedef smart_pointer_lu_tt<lookup_it> sp_lookup_t;

/*[tip] smart pointer convertion macro 
 *[desc] macro to do type cast between parent type and its child type smart pointer
 *[memo] destination smart pointer will shallow copy from source smart pointer but not attach from it
 *            dest_type must be parent or child of src_type
 *[history] 
 * Initialize: 2007-3-21
 */
#define SP_CAST(dest_type, src_type, src_sp) (smart_pointer_tt< dest_type >((dest_type*)(src_type*)src_sp, true))

/*[tip]smart pointer convertion macro which support lookup_it interface
 *[desc] this macro can do type cast between multi interfaces, which inherits from lookup_it,implemented by same class
 *[history] 
 * Initialize: 2007-3-21
 */
#define SP_LU_CAST(dest_type, dest_type_id, src_sp) (smart_pointer_lu_tt< dest_type >(\
                   (dest_type*)src_sp->lookup(dest_type_id),true))

/*[tip]ref_cnt_it adaptor
 *[desc] adapt ref_cnt_it mechansim to any type without it
 *[memo] raw type must be allocated on heap
 *[history] 
 * Initialize: 2007-2-5
 */
template < typename T> class ref_cnt_adaptor_tt : public ref_cnt_impl_t
{
public:
        //after this call, life cycle of pointer_on_heap will be controlled by this object
        static smart_pointer_tt< ref_cnt_adaptor_tt<T> > create(T *pointer_on_heap, uint32 array_size=0, bool thread_safe=false)
        {
                ref_cnt_adaptor_tt<T> *p=new ref_cnt_adaptor_tt<T>(pointer_on_heap, array_size);
                p->add_reference();
                if(thread_safe) p->set_lock(&p->_cs);

                return smart_pointer_tt<ref_cnt_adaptor_tt<T> >(p);
        }

        T *get_p() throw(){ return _pointer_on_heap; } //don't delete pointer returned by this call
        const T *get_p_const() const throw() { return _pointer_on_heap; }
        T *operator ->() throw() { return _pointer_on_heap; }
public:
        ~ref_cnt_adaptor_tt()
        {
                if(_pointer_on_heap)
                {
                        if(_array_size)
                                delete [] _pointer_on_heap;
                        else
                                delete _pointer_on_heap;
                }
        } //virtual destructor
        uint32 get_array_size() const throw() { return _array_size; }
private:
        //disable direct calling constructor;
        ref_cnt_adaptor_tt() throw(){ _pointer_on_heap = 0; _array_size=0; }
        ref_cnt_adaptor_tt(T *pointer_on_heap, uint32 array_size=0) throw()
        {
                _pointer_on_heap = pointer_on_heap;
                _array_size = array_size;
        }
private:
        T *_pointer_on_heap; //object without ref_cnt_it mechanism,it must be allocated on heap
        uint32 _array_size;//if T* points to array, it's array size, otherwise, it's zero
        critical_section_t _cs;
};

/*[tip] clone interface,which implies deep-copy
 *[desc] clone a new object from original object, this new object is a deep copy of original object,
 * it is completely independent from original one. clone is an opposite mechanism from ref_cnt_it.
 *
 *[history] 
 * Initialize: 2006-6-19
 */
class clone_it : public lookup_it
{
public:
        //deep copy a new object from this object(origin),any member variable must recursively do deep copy.
        //prototype_flag is true means only new a dest object with initial state and don't need to copy
        //current object's state to dest object
        virtual sp_lookup_t clone(bool prototype_flag=false)=0;
};

typedef smart_pointer_lu_tt<clone_it> sp_clone_t;

/*[tip] prototype manager, singleton
 *[desc] it's often useful to activate/deactivate object from stream media without precreating relavent object
 *
 *[history] 
 * Initialize: 2008-5-26
 */
const uint32 MAX_PROTOTYPE_ID=65535;

//state enum
const uint8 PTM_STATE_NULL=0;
const uint8 PTM_STATE_OPENING=1;
const uint8 PTM_STATE_OPEN=2;

//return value
const int PTM_RET_SUCCESS=0;
const int PTM_RET_WRONGSTATE=1;
const int PTM_RET_WRONGPARAM=2;
const int PTM_RET_IDOVERFLOW=3;
const int PTM_RET_EXISTED=4;
const int PTM_RET_FAILCLONE=5;

class prototype_manager_t
{
public:
        static prototype_manager_t *instance() throw();

        //it must be called before calling registering
        int begin_register() throw();

        //register prototype object pt,which must realize clone_i interface, ref_cnt_i interface is optional
        int reg_prototype(uint32 ptid, clone_it *pt);

        //it must be called after completing registering and before calling get_object
        int end_register() throw();

        //to keep it as efficient as possible, after end_register, _pt_vec will be read-only,
        //then successive get_object will not need to lock
        sp_clone_t get_prototype(uint32 ptid);
private:
        //to keep prototype service is as efficient as possible, prototype object will be hold in vector
        //and suffix is prototype id
        typedef std::vector<sp_clone_t> pt_vec_t;
private:
        prototype_manager_t() throw();
private:
        static prototype_manager_t *_s_inst;//singleton instance
        static critical_section_t _s_cs;

        pt_vec_t _pt_vec; //protottype object vector
        int8 _state;
        uint32 _capacity;//current used max prototype id
};

/*[tip]heart_beat interface 
 *[desc] it implements a kind of asychronous mechanism which decouple system's static view(data structure)
 *       from dynamic view(thread mode).
 *[history] 
 * Initialize: 2007-4-9
 */
//return of heart_beat
//done nothing
int8 const RET_HB_IDLE=0;

//completed something
int8 const RET_HB_BUSY=1;

//interrupted by slice check
int8 const RET_HB_INT=2;

class heart_beat_it : public lookup_it
{
public:
        //called by thread loop
        virtual int8 heart_beat()=0;

        //expected max slice(user tick-count) for heart_beat once call
        virtual void set_max_slice(uint32 max_slice)=0;
        virtual uint32 get_max_slice() const throw() =0;

        //indicate callee expected called frequency: 0 means expected to be called as frequent as possible,
        //otherwise, means expected interval user tick-count between two calling
        virtual uint32 get_expected_interval() const throw() =0;
};

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_BASE_DREAMFREELANCER_20080322_H__
