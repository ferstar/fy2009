/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@hotmail.com
 *
 * [History]
 * initialize: 2009-8-20
 * ====================================================================
 */
#ifdef LINUX
#define FY_TEST_SOK
#endif

#ifdef FY_TEST_SOK

#include "fy_socket.h"

USING_FY_NAME_SPACE
/*
#define SERVPORT 8888
#define BACKLOG 128

class listen_thread : public thread_t
{
public:
	listen_thread(sp_aiosap_t aio_prvd)
	{
		_aio_prvd = aio_prvd;
	}
	void run()
	{
                _lisner=socket_listener_t::s_create(true);
                in_addr_t lisn_addr=(in_addr_t)htonl(INADDR_ANY);

		//******test proves incoming rate control works well, 2008-10-10
		_lisner->set_ctrl_window(20);
                _lisner->set_max_incoming_cnt_inwin(50);

		aio_proxy_t *raw_aio_proxy=get_aio_proxy();
		sp_aiosap_t aio_proxy((aio_sap_it*)raw_aio_proxy, true);
                _lisner->listen(aio_proxy, _aio_prvd, lisn_addr, SERVPORT);
		msg_proxy_t *msg_proxy=get_msg_proxy();
		int hb_ret_msg, hb_ret_aio;
		_unlock_start();
		while(1)
		{
			hb_ret_aio=raw_aio_proxy->heart_beat();
			hb_ret_msg=msg_proxy->heart_beat();

			if(hb_ret_aio == RET_HB_IDLE && hb_ret_msg == RET_HB_IDLE)
			{
				_on_idle(400);
			}
			if(_is_stopping()) break;
		}
	}

        void on_cancel()
        {
		_lisner->stop_listen();       
                printf("thread has been canceled\n");
        }
private:
	sp_listener_t _lisner;   
	sp_aiosap_t _aio_prvd;    
};

//test listener in thread_pool
class listener_msg_rcver : public msg_receiver_it
{
public:
	void on_msg(msg_t *msg)
	{
	}	
};

const int32 MSG_TEST_TIMER = MSG_USER+100;
class test_conn_t : public socket_connection_t
{
public:
	void init_send()
	{
		_susppend_pollout();			
	}
	void *lookup(uint32 iid) throw()
	{
        	switch(iid)
        	{
        	case IID_self:
        	case IID_lookup:
                	return this;
		default:
			return socket_connection_t::lookup(iid);
		}
	}

	void on_msg(msg_t *msg)
	{
		switch(msg->get_msg())
		{
		case MSG_TEST_TIMER:
			printf("++++total: in bytes: %d, out bytes: %d\n", _accu_in_bytes, _accu_out_bytes);
			break;
		default:
			socket_connection_t::on_msg(msg);
			break;
		}
	}
protected:
	void _on_pollin()
	{
		char buf[100];

		//raw socket operation
		while(true)
		{
			int read_cnt=::recv(_fd, buf, 100,0);
			if(read_cnt > 0)
			{
				_in_bytes_inwin+=read_cnt;
				_accu_in_bytes += read_cnt;
				if(_in_bytes_inwin >= _max_in_bytes_inwin)
				{
					printf("!!!in bytes(%d) reachs max_in_bytes_inwin(%d)\n", _in_bytes_inwin, 
						_max_in_bytes_inwin);
					_susppend_pollin();

					return;
				}	
			}
			else if(read_cnt == 0)
			{
				printf("connection is closed by peer\n");
				return;
			}
			else
			{
				if(errno == EAGAIN || errno == EINTR) return;
			}
		}


		//stream interface operation
		stream_it *stm=(stream_it*)this;
		while(1)
		{
			int32 recv_bytes=stm->read(buf, 100);
			 _accu_in_bytes+=recv_bytes;

			if(!recv_bytes) break;
		}


		//iovec_it interface operation
		iovec_it *p_iov=(iovec_it*)this;
		struct iovec iov[3];
		char cc[3][100];
		for(int i=0; i<3; ++i)
		{
			iov[i].iov_base =&cc[i];
			iov[i].iov_len=100;
		} 		
		while(1)
		{
			int32 recv_bytes = p_iov->readv(iov, 3);
			_accu_in_bytes += recv_bytes;
		
			if(!recv_bytes) break;
		}
	}
	
	void _on_pollout()
	{
                char buf[99];

		//raw socket operation
                while(true)
                {
                        int send_cnt=::send(_fd, buf, 99,0);
                        if(send_cnt > 0)
                        {
                                _out_bytes_inwin+=send_cnt;
				_accu_out_bytes+=send_cnt;
                                if(_out_bytes_inwin >= _max_out_bytes_inwin)
                                {
                                        printf("!!!out bytes(%d) reachs max_out_bytes_inwin(%d)\n", _out_bytes_inwin,
                                                _max_out_bytes_inwin);
					_susppend_pollout();

                                        return;
                                }
                        }
                        else if(send_cnt == 0)
                        {
                                printf("connection is closed by peer\n");
                                return;
                        }
                        else
                        {
                                if(errno == EAGAIN || errno == EINTR) return;
                        }
                }    


		//stream interface operation
		stream_it *stm=(stream_it*)this;
		while(1)
		{
			int32 snd_bytes=stm->write(buf, 99);
			_accu_out_bytes+=snd_bytes;

			if(!snd_bytes) break;
		} 

		//iovec_it interface operation
                iovec_it *p_iov=(iovec_it*)this;
                struct iovec iov[3];
                char cc[3][100];
                for(int i=0; i<3; ++i)
                {
                        iov[i].iov_base =&cc[i];
                        iov[i].iov_len=100;
                }
                while(1)
                {
                        int32 snd_bytes = p_iov->writev(iov, 3);
                        _accu_out_bytes += snd_bytes;

                        if(!snd_bytes) break;
                }           
	}

	void _on_msg_pollerr()
	{
		int32 fd=detach();
		::close(fd);
		socket_connection_t::_on_msg_pollerr();

                sp_msg_t msg=msg_t::s_create(MSG_TEST_TIMER, 0, false);
                sp_msg_rcver_t rcver((msg_receiver_it*)this, true);
                msg->set_receiver(rcver);
                msg->set_repeat(-1);
                msg->set_tc_interval(100);
                msg_proxy_t *msg_proxy=msg_proxy_t::s_tls_instance();
                msg_proxy->post_msg(msg);	
	}
	void _on_msg_pollhup()
	{
		int32 fd=detach();
		::close(fd);
		socket_connection_t::_on_msg_pollhup();
	}
	void _on_msg_close_by_peer()
	{
		int32 fd=detach();
		::close(fd);
		socket_connection_t::_on_msg_close_by_peer();
	}	
private:
	int32 _accu_in_bytes;
	int32 _accu_out_bytes;
};

class test_listener_t : public socket_listener_t
{
public:
	test_listener_t(sp_aiosap_t aio_prvd)
	{
		this->aio_prvd = aio_prvd;
	}
        void *lookup(uint32 iid) throw()
        {
                switch(iid)
                {
                case IID_self:
                case IID_lookup:
                        return this;
                default:
                        return socket_listener_t::lookup(iid);
                }
        }
protected:
	void _on_incoming(int32 incoming_fd, in_addr_t remote_addr, uint16 remote_port,
                                        in_addr_t local_addr, uint16 local_port)
	{
		test_conn_t *my_conn=new test_conn_t();
		my_conn->add_reference();

                my_conn->set_max_bytes_inwin(1000, true);
                my_conn->set_max_bytes_inwin(1200, false);

                my_conn->set_ctrl_window(20);

		my_conn->attach(aio_prvd, aio_prvd, incoming_fd, local_addr, local_port, remote_addr, remote_port);
		my_conn->init_send();

		sp_msg_t msg=msg_t::s_create(MSG_TEST_TIMER, 0, false);	
       		sp_msg_rcver_t rcver((msg_receiver_it*)my_conn, true);
        	msg->set_receiver(rcver);
		msg->set_repeat(-1);
		msg->set_tc_interval(100);
		msg_proxy_t *msg_proxy=msg_proxy_t::s_tls_instance();
		msg_proxy->post_msg(msg);

		my_conn->release_reference();	
	}
private:
	sp_aiosap_t aio_prvd;	
};
*/ 
bool g_stop_flag=false;

