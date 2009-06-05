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
#ifdef LINUX
#define FY_TEST_STREAM
#endif //LINUX

#ifdef FY_TEST_STREAM

#include "fyt.h"
#include "fy_stream.h"

#ifdef LINUX

#include <netinet/in.h>
#include <arpa/inet.h>

#elif defined(WIN32)

#include <winsock.h>

#endif //LINUX

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

//test memory_stream_t
void test_memory_stream_self_allocated()
{
        //test auto-allocated memory block,2006-6-30
        sp_mstream_t sp_mstm=memory_stream_t::s_create();

        //object_id_it
        //printf("********test object_id_it*********\n");
        printf("object_id=%s\n",sp_mstm->get_object_id());

        //stream_it
        //printf("********test stream_it*************\n");//array version,pass test 2006-7-19
        char c='K';
        stream_it *pStm=(stream_it*)(random_stream_it*)sp_mstm;
        uint32 w_len=pStm->write((int8*)&c,sizeof(char));
        printf("wrote a char,len:%d\n",w_len);
        int16 i=8888;
        w_len=pStm->write((int8*)&i,sizeof(int16));
        printf("wrote a int16,len:%d\n",w_len);
        int32 l=1048576;
        w_len=pStm->write((int8*)&l,sizeof(int32));
        printf("wrote a int32,len:%d\n",w_len);
        //random_stream_it
        printf("*********test random_stream_it*********\n");
        random_stream_it *p_rstm=(random_stream_it *)(pStm->lookup(IID_random_stream, PIN_random_stream));

        printf("******test seek from begin*********\n"); //array version,pass test 2006-7-19
        for(i=-2;i<16;i++)
        {
                uint32 pos=p_rstm->seek(STM_SEEK_BEGIN,i);
                printf("seek to %d from begin,pos:%d\n",i,pos);
                pos=p_rstm->seek(STM_SEEK_CUR,0);
                printf("current position is:%d\n",pos);
        }

        printf("*******test seek to end**************\n");//array version,pass test 2006-7-19
        p_rstm->seek(STM_SEEK_BEGIN,3);
        for(i=-2;i<16;i++)
        {
                uint32 pos=p_rstm->seek(STM_SEEK_END,i);
                printf("seek to %d to end,pos:%d\n",i,pos);
                pos=p_rstm->seek(STM_SEEK_CUR,0);
                printf("current position is:%d\n",pos);
        }


        printf("*********test seek to cur***************\n");//array version,pass test 2006-7-19
        for(i=-8;i<9;i++)
        {
                uint32 origin_pos=7;//0,1,2,3,6,7
                p_rstm->seek(STM_SEEK_BEGIN,origin_pos);
                printf("-----origin position:%d--------\n",origin_pos);
                uint32 pos=p_rstm->seek(STM_SEEK_CUR,i);
                printf("seek to %d to cur,pos:%d\n",i,pos);
                pos=p_rstm->seek(STM_SEEK_CUR,0);
                printf("current position is:%d\n",pos);
        }

        printf("*******test read sequentially***********\n");//array version,pass test 2006-7-19
        {
                p_rstm->seek(STM_SEEK_BEGIN,0);
                char cr=0;
                uint32 r_len=pStm->read((int8*)&cr,sizeof(char));
                printf("read a char:%c,len:%d\n",cr,r_len);

                int16 ir=0;
                r_len=pStm->read((int8*)&ir,sizeof(int16));
                printf("read a int16:%d,len:%d\n",ir,r_len);

                int32 lr=0;
                r_len=pStm->read((int8*)&lr,sizeof(int32));
                printf("read a int32:%d,len:%d\n",lr,r_len);

                char cc=0;
                r_len=pStm->read((int8*)&cc,sizeof(char));
                printf("try to read a char:%c over end,len:%d\n",cc,r_len);
        }

        printf("***********test random read***************\n");//array version,pass test 2006-7-19
        {
                uint32 pos=p_rstm->seek(STM_SEEK_BEGIN,1);//read int16
                int16 ir=0;
                uint32 r_len=pStm->read((int8*)&ir,sizeof(int16));
                printf("read an int16:%d from pos:%d,len:%d\n",ir,pos,r_len);

                pos=p_rstm->seek(STM_SEEK_CUR,-3);//to begin
                char c=0;
                r_len=pStm->read((int8*)&c,sizeof(char));
                printf("read a char:%c from pos:%d,len:%d\n",c,pos,r_len);

                pos=p_rstm->seek(STM_SEEK_END,4);//read int32
                int32 lr=0;
                r_len=pStm->read((int8*)&lr,sizeof(int32));
                printf("read a int32:%d from pos:%d,len:%d\n",lr,pos,r_len);
        }

        printf("***********test copy_to bb_t********************\n");//array version,pass test 2008-4-14
        {
                bb_t bb;
                uint32 buf_len=sp_mstm->copy_to(bb);
                printf("copy stream to a buf,len:%d,bb.get_filled_len:%d\n",buf_len,bb.get_filled_len());
                printf("a char in buffer is:%c\n",*bb.get_buf());

                int16 s=0;
                memcpy((char*)&s,bb.get_buf()+sizeof(char),sizeof(int16));
                printf("an int16 in buffer is:%d\n",s);

                int32 l=0;
                memcpy((char*)&l,bb.get_buf()+sizeof(char)+sizeof(int16),sizeof(int32));
                printf("an int32 in buffer is:%d\n",l);


                printf("------test stream is still ok or not------\n");
                uint32 pos=p_rstm->seek(STM_SEEK_END,4);
                p_rstm->read((int8*)&l,sizeof(int32));
                printf("read int32:%d from pos:%d\n",l,pos);
        }

        printf("***********test copy_to int8v_t********************\n");//array version,pass test 2006-7-19
        {
                int8v_t i8v;
                uint32 buf_len=sp_mstm->copy_to(i8v);
                printf("copy stream to a buf,len:%d,i8v.size:%d\n",buf_len,i8v.size());
                printf("a char in buffer is:%c\n",i8v[0]);

                int16 s=0;
                memcpy((char*)&s,&i8v[0]+sizeof(char),sizeof(int16));
                printf("an int16 in buffer is:%d\n",s);

                int32 l=0;
                memcpy((char*)&l,&i8v[0]+sizeof(char)+sizeof(int16),sizeof(int32));
                printf("an int32 in buffer is:%d\n",l);


                printf("------test stream is still ok or not------\n");
                uint32 pos=p_rstm->seek(STM_SEEK_END,4);
                p_rstm->read((int8*)&l,sizeof(int32));
                printf("read int32:%d from pos:%d\n",l,pos);
        }

        printf("***********test jump read/write***********\n");//pass test,2006-7-20
        {
                sp_mstream_t sp_stm=memory_stream_t::s_create();
                uint32 w_len=sp_stm->write(&c,sizeof(char));
                 printf("wrote a char,len:%d\n",w_len);
                //jump int16
                w_len=sp_stm->write(0,sizeof(int16));
                printf("write jump an int16,len:%d\n",w_len);
                w_len=sp_stm->write((int8*)&l,sizeof(int32));
                printf("wrote an int32,len:%d\n",w_len);

                //read
                sp_stm->seek(STM_SEEK_BEGIN,0);
                char cr=0;
                uint32 r_len=sp_stm->read((int8*)&cr,sizeof(char));
                printf("read a char:%c,len:%d\n",cr,r_len);

                int16 ir=0;
                r_len=sp_stm->read((int8*)&ir,sizeof(int16));
                printf("read a int16:%d,len:%d\n",ir,r_len);

                int32 lr=0;
                r_len=sp_stm->read((int8*)&lr,sizeof(int32));
                printf("read a int32:%d,len:%d\n",lr,r_len);

                char cc=0;
                r_len=sp_stm->read((int8*)&cc,sizeof(char));
                printf("try to read a char:%c over end,len:%d\n",cc,r_len);
        }
#ifdef LINUX
        printf("*********test get_iovec**************\n");//2008-3-25
        {
                sp_mstream_t sp_mstm_iov=memory_stream_t::s_create();
                sp_mstm_iov->set_min_memory_blk_size(200);
                int8 str[]="asfasdfasfasdfasdfasdfasfas";
                printf("piece str len:%d\n",sizeof(str));

                uint32 len_written=0;
                for(int i=0;i<55;++i) len_written+=sp_mstm_iov->write(str,sizeof(str));
                printf("total written bytes:%d\n",len_written);

                int8v_t i8v_iov;
                uint32 copy_len=sp_mstm_iov->copy_to(i8v_iov);
                printf("copy_len:%d\n",copy_len);

                uint32 total_size=sp_mstm_iov->seek(STM_SEEK_END,0);
                printf("total_size:%d\n",total_size);

                memory_stream_t::iovec_box_t iov_box;
                uint32 bytes_len=sp_mstm_iov->get_iovec(iov_box,false);//true);
                printf("all bytes in iovec:%d\n",bytes_len);
                for(int i=0;i<iov_box.get_vec_size();++i)
                {
                        printf("iovec[%d].iov_len=%d\n",i,iov_box.get_iovec()[i].iov_len);
                }
                uint32 pos=sp_mstm_iov->seek(STM_SEEK_END,0);
                printf("end pos after get_iov:%d\n",pos);
                sp_mstm_iov->write(str,sizeof(str));
                pos=sp_mstm_iov->seek(STM_SEEK_END,0);
                printf("end pos after << string:%d\n",pos);
        }
#endif //LINUX
}

