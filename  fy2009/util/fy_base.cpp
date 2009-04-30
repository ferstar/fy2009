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

