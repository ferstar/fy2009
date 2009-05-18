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
#include <stdio.h>
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


//event_slot_t
event_slot_t::event_slot_t(uint16 slot_count) : _cs(true),_evt(false)
{
        _slot_cnt = slot_count;
        _slot = new int8[ _slot_cnt ];
        memset(_slot, 0, _slot_cnt);
}

event_slot_t::~event_slot_t()
{
        if(_slot) delete [] _slot;
}

void event_slot_t::signal(uint16 slot_index)
{
        if(slot_index >= _slot_cnt)
        {
                int8 sz_tmp[256];
                ::sprintf(sz_tmp,"event_slot_t::signal,invalid slot_index:%d, _slot_cnt:%d\r\n", slot_index, _slot_cnt);
                __INTERNAL_FY_TRACE(sz_tmp);
                return;
        }

        if(_slot[slot_index]) return; 

        smart_lock_t slock(&_cs);

        _slot[slot_index] = 1;
	
	slock.unlock();

	_evt.signal(); //free wait thread
}

int32 event_slot_t::wait(slot_vec_t& signalled_vec, uint32 ms_timeout)
{
        signalled_vec.clear();

        smart_lock_t slock(&_cs);

        for(uint16 i=0; i<_slot_cnt; ++i)
        {
                if(_slot[i]) signalled_vec.push_back(i);
        }
        if(!signalled_vec.empty())
        {
                memset(_slot, 0, _slot_cnt);//reset slots
                return signalled_vec.size(); //only wait on demand
        }
	
	slock.unlock();

	if(!_evt.wait(ms_timeout)) //not time-out
        {
		smart_lock_t slock(&_cs);

                for(uint16 i=0; i<_slot_cnt; ++i)
                {
                        if(_slot[i]) signalled_vec.push_back(i);
                }
                memset(_slot, 0, _slot_cnt);//reset slots
		
		return signalled_vec.size();
        }
        return 0; 
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

//string_util_t
bool string_util_t::_s_is_ws(int8 c)
{
        switch(c)
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
                return true;
        default:
                return false;
        }
}

const int8 *string_util_t::s_trim_inplace(const int8 *str,uint32 *p_len)
{
        if(!str)
        {
                if(p_len) *p_len =0;
                return str;
        }

        //trim prefix white space
        const int8 *p_s=str;
        while(_s_is_ws(*p_s)) { ++p_s; }

        //trim surfix white space
        const int8 *p_e=str+strlen(str) - 1;

        if(p_e < p_s)//pure white spaces string
        {
                if(p_len) *p_len=0;
                return p_s;
        }

        while(_s_is_ws(*p_e)) { --p_e; }

        if(p_len) *p_len= p_e - p_s + 1;

        return p_s;
}

//string_builder_t
string_builder_t::string_builder_t()
{
        _piece_v=0;
        _buf_n=0;
        _len_v=_cur_pos=_len_total=0;
        _len_buf_n=_cur_pos_buf_n=0;
        _cpy_str_flag=eCPY_STR;
}

string_builder_t::~string_builder_t()
{
        reset();
        if(_piece_v) delete [] _piece_v;
        if(_buf_n) delete [] _buf_n;
}

void string_builder_t::reset(bool bShrinkage)
{
        if(!_cur_pos) return;//empty object
        //free string pieces with need_free=true
        for(uint32 i=0;i<_cur_pos;i++)
        {
                if(!(_piece_v[i].need_free)) continue;
                delete [] const_cast<char*>(_piece_v[i].str);
                _piece_v[i].need_free=false;
        }
        if(bShrinkage)
        {
                 delete [] _piece_v;
                 _piece_v=0;
                 _len_v=0;
        }
        _cur_pos=_len_total=0;//reset
        _cur_pos_buf_n=0;
}

const uint32 STR_LEN_I8=4;
string_builder_t& string_builder_t::operator <<(int8 data)
{
        int8 *str_data;
        bool need_free;
        if(_len_buf_n - _cur_pos_buf_n>=STR_LEN_I8)
        {
                str_data=_buf_n+_cur_pos_buf_n;
                need_free=false;
                _cur_pos_buf_n+=STR_LEN_I8;
        }
        else
        {
                str_data=new int8[STR_LEN_I8];
                need_free=true;
        }
        ::sprintf(str_data,"%d",data);
        _append_str(str_data,need_free);

        return *this;
}