//test fast_memory_stream_t,2006-7-12
//new revision pass test,2008-3-25
void test_fast_memory_stream_t()
{
	int i=0;
        const uint32 buf_len=8;//6,7,8,12
        printf("attach a buf to fast_memory_stream_t:size:%d\n",buf_len);
        int8 out_buf[buf_len];
        sp_fmstream_t sp_mstm=fast_memory_stream_t::s_create(out_buf,buf_len);
        //object_id_it
        //printf("********test object_id_it*********\n");
        printf("fast_memory_stream_t object_id=%s\n",sp_mstm->get_object_id());

        //stream_it
        //printf("********test stream_it*************\n");//pass test 2006-7-12
        char c='K';
        stream_it *pStm=(stream_it *)(fast_memory_stream_t*)sp_mstm;
        uint32 w_len=pStm->write((int8*)&c,sizeof(char));
        printf("wrote a char,len:%d\n",w_len);
        int16 i16=8888;
        w_len=pStm->write((int8*)&i16,sizeof(int16));
        printf("wrote a int16,len:%d\n",w_len);
        int32 l=1048576;
        w_len=pStm->write((int8*)&l,sizeof(int32));
        printf("wrote a int32,len:%d\n",w_len);
        //random_stream_it
        printf("*********test random_stream_it*********\n");
        random_stream_it *p_rstm=(random_stream_it *)(pStm->lookup(IID_random_stream, PIN_random_stream));

        printf("******test seek from begin*********\n"); //pass test 2006-7-12
        for(i=-2;i<16;i++)
        {
                uint32 pos=p_rstm->seek(STM_SEEK_BEGIN,i);
                printf("seek to %d from begin,pos:%d\n",i,pos);
                pos=p_rstm->seek(STM_SEEK_CUR,0);
                printf("current position is:%d\n",pos);
        }

        printf("*******test seek to end**************\n");//pass test 2006-7-12
        p_rstm->seek(STM_SEEK_BEGIN,3);
        for(i=-2;i<16;i++)
        {
                uint32 pos=p_rstm->seek(STM_SEEK_END,i);
                printf("seek to %d to end,pos:%d\n",i,pos);
                pos=p_rstm->seek(STM_SEEK_CUR,0);
                printf("current position is:%d\n",pos);
        }


        printf("*********test seek to cur***************\n");//pass test 2006-7-12
        for(i=-8;i<9;i++)
        {
                uint32 origin_pos=7;//0,1,2,3,6,7
                p_rstm->seek(STM_SEEK_BEGIN,origin_pos);
                printf("-----origin position:%d--------\n",origin_pos);
                uint32 pos=p_rstm->seek(STM_SEEK_CUR,i);
                printf("seek to %d to cur,pos:%d\n",i,pos);
                pos=p_rstm->seek(STM_SEEK_CUR,0);
                printf("current position is:%d\n",pos);
        }

        printf("*******test read sequentially***********\n");//pass test 2006-7-12
        {
                p_rstm->seek(STM_SEEK_BEGIN,0);
                char cr=0;
                uint32 r_len=pStm->read((int8*)&cr,sizeof(char));
                printf("read a char:%c,len:%d\n",cr,r_len);

                int16 ir=0;
                r_len=pStm->read((int8*)&ir,sizeof(int16));
                printf("read a int16:%d,len:%d\n",ir,r_len);

                int32 lr=0;
                r_len=pStm->read((int8*)&lr,sizeof(int32));
                printf("read a int32:%d,len:%d\n",lr,r_len);

                char cc=0;
                r_len=pStm->read((int8*)&cc,sizeof(char));
                printf("try to read a char:%c over end,len:%d\n",cc,r_len);
        }

        printf("***********test random read***************\n");//pass test 2006-7-12
        {
                uint32 pos=p_rstm->seek(STM_SEEK_BEGIN,1);//read int16
                int16 ir=0;
                uint32 r_len=pStm->read((int8*)&ir,sizeof(int16));
                printf("read an int16:%d from pos:%d,len:%d\n",ir,pos,r_len);

                pos=p_rstm->seek(STM_SEEK_CUR,-3);//to begin
                char c=0;
                r_len=pStm->read((int8*)&c,sizeof(char));
                printf("read a char:%c from pos:%d,len:%d\n",c,pos,r_len);

                pos=p_rstm->seek(STM_SEEK_END,4);//read int32
                int32 lr=0;
                r_len=pStm->read((int8*)&lr,sizeof(int32));
                printf("read a int32:%d from pos:%d,len:%d\n",lr,pos,r_len);
        }

        printf("***********test detach********************\n");//pass test 2006-7-12
        {
                int8* buf=0;
                uint32 filled_len=0;
                uint32 buf_size=sp_mstm->detach(&buf,&filled_len);
                printf("detach buf from stream,buf size:%d,filled_len=%d\n",buf_size,filled_len);
                char c=0;
                memcpy((char*)&c,buf,sizeof(char));
                printf("a char in buffer is:%c\n",c);

                int16 s=0;
                memcpy((char*)&s,buf+sizeof(char),sizeof(int16));
                printf("an int16 in buffer is:%d\n",s);

                int32 l=0;
                memcpy((char*)&l,buf+sizeof(char)+sizeof(int16),sizeof(int32));
                printf("an int32 in buffer is:%d\n",l);
        }
        printf("***********test jump read/write***********\n");//pass test,2006-7-20
        {
                char buf[32];
                sp_fmstream_t sp_stm=fast_memory_stream_t::s_create(buf,32);
                uint32 w_len=sp_stm->write(&c,sizeof(char));
                 printf("wrote a char,len:%d\n",w_len);
                //jump int16
                w_len=sp_stm->write(0,sizeof(int16));
                printf("write jump an int16,len:%d\n",w_len);
                w_len=sp_stm->write((int8*)&l,sizeof(int32));
                printf("wrote an int32,len:%d\n",w_len);

                //read
                sp_stm->seek(STM_SEEK_BEGIN,0);
                char cr=0;
                uint32 r_len=sp_stm->read((int8*)&cr,sizeof(char));
                printf("read a char:%c,len:%d\n",cr,r_len);

                int16 ir=0;
                r_len=sp_stm->read((int8*)&ir,sizeof(int16));
                printf("read a int16:%d,len:%d\n",ir,r_len);

                int32 lr=0;
                r_len=sp_stm->read((int8*)&lr,sizeof(int32));
                printf("read a int32:%d,len:%d\n",lr,r_len);

                char cc=0;
                r_len=sp_stm->read((int8*)&cc,sizeof(char));
                printf("try to read a char:%c over end,len:%d\n",cc,r_len);
        }
}

