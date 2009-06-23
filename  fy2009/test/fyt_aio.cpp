/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@hotmail.com
 *
 * [History]
 * initialize: 2009-6-22
 * ====================================================================
 */ 
#include "fy_aio.h"

#ifdef LINUX
#include <sys/socket.h>
#include <netinet/in.h>
#endif //LINUX

USING_FY_NAME_SPACE

//test aio
int g_conn_fd=0;
bool read_flag=false;
class test_aio_eh_t : public aio_event_handler_it,
			public ref_cnt_impl_t
{
public:
	test_aio_eh_t(sp_aiop_t aiop, bool listen_flag=false): _listen_flag(listen_flag),
			_aiop(aiop)
	{
	}
	void on_aio_events(fyfd_t fd, uint32 aio_events)
	{
		printf("==on_aio_events is called,_listen_flag:%d, fd:%d, aio_events:%d==\n", _listen_flag, fd, aio_events);
		if(_listen_flag) //as server
		{
			if((aio_events & AIO_POLLIN) == AIO_POLLIN)
			{
				printf("listen socket received an AIO_POLLIN\n");

				int conn_fd=::accept(fd, 0, 0);
                        	if(conn_fd == -1) 
				{
					perror("accept failed\n");
                        		return;
				}
                		printf("accepted an incoming connection:%d\n",conn_fd);
				sp_aioeh_t aio_eh(new test_aio_eh_t(_aiop, false), true);
				_aiop->register_fd(0, conn_fd, aio_eh);
				g_conn_fd=conn_fd;

				return;	
			}
			if((aio_events & AIO_POLLERR) == AIO_POLLERR)
			{
				printf("listen socket received an AIO_POLLERR\n");
				_aiop->unregister_fd(fd);
				::close(fd);
				
				return;
			}
			if((aio_events & AIO_POLLHUP) == AIO_POLLHUP)
			{
				printf("listen socket received an AIO_POLLHUP\n");
				_aiop->unregister_fd(fd);
				::close(fd);

				return;
			}
			printf("listen socket received an unknow event\n");

			return;
		}
		else //connection 
		{
			if((aio_events & AIO_POLLIN) == AIO_POLLIN)
			{
				printf("conn received an AIO_POLLIN\n");
//if(!read_flag)
//{
//read_flag=true;
				const int BUF_SIZE=1024;
				char buf[BUF_SIZE];
				int ret=::recv(fd, buf, BUF_SIZE, 0);
printf("<<<<received %d\n",ret);
				int rcv_cnt=0;
				while(ret > 0) {rcv_cnt+= ret; ret=::recv(fd, buf, BUF_SIZE, 0); }	
				if(ret == 0)
				{ 
					printf("connection has been closed by peer\n");
					_aiop->unregister_fd(fd);
					::close(fd);
				}
				printf("recieved %d bytes from %d\n",rcv_cnt, fd);
//}
			}

			if((aio_events & AIO_POLLOUT) == AIO_POLLOUT)
			{
				printf("conn received an AIO_POLLOUT\n");
/*
                                const int BUF_SIZE=1025;
                                char buf[BUF_SIZE];
                                int ret=::send(fd, buf, BUF_SIZE, 0);
				int sent_cnt=0;
				while(ret > 0) { sent_cnt+=ret; ret=::send(fd, buf, BUF_SIZE, 0); }
				if(ret == 0)
				{ 
					printf("connection has been closed by peer\n");
					_aiop->unregister_fd(fd);
					::close(fd);
				}
				printf("sent %d bytes from %d\n", sent_cnt, fd);
*/
				return;				
			}
                        
			if((aio_events & AIO_POLLERR) == AIO_POLLERR)
                        {
                                printf("conn socket received an AIO_POLLERR\n");
				_aiop->unregister_fd(fd);
				::close(fd);

				return;
                        }
                        if((aio_events & AIO_POLLHUP) == AIO_POLLHUP)
                        {
                                printf("conn socket received an AIO_POLLHUP\n");
				_aiop->unregister_fd(fd);
				::close(fd);

				return;
                        }
			return;
		}
	}

	void *lookup(uint32 iid, uint32 pin) throw()
	{
        	switch(iid)
        	{
        	case IID_self:
			if(pin != PIN_self) return 0;
        	case IID_lookup:
			if(pin != PIN_lookup) return 0;
                	return this;

        	case IID_aio_event_handler:
			if(pin != PIN_aio_event_handler) return 0;
                	return static_cast<aio_event_handler_it*>(this);

        	default:
                	return ref_cnt_impl_t::lookup(iid, pin);
        	}
	}
private:
	sp_aiop_t _aiop;
	bool _listen_flag;
};

