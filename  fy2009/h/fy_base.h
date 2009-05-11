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
#include <string.h>
#include <exception>
#include <sys/types.h>
#ifdef POSIX

#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

#elif defined(WIN32)

#include <windows.h>
#include <time.h>

#endif //POSIX

//macros about fy2009 namespace declaration and using

#define DECL_FY_NAME_SPACE_BEGIN namespace fy2009{
#define DECL_FY_NAME_SPACE_END   } 

#define USING_FY_NAME_SPACE using namespace fy2009;

DECL_FY_NAME_SPACE_BEGIN

/*[tip] primitive typedefs
 *[desc] which indicates length of type. it makes it easy to calculate the serialized length
 *
 *[history]
 * passed test on win32 and linux,2006-5-23
 */
typedef char int8; //size is 8 bits
typedef unsigned char uint8; //size is 8 bits

typedef short int16; //size is 16 bits
typedef unsigned short uint16; //size is 16 bits

typedef long int32; //size is 32 bits 
typedef unsigned long uint32; //size is 32 bits

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

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_BASE_DREAMFREELANCER_20080322_H__