//stream performance benchmark test
void test_memory_stream_performance(void)
{
		int i=0;
        uint32 len=10485760;
        int8* buf=new int8[len];
        int8 c='K';
#ifdef LINUX
        struct timeval tv1,tv2;
#elif defined(WIN32)
		long tc1,tc2;
#endif
        printf("********benchmark base overhead**********\n");
        const uint32 block_size=1;//1,2,4,8,32
        uint32 blk_cnt=len/block_size;
        uint32 cur_pos=0;
#ifdef LINUX
        gettimeofday(&tv1,0);
#elif defined(WIN32)
		tc1=::GetTickCount();
#endif
        for(i=0;i<blk_cnt;i++){}
#ifdef LINUX
        gettimeofday(&tv2,0);
        int32 tc=timeval_util_t::diff_of_timeval_tc(tv1,tv2);
#elif defined(WIN32)
		tc2=::GetTickCount();
		int32 tc=tc2 - tc1;
#endif
        //test results:2006-7-13
        //block_size=1:avg=21
        //block_size=2:avg=10
        //block_size=4:avg=5
        //block_size=8:avg=2
        //block_size=32:avg=0
        printf("benchmark base overhead,len=%d,blocksize=%d,tc=%d\n",
                len,block_size,tc);

        printf("***************performance base-line******************\n");
        {
        const uint32 block_size=1;//1,2,4,8,32,128,1024,65536
        uint32 blk_cnt=len/block_size;
        char blk[block_size];
        uint32 cur_pos=0;
#ifdef LINUX
        gettimeofday(&tv1,0);
#elif defined(WIN32)
		tc1=::GetTickCount();
#endif
        for(uint32 i=0;i<blk_cnt;i++)
        {
                memcpy(buf+cur_pos,blk,block_size);
                cur_pos+=block_size;
        }
#ifdef LINUX
        gettimeofday(&tv2,0);
        int32 tc=timeval_util_t::diff_of_timeval_tc(tv1,tv2);
#elif defined(WIN32)
		int32 tc=::GetTickCount() - tc1;
#endif
        //test results:2006-7-10,adjust 2006-7-13,considering base overhead
        //test 4 times,block_size=1:651,650,650,651,avg=651,net=651-21=630,bw(bandwidth)=127MB(baund rate)
        //             block_size=2:343,340,341,341,avg=341,net=341-10=331
        //             block_size=4:180,180,180,180,avg=180,net=180-5=175
        //             block_size=8:102,102,102,103,avg=102,net=102-2=100
        //             block_size=32:44,44,44,44,   avg=44 ,net=44
        //             block_size=128:32,32,32,32,  avg=32 ,net=32
        //             block_size=1024:30,30,30,30, avg=30 ,net=30
        //             block_size=65536:29,29,29,29,avg=29 ,net=29,bw=2.76GB
        printf("[benchmark]:duration of copy to buffer one block by one block,len=%d,blocksize=%d,tc=%d\n",
                len,block_size,tc);
        }

        printf("*************memory_stream_t performance*************\n");
        {
        sp_mstream_t sp_stm=memory_stream_t::s_create();
        const uint32 block_size=1;//1,2,4,8,32,128,1024,65536
        uint32 blk_cnt=len/block_size;
        char blk[block_size];
        //sp_stm->prealloc_buffer(10485760-1024);
#ifdef LINUX
        gettimeofday(&tv1,0);
#elif defined(WIN32)
		tc1=::GetTickCount();
#endif
        for(i=0;i<blk_cnt;i++)
        {
                sp_stm->write(blk,block_size);
        }
#ifdef LINUX
        gettimeofday(&tv2,0);
        int32 tc=timeval_util_t::diff_of_timeval_tc(tv1,tv2);
#elif defined(WIN32)
		int32 tc=::GetTickCount() - tc1;
#endif
		uint32 wrote_len=sp_stm->seek(STM_SEEK_END,0);
        //test results:2006-7-11,adjust 2006-7-13,considering base overhead--implement based on std::list
        //test 4 times,block_size=1:1923,1917,1919,1922,avg=1920,net=1920-21=1899,pc(performance coeffient)=630/1899=0.33
        //             block_size=2:994,995,993,995,avg=994,net=994-10=984, pc=331/984=0.34
        //             block_size=4:503,505,504,506,avg=505,net=505-5 =500, pc=175/500=0.35
        //             block_size=8:266,264,264,264,avg=265,net=265-2 =263, pc=100/263=0.38
        //             block_size=32:84,84,84,84,   avg=84, net=84, pc=44/84=0.52
        //             block_size=128:42,42,42,42,  avg=42, net=42, pc=32/42=0.76
        //             block_size=1024:31,31,31,31, avg=31, net=31, pc=30/31=0.97
        //             block_size=65536:29,29,29,29,avg=29, net=29  pc=29/29=1.0
        //test results:2006-7-19,implement based on char array
        //test 4 times,block_size=1:996,998,998,996,avg=997, net=997-21=976,pc=630/976=0.65,bw=82MB
        //no pre-alloc_buffer,block_size=1:1002,998,1003,998,avg=1000,net=1000-21=979,pc=630/979=0.64
        printf("write to attached stream one block by one block,len=%d,block_size=%d,tc=%d\n",
                wrote_len,block_size,tc);
        //88
        bb_t bb;
        uint32 buf_len=sp_stm->copy_to(bb);
        printf("=====copy_to,buf_len=%d\n",buf_len);
        }

        printf("*************fast_memory_stream_t performance*************\n");
        {
        sp_fmstream_t sp_fstm=fast_memory_stream_t::s_create(buf,len);
        const uint32 block_size=1;//,32,128,1024
        uint32 blk_cnt=len/block_size;
        char blk[block_size];
#ifdef LINUX
        gettimeofday(&tv1,0);
#elif defined(WIN32)
		tc1=::GetTickCount();
#endif
        for(i=0;i<blk_cnt;i++)
        {
                sp_fstm->write(blk,block_size);
        }
#ifdef LINUX
        gettimeofday(&tv2,0);
        int32 tc=timeval_util_t::diff_of_timeval_tc(tv1,tv2);
#elif defined(WIN32)
		int32 tc=::GetTickCount() - tc1;
#endif
		uint32 wrote_len=sp_fstm->seek(STM_SEEK_END,0);
        //test results:2006-7-12,adjust 2006-7-13,considering base overhead
        //test 4 times,block_size=1:833,830,828,828,avg=830,net=830-21=809,pc(performance coeffient)=630/809=0.78,bw=99MB
        //             block_size=32:50,50,50,50,   avg=50, net=50, pc=44/50=0.88
        //             block_size=128:33,33,33,33,  avg=33, net=33, pc=32/33=0.97
        //             block_size=1024:30,30,30,30, avg=30, net=30, pc=30/30=1.00
        printf("write to simple stream one block by one block,len=%d,block_size=%d,tc=%d\n",
                wrote_len,block_size,tc);
        }

        printf("**********push to std::deque**********\n");
        {
        typedef std::deque<char> cq_t;
        cq_t cq;
        uint32 block_size=1;
        uint32 blk_cnt=len/block_size;
        char blk;
#ifdef LINUX
        gettimeofday(&tv1,0);
#elif defined(WIN32)
		tc1=::GetTickCount();
#endif
        for(i=0;i<blk_cnt;i++)
        {
                cq.push_back(blk);
        }
#ifdef LINUX
        gettimeofday(&tv2,0);
        int32 tc=timeval_util_t::diff_of_timeval_tc(tv1,tv2);
#elif defined(WIN32)
		int32 tc=::GetTickCount() - tc1;
#endif
		uint32 wrote_len=cq.size();
        //as block_size=1, deque has best performance, even much better than benchmark memcpy,2008-4-21
        printf("push to a deque one block by one block,len=%d,block_size=%d,tc=%d\n",
                wrote_len,block_size,tc);
        }
        if(buf) delete [] buf;
}