string_builder_t& string_builder_t::operator <<(uint8 data)
{
        int8 *str_data;
        bool need_free;
        if(_len_buf_n - _cur_pos_buf_n>=STR_LEN_I8)
        {
                str_data=_buf_n+_cur_pos_buf_n;
                need_free=false;
                _cur_pos_buf_n+=STR_LEN_I8;
        }
        else
        {
                str_data=new int8[STR_LEN_I8];
                need_free=true;
        }
        ::sprintf(str_data,"%u",data);
        _append_str(str_data,need_free);

        return *this;
}

const uint32 STR_LEN_I16=6;
string_builder_t& string_builder_t::operator <<(int16 data)
{
        int8 *str_data;
        bool need_free;
        if(_len_buf_n - _cur_pos_buf_n>=STR_LEN_I16)
        {
                str_data=_buf_n+_cur_pos_buf_n;
                need_free=false;
                _cur_pos_buf_n+=STR_LEN_I16;
        }
        else
        {
                str_data=new int8[STR_LEN_I16];
                need_free=true;
        }
        ::sprintf(str_data, "%d",data);
        _append_str(str_data,need_free);

        return *this;
}

string_builder_t& string_builder_t::operator <<(uint16 data)
{
        int8 *str_data;
        bool need_free;
        if(_len_buf_n - _cur_pos_buf_n>=STR_LEN_I16)
        {
                str_data=_buf_n+_cur_pos_buf_n;
                need_free=false;
                _cur_pos_buf_n+=STR_LEN_I16;
        }
        else
        {
                str_data=new int8[STR_LEN_I16];
                need_free=true;
        }
        ::sprintf(str_data,"%u",data);
        _append_str(str_data,need_free);

        return *this;
}

const uint32 STR_LEN_I32=12;
string_builder_t& string_builder_t::operator <<(int32 data)
{
        int8 *str_data;
        bool need_free;
        if(_len_buf_n - _cur_pos_buf_n>=STR_LEN_I32)
        {
                str_data=_buf_n+_cur_pos_buf_n;
                need_free=false;
                _cur_pos_buf_n+=STR_LEN_I32;
        }
        else
        {
                str_data=new int8[STR_LEN_I32];
                need_free=true;
        }
        ::sprintf(str_data,"%ld",data);
        _append_str(str_data,need_free);

        return *this;
}

string_builder_t& string_builder_t::operator <<(uint32 data)
{
        int8 *str_data;
        bool need_free;
        if(_len_buf_n - _cur_pos_buf_n>=STR_LEN_I32)
        {
                str_data=_buf_n+_cur_pos_buf_n;
                need_free=false;
                _cur_pos_buf_n+=STR_LEN_I32;
        }
        else
        {
                str_data=new int8[STR_LEN_I32];
                need_free=true;
        }
        ::sprintf(str_data,"%lu",data);
        _append_str(str_data,need_free);

        return *this;
}

//min:1.17549435e-38F; max:3.40282347e+38F--ref:/usr/include/c++/3.2/limits
const uint32 STR_LEN_F32=16;
string_builder_t& string_builder_t::operator <<(float data)
{
        int8 *str_data;
        bool need_free;
        if(_len_buf_n - _cur_pos_buf_n>=STR_LEN_F32)
        {
                str_data=_buf_n+_cur_pos_buf_n;
                need_free=false;
                _cur_pos_buf_n+=STR_LEN_F32;
        }
        else
        {
                str_data=new int8[STR_LEN_F32];
                need_free=true;
        }
        ::sprintf(str_data,"%g",data);
        _append_str(str_data,need_free);

        return *this;
}

//min:2.2250738585072014e-308; max:1.7976931348623157e+308
const uint32 STR_LEN_F64=24;
string_builder_t& string_builder_t::operator <<(double data)
{
        int8 *str_data;
        bool need_free;
        if(_len_buf_n - _cur_pos_buf_n>=STR_LEN_F64)
        {
                str_data=_buf_n+_cur_pos_buf_n;
                need_free=false;
                _cur_pos_buf_n+=STR_LEN_F64;
        }
        else
        {
                str_data=new int8[STR_LEN_F64];
                need_free=true;
        }
        ::sprintf(str_data,"%g",data);
        _append_str(str_data,need_free);

        return *this;
}

