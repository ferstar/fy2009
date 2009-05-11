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
//to enable TryEnterCriticalSection definition
#ifdef WIN32
#	ifndef _WIN32_WINNT
#	define _WIN32_WINNT 0x0400
#	endif
#endif

#include "fy_base.h"

USING_FY_NAME_SPACE

//critical_section_t
critical_section_t::critical_section_t(bool recursive_flag) throw()
{
#ifdef POSIX

        pthread_mutexattr_t mtx_attr;
        pthread_mutexattr_init(&mtx_attr);
        if(recursive_flag) pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&_mtx, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);

#elif defined(WIN32)

	InitializeCriticalSection(&_cs);

#endif //POSIX
}

critical_section_t::~critical_section_t() throw()
{
#ifdef POSIX

        pthread_mutex_destroy(&_mtx);

#elif defined(WIN32)

	DeleteCriticalSection(&_cs);

#endif //POSIX
}

void critical_section_t::lock() throw()
{
#ifdef POSIX
	
	pthread_mutex_lock(&_mtx);

#elif defined(WIN32)

	EnterCriticalSection(&_cs);

#endif	
}

void critical_section_t::unlock() throw() 
{ 
#ifdef POSIX

	pthread_mutex_unlock(&_mtx); 

#elif defined(WIN32)

	LeaveCriticalSection(&_cs);

#endif
}

bool critical_section_t::try_lock() throw() 
{ 
#ifdef POSIX

	return pthread_mutex_trylock(&_mtx)==0; 

#elif defined(WIN32)

	return TryEnterCriticalSection(&_cs)!=0;

#endif
}

//event_t
event_t::event_t(bool initial_signalled)
{
#ifdef POSIX

        pthread_cond_init(&_cnd,0);
        _signalled=initial_signalled;

#elif defined(WIN32)

	//it's resetted automatically
	_he = ::CreateEvent(NULL, FALSE, initial_signalled, NULL);
	
#endif //POSIX
}

event_t::~event_t()
{
#ifdef POSIX

        pthread_cond_destroy(&_cnd);

#elif defined(WIN32)

	if(_he)
	{
		::CloseHandle(_he);
		_he = 0;
	}
#endif
}

void event_t::signal()
{
#ifdef POSIX

        if(_signalled) return; //only lock on demand

        smart_lock_t slock(&_cs);

        pthread_cond_signal(&_cnd);
        _signalled=true;

#elif defined(WIN32)

	 ::SetEvent(_he);

#endif
}

int32 event_t::wait(uint32 ms_timeout)
{
        int32 ret=0;

#ifdef POSIX

        smart_lock_t slock(&_cs);

        if(_signalled)//current state is signalled
        {
                _signalled=false;
                return ret; //only lock on demand
        }

        if(EVENT_WAIT_INFINITE == ms_timeout)
        {
                pthread_cond_wait(&_cnd,&(_cs.get_mutex()));
                _signalled=false;
        }
        else
        {
                struct timeval ct;
                struct timespec abstime;

                gettimeofday(&ct,0);
                uint32 dest_us = ct.tv_usec + ms_timeout*1000;
                abstime.tv_sec = ct.tv_sec + dest_us/1000000;
                abstime.tv_nsec = (dest_us % 1000000)*1000;

                ret = pthread_cond_timedwait(&_cnd, &(_cs.get_mutex()), &abstime);
		if(!ret) _signalled=false;
        }

#elif defined(WIN32)

	if (::WaitForSingleObject(_he, ms_timeout) == WAIT_OBJECT_0)
		return 0;

	else //time out
		return -1;

#endif //POSIX

        return ret;
}

//tc_util_t
bool tc_util_t::is_over_tc_end(uint32 tc_start, uint32 tc_deta, uint32 tc_cur) throw()
{
        uint32 tc_end=tc_start + tc_deta;

        //none of tc_end and tc_start overflow
        if(tc_end >= tc_start)
        {
                if(tc_cur > tc_end ||
                   tc_cur < tc_start) return true;//tc_cur<tc_start means tc_cur overflow
        }
        else //tc_end overflow
        {
                if(tc_cur < tc_start && tc_cur > tc_end) return true;
        }

        return false;
}

uint32 tc_util_t::diff_of_tc(uint32 tc_start, uint32 tc_end) throw()
{   
        if(tc_end >= tc_start) return tc_end - tc_start; 
        //tc_end overflow
        return 0xffffffff - tc_start + tc_end;
}