void test_stream_adaptor_perormance(void)
{
        uint32 loops=10485760;
#ifdef LINUX
        struct timeval tv1,tv2;
#elif defined(WIN32)
	long tc1;
#endif
        int8 i8='K';
        int16 i16=1024;
        int32 i32=1048576;
        char str_hello[33];
        memset(str_hello,'K',32);
        uint32 len_str=32;
        str_hello[32]=0;

        printf("********benchmark base overhead**********\n");
#ifdef LINUX
        gettimeofday(&tv1,0);
#elif defined(WIN32)
	tc1= ::GetTickCount();
#endif
        for(uint32 i=0;i<loops;i++){}
#ifdef LINUX
        gettimeofday(&tv2,0);
        int32 tc=timeval_util_t::diff_of_timeval_tc(tv1,tv2);
#elif defined(WIN32)
	int32 tc=::GetTickCount() - tc1;
#endif
        //test results:2006-7-25
        //loops=10485760,tc=21
        printf("benchmark base overhead,loops=%d,tc=%d\n",
                loops,tc);
        //*************memory_stream_t performance*************
        {
        sp_mstream_t sp_stm=memory_stream_t::s_create();
#ifdef LINUX
        gettimeofday(&tv1,0);
#elif defined(WIN32)
	tc1=::GetTickCount();
#endif
        for(uint32 i=0;i<loops;i++)
        {
                //write int8
                if(sp_stm->write(&i8,sizeof(int8))!= sizeof(int8))
                {
                        printf("write int8 failure\n");
                        break;
                }
/*
                //write int32
                if(sp_stm->write((int8*)&i32,sizeof(int32)) != sizeof(int32))
                {
                        printf("write int32 failure\n");
                        break;
                }

                //write string
                if(sp_stm->write((int8*)&len_str,sizeof(uint32)) != sizeof(uint32))
                {
                        printf("write uint32 failure\n");
                        break;
                }
                if(sp_stm->write((int8*)str_hello,len_str) != len_str)
                {
                        printf("write string failure\n");
                        break;
                }
*/
        }
#ifdef LINUX
        gettimeofday(&tv2,0);
        int32 tc=timeval_util_t::diff_of_timeval_tc(tv1,tv2);
#elif defined(WIN32)
	int32 tc = ::GetTickCount() - tc1;
#endif
        uint32 wrote_len=sp_stm->seek(STM_SEEK_END,0);
        //test results:2006-7-25
        //i8:989,986,995,994,avg=991,net=991-21=970
        //i32:1057,1060,1060,1062,avg=1060,net=1060-21=1039
        //str(len=32):2809,2806,2802,2803,avg=2805,net=2805-21=2784
        //****test for revision,2008-3-25*****
        //i8:987,987,1004,989,avg=992,net=avg-24=968
        //i32:1064,1063,1061,1065,avg=1063,net=avg-24=1039
        //str(len=32):2831,2830,2831,2830,avg=2831,net=avg-24=2807
        printf("write to memory stream directly,len=%d,loops=%d,tc=%d\n",
                wrote_len,loops,tc);
       }

        //***************stream_adaptor_t performance***************
        {
        sp_mstream_t sp_stm=memory_stream_t::s_create();
        stream_adaptor_t adp((stream_it *)(memory_stream_t*)sp_stm,STM_ADP_OPT_STANDALONE);
        //adp.set_nbo();
        //adp.set_byte_order(BO_UNKNOWN);//BO_LITTLE_ENDIAN);
#ifdef LINUX
        gettimeofday(&tv1,0);
#elif defined(WIN32)
	tc1=::GetTickCount();
#endif
        for(uint32 i=0;i<loops;i++)
        {
                adp<<i8;
                //adp<<i32;
                //adp<<str_hello;
        }
#ifdef LINUX
        gettimeofday(&tv2,0);
        int32 tc=timeval_util_t::diff_of_timeval_tc(tv1,tv2);
#elif defined(WIN32)
	int32 tc=::GetTickCount() - tc1;
#endif
        uint32 wrote_len=sp_stm->seek(STM_SEEK_END,0);
        //test results:2006-7-25
        //i8:1093,1097,1096,1093,avg=1095,net=1095-21=1074,pdp(performance decrease percentage=(1074-970)/970=0.107,bw=74MB
        //i32:1156,1158,1155,1156,avg=1156,net=1156-21=1135,pdp=(1135-1039)/1039=0.092,bw=282MB
        //str(len=32):3194,3195,3191,3191,avg=3193,net=3193-21=3172,pdp=(3172-2784)/2784=0.139
        //===add network byte order feature====
        //disable nbo flag: about 1% performance descrease than adaptor without this feature,this descrease can be ignored
        //--2006-7-25
        //i32:1169,1167,1167,1168,avg=1168,net=1168-21=1147,pdp=(1147-1039)/1039=0.104
        //enabled nbo flag:incomparison with host byte order,pc of network byte order stream i/o is just 0.765,isn't good enough
        //,except for necessary,host byte order is preferred--2006-7-25
        //i32:1506,1502,1502,1504,avg=1504,net=1504-21=1483,pc=1135/1483=0.765//note,here is pc,but not pdp
        //****test for revision,2008-3-25*****
        //i8:1133,1140,1127,1168,avg=1142,net=avg-24=1118,pc=(1118-970)/970=0.152
        //i32:1356,1493,1374,1345,avg=1392,net=avg-24=1368,pdp=(1368-1039)/1039=0.317
        //str(len=32):3426,3437,3422,3437,avg=3427,net=avg-24=3403,pc=(3403-2807)/2807=0.212
        printf("write to stream by adaptor,len=%d,loops=%d,tc=%d\n",
                wrote_len,loops,tc);

        }
}