//the following 3 is 2("0x")+1('\0')
const uint32 STR_LEN_POINTER=sizeof(int)*2+3;
string_builder_t& string_builder_t::operator <<(const void *data)
{
        int8 *str_data;
        bool need_free;
        if(_len_buf_n - _cur_pos_buf_n>=STR_LEN_POINTER)
        {
                str_data=_buf_n+_cur_pos_buf_n;
                need_free=false;
                _cur_pos_buf_n+=STR_LEN_POINTER;
        }
        else
        {
                str_data=new int8[STR_LEN_POINTER];
                need_free=true;
        }
        ::sprintf(str_data,"0x%lx",(uint32)data);
        _append_str(str_data,need_free);

        return *this;
}

string_builder_t& string_builder_t::operator <<(const int8* cstr)
{
        if(!cstr) return *this;

        if(_cpy_str_flag == eCPY_STR)
        {
                int8 *str_data;
                uint32 para_len=::strlen(cstr)+1;
                bool need_free;
                if(_len_buf_n - _cur_pos_buf_n>=para_len)
                {
                        str_data=_buf_n+_cur_pos_buf_n;
                        need_free=false;
                        _cur_pos_buf_n+=para_len;
                }
                else
                {
                        str_data=new int8[para_len];
                        need_free=true;
                }
                memcpy((int8*)str_data,(const int8*)(cstr),para_len);
                _append_str(str_data,need_free);
        }
        else
        {
                _append_str(cstr,false);
        }
        return *this;
}

string_builder_t& string_builder_t::operator <<(const cpy_str_e& cpy_str_flag)
{
        _cpy_str_flag=cpy_str_flag;

        return *this;
}

uint32 string_builder_t::build(bb_t& bb)
{
        if(!_len_total)
        {
                bb.fill_from("",1,0);
                bb.set_filled_len(0);
                return 0;
        }
        bb.reserve(_len_total+1);
        uint32 cur_offset=0;
        for(uint32 i=0;i<_cur_pos;i++)
        {
                cur_offset=bb.fill_from(_piece_v[i].str,_piece_v[i].len);
        }
        int8 *bb_buf=(int8*)bb;
        bb_buf[cur_offset]=0;

        return _len_total;
}

void string_builder_t::_append_str(const int8 *str,bool bNeedFree)
{
        if(!str) return;
        if(_cur_pos==_len_v) //_piece_v is full,expand _piece_v
        {
                uint32 new_len=(_len_v?_len_v<<1:2);
                _piece_t*pp_v=new _piece_t[new_len];
                if(_len_v) memcpy(pp_v,_piece_v,_len_v*sizeof(_piece_t));//copy old _piece_v to new buffer
                //free old _piece_v
                if(_piece_v) delete [] _piece_v;
                _piece_v=pp_v;
                _len_v=new_len;
        }

        _piece_v[_cur_pos++]=_piece_t(str,bNeedFree);
        _len_total+=_piece_v[_cur_pos - 1].len;
}

void string_builder_t::prealloc(uint32 alloc_size)
{
        if(_cur_pos_buf_n) return;
        if(_buf_n) delete [] _buf_n;//free old buffer
        _buf_n=0;
        _len_buf_n=0;

        if(alloc_size)//allocate new buffer
        {
                _len_buf_n=alloc_size;
                _buf_n=new int8[_len_buf_n];
        }
}

void  string_builder_t::prealloc_n(uint32 count_n)
{
        prealloc(count_n*STR_LEN_F64);
}

//__exception_body_t::_exp_row_t
__exception_body_t::_exp_row_t::_exp_row_t() throw(): m_line_num(0){}

__exception_body_t::_exp_row_t::_exp_row_t(const int8 *src_file, int32 line_num,
                const int8 *object_id, const int8 *function_name, const int8 *spot, const int8 *desc) throw()
{
        m_src_file.fill_from(src_file, ::strlen(src_file)+1, 0);
        m_line_num=line_num;
        m_obj_id.fill_from(object_id, ::strlen(object_id)+1, 0);
        m_fun.fill_from(function_name, ::strlen(function_name)+1, 0);
        m_spot.fill_from(spot, ::strlen(spot)+1, 0);
        m_desc.fill_from(desc, ::strlen(desc)+1, 0);
}