void sig_usr2(int sig)
{
	switch(sig)
	{
	case SIGUSR2:
		g_stop_flag=true;
		break;		
	}
}

//test socket_util_t
void test_ifr_get_name_list()
{
	bb_t if_names[100];
	uint16 if_cnt=socket_util_t::s_ifr_get_name_list(if_names, 100);
	printf("===this machine has installed following network interface cards:====\n");
	for(uint16 i=0;i<if_cnt;++i)
	{
		if(if_names[i].get_filled_len()) printf("if card[ifindex=%d]: %s\n", i+1, if_names[i].get_buf());
	}
}

void test_ifr_get_mtu()
{
	int8 if_name[]="eth0";
	printf("if device:%s, MTU=%d\n", if_name, socket_util_t::s_ifr_get_mtu(if_name));	
}

void test_ifr_get_mac()
{
	mac_addr_t mac;
	if(socket_util_t::s_ifr_get_mac_addr("eth0", mac)) 
	{
		perror("get MAC address fail\n");	
		return;
	}
	printf("==MAC address==\n");
	for(int i=0; i<MAC_ADDR_LEN; ++i) printf("MAC[%d]=%d\n",i, mac[i]);

	bb_t mac_str;
	if(socket_util_t::s_convert_mac_addr(mac, mac_str, true))
	{
		perror("convert MAC to readable fail\n");
		return;
	}
	printf("MAC address string:%s\n", mac_str.get_buf());
	mac_addr_t mac_1;
	if(socket_util_t::s_convert_mac_addr(mac_1, mac_str, false))
	{
		perror("convert MAC string to MAC fail\n");
		return;
	}
	printf("==reverse to convert MAC address from string==\n");
	for(int i=0; i<MAC_ADDR_LEN; ++i) printf("rMAC[%d]=%d\n",i, mac_1[i]);

	bb_t mac_str_1;
        if(socket_util_t::s_convert_mac_addr(mac_1, mac_str_1, true))
        {
                perror("repeat to convert MAC to readable fail\n");
                return;
        }
        printf("repeat convert MAC address string:%s\n", mac_str_1.get_buf());	
}