//test oneway_pipe_t
//test interthread communication performance
class ITC_Area
{
public:
        ITC_Area()
        {
                io_pos=-1;
                buf=new char[BUF_SIZE];
        }
        ~ITC_Area(){ if(buf) delete [] buf; }
public:
        static int BUF_SIZE;
        char *buf;
        int io_pos;
        critical_section_t cs;
        critical_section_t cs_ready;
#ifdef LINUX
        struct timeval tvs;
        struct timeval tve;
#elif defined(WIN32)
	long tcs;
	long tce;
#endif
        bool no_data;
};
int ITC_Area::BUF_SIZE=1048576;

class ITC_Area_Nolock
{
public:
        ITC_Area_Nolock()
        {
                i_pos=o_pos-1;
                buf=new char[BUF_SIZE];
		no_data=false;
        }
        ~ITC_Area_Nolock(){ if(buf) delete [] buf; }
public:
        static int BUF_SIZE;
        char *buf;
        int i_pos;
        int o_pos;
        critical_section_t cs_ready;
#ifdef LINUX
        struct timeval tvs;
        struct timeval tve;
#elif defined(WIN32)
	long tcs;
	long tce;
#endif
        bool no_data;
};
int ITC_Area_Nolock::BUF_SIZE=1048576;

#ifdef POSIX