#ifdef POSIX

//timeval_util_t
struct timeval timeval_util_t::diff_of_timeval(const struct timeval& tv1,const struct timeval& tv2) throw()
{
      struct timeval tvDiff;
      tvDiff.tv_sec=tv2.tv_sec-tv1.tv_sec;
      tvDiff.tv_usec=tv2.tv_usec-tv1.tv_usec;

      //tv_sec and tv_usec must have same sign
      if(tvDiff.tv_sec>0 && tvDiff.tv_usec<0)
      {
        tvDiff.tv_sec--;
        tvDiff.tv_usec+=1000000;
      }
      else if(tvDiff.tv_sec<0 && tvDiff.tv_usec>0)
      {
        tvDiff.tv_sec++;
        tvDiff.tv_usec-=1000000;
      }

      return tvDiff;
}

int32 timeval_util_t::diff_of_timeval_tc(const struct timeval& tv1,const struct timeval& tv2) throw()
{
      struct timeval tvDiff=diff_of_timeval(tv1,tv2);
      return tvDiff.tv_sec*1000+tvDiff.tv_usec/1000;
}

//user_clock_t
user_clock_t *user_clock_t::_s_inst=0;//initialize static member
critical_section_t user_clock_t::_s_cs=critical_section_t();

user_clock_t *user_clock_t::instance() throw()
{
	FY_TRY

        if(_s_inst)
        {
                return _s_inst;
        }
        _s_cs.lock();
        if(_s_inst)
        {
                _s_cs.unlock();
                return _s_inst;
        }
        user_clock_t *tmp_inst=new user_clock_t();
        tmp_inst->_start();

        _s_inst = tmp_inst; //it must be last statement to makse sure thread-safe,2007-3-5
        _s_cs.unlock();

	__INTERNAL_FY_EXCEPTION_TERMINATOR()

        return _s_inst;
}

int8 user_clock_t::_compare_timeval(const struct timeval& tv1,const struct timeval& tv2) throw()
{
        if(tv1.tv_sec>tv2.tv_sec)
        {
                return 1;
        }
        else if(tv1.tv_sec<tv2.tv_sec)
        {
                return -1;
        }
        else //tv1.tv_sec==tv2.tv_sec
        {
                if(tv1.tv_usec>tv2.tv_usec)
                        return 1;
                else if(tv1.tv_usec<tv2.tv_usec)
                        return -1;
                else //tv1.tv_usec==tv2.tv_usec
                        return 0;
        }
}

void *user_clock_t::_thd_f(void *arg) throw()
{       
	user_clock_t * pClk=(user_clock_t *)arg;
	const uint32 cp_len=100; 
	bool bCheckPoint=false;
	bool bStart=true;
	pClk->_running_flag=true;//service is running
	
	FY_TRY

	while(true)
	{
			//check point
			if(pClk->_tick % cp_len == 0)
			{
					bCheckPoint=true;
					//move current check point to last check point
					memcpy((void*)&(pClk->_l_cp),(void*)&(pClk->_c_cp),sizeof(struct timeval));
					gettimeofday(&(pClk->_c_cp),0);//get current check point
					int8 ret=_compare_timeval(pClk->_c_cp,pClk->_local_time);
					//adjust _local_time but avoid it go back
					pClk->_lt_ahead_cp=0;
					if(ret>0)//_c_cp>_local_time
					{
							memcpy((void*)&(pClk->_local_time),(void*)&(pClk->_c_cp),sizeof(struct timeval));
							if(bStart)
							{
									bStart=false;
									pClk->_cs.unlock(); //this service is ready for get_localtime,2007-3-5
							}       
					}
					else if(ret<0)//_c_cp<_local_time,don't adjust _local_time to _c_cp
					{
							pClk->_lt_ahead_cp=timeval_util_t::diff_of_timeval_tc(
									pClk->_c_cp,pClk->_local_time);
					}
					if(pClk->_l_cp.tv_sec!=0 || pClk->_l_cp.tv_usec!=0)//adjust usleep para value to keep tick interval
					{
							struct timeval tv_d=timeval_util_t::diff_of_timeval(
									pClk->_l_cp,pClk->_c_cp);
							uint32 us_per=(tv_d.tv_sec*1000000+tv_d.tv_usec)/cp_len;
							int32 deta=UCLK_PRECISION*1000 - us_per;
							pClk->_sleep_para+=deta;
							pClk->_resolution=(us_per+500)/1000;//record current resolution(unit:ms--omit 4 increase 5)
					}
			}
			else
			{
					bCheckPoint=false;
			}
			if(pClk->_stop_flag) break;
			usleep(pClk->_sleep_para);
			pClk->_tick++;
			if(!bCheckPoint)//increase _local_time
			{
					uint32 ms_inc;
					if(pClk->_lt_ahead_cp>UCLK_PRECISION)
					{
							pClk->_lt_ahead_cp -=UCLK_PRECISION;
							ms_inc=0;
					}
					else if(pClk->_lt_ahead_cp>0)
					{
							ms_inc=UCLK_PRECISION - pClk->_lt_ahead_cp;
							pClk->_lt_ahead_cp=0;
					}
					else
					{
							ms_inc=UCLK_PRECISION;
					}
					pClk->_local_time.tv_usec+=ms_inc*1000;
					if(pClk->_local_time.tv_usec>=1000000)//normalize
					{
							pClk->_local_time.tv_sec+=1;
							pClk->_local_time.tv_usec -=1000000;
					}
			}//if(!bCheckPoint)
	}//while(true)

	__INTERNAL_FY_EXCEPTION_TERMINATOR()

	pClk->_running_flag=false;

	return 0;
}

