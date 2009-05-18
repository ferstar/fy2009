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
                printf("got some slots(LINUX),cnt=%d,wait ret:%d\n",cnt,ret);
                bool exit_flag=false;
                for(int32 i=0;i<cnt;++i)
                {
                        printf("got signal slot(LINUX):%d\n",slot_vec[i]);
                        if(slot_vec[i] == 0) exit_flag=true;
                }
                if(exit_flag)
                {
                        printf("exit event slot thread(LINUX)\n");
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
        printf("before main thread sleep(LINUX)\n");
        usleep(1000000);
        printf("after main thread wake up(LINUX)\n");

        es.signal(1);
        usleep(1000);
        es.signal(2);
        usleep(1000);
        es.signal(0);
        es.signal(2);

        pthread_join(thd,0);
}

#define test_event_slot test_event_slot_linux

#elif defined(WIN32)

DWORD WINAPI tf_es(LPVOID pParam)
{
        event_slot_t *pes=(event_slot_t *)pParam;
        event_slot_t::slot_vec_t slot_vec;

        while(true)
        {
                int ret=pes->wait(slot_vec);
                int32 cnt=slot_vec.size();
                printf("got some slots(WIN),cnt=%d,wait ret:%d\n",cnt,ret);
                bool exit_flag=false;
                for(int32 i=0;i<cnt;++i)
                {
                        printf("got signal slot(WIN):%d\n",slot_vec[i]);
                        if(slot_vec[i] == 0) exit_flag=true;
                }
                if(exit_flag)
                {
                        printf("exit event slot thread(WIN)\n");
                        return 0;
                }
        }
        return 0;
}

//pass test,2009-5-12
void test_event_slot_win()
{
        event_slot_t es(4);
		HANDLE h_thd=::CreateThread(NULL, 0, tf_es, (void*)&es, 0, NULL);
        printf("before main thread sleep(WIN)\n");
        ::Sleep(1000);
        printf("after main thread wake up(WIN)\n");

        es.signal(1);
        ::Sleep(1);
        es.signal(2);
        ::Sleep(1);
        es.signal(0);
        es.signal(2);
		::Sleep(1000);
}

#define test_event_slot test_event_slot_win

#endif //POSIX

void test_string_builder(void)
{
        int8 i8=-10;
        uint8 ui8=10;
        int16 i16=-1024;
        uint16 ui16=1024;
        int32 i32=-1048576;
        uint32 ui32=1048576;
        float f32=3.1415926535;
        double f64=-314159265358979323.84626;

        int8 hello[]="hello everyone!";

        string_builder_t sb;
        sb<<string_builder_t::eUNCPY_STR<<hello<<"i8="<<i8<<";ui8="<<ui8<<";i16="<<i16<<";ui16="<<ui16
		<<";i32="<<i32<<";ui32="<<ui32<<";f32="<<f32<<";f64="<<f64;

        sb<<";pointer of f64 variable="<<(void *)&f64;
        bb_t bb;
        uint32 len=sb.build(bb);
        printf("%s\n",(int8*)bb);

        string_builder_t sb2;
        sb2<<"";
        printf("sb2****\n");
        sb2.build(bb);
        printf("%s\n",(int8*)bb);
}
#include <sstream>
void test_string_builder_performance()
{
        int8 i8=-10;
        uint8 ui8=10;
        int16 i16=-1024;
        uint16 ui16=1024;
        int32 i32=-1048576;
        uint32 ui32=1048576;
        float f32=3.1415926535;
        double f64=-314159265358979323.84626;

        int8 hello[]="hello everyone!";

	struct timeval tv1,tv2;
	::gettimeofday(&tv1, 0);
	for(int i=0;i<10000;++i)
	{

		//string builder performance
        	string_builder_t sb;
		sb.prealloc(256);
		sb<<string_builder_t::eUNCPY_STR<<hello<<"i8="<<i8<<";ui8="<<ui8<<";i16="<<i16<<";ui16="<<ui16
                	<<";i32="<<i32<<";ui32="<<ui32<<";f32="<<f32<<";f64="<<f64;
		bb_t bb;
        	sb.build(bb);

/*
		//sprintf perforamnce
		char buf[256];
		::sprintf(buf, "%si8=%d;ui8=%lu;i16=%d;ui16=%lu;i32=%d;ui32=%lu;f32=%f;f64=%f",
			hello, i8, ui8,i16,ui16,i32,ui32,f32,f64);
*/
/*
		//stringstream performance
		std::stringstream ss;
		ss<<hello<<"i8="<<i8<<";ui8="<<ui8<<";i16="<<i16<<";ui16="<<ui16
                        <<";i32="<<i32<<";ui32="<<ui32<<";f32="<<f32<<";f64="<<f64;
		const char *rst_str=(ss.str()).c_str();
*/
	}
	::gettimeofday(&tv2,0);
	printf("string builder takes time:%lu\n",timeval_util_t::diff_of_timeval_tc(tv1,tv2));
}