void *tf_itc(void *para)

#elif defined(WIN32)

DWORD WINAPI tf_itc(void *para)

#endif
{
        ITC_Area *itc_a=(ITC_Area *)para;
        itc_a->cs_ready.unlock();
        char *tmp=new char[1048576];//for len=10
        uint32 total_size=0;
        while(true)
        {
                itc_a->cs.lock();
                while(itc_a->io_pos>=0)
                {
                        //read byte one by one
                        //->
                        uint32 read_len=1;
                        char c=itc_a->buf[itc_a->io_pos--];//read a char
                        //<-
                /*
                        //read bytes as many as possible
                        //->
                        read_len=(itc_a->io_pos+1>1048576?1048576:itc_a->io_pos+1);
                        memcpy(tmp,itc_a->buf+itc_a->io_pos,read_len);
                        itc_a->io_pos-=read_len;
                        //<-
                */
                        total_size+=read_len;
                }
                itc_a->cs.unlock();
                if(itc_a->no_data)
                {
#ifdef POSIX
                        gettimeofday(&(itc_a->tve),0);
#elif defined(WIN32)
			itc_a->tce=::GetTickCount();
#endif
                        printf("read total_size in tf_itc:%d\n",total_size);
                        break;
                }
        }
        if(tmp) delete [] tmp;

        return 0;
}

