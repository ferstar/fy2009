/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@hotmail.com
 *
 * [History]
 * initialize: 2009-5-21
 * ====================================================================
 */     
#include "fyt.h"
#include "fy_stream.h"
#include <netinet/in.h>
#include <arpa/inet.h>

USING_FY_NAME_SPACE

//test network order to host order converting
void test_ns()
{
        int16 ns=0;
        for(int16 s=-32768;;s++)
        {
                stream_adaptor_t::s_reverse_bytes((uint8*)&s, (uint8*)&ns, sizeof(s));
                if(ns != (int16)htons(s)) printf("s:%d, ns:%d != htons:%d\n",s, ns, htons(s));
                if(s == 32767) break;
        }
        printf("all ns transfermation is ok\n");

        int32 ni=0;
        for(int32 i=0x80000000;;i++)
        {
                stream_adaptor_t::s_reverse_bytes((uint8*)&i, (uint8*)&ni, sizeof(i));
                if(ni != (int32)htonl(i)) printf("i:%d, ni:%d != htonl:%d\n",i, ni, htonl(i));
                if(ni == 0x7fffffff) break;
        }
        printf("all ni transformation is ok\n");
}

void test_stream_adaptor()
{
        sp_mstream_t sp_stm=memory_stream_t::s_create();
        //pass test,2006-7-24
        //->
        printf("just created,sp_stm->get_ref_cnt:%d\n",sp_stm->get_ref_cnt());
        stream_adaptor_t adp((stream_it *)(memory_stream_t*)sp_stm,STM_ADP_OPT_STANDALONE);
        printf("after attach to adaptor,sp_stm->get_ref_cnt:%d\n",sp_stm->get_ref_cnt());
        //adp.attach((stream_it*)0);
        printf("after clear adaptor,sp_stm->get_ref_cnt:%d\n",sp_stm->get_ref_cnt());

        adp.set_nbo();
        //adp.set_byte_order(BO_UNKNOWN);

        adp<<(int8)(-8)<<(uint8)(8);
        adp<<(int16)(-32767)<<(uint16)(32767);
        adp<<(int32)(-1048576)<<(uint32)(1048576);
        adp<<"hello every one!!!";
        char ca[]={'A','B','C'};
        adp<<stream_adaptor_t::raw_buf_t(ca,3);

        char cb[]="morining from bb_t!!!";
        bb_t bb;
        bb.fill_from(cb,sizeof(cb),0);
        adp<<bb;

        int8 i8=0;
        uint8 ui8=0;
        int16 i16=0;
        uint16 ui16=0;
        int32 i32=0;
        uint32 ui32=0;
        int8 str[32];
        stream_adaptor_t::raw_str_t raw_str(str,32,true); //len: control if the tail will be truncated
        stream_adaptor_t::raw_buf_t raw_buf(str,32);

        sp_stm->seek(STM_SEEK_BEGIN,0);
        adp>>i8>>ui8;
        printf("read from stream:i8=%d,ui8=%d\n",i8,ui8);

        adp>>i16>>ui16;
        printf("read from stream:i16=%d,ui16=%d\n",i16,ui16);

        adp>>i32>>ui32;
        printf("read from stream:i32=%d,ui32=%d\n",i32,ui32);


        adp>>raw_str;
        printf("read from stream:string=%s\n",str);

        char szTmp[4]={0};
        stream_adaptor_t::raw_buf_t raw_buf_tmp(szTmp,4);
        adp>>raw_buf_tmp;
        for(int i=0;i<raw_buf_tmp.get_data_size();++i) printf("char in szTmp[%d]=%c\n",i,szTmp[i]);

        bb_t bbo;
        adp>>bbo;
        printf("read bb_t from stream:%s,size=%d\n",(int8*)bbo,bbo.get_filled_len());

        //test network byte order--pass test,2006-7-25
        printf("********read from a network byte order with/without transfer to host byte order********\n");
        sp_stm->seek(STM_SEEK_BEGIN,0);
        stream_adaptor_t adp1((stream_it*)(memory_stream_t*)sp_stm,STM_ADP_OPT_STANDALONE);
        adp1.set_nbo();

        adp1>>i8>>ui8;
        printf("read from stream:i8=%d,ui8=%d\n",i8,ui8);

        adp1>>i16>>ui16;
        printf("read from stream:i16=%d,ui16=%d\n",i16,ui16);

        adp1>>i32>>ui32;
        printf("read from stream:i32=%d,ui32=%d\n",i32,ui32);
        //<-
}

int main(int argc, char **argv)
{
	char *g_buf=0;

	FY_TRY

	//test_ns();
	test_stream_adaptor();

	__INTERNAL_FY_EXCEPTION_TERMINATOR(if(g_buf){printf("g_buf is deleted\n");delete [] g_buf;g_buf=0;});
	
	return 0;	
}