//pass test 2009-5-13
void test_internal_fy_trace_ex()
{
	__INTERNAL_FY_TRACE_EX("hello internal trace_ex:(int8)--"<<(int8)8<<",i16="<<(int16)16<<",i32="<<(int32)32<<"\n");
}

//test exception mechanism
//2008-3-19
class exp_test_t
{
public:
        void f1()
        {
                FY_DECL_EXCEPTION_CTX("exp_test_t","f1");
                FY_TRY

                FY_THROW("ef1s1","throw an exception");

                FY_CATCH_N_THROW_AGAIN("ef1s2","f1 pass",);
        }
        void f2()
        {
                FY_DECL_EXCEPTION_CTX("exp_test_t","f2");

                FY_TRY

                f1();

                FY_CATCH_N_THROW_AGAIN("ef2s1","f2 pass",);
        }
        void f3()
        {
                FY_DECL_EXCEPTION_CTX("exp_test_t","f3");

                FY_TRY

                f2();

                FY_CATCH_N_THROW_AGAIN("ef3s1","f3 pass",);
        }
};

class exp_test_ex_t : public object_id_impl_t
{
public:
        void f1()
        {
                FY_DECL_EXCEPTION_CTX_EX("f1");
                FY_TRY

                FY_THROW_EX("exf1s1","throw an exception");

                FY_CATCH_N_THROW_AGAIN_EX("exf1s2","pass",);
        }
        void f2()
        {
                FY_DECL_EXCEPTION_CTX_EX("f2");
                FY_TRY

                f1();

                FY_CATCH_N_THROW_AGAIN_EX("exf2s1","pass",);
        }
protected:
        void _lazy_init_object_id() throw(){ OID_DEF_IMP("exp_test_ex_t"); }
};

//pass test against both narrow and wide charater,2008-4-10
//pass re-test, 2009-5-13
void test_exception()
{
        exp_test_t et;
        FY_TRY
        et.f3();
        }catch(exception_t& e){
                bb_t tbb;
                e.to_string(tbb,true);
                printf("cc count:%d,exception desc:%s\n",e.get_cc_count(),(int8*)tbb);
        }
}
//pass test against both narrow and wide charater,2008-4-10
//pass re-test,2009-5-13
void test_exception_ex()
{
        exp_test_ex_t etx;
        FY_TRY
        etx.f2();
        }catch(exception_t& e){
                bb_t tbb;
                e.to_string(tbb,false);
                printf("cc count:%d,exception desc:%s\n",e.get_cc_count(),(int8*)tbb);
        }
}

//pass test against both narrow and wide charater,2008-4-10
//pass re-test,2009-5-13, define/undefine FY_ENABLE_ASSERT
void test_assert()
{
        FY_TRY

        FY_ASSERT(20>30);

        }catch(exception_t& e){
                bb_t tbb;
                e.to_string(tbb);
                printf("%s\n",(int8*)tbb);
        }
}

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
	//test_event_slot();
	//test_string_builder();
	//test_string_builder_performance();
	//test_internal_fy_trace_ex();
	//test_exception();
	test_exception_ex();
	//test_assert();

	__INTERNAL_FY_EXCEPTION_TERMINATOR(if(g_buf){printf("g_buf is deleted\n");delete [] g_buf;g_buf=0;});
	
	return 0;	
}
