/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@hotmail.com
 *
 * [History]
 * initialize: 2009-4-29
 * ====================================================================
 */     
#include "fyt.h"

USING_FY_NAME_SPACE

/*[tip] test bufffer_tt
 *[desc] test bb_t to verify buffer_t
 *[history]
 *pass LINUX 2.4 test, 2008-4-9
 *revise and pass LINUX2.4 re-test,2009-4-29
 *pass windows test, 2009-4-29
 */
void test_bb_t(argv_t& argv)
{
	if(argv.has_arg("-h"))
	{
		printf("====usage of test_bb_t=====\n");
		printf("objective: test bb_t\n");
		printf("arguments:\n");
		printf("a--attach,enable attach operation\n");
		printf("rh--relay a heap buffer\n");	
		return;
	}

        bb_t bb;
        printf("bb prepared buffer:%x,size:%d, on stack?%d\n",(int8*)bb,bb.get_size(), bb.is_on_stack());

        int8 cc[]="hello, good,kadifadfkekdsfeflkie";
        printf("cc pointer:%x,sizeof:%d\n",cc,sizeof(cc));

        printf("bb filled length after fill_from cc:%d\n",bb.fill_from(cc, strlen(cc)));

	if(argv.has_arg("a"))
	{
        	bb.attach(cc,sizeof(cc),true);
		printf("bb buffer(same with cc) after attach cc:%x,size:%d, on stack?%d\n",
			(int8*)bb,bb.get_size(), bb.is_on_stack());
	}
	else
        	printf("bb buffer(same with prepared buffer) after fill_from cc:%x,size:%d,on stack?%d\n",
			(int8*)bb,bb.get_size(), bb.is_on_stack());

        int8 cc1[]="morning,morning,";
        printf("cc1 pointer:%x\n",cc1);
        printf("bb filled length after fill_from cc1:%d\n",bb.fill_from(cc1,strlen(cc1)));
        printf("bb buffer after fill_from cc1:%x,size:%d, on stack?%d\n",(int8*)bb,bb.get_size(), bb.is_on_stack());

        int8 cc2[]="noon,noon,afasfasfasfa;fa;sfasfklas;fas;fakl;sdfajkl;sdflasfl;asdfl;asdl;fkl;!";
        printf("cc2 pointer:%x\n",cc2);
        printf("bb filled length after fill_from cc2:%d\n",bb.fill_from(cc2,strlen(cc2)+1));
        printf("bb buffer after fill_from cc2:%x,size:%d,on stack?%d\n",(int8*)bb,bb.get_size(),bb.is_on_stack());

        int8 cc3[]="piggy wiggy";
        printf("bb filled length after fill_from with specified position(6) cc3:%d\n",bb.fill_from(cc3,strlen(cc3)+1,6));

        printf("bb content:%s\n",(int8*)bb);

        int8 cstr[]="hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhho";
        bb_t bb2(cstr, strlen(cstr)+1, true, strlen(cstr));//, strlen(cstr));
        printf("bb2,origin:%x, filled_len:%d,content:%s\n",(int8*)bb2, bb2.get_filled_len(),(int8*)bb2);

	if(argv.has_arg("rh"))
	{
		bb2.fill_from(bb);
		printf("bb2 after fill_from:%x, filled_len:%d, on stack?:%d\n",
			(int8*)bb2, bb2.get_filled_len(), bb2.is_on_stack());
	}
	else
	{
        	bb2.copy_from(bb);
        	printf("bb2 after copy_from bb:%x, content:%s\n", (int8*)bb2, (int8*)bb2);
	}
        printf("bb2 filled length after fill_from bb(pos=2):%d\n", bb2.fill_from(bb,2));
        printf("bb2 content:%s\n",(int8*)bb2);
        printf("bb contect:%s\n",(int8*)bb);

        bb_t bb3;
        bb3.relay(bb2);
        printf("bb2 after being relayed:%x, filled_lend:%d, on stack?%d, content:%s\n",
		(int8*)bb2, bb2.get_filled_len(), bb2.is_on_stack(),(int8*)bb2); //it's null pointer,valgrind will report error
        printf("bb3 after relay:%x, filled_len:%d, on stack?%d, content:%s\n",
		(int8*)bb3,bb3.get_filled_len(), bb3.is_on_stack(),(int8*)bb3);

	bool on_stack_flag=bb3.is_on_stack();
        int8 *tmp_buf=bb3.detach();
        printf("tmp_buf:%s\n",tmp_buf);
        printf("bb3 after detach:%x, filled len:%d, on stack?%d\n",(int8*)bb3, bb3.get_filled_len(), bb3.is_on_stack());

        if(!on_stack_flag && tmp_buf) delete [] tmp_buf;
        tmp_buf=0;
}