void test_uuid()
{
	for(int i=0; i<10; ++i)
	{
		uuid_t uuid;
		uuid_util_t::uuid_clear(uuid);
		uuid_util_t::uuid_generate(uuid);
	
		char uuid_str[128];
		uuid_util_t::uuid_unparse(uuid, uuid_str);
		printf("       uuid[%d]=%s\n", i, uuid_str);

		uuid_t uuid_1;
		uuid_util_t::uuid_parse(uuid_str, uuid_1);
		char uuid_str_1[128];
		uuid_util_t::uuid_unparse(uuid_1, uuid_str_1);
		printf("repeat uuid[%d]=%s\n", i, uuid_str_1);
	}
}

int main(int argc,char **argv)
{
	//test ifr
	//->
	//test_ifr_get_name_list(); //pass, 2008-11-21
	//test_ifr_get_mtu(); //pass, 2008-11-21
	//test_ifr_get_mac(); //pass, 2008-11-21
	//test_uuid(); //pass, 2008-11-21

	return 0;
	//<-
/*
	signal(SIGUSR2,sig_usr2);

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

        trace_provider_t *trace_prvd=trace_provider_t::instance();
	trace_prvd->set_enable_flag(TRACE_LEVEL_IO,true);
	trace_prvd->open();
        trace_provider_t::tracer_t *tracer=trace_prvd->register_tracer();

        sp_aiop_t aiop=aio_provider_t::s_create(1024, true);
        aiop->init_hb_thd();
	sp_aiosap_t aio_sap((aio_sap_it*)aiop->lookup(IID_aio_sap,PIN_aio_sap), true);
	msg_proxy_t *raw_msg_proxy= msg_proxy_t::s_tls_instance();
	
	sp_listener_t sok_lisner;
	const int CONN_CNT=8;
	sp_conn_t sok_conn,sok_conns[CONN_CNT];
	test_listener_t *raw_my_listener=0;
	test_conn_t *raw_my_conn=0;
	int conn_fd=INVALID_SOCKET;
	listen_thread thd(aio_sap);
	sp_tpool_t thd_pool;
	if(is_svr)
	{
*/
/*
		printf("==single thread listener==\n");
		//->
		sok_lisner=socket_listener_t::s_create();
		in_addr_t lisn_addr=(in_addr_t)htonl(INADDR_ANY);

		sok_lisner->set_max_incoming_cnt_inwin(2);

		sok_lisner->listen(aio_sap, aio_sap, lisn_addr, SERVPORT);	
		//<-
*/
/*
		printf("==multi-thread listner==\n");
		//->
		thd.enable_aio();
		thd.enable_msg();
		thd.enable_trace();

		thd.start();	
		//<-
*/
/*
		printf("==test listener in thread-pool==\n");
		//->
		thd_pool=thread_pool_t::s_create(1);
		sp_thd_t thd=thd_pool->assign_thd(0);

                sok_lisner=socket_listener_t::s_create();
                in_addr_t lisn_addr=(in_addr_t)htonl(INADDR_ANY);

                sok_lisner->set_max_incoming_cnt_inwin(2);
		sok_lisner->post_listen(thd, aio_sap, lisn_addr, SERVPORT);
		//<-

		//printf("==test data receiving/sending==\n");
		//->
		raw_my_listener = new test_listener_t(aio_sap);
		raw_my_listener->add_reference();

		raw_my_listener->listen(aio_sap, aio_sap, htonl(INADDR_ANY), SERVPORT);
		//<-	
	}
       else //as client, connect to
        {
*/
/*
		printf("==raw socket client==\n");
		//->
        	//set server address
		struct sockaddr_in svr_addr;
        	svr_addr.sin_family=PF_INET; //protocol family
       	 	svr_addr.sin_port=htons(SERVPORT); //listening port number--transfer short type to network sequence
        	svr_addr.sin_addr.s_addr = INADDR_ANY; //IP address(autodetect)

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
				CCP_ERROR("::connect to server failure:errno="<<(uint32)errno);
				::close(conn_fd);
				conn_fd=0;
				usleep(1000000);
			}
			else
			{
				CCP_INFOD("+++++sueedeed in connect,fd:"<<(int32)conn_fd);
				::close(conn_fd);
				conn_fd=0;
			}
			usleep(100);
		}
		//<-
*/
/*
		printf("==test socket_connect_t in main thread==\n");
		//->
		sok_conn=socket_connection_t::s_create(false);	
		sok_conn->connect(aio_sap, aio_sap, INADDR_ANY, SERVPORT, 16777343, 19999);	
		//<-
*/
/*
		printf("==test socket_connect_t in thread-pool==\n");
		//->
		thd_pool=thread_pool_t::s_create(4);	
		for(int i=0; i<CONN_CNT; ++i)
		{
			sok_conns[i]=socket_connection_t::s_create(true);
			uint16 thd_idx=0;
			sp_thd_t thd=thd_pool->assign_thd(&thd_idx);
			printf("assigned thd idx:%d for conn[%d]\n",thd_idx, i);
			sok_conns[i]->post_connect(thd, aio_sap, INADDR_ANY, SERVPORT);
		}
		//<-
*/
/*		printf("==test data receiving/sending==\n");
		//->
		thd_pool=thread_pool_t::s_create(1);
		raw_my_conn=new test_conn_t();
		raw_my_conn->add_reference();
		raw_my_conn->set_max_bytes_inwin(1000, true);
		raw_my_conn->set_max_bytes_inwin(1200, false);

		raw_my_conn->set_ctrl_window(20);

		sp_thd_t thd=thd_pool->assign_thd(0);
		raw_my_conn->post_connect(thd, aio_sap, INADDR_ANY, SERVPORT);	

                sp_msg_t msg=msg_t::s_create(MSG_TEST_TIMER, 0, false);
                sp_msg_rcver_t rcver((msg_receiver_it*)raw_my_conn, true);
                msg->set_receiver(rcver);
                msg->set_repeat(-1);
                msg->set_tc_interval(100);
                msg_proxy_t *msg_proxy=thd->get_msg_proxy();
                msg_proxy->post_msg(msg);
		//<-
        }

        int8 ret_hb=0;
        aiop->set_max_slice(100);
        const int BUF_SIZE=32768;
        char buf[BUF_SIZE];
        while(true)
        {
		if(g_stop_flag)
		{
			printf("!!!stopped by user!!!!\n");
			break;
		}

                aiop->heart_beat();
                raw_msg_proxy->heart_beat();
        }

	if(is_svr && !sok_lisner.is_null())
	{
		sok_lisner->stop_listen();
	}
	else
	{
		if(conn_fd != INVALID_SOCKET) ::close(conn_fd);
	}
	if(!sok_conn.is_null())
	{
		sok_conn->close();		
	}
	for(int i=0;i<CONN_CNT; ++i)
	{
		if(!sok_conns[i].is_null())
		{ 
			sok_conns[i]->close();
		}
	}
	if(raw_my_listener) 
	{
		raw_my_listener->stop_listen();
		raw_my_listener->release_reference();
	}
	if(raw_my_conn) raw_my_conn->release_reference();

	thd.stop();
	thd_pool->stop_all();

        trace_prvd->unregister_tracer();
        trace_prvd->close();
	msg_proxy_t::s_delete_tls_instance();
 */
        return 0;
}

#endif //FY_TEST_SOK