void user_clock_t::_start() throw()
{           
	FY_TRY
    
	if(_thd) return;
	_cs.lock();     
	if(pthread_create(&_thd,0,_thd_f,(void *)this)) //thread will unlock _cs when ready
	{       
			_cs.unlock(); //create thread error
			return;
	}       
	//UserClock service is ready for get_localtime,2007-3-5
	_cs.lock();     
	_cs.unlock();  

	__INTERNAL_FY_EXCEPTION_TERMINATOR() 
}                               
                                
void user_clock_t::_stop() throw()
{
	FY_TRY
                       
	if(!_thd) return;
	_stop_flag=true;//notify thread stop
	pthread_join(_thd,0);   
	_thd=0;

	__INTERNAL_FY_EXCEPTION_TERMINATOR()         
}                       
                        
user_clock_t::user_clock_t() throw() 
{ 
	FY_TRY
                      
	_tick=0;        
	_resolution=10;//fix get_resolution return zero initially,2007-3-16
	_thd=0;         
	_sleep_para=UCLK_PRECISION*1000 - 4000;//4000 as initial para adjustment is determined by special test
	memset((void*)&_l_cp,0,sizeof(struct timeval));
	memset((void*)&_c_cp,0,sizeof(struct timeval));
	memset((void*)&_local_time,0,sizeof(struct timeval));
	memset((void*)&_tm_ltb,0,sizeof(struct tm));
	_lt_ahead_cp=0;
	_stop_flag=false;
	_running_flag=false;

	__INTERNAL_FY_EXCEPTION_TERMINATOR()
}

user_clock_t::~user_clock_t() throw()
{
        _stop();
}

uint32 user_clock_t::get_localtime(struct tm *lt) throw()
{
	FY_TRY

	if(!lt) return 0;
	if(_tm_ltb.tm_year==0)//first call this function
	{
			_s_ltb=_local_time.tv_sec;
			localtime_r(&(_local_time.tv_sec),&_tm_ltb);
			_tm_ltb.tm_year+=1900;
			++_tm_ltb.tm_mon;
			memcpy(lt,(void*)&_tm_ltb,sizeof(struct tm));
	}
	else //not first call
	{
			uint32 deta_s=_local_time.tv_sec - _s_ltb;
			if(_tm_ltb.tm_sec + deta_s > 59)//more than 1min from last call localtime_r will recall it
			{
					_s_ltb=_local_time.tv_sec; 
					localtime_r(&(_local_time.tv_sec),&_tm_ltb);
					_tm_ltb.tm_year+=1900;
					++_tm_ltb.tm_mon;
					memcpy(lt,(void*)&_tm_ltb,sizeof(struct tm)); 
			}
			else// little than 1min from last call localtime_r
			{
					memcpy(lt,(void*)&_tm_ltb,sizeof(struct tm));
					lt->tm_sec+=deta_s;
			}
	}

	__INTERNAL_FY_EXCEPTION_TERMINATOR()

	return _local_time.tv_usec/1000;
}

#endif //POSIX