//test user_clock_t,pass 2009-5-5
void test_user_clock()
{
	user_clock_t *usr_clk=user_clock_t::instance();
#ifdef POSIX
	sleep(1);
#elif defined(WIN32)
	Sleep(1);
#endif

	printf("user_clk tick:%lu, resolution:%lu\n", get_tick_count(usr_clk), get_tick_count_res(usr_clk));
}

#ifdef LINUX

void test_user_clock_performance_linux()
{
        user_clock_t *usr_clk=user_clock_t::instance();
        sleep(1);
        struct timeval tv1,tv2;
        int32 ll;
        ::gettimeofday(&tv1,0);
        for(int i=0;i<10000000; ++i)
        {
                ll=get_tick_count(usr_clk);
        }
        ::gettimeofday(&tv2,0);
		//in my test, it's 80ms, as efficient as a stub time function call--2009-5-6
        printf("elasped time(linux):%lu\n", timeval_util_t::diff_of_timeval_tc(tv1,tv2));
}

#define test_user_clock_performance test_user_clock_performance_linux

void test_localtime_performance_linux()
{
        struct tm var_tm;
        int32 ms;
        user_clock_t *usr_clk=user_clock_t::instance();
        sleep(1);
        struct timeval tv1,tv2;
        ::gettimeofday(&tv1,0);
        for(int i=0;i<10000000; ++i)
        {
		ms=get_local_time(&var_tm,usr_clk);
	/*
		printf("year:%d, mon:%d, day:%d, h:%d, min:%d, sec:%d, msec:%d\n",
			var_tm.tm_year, var_tm.tm_mon, var_tm.tm_mday,
			var_tm.tm_hour, var_tm.tm_min, var_tm.tm_sec, ms);
		return;
	*/
        }
        ::gettimeofday(&tv2,0);
        //in my test, it's 1084ms--it's more expensive than get_tick_count,
        //fortunately, it's seldom called in general,2009-5-6
        printf("elasped time(linux):%lu\n", timeval_util_t::diff_of_timeval_tc(tv1,tv2));
}

#define test_localtime_performance  test_localtime_performance_linux

#elif defined(WIN32)

void test_user_clock_performance_win()
{
        user_clock_t *usr_clk=user_clock_t::instance();
        long tc1,tc2;
        int32 ll;
        tc1=::GetTickCount();
        for(int i=0;i<10000000; ++i)
        {
                ll=get_tick_count(usr_clk);
        }
        tc2=::GetTickCount();
		//in my test, it's 125ms, as efficient as calling ::GetTickCount() direclty
		//2009-5-6
        printf("elasped time(win32):%lu\n", tc2 - tc1);
}

#define test_user_clock_performance test_user_clock_performance_win

void test_localtime_performance_win()
{
        struct tm var_tm;
        int32 ms;
        user_clock_t *usr_clk=user_clock_t::instance();
        long tc1,tc2;
        tc1=::GetTickCount();
        for(int i=0;i<10000000; ++i)
        {
                ms=get_local_time(&var_tm,usr_clk);
		/*
				printf("year:%d, mon:%d, day:%d, wday:%d, h:%d, min:%d, sec:%d, msec:%d\n",
					var_tm.tm_year, var_tm.tm_mon, var_tm.tm_mday, var_tm.tm_wday, 
					var_tm.tm_hour, var_tm.tm_min, var_tm.tm_sec, ms);
				return ;
		*/
        }
        tc2=::GetTickCount();
        printf("elasped time(win):%lu\n", tc2-tc1);
}

#define test_localtime_performance  test_localtime_performance_win


#endif

#ifdef POSIX