critical_section_t g_cs_thd_ready;
#ifdef POSIX

struct timeval g_tvs;
struct timeval g_tve;

#elif defined(WIN32)

long g_tcs;
long g_tce;

#endif

event_slot_t es_r;
event_slot_t es_w;

#ifdef LINUX

void *tf_itc_nlp(void *para)

#elif defined(WIN32)

DWORD WINAPI tf_itc_nlp(void *para)

#endif
{
        oneway_pipe_t *nlp=(oneway_pipe_t *)para;
        sp_owp_t sp_nlp(nlp,true);

        sp_nlp->register_read();
        g_cs_thd_ready.unlock();
        uint32 total_size=0;
        const int32 R_BUF_SIZE=1024;
        int8 buf[R_BUF_SIZE];
        while(true)
        {
                uint32 read_len=sp_nlp->read(buf,R_BUF_SIZE);
                while(read_len)
                {
                        total_size+=read_len;
                        read_len=sp_nlp->read(buf,R_BUF_SIZE);
                }
                if(!sp_nlp->is_write_registered())
                {
#ifdef POSIX
                        gettimeofday(&(g_tve),0);
#elif defined(WIN32)
			g_tce = ::GetTickCount();
#endif
                        sp_nlp->unregister_read();
                        printf("read total size in tf_itc_nlp:%d\n",total_size);
                        break;
                }
                //printf("total_size=%d\n",total_size);
                es_w.signal(0);
                event_slot_t::slot_vec_t ev;
                es_r.wait(ev);

        }
        return 0;
}

