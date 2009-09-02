/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@hotmail.com
 *
 * [History]
 * initialize: 2009-8-27
 * ====================================================================
 */
#ifdef LINUX
#define FY_TEST_RAWSOK
#endif

#ifdef FY_TEST_RAWSOK

#include "fy_socket.h"

USING_FY_NAME_SPACE

#define SERVPORT 8888
#define BACKLOG 128

/*[tip]
 *[desc]test the rate of accept() at listen side
 */
int main(int argc,char **argv)
{
#ifdef WIN32
	    WSADATA wsa_data;
        WSAStartup(0x0202, &wsa_data);
#endif
        bool is_svr=true;
        int conn_cnt=0;
		int expect_conn_cnt = 100;
		int backlog = BACKLOG;
        if(argc > 1)
        {
                if(strcmp(argv[1],"client") == 0) is_svr=false;
				if(argc > 2)
				{
					expect_conn_cnt = atoi(argv[2]);
					if(argc > 3)
					{
						backlog = atoi(argv[3]);
					}
				}		
        }
#ifdef LINUX    
	struct timeval tv1,tv2;
#elif defined(WIN32)
	int32 tc1,tc2;
#endif
        //set server address
        struct sockaddr_in svr_addr;
        svr_addr.sin_family=PF_INET; //protocol family
        svr_addr.sin_port=htons(SERVPORT); //listening port number--transfer short type to network sequence
        svr_addr.sin_addr.s_addr = inet_addr("10.224.172.129");//htonl(INADDR_ANY); //IP address(autodetect)
          
	if(is_svr)
	{
		printf("==raw socket server,expect_conn_cnt:%d,backlog:%d==\n",expect_conn_cnt,backlog);
		int32 sok_lsn=socket(AF_INET, SOCK_STREAM, 0);
#ifdef LINUX
        	int32 opt_reuse=1;
        	::setsockopt(sok_lsn, SOL_SOCKET, SO_REUSEADDR, (void*)&opt_reuse, sizeof(int32));
#endif //LINUX
		int32 ret = ::bind(sok_lsn, (const sockaddr*)&svr_addr, sizeof(sockaddr_in));	
		if(ret)
		{
			perror("bind fail\n");
			fy_close_sok(sok_lsn);

			return 0;
		}
		ret=::listen(sok_lsn, backlog);
		if(ret)
		{
			perror("listen fail\n");
			
			fy_close_sok(sok_lsn);
			
			return 0;
		}
	
		sockaddr rmt_addr;
		socklen_t len = sizeof(sockaddr);
#ifdef LINUX
		gettimeofday(&tv1,0);
#elif defined(WIN32)
		tc2=GetTickCount();
#endif
		while(true)
		{
#ifdef LINUX
			int32 ret_sock = ::accept(sok_lsn, &rmt_addr, &len);
#elif defined(WIN32)
			int32 ret_sock = ::accept(sok_lsn, &rmt_addr, (int*)&len);
#endif
			if(INVALID_SOCKET == ret_sock)
			{
				perror("accept fail\n");
				fy_close_sok(sok_lsn);
				sok_lsn = INVALID_SOCKET;
				break;
			}

			fy_close_sok(ret_sock);
                        if(!conn_cnt)
                        {
#ifdef LINUX
                                gettimeofday(&tv1, 0);
#elif defined(WIN32)
                                tc1=GetTickCount();
#endif
                        }
                        else if(conn_cnt == expect_conn_cnt)
                        {
#ifdef LINUX
                                gettimeofday(&tv2, 0);
#elif defined(WIN32)
                                tc2 = GetTickCount();
#endif
                                printf("####server stopped####\n");
                                break;
                        }
			++conn_cnt;
		}
		if(sok_lsn != INVALID_SOCKET) fy_close_sok(sok_lsn);				
	}
       	else //as client, connect to
        {
		printf("==raw socket client==\n");

		int32 conn_fd = INVALID_SOCKET;
		while(true)
		{
			if ((conn_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) 
			{
                		perror("fail to create a socket descriptor to connect!\n");
                		break;
        		}       
			int ret=::connect(conn_fd, (struct sockaddr *)&svr_addr, sizeof(struct sockaddr));
			if(ret == -1)
			{
				perror("::connect to server failure\n");
				fy_msleep(1000);
			}
			else
			{
				//printf("c:%d\n",conn_cnt);
				if(!conn_cnt)
				{
#ifdef LINUX
					gettimeofday(&tv1, 0);
#elif defined(WIN32)
					tc1=GetTickCount();
#endif
				}
				else if(conn_cnt == expect_conn_cnt)
				{
#ifdef LINUX
					gettimeofday(&tv2, 0);
#elif defined(WIN32)
					tc2 = GetTickCount();
#endif
					printf("####client stopped####\n");
					break;
				}
				++conn_cnt;
			}
			fy_close_sok(conn_fd);
			conn_fd=0;
		}
	}//if(is_svr)
#ifdef LINUX
	
	int32 deta_tc = timeval_util_t::diff_of_timeval_tc(tv1, tv2);
 
#elif defined(WIN32)

	int32 deta_tc = tc_util_t::diff_of_tc(tc1, tc2);
	 WSACleanup();
#endif
	printf("==summary, conn_cnt:%d, duration(ms):%d, rate(/ms):%f\n",
		conn_cnt, deta_tc, (deta_tc? (double)conn_cnt/deta_tc : -1));

	return 0;
}

#endif //FY_TEST_SOK