__exception_body_t::_exp_row_t::_exp_row_t(const _exp_row_t& er) throw()
{
        m_src_file.copy_from(er.m_src_file);
        m_line_num=er.m_line_num;
        m_obj_id.copy_from(er.m_obj_id);
        m_fun.copy_from(er.m_fun);
        m_spot.copy_from(er.m_spot);
        m_desc.copy_from(er.m_desc);
}

//__exception_body_t
__exception_body_t::__exception_body_t(const int8 *src_file, int32 line_num,
                const int8 *object_id, const int8 *function_name, const int8 *spot, const int8 *desc) throw()
{
        FY_TRY

        _ref_cnt=0;
        _exp_vec.push_back(_exp_row_t(src_file, line_num, object_id, function_name, spot, desc));

        __INTERNAL_FY_EXCEPTION_TERMINATOR()
}

void __exception_body_t::push_back(const int8 *src_file, int32 line_num,
                const int8 *object_id, const int8 *function_name, const int8 *spot, const int8 *desc) throw()
{
        FY_TRY

        _exp_vec.push_back(_exp_row_t(src_file, line_num, object_id, function_name, spot, desc));

        __INTERNAL_FY_EXCEPTION_TERMINATOR()
}

void __exception_body_t::to_string(bb_t& bb, bool verbose_flag) const throw()
{
        FY_TRY

        int32 max_row_index=_exp_vec.size()-1;
        if(max_row_index < 0)
        {
                bb.set_filled_len(0);
                return;
        }

        string_builder_t sb_desc;
        for(int i=max_row_index; i>=0; --i)
        {
                if(verbose_flag)
                {
                        sb_desc << (i == max_row_index? "<src=" : "\r\n<src=") \
                                << _exp_vec[i].m_src_file.get_buf() << "><ln=" << _exp_vec[i].m_line_num \
                                << "><oid=" << _exp_vec[i].m_obj_id.get_buf() << "><fn=" \
                                << _exp_vec[i].m_fun.get_buf() << "><spt=" << _exp_vec[i].m_spot.get_buf() \
                                << ">--" << _exp_vec[i].m_desc.get_buf();
                }
                else if(i != max_row_index)//non-topmost row
                {
                        sb_desc << "." << _exp_vec[i].m_spot.get_buf();
                }
                else //topmost row
                {
                        sb_desc<< _exp_vec[i].m_spot.get_buf();
                }
        }

        sb_desc.build(bb);

        __INTERNAL_FY_EXCEPTION_TERMINATOR()
}

bool __exception_body_t::iterate(int32 step_index, bb_t& source_file_ref, int32& line_num_ref,
                        bb_t& object_id_ref, bb_t& function_name_ref, bb_t& spot_ref, bb_t& desc_ref) throw()
{
        FY_TRY

        if(step_index >= _exp_vec.size() ||
                step_index < 0) return false;

        source_file_ref.copy_from(_exp_vec[step_index].m_src_file);
        line_num_ref = _exp_vec[step_index].m_line_num;
        object_id_ref.copy_from(_exp_vec[step_index].m_obj_id);
        function_name_ref.copy_from(_exp_vec[step_index].m_fun);
        spot_ref.copy_from(_exp_vec[step_index].m_spot);
        desc_ref.copy_from(_exp_vec[step_index].m_desc);

        return true;

        __INTERNAL_FY_EXCEPTION_TERMINATOR()

        return false;
}

void __exception_body_t::_add_reference()
{
        ++_ref_cnt;
}

void __exception_body_t::_release_reference()
{
        if(--_ref_cnt == 0) delete this;
}

//exception_t
exception_t::exception_t() throw()
{
        FY_TRY

        _cc_cnt=0;
        _exp_body=new __exception_body_t();
        _exp_body->_add_reference();

        __INTERNAL_FY_EXCEPTION_TERMINATOR();
}

exception_t::exception_t(const int8 *src_file, int32 line_num, const int8 *object_id, const int8 *function_name,
        const int8 *spot, const int8 *desc) throw()
{
       FY_TRY

       _cc_cnt=0;

       _exp_body=new __exception_body_t(src_file, line_num, object_id, function_name, spot, desc);
       _exp_body->_add_reference();

       __INTERNAL_FY_EXCEPTION_TERMINATOR();
}

exception_t::exception_t(const exception_t& e) throw()
{
        _cc_cnt = e._cc_cnt + 1;
        _exp_body=e._exp_body;
        if(_exp_body) _exp_body->_add_reference();
}