void test_itc_with_nlpipe_performance()
{
        uint32 len=1048576;
#ifdef POSIX
        struct timeval tv1,tv2;
#elif defined(WIN32)
	long tc1;
#endif
        printf("*********base over head**************\n");
        {
#ifdef POSIX
        gettimeofday(&tv1,0);
#elif defined(WIN32)
	tc1=::GetTickCount();
#endif
        for(uint32 i=0;i<len;i++){}
#ifdef POSIX
        gettimeofday(&tv2,0);
        int32 tc=timeval_util_t::diff_of_timeval_tc(tv1,tv2);
#elif defined(WIN32)
	int32 tc=::GetTickCount()-tc1;
#endif
        printf("tc=:%d\n",tc);
        }

        printf("***********itc with lock***********\n");
        {
        ITC_Area *itca=new ITC_Area();
#ifdef POSIX
        pthread_t thd;
#elif defined(WIN32)
	HANDLE thd;
#endif
        itca->no_data=false;
        itca->cs_ready.lock();
#ifdef POSIX
        pthread_create(&thd,0,tf_itc,(void *)itca);
#elif defined(WIN32)
	thd=::CreateThread(0,0,tf_itc, (void*)itca, 0,0);
#endif
        itca->cs_ready.lock();
#ifdef POSIX
        gettimeofday(&(itca->tvs),0);
#elif defined(WIN32)
	itca->tcs=::GetTickCount();
#endif
        for(uint32 i=0;i<len;i++)
        {
                itca->cs.lock();
                itca->buf[++(itca->io_pos)]='K';
               itca->cs.unlock();
        }
        itca->no_data=true;
#ifdef POSIX
        pthread_join(thd,0);
        int32 tc=timeval_util_t::diff_of_timeval_tc(itca->tvs,itca->tve);
#elif defined(WIN32)
	WaitForSingleObject(thd,INFINITE);
	int32 tc=itca->tce - itca->tcs;
#endif
        printf("tc=%d\n",tc);
        delete itca;
        }

        printf("***************itc with nolock_pipe_t***********\n");
        {
        sp_owp_t sp_nlp=oneway_pipe_t::s_create(524287);
        sp_nlp->register_write();
        char buf[]={'K'};
#ifdef POSIX
        pthread_t thd;
#elif defined(WIN32)
	HANDLE thd;
#endif
        g_cs_thd_ready.lock();
#ifdef POSIX
        pthread_create(&thd,0,tf_itc_nlp,(void *)(oneway_pipe_t*)sp_nlp);
#elif defined(WIN32)
	thd=::CreateThread(0,0, tf_itc_nlp, (void*)(oneway_pipe_t*)sp_nlp,0,0);
#endif
        g_cs_thd_ready.lock();
#ifdef POSIX
        gettimeofday(&(g_tvs),0);
#elif defined(WIN32)
	g_tcs=::GetTickCount();
#endif
        for(uint32 i=0;i<len;i++)
        {
                while(sp_nlp->write(buf,1) == 0)
                {
                        es_r.signal(0);
                        event_slot_t::slot_vec_t ev;
                        es_w.wait(ev);
                }
        }
        sp_nlp->unregister_write();
        es_r.signal(1);
#ifdef POSIX
        pthread_join(thd,0);
        int32 tc=timeval_util_t::diff_of_timeval_tc(g_tvs,g_tve);
#elif defined(WIN32)
	WaitForSingleObject(thd,INFINITE);
	int32 tc=g_tce - g_tcs;
#endif
        //results:2006-8-17
        //avg=344,net=avg-4=340,pip=(11324-340)/340=32.3,bw=12.3MB
        printf("tc=%d\n",tc);
        }
}

#include "fy_trace.h"

void test_trace_provider()
{
	int trace_to_flag=1;//0:stdout; 1: to file; 2: to debugview(windows)
        trace_provider_t *trace_prvd=trace_provider_t::instance();
	uint8 i;
	switch(trace_to_flag)
	{
	case 0: //stdout(default)
		break;
	case 1: //to file
		{
        		trace_prvd->register_trace_stream(0, sp_trace_stream_t(), REG_TRACE_STM_OPT_ALL);

        		for(i=0; i<MAX_TRACE_LEVEL_COUNT; ++i)
        		{
                		sp_trace_stream_t sp_trace_file=trace_file_t::s_create(i);
                		trace_prvd->register_trace_stream(i, sp_trace_file, REG_TRACE_STM_OPT_EQ);
        		}
		}
		break;

	case 2: //to debugview(windows)
#ifdef WIN32
		{
			trace_prvd->register_trace_stream(0, sp_trace_stream_t(), REG_TRACE_STM_OPT_ALL);
                sp_trace_stream_t sp_trace_debugview=trace_debugview_t::s_create();
        		for(i=0; i<MAX_TRACE_LEVEL_COUNT; ++i)
        		{
                		trace_prvd->register_trace_stream(i, sp_trace_debugview, REG_TRACE_STM_OPT_EQ);
        		}
		}
#endif
		break;

	default: //equal zero
		break;
	}
        trace_prvd->open();
        trace_provider_t::tracer_t *tracer=trace_prvd->register_tracer();
        uint8 level=0;
#ifdef POSIX
	usleep(100000);
#elif defined(WIN32)
	Sleep(100);
#endif
        for(level=0; level<35; ++level)
        { 
                tracer->prepare_trace_prefix(level,__FILE__,__LINE__)<<"hello, from trace provider test \r\n";
				tracer->write_trace(level);
#ifdef POSIX
                usleep(10000);
#elif defined(WIN32)
		Sleep(10);
#endif
        }
        trace_prvd->unregister_tracer();
        trace_prvd->close();
}

int main(int argc, char **argv)
{
	char *g_buf=0;

	FY_TRY

	//test_ns();
	//test_stream_adaptor();
	test_memory_stream_self_allocated();
	//test_fast_memory_stream_t();
	//test_memory_stream_performance();
	//test_stream_adaptor_perormance();
	//test_itc_with_nlpipe_performance();
	//test_trace_provider();

	__INTERNAL_FY_EXCEPTION_TERMINATOR(if(g_buf){printf("g_buf is deleted\n");delete [] g_buf;g_buf=0;});
	
	return 0;	
}

#endif //FY_TEST_STREAM
