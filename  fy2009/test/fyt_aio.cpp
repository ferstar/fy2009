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
#ifdef LINUX
#define FY_TEST_AIO
#endif

#ifdef FY_TEST_AIO

#include "fy_aio.h"

USING_FY_NAME_SPACE

#ifdef LINUX

#include <sys/socket.h>
#include <netinet/in.h>

#define fy_close_sok close

#elif defined(WIN32)

#define fy_close_sok closesocket

LPFN_ACCEPTEX lpfnAcceptEx = NULL;
GUID GuidAcceptEx = WSAID_ACCEPTEX;

#ifdef __ENABLE_COMPLETION_PORT__

FY_OVERLAPPED a_ovlp; //accept overlapped
#define MAX_BUF_SIZE 1024
char r_buf[MAX_BUF_SIZE];
char s_buf[MAX_BUF_SIZE];
FY_OVERLAPPED r_ovlp; //receive overlapped
FY_OVERLAPPED s_ovlp; //send overlapped

#endif //__ENABLE_COMPLETION_PORT__
#endif //LINUX


//test aio
int32 g_conn_fd=0;
int32 g_listen_fd=0;
bool read_flag=false;

class test_aio_eh_t : public aio_event_handler_it,
			public ref_cnt_impl_t
{
public:
	test_aio_eh_t(sp_aiop_t aiop, bool listen_flag=false): _listen_flag(listen_flag),
			_aiop(aiop)
	{
	}
	void on_aio_events(int32 fd, uint32 aio_events, pointer_box_t ex_para)
	{
		FY_INFOI("==on_aio_events is called,_listen_flag:"<<(int8)_listen_flag<<",fd:"<<(uint32)fd
			<<",aio_events:"<<aio_events);
		if(_listen_flag) //as server
		{
			if((aio_events & AIO_POLLIN) == AIO_POLLIN)
			{
				FY_INFOI("listen socket received an AIO_POLLIN");
#ifdef LINUX
				int32 conn_fd=::accept(fd, 0, 0);
                if(conn_fd == -1) 
				{
					FY_ERROR("accept failed");
                        		return;
				}
#elif defined(WIN32)
#ifdef __ENABLE_COMPLETION_PORT__
				LPFY_OVERLAPPED p_ovlp= (LPFY_OVERLAPPED)ex_para;
				if(!p_ovlp)
				{
					FY_ERROR("invalid overlapped pointer");
					return;
				}
				int32 conn_fd= p_ovlp->fd;
				_listen_flag = false; //indicate connection has been accepted
#endif //iocp
#endif
            	FY_INFOD("accepted an incoming connection:"<<(uint32)conn_fd
						<<",_aiop is null?"<<(int8)_aiop.is_null());
				g_conn_fd=conn_fd;
#ifdef __ENABLE_COMPLETION_PORT__
				//sync properties of accepted socket with listening socket
				int32 err = setsockopt( g_conn_fd, 
					SOL_SOCKET, 
					SO_UPDATE_ACCEPT_CONTEXT, 
					(char *)&g_listen_fd, 
					sizeof(g_listen_fd) );
				if(err == SOCKET_ERROR)
				{
					int32 wsa_err=WSAGetLastError();
					FY_ERROR("sync properties of accepted socket failed, err:"<<wsa_err);
					fy_close_sok(g_conn_fd);
					return;
				}
				else
				{
					FY_INFOD("Sync properties of accpeted socket successfully");
				}
				//asyn receive request on accepted socket
				ZeroMemory(&(r_ovlp.overlapped), sizeof(OVERLAPPED));
				r_ovlp.transferred_bytes = 0;
				r_ovlp.fd = g_conn_fd;
				r_ovlp.aio_events = AIO_POLLIN;
				uint32 recv_bytes=0, flags=0;
				if(WSARecv(g_conn_fd, &(r_ovlp.wsa_buf), 1, &recv_bytes, &flags,
					&(r_ovlp.overlapped), NULL) == SOCKET_ERROR)
				{
					int32 wsa_err=WSAGetLastError();
					if (wsa_err != ERROR_IO_PENDING)
					{
						FY_ERROR("WSARecv fail:"<<wsa_err);
						fy_close_sok(g_conn_fd);
						return;
					}
				}
				FY_INFOD("asyn receive is pending...");
#endif
				return;	
			}
			if((aio_events & AIO_POLLERR) == AIO_POLLERR)
			{
				FY_INFOD("listen socket received an AIO_POLLERR");
				_aiop->unregister_fd(fd);
				fy_close_sok(fd);
				
				return;
			}
			if((aio_events & AIO_POLLHUP) == AIO_POLLHUP)
			{
				FY_INFOD("listen socket received an AIO_POLLHUP");
				_aiop->unregister_fd(fd);
				fy_close_sok(fd);

				return;
			}
			FY_INFOD("listen socket received an unknow event");

			return;
		}
		else //connection 
		{
			if((aio_events & AIO_POLLIN) == AIO_POLLIN)
			{
				FY_INFOD("conn received an AIO_POLLIN");
//if(!read_flag)
//{
//read_flag=true;
#ifdef LINUX
				const int BUF_SIZE=1024;
				char buf[BUF_SIZE];
				int ret=::recv(fd, buf, BUF_SIZE, 0);
				FY_INFOD("<<<<received:"<<(int32)ret);
				int rcv_cnt=0;
				while(ret > 0) {rcv_cnt+= ret; ret=::recv(fd, buf, BUF_SIZE, 0); }	
				if(ret == 0)
				{ 
					FY_INFOD("connection has been closed by peer");
					_aiop->unregister_fd(fd);
					fy_close_sok(fd);
				}
#elif defined(WIN32)
#ifdef __ENABLE_COMPLETION_PORT__
				LPFY_OVERLAPPED p_ovlp= (LPFY_OVERLAPPED)ex_para;
				if(!p_ovlp)
				{
					FY_ERROR("invalid overlapped pointer");
					return;
				}
				int rcv_cnt = p_ovlp->transferred_bytes;
#endif //iocp				
#endif
				FY_INFOD("recieved "<<(int32)rcv_cnt<<" bytes from "<<(uint32)fd);
#ifdef __ENABLE_COMPLETION_PORT__
				//asyn receive request
				ZeroMemory(&(r_ovlp.overlapped), sizeof(OVERLAPPED));
				r_ovlp.transferred_bytes = 0;
				r_ovlp.fd = fd;
				r_ovlp.aio_events = AIO_POLLIN;
				uint32 recv_bytes=0, flags=0;
				if(WSARecv(fd, &(r_ovlp.wsa_buf), 1, &recv_bytes, &flags,
					&(r_ovlp.overlapped), NULL) == SOCKET_ERROR)
				{
					int32 wsa_err=WSAGetLastError();
					if (wsa_err != ERROR_IO_PENDING)
					{
						FY_ERROR("WSARecv fail,err:"<<wsa_err);
						fy_close_sok(fd);
						return;
					}
				}
				FY_INFOD("next asyn receive is pending...");
#endif //__ENABLE_COMPLETION_PORT__
//}
				return;
			}

			if((aio_events & AIO_POLLOUT) == AIO_POLLOUT)
			{
				FY_INFOD("conn received an AIO_POLLOUT");

#ifdef LINUX
                const int BUF_SIZE=1025;
                char buf[BUF_SIZE];
                int ret=::send(fd, buf, BUF_SIZE, 0);
				int sent_cnt=0;
				while(ret > 0) { sent_cnt+=ret; ret=::send(fd, buf, BUF_SIZE, 0); }
				if(ret == 0)
				{ 
					FY_INFOD("connection has been closed by peer");
					_aiop->unregister_fd(fd);
					fy_close_sok(fd);
				}
#elif defined(WIN32)
#ifdef __ENABLE_COMPLETION_PORT__
				LPFY_OVERLAPPED p_ovlp= (LPFY_OVERLAPPED)ex_para;
				if(!p_ovlp)
				{
					FY_ERROR("invalid overlapped pointer");
					return;
				}
				int sent_cnt=p_ovlp->transferred_bytes;
#endif //iocp
#endif
				FY_INFOD("sent "<<(int32)sent_cnt<<" bytes from "<<(uint32)fd);
#ifdef __ENABLE_COMPLETION_PORT__
				//asyn send request
				ZeroMemory(&(s_ovlp.overlapped), sizeof(OVERLAPPED));
				s_ovlp.transferred_bytes = 0;
				s_ovlp.fd = fd;
				s_ovlp.aio_events = AIO_POLLOUT;
				uint32 send_bytes=0, flags=0;
				if(WSASend(fd, &(s_ovlp.wsa_buf), 1, & send_bytes, flags,
					&(s_ovlp.overlapped), NULL) == SOCKET_ERROR)
				{
					if (WSAGetLastError() != ERROR_IO_PENDING)
					{
						FY_ERROR("WSASend fail");
						fy_close_sok(fd);
						return;
					}
				}
				FY_INFOD("next asyn send is pending...");
#endif //__ENABLE_COMPLETION_PORT__
				return;				
			}
                        
			if((aio_events & AIO_POLLERR) == AIO_POLLERR)
            {
                FY_INFOD("conn socket received an AIO_POLLERR");
				_aiop->unregister_fd(fd);
				fy_close_sok(fd);

				return;
            }
            if((aio_events & AIO_POLLHUP) == AIO_POLLHUP)
            {
				FY_INFOD("conn socket received an AIO_POLLHUP");
				_aiop->unregister_fd(fd);
				fy_close_sok(fd);

				return;
            }
			FY_INFOD("conn socket received an unknown event");
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
	trace_provider_t *trace_prvd=trace_provider_t::instance();
    trace_prvd->open();
	trace_prvd->set_enable_flag(TRACE_LEVEL_IO,true);

    trace_provider_t::tracer_t *tracer=trace_prvd->register_tracer();

#ifdef WIN32

	WSADATA wsa_data;
	WSAStartup(0x0202, &wsa_data);

#ifdef __ENABLE_COMPLETION_PORT__

a_ovlp.aio_events = AIO_POLLIN;

r_ovlp.wsa_buf.buf=r_buf;
r_ovlp.wsa_buf.len = MAX_BUF_SIZE;
r_ovlp.aio_events  = AIO_POLLIN;

s_ovlp.wsa_buf.buf=s_buf;
s_ovlp.wsa_buf.len=MAX_BUF_SIZE;
s_ovlp.aio_events = AIO_POLLOUT;

#endif //__ENABLE_COMPLETION_PORT__
#endif //WIN32
	
	bool is_svr=true;
	int conn_cnt=0;

	if(argc > 1)
	{
		if(strcmp(argv[1],"client") == 0) is_svr=false;
	}
	if(is_svr)
		FY_INFOD("==as is running as server==");
	else
		FY_INFOD("==as is running as client==");

	sp_aiop_t aiop=aio_provider_t::s_create();
	aiop->init_hb_thd();

	sp_aiosap_t aio_sap((aio_sap_it*)aiop->lookup(IID_aio_sap, PIN_aio_sap), true);

    int32 sockfd; 
#ifdef LINUX
    struct sockaddr_in svr_addr;
    struct sockaddr_in remote_addr;
#elif defined(WIN32)
	SOCKADDR_IN svr_addr;
	SOCKADDR_IN remote_addr;
#endif
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) 
	{
#ifdef WIN32
		FY_INFOD("fail to create socket to listen,error:"
			<<(uint32)WSAGetLastError());
#else
        FY_ERROR("Fail to create a socket descriptor to listen");
#endif
        return 0;
    }

	if(is_svr)
	{
        int opt=1;
        setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(const char *)&opt,sizeof(int));
	}
    //set server address
    svr_addr.sin_family=PF_INET; //protocol family
    svr_addr.sin_port=htons(SERVPORT); //listening port number--transfer short type to network sequence
    svr_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //IP address(autodetect)

	if(is_svr)
	{
#ifdef WIN32
#ifdef __ENABLE_COMPLETION_PORT__
		//----------------------------------------
		// Load the AcceptEx function into memory using WSAIoctl.
		// The WSAIoctl function is an extension of the ioctlsocket()
		// function that can use overlapped I/O. The function's 3rd
		// through 6th parameters are input and output buffers where
		// we pass the pointer to our AcceptEx function. This is used
		// so that we can call the AcceptEx function directly, rather
		// than refer to the Mswsock.lib library.
		DWORD dwBytes;
		WSAIoctl(sockfd, 
			SIO_GET_EXTENSION_FUNCTION_POINTER, 
			&GuidAcceptEx, 
			sizeof(GuidAcceptEx),
			&lpfnAcceptEx, 
			sizeof(lpfnAcceptEx), 
			&dwBytes, 
			NULL, 
			NULL);

#endif //completion port
#endif 
		//bind socket to address
		if (bind(sockfd, (struct sockaddr *)&svr_addr, sizeof(struct sockaddr)) == -1) 
		{
			FY_ERROR("bind failed");
			return 0;
		}
	}
	sp_aioeh_t aio_eh(new test_aio_eh_t(aiop, is_svr), true);
	aiop->register_fd(0, sockfd, aio_eh);
	FY_INFOD("register_fd, is_svr:"<<(int8)is_svr<<",fd:"<<(int32)sockfd);

	if(is_svr)// as server, listen on
	{
		//listen on address and port
		if (listen(sockfd, BACKLOG) == -1) 
		{
			FY_ERROR("listen failed");
			return 0;
		}
		g_listen_fd = sockfd;

#ifdef WIN32
#ifdef __ENABLE_COMPLETION_PORT__
		//asyn accept request on listen socket
		ZeroMemory(&(a_ovlp.overlapped), sizeof(OVERLAPPED));
		a_ovlp.fd = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		sp_aioeh_t aio_eh(new test_aio_eh_t(aiop, true/*to indicate it's not accepted*/), true);
		aiop->register_fd(0, a_ovlp.fd, aio_eh);

		uint32 bytes;
		if(lpfnAcceptEx(sockfd, 
			a_ovlp.fd,
			r_buf, 
			0, //don't receive first block
			sizeof(sockaddr_in) + 16, 
			sizeof(sockaddr_in) + 16, 
			&bytes, 
			&(a_ovlp.overlapped)) == 0)
		{
			if (WSAGetLastError() != ERROR_IO_PENDING)
			{
				FY_ERROR("lpfnAcceptEx fail");
				return 0;
			}
		}
#endif //completion port
#endif
	}
	else //as client, connect to
	{
		int32 ret_conn=connect(sockfd, (struct sockaddr *)&svr_addr, sizeof(struct sockaddr));
#ifdef LINUX
		if ( ret_conn== -1) 
		{
			if(errno == EAGAIN)
			{
				FY_INFOD("try again later ...");
			}
			else if(errno == EINPROGRESS)
			{
				FY_INFOD("errno=EINPROGRESS");

			}
			else
			{
				FY_ERROR("exit on error:errno="<<(uint32)errno);
				return 0;
			}
		}
#elif defined(WIN32)
		if(ret_conn == SOCKET_ERROR)
		{
			int32 err=WSAGetLastError();
			if( err ==  WSAEWOULDBLOCK)
			{
				FY_INFOD("connect request is pending...");
			}
			else
			{
				FY_ERROR("connect request failed,err:"<<err);
				return 0;
			}
		}
		else
		{
			FY_INFOD("succeeded in connecting");
		}
#endif
		g_conn_fd=sockfd;
	}

	int8 ret_hb=0;
	aiop->set_max_slice(1000);
	const int BUF_SIZE=32768;
	char buf[BUF_SIZE];
	//88
	bool sent_flag=false;
	while(true)
	{
		ret_hb=aiop->heart_beat();
		FY_INFOD("heart_beat,ret="<<ret_hb);

		if(g_conn_fd && !sent_flag)
		{
			sent_flag=true;
			int sent_cnt=0;
#ifdef LINUX
			int ret=::send(g_conn_fd, buf, BUF_SIZE, 0);
			FY_INFOD("sent "<<(int32)sent_cnt<<" bytes from "<<(uint32)g_conn_fd);
#elif defined(WIN32)
#ifdef __ENABLE_COMPLETION_PORT__
				//asyn send request
				ZeroMemory(&(s_ovlp.overlapped), sizeof(OVERLAPPED));
				s_ovlp.transferred_bytes = 0;
				s_ovlp.fd = g_conn_fd;
				s_ovlp.aio_events = AIO_POLLOUT;
				uint32 send_bytes=0, flags=0;
				if(WSASend(g_conn_fd, &(s_ovlp.wsa_buf), 1, & send_bytes, flags,
					&(s_ovlp.overlapped), NULL) == SOCKET_ERROR)
				{
					int32 err=WSAGetLastError();
					if (err != ERROR_IO_PENDING)
					{
						FY_ERROR("WSASend fail,err:"<<err);
						fy_close_sok(g_conn_fd);
						return 0;
					}
				}
				FY_INFOD("asyn send is pending...");
				//asyn receive request
				ZeroMemory(&(r_ovlp.overlapped), sizeof(OVERLAPPED));
				r_ovlp.transferred_bytes = 0;
				r_ovlp.fd = g_conn_fd;
				r_ovlp.aio_events = AIO_POLLIN;
				uint32 recv_bytes=0;
				flags=0;
				if(WSARecv(g_conn_fd, &(r_ovlp.wsa_buf), 1, &recv_bytes, &flags,
					&(r_ovlp.overlapped), NULL) == SOCKET_ERROR)
				{
					int32 wsa_err=WSAGetLastError();
					if (wsa_err != ERROR_IO_PENDING)
					{
						FY_ERROR("WSARecv fail,err="<<wsa_err);
						fy_close_sok(g_conn_fd);
						return 0;
					}
				}
				FY_INFOD("asyn receive is pending...");
#endif
#endif
		}
	}

	aiop->unregister_fd(sockfd);
	fy_close_sok(sockfd);

#ifdef WIN32

	WSACleanup(); 

#endif
    trace_prvd->unregister_tracer();
    trace_prvd->close();

	return 0;
}

#endif //FY_TEST_AIO