#define SERVPORT 8888
#define BACKLOG 128

int main(int argc,char **argv)
{
	bool is_svr=true;
	int conn_cnt=0;

	if(argc > 1)
	{
		if(strcmp(argv[1],"client") == 0) is_svr=false;
	}
	if(is_svr)
		printf("==as is running as server==\n");
	else
		printf("==as is running as client==\n");

	sp_aiop_t aiop=aio_provider_t::s_create();
	aiop->init_hb_thd();

	sp_aiosap_t aio_sap((aio_sap_it*)aiop->lookup(IID_aio_sap, PIN_aio_sap), true);

        fyfd_t sockfd; 
        struct sockaddr_in svr_addr;
        struct sockaddr_in remote_addr;

        if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
                perror("fail to create a socket descriptor to listen!\n");
                exit(1);
        }
	if(is_svr)
	{
        	int opt=1;
        	setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(int));
	}
        //set server address
        svr_addr.sin_family=PF_INET; //protocol family
        svr_addr.sin_port=htons(SERVPORT); //listening port number--transfer short type to network sequence
        svr_addr.sin_addr.s_addr = INADDR_ANY; //IP address(autodetect)

	if(is_svr)
	{
        	//bind socket to address
        	if (bind(sockfd, (struct sockaddr *)&svr_addr, sizeof(struct sockaddr)) == -1) {
                	perror("bind failed\n");
                	exit(1);
        	}
	}

	sp_aioeh_t aio_eh(new test_aio_eh_t(aiop, is_svr), true);

	aiop->register_fd(0, sockfd, aio_eh);

	if(is_svr)// as server, listen on
	{
        	//listen on address and port
        	if (listen(sockfd, BACKLOG) == -1) {
                	perror("listen failed\n");
                	exit(1);
        	}
	}
	else //as client, connect to
	{
        	int ret_conn=connect(sockfd, (struct sockaddr *)&svr_addr, sizeof(struct sockaddr));
        	if(ret_conn == 0) printf("%d:ret_conn=%d\n",++conn_cnt,ret_conn);
        	if ( ret_conn== -1) {
                	if(errno == EAGAIN)
                	{
                        	printf("try again later ...\n");
                	}
                	else if(errno == EINPROGRESS)
                	{
                        	printf("errno=EINPROGRESS\n");

                	}
                	else
                	{
                        	printf("exit on error:errno=%d\n",errno);
                        	exit(1);
                	}
		}
		g_conn_fd=sockfd;
	}

	int8 ret_hb=0;
	aiop->set_max_slice(100);
	const int BUF_SIZE=32768;
	char buf[BUF_SIZE];
	//88
	bool sent_flag=false;
	while(true)
	{
		ret_hb=aiop->heart_beat();
		printf("heart_beat99,ret=%d\n",ret_hb);

		if(g_conn_fd && !sent_flag)
		{
			sent_flag=true;
			int sent_cnt=0;
			int ret=::send(g_conn_fd, buf, BUF_SIZE, 0);
/*			while(ret > 0)
			{
				sent_cnt+=ret;
				ret=::send(g_conn_fd, buf, BUF_SIZE, 0);
			}
*/			printf("sent %d bytes from %d\n",sent_cnt, g_conn_fd);
		}
	}

	aiop->unregister_fd(sockfd);
	::close(sockfd);
	sockfd=0;
	
	return 0;
}