void *test_event_thd_fun(void * pParam)
{
        event_t *pe=(event_t*)pParam;
        int cnt=0;
        while(cnt<10)
        {
                if(pe->wait())
                        printf("wait event[%d] time-out(linux)\n",++cnt);
                else
                        printf("wait and got event[%d](linux)\n",++cnt);
        }
        return 0;
}

//pass test, 2009-5-12
void test_event_linux()
{
	event_t evt;
                
	pthread_t thd=0;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);	
	pthread_create(&thd, &attr, test_event_thd_fun, (void*)&evt);

        int cnt=0;
        while(cnt<10)
        {
                printf("signal[%d]\n",cnt);
                evt.signal();
                ::sleep(1);
                ++cnt;
        }
}

#define test_event test_event_linux

#elif defined(WIN32)

DWORD WINAPI test_event_thd_fun(LPVOID pParam)
{
	event_t *pe=(event_t*)pParam;
	int cnt=0;
	while(cnt<10)
	{
		if(pe->wait())
			printf("wait event[%d] time-out(win)\n",++cnt);
		else
			printf("wait and got event[%d](win)\n",++cnt);
	}
	return 0;
}
//pass test 2009-5-11
void test_event_win()
{
	event_t evt(true);
	HANDLE h_thd=::CreateThread(NULL, 0, test_event_thd_fun, (void*)&evt, 0, NULL);
	int cnt=0;
	while(cnt<10)
	{
		printf("signal[%d]\n",cnt);
		evt.signal();
		::Sleep(1000);
		++cnt;
	}
}

#define test_event test_event_win

#endif

#ifdef POSIX

//2008-4-18,passed test
//2009-5-12, passed re-test
void *tf_es(void *arg)
{
        event_slot_t *pes=(event_slot_t *)arg;
        event_slot_t::slot_vec_t slot_vec;

        while(true)
        {
                int ret=pes->wait(slot_vec);
                int32 cnt=slot_vec.size();
                printf("got some slots,cnt=%d,wait ret:%d\n",cnt,ret);
                bool exit_flag=false;
                for(int32 i=0;i<cnt;++i)
                {
                        printf("got signal slot:%d\n",slot_vec[i]);
                        if(slot_vec[i] == 0) exit_flag=true;
                }
                if(exit_flag)
                {
                        printf("exit event slot thread\n");
                        return 0;
                }
        }
        return 0;
}

void test_event_slot_linux()
{
        event_slot_t es(4);
        pthread_t thd;
        pthread_create(&thd, 0, tf_es, (void *)&es);
        printf("before main thread sleep\n");
        usleep(1000000);
        printf("after main thread wake up\n");

        es.signal(1);
        usleep(1000);
        es.signal(2);
        usleep(1000);
        es.signal(0);
        es.signal(2);

        pthread_join(thd,0);
}

#define test_event_slot test_event_slot_linux

#endif //POSIX

int main(int argc, char **argv)
{
	char *g_buf=0;

	FY_TRY
/*
	//test throw and catch
	g_buf = new char[10];
	throw 2;

	__INTERNAL_FY_EXCEPTION_TERMINATOR(if(g_buf){printf("g_buf is deleted\n");delete [] g_buf;g_buf=0;};return -1;);
	printf("exit normally\n");
	if(g_buf) delete [] g_buf;

	return 0;
*/
	argv_t fy_argv(argc, argv);
/*
	//test argv_t
	int fy_argc=fy_argv.get_size();
	for(int i=0;i<fy_argc;++i)
	{
		printf("arg[%d]=%s\n",i, fy_argv.get_arg(i).c_str());
	}
	if(fy_argv.has_arg(std::string("a2")))
		printf("has arg a2\n");
	else
		printf("hasn't arg a2\n");

	if(fy_argv.has_arg("a3"))
		printf("has arg a3\n");
	else
		printf("hasn't arg a3\n");
*/
	//test_bb_t(fy_argv);
	//test_user_clock();
	//test_user_clock_performance();
	//test_localtime_performance();
	//test_event();
	test_event_slot();

	__INTERNAL_FY_EXCEPTION_TERMINATOR(if(g_buf){printf("g_buf is deleted\n");delete [] g_buf;g_buf=0;});
	
	return 0;	
}