exception_t::~exception_t()
{
        if(_exp_body) _exp_body->_release_reference();
}

//ref_cnt_impl_t
ref_cnt_impl_t::ref_cnt_impl_t() throw() : _ref_cnt(0), _p_cs(0){}

ref_cnt_impl_t::ref_cnt_impl_t(critical_section_t *cs) throw() : _ref_cnt(0), _p_cs(cs){}

void ref_cnt_impl_t::set_lock(critical_section_t *cs) throw()
{
        _p_cs=cs;
}

void *ref_cnt_impl_t::lookup(uint32 iid) throw()
{
        switch(iid)
        {
        case IID_self:
        case IID_lookup:
                return this;

        case IID_ref_cnt:
                return static_cast<ref_cnt_it *>(this);

        default:
                return 0;
        }

        return 0;
}

void ref_cnt_impl_t::add_reference()
{
        if(_p_cs)//thread-safe
        {
             _p_cs->lock();
             ++_ref_cnt;
             _p_cs->unlock();
        }
        else
             ++_ref_cnt;
}

void ref_cnt_impl_t::release_reference()
{
        if(_p_cs) //thread-safe
        {
                bool del_flag=false;

                _p_cs->lock();

                del_flag =(--_ref_cnt == 0);

                _p_cs->unlock();

                if(del_flag) delete this;
        }
        else
                if(--_ref_cnt == 0) delete this;
}

//prototype_manager_t
prototype_manager_t *prototype_manager_t::_s_inst=0;//initialize static member
critical_section_t prototype_manager_t::_s_cs=critical_section_t();

prototype_manager_t *prototype_manager_t::instance() throw()
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
        prototype_manager_t *tmp_inst=new prototype_manager_t();
        //initializing sequence
        _s_inst = tmp_inst; //it must be last statement to makse sure thread-safe,2007-3-5
        _s_cs.unlock();

        __INTERNAL_FY_EXCEPTION_TERMINATOR()

        return _s_inst;
}

prototype_manager_t::prototype_manager_t() throw()
{
        _state=PTM_STATE_NULL;
        _capacity=0;
}

int prototype_manager_t::begin_register() throw()
{
        smart_lock_t slock(&_s_cs);

        if(_state != PTM_STATE_NULL) return PTM_RET_WRONGSTATE; //has finished or in progress registeration

        _state = PTM_STATE_OPENING;

        return PTM_RET_SUCCESS;
}

int prototype_manager_t::reg_prototype(uint32 ptid, clone_it *pt)
{
        smart_lock_t slock(&_s_cs);

        if(PTM_STATE_OPENING != _state) return PTM_RET_WRONGSTATE; //not in opening state
        if(!pt) return PTM_RET_WRONGPARAM;

        if(ptid > MAX_PROTOTYPE_ID) return PTM_RET_IDOVERFLOW;

        if(ptid > _capacity)
        {
                _capacity = ptid + 10;
                //new item will implicitly be intialized with zero
                _pt_vec.resize(_capacity);
        }
        if(!_pt_vec[ptid].is_null()) return PTM_RET_EXISTED;

        sp_lookup_t sp_cln=pt->clone(true);//clone a prototype to make prototype lifecycle independent from para pt
        if(sp_cln.is_null()) return PTM_RET_FAILCLONE;

        _pt_vec[ptid]=SP_LU_CAST(clone_it, IID_clone, sp_cln);

        return PTM_RET_SUCCESS;
}

int prototype_manager_t::end_register() throw()
{
        smart_lock_t slock(&_s_cs);

        if(PTM_STATE_OPENING != _state) return PTM_RET_WRONGSTATE; //not in opening state

        _state = PTM_STATE_OPEN;

        return PTM_RET_SUCCESS;
}

sp_clone_t prototype_manager_t::get_prototype(uint32 ptid)
{
        if(PTM_STATE_OPEN != _state) return sp_clone_t();

        if(ptid >= _capacity) return sp_clone_t();

        if(_pt_vec[ptid].is_null()) return sp_clone_t();

        sp_lookup_t sp_cln=_pt_vec[ptid]->clone(true);//clone a prototype object
        if(sp_cln.is_null()) return sp_clone_t();

        return SP_LU_CAST(clone_it, IID_clone, sp_cln);
}

