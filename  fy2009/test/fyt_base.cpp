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
	test_bb_t(fy_argv);

	__INTERNAL_FY_EXCEPTION_TERMINATOR(if(g_buf){printf("g_buf is deleted\n");delete [] g_buf;g_buf=0;});
	
	return 0;	
}
