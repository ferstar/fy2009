/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2009-8-20
 * ====================================================================
 */
#include "fy_socket.h"

USING_FY_NAME_SPACE

#ifdef __FY_DEBUG_RECONNECT__

extern bool g_broke_side;

#endif

//socket_util_t
bool socket_util_t::is_nd_ip(const int8 *ip_addr)
{
        if(!ip_addr) return false;

        int8 d[3];
        int8 d_cnt=0;
        int8 b_cnt=0;
        const int8 *p_cur=ip_addr;

        while(true)
        {
                switch(*p_cur)
                {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                        if(d_cnt >= 3) return false; //max:255
                        if(d_cnt == 2 && (d[0] > '2' || (d[0] == '2' && (d[1] > '5' || *p_cur > '5')))) return false; //max:255
                        if(d_cnt == 1 && d[0] == '0') return false; //prefix '0' is invalid
                        d[d_cnt++]=*(p_cur++);

                        break;

                case '.':
                        if(b_cnt == 3) return false;//max:4bytes

                        d_cnt = 0;
                        ++b_cnt;
                        ++p_cur;

                        break;

                case ':':
                case '/':
                case '\0':
                        if(b_cnt !=3) return false; //max:4bytes
                        return true;

                case ' ':
                case '\t':
                        if(d_cnt == 0 && b_cnt == 0)//skip prefix white spaces
                        {
                                ++p_cur;
                                continue;
                        }
                        return false;

                default:
                        return false;

                }
        }

        return false;
}

void socket_util_t::s_in_addr_to_nd(in_addr_t in_addr_ip, bb_t& nd_addr)
{
	int8 *p=(int8*)&in_addr_ip;	
	nd_addr.reserve(16);
	short s[4]={0x00ff & p[0], 0x00ff & p[1], 0x00ff & p[2], 0x00ff & p[3]};
	::sprintf(nd_addr.get_buf(), "%u.%u.%u.%u", s[0], s[1], s[2], s[3]);
	nd_addr.set_filled_len(::strlen(nd_addr.get_buf())+1);
}

const int8 *socket_util_t::s_ip_address_parse_inplace(const int8 *ip_addr,in_addr_t *p_ip,uint16 *p_port)
{
	FY_DECL_EXCEPTION_CTX("socket_util_t", "s_ip_address_parse_inplace");

        FY_ASSERT(ip_addr);

        const int8 *url_end=strstr(ip_addr,":");
        const int8 *rest_str=0;

        if(!url_end) //no port part,format:<ip or url>[/...]
        {
                url_end=strstr(ip_addr,"/");
                if(url_end) //format:<ip or url>/...
                        rest_str=url_end+1;
                else //format:<ip or url>
                {
                        rest_str=ip_addr + strlen(ip_addr);
                        url_end = rest_str;
                }
        }
        else //format:<ip or url>:<port number>[/...]
        {
                const int8 *port_end=strstr(url_end,"/");
                if(port_end) //format: <ip or url>:<port></...>
                {
                        rest_str=port_end+1;
                }
                else //format: <ip or url>:<port>
                {
                        port_end = url_end + strlen(url_end); //refer to '\0'
                        rest_str=port_end;
                }
                int32 len_port_str= port_end - url_end - 1;
                if(len_port_str < 1) FY_THROW("sokuti-np", "no port number follows \':\'"); //format: <ip or url>:[/...]

		bb_t tmp_port_str;
		tmp_port_str.reserve(len_port_str+1);
		int8 *tmp_buf_bb=tmp_port_str.get_buf();

                tmp_buf_bb[len_port_str]=0;
                strncpy(tmp_buf_bb, url_end+1, len_port_str);
                if(p_port) *p_port=atoi(tmp_buf_bb);//convert port string to port number
        }

        if(p_ip)
        {
                //parse ip or url
                int32 len_url=url_end - ip_addr;
		bb_t url_bb;
		url_bb.reserve(len_url+1);
                int8 *url_str=url_bb.get_buf();

                url_str[len_url]=0;
                strncpy(url_str, ip_addr, len_url);

                uint32 len_neturl=0;
                string_util_t::s_trim_inplace(url_str, &len_neturl);
                if(len_neturl == 0) //no specified url, listener can listen on this kind url
                {
                        *p_ip=0;
                }
                else if(is_nd_ip(url_str)) //number-dot ip address string
                {
                        *p_ip=inet_addr(url_str);
                }
                else //url string
                {
                        //parse url
                        struct hostent he;
                        struct hostent* phe=0;
                        int herr=0;
                        int8 tmp_buf[1024]; //test indicates 512 bytes can work

                        //it's a very expensive call
                        if(gethostbyname_r(url_str,&he,tmp_buf,1024,&phe,&herr))//thread-safe,but gethostbyname isn't thread-safe
                        {
                                FY_THROW("sokuti-invurl", "parse url:"<<url_str<<" error:"<<(int32)herr);
                        }

                        *p_ip=reinterpret_cast<struct in_addr *>(he.h_addr_list[0])->s_addr;
                }
        }

        return rest_str;
}

uint16 socket_util_t::s_ifr_get_name_list(bb_t *out_ifnames, uint16 out_ifnames_size)
{
	int sock=socket(PF_INET, SOCK_STREAM, 0);
	struct ifreq ifr;

	//--interface card index starts from 1 but not 0
	uint16 i=0;
	for(i=1;i<=out_ifnames_size; ++i)
	{
		ifr.ifr_ifindex = i;
		if (-1==ioctl(sock, SIOCGIFNAME, &ifr)) 
		{
			::close(sock);
			return i-1;
		}
		if(!out_ifnames) continue;
		out_ifnames[i-1].fill_from(ifr.ifr_name, ::strlen(ifr.ifr_name) + 1, 0);
	}
	::close(sock);
	return i;	
}

int32 socket_util_t::s_ifr_get_mtu(const int8 *if_name)
{
	if(!if_name) return -1;

	int sock=socket(PF_INET, SOCK_STREAM, 0);
	struct ifreq ifr;
	::strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	
	if(-1==ioctl(sock, SIOCGIFMTU, &ifr))
	{
		::close(sock);
		return -1;
	}
	::close(sock);

	return ifr.ifr_mtu;		
}

int socket_util_t::s_ifr_get_mac_addr(const int8 *if_name, mac_addr_t out_mac_addr)
{
	if(!if_name) return -1;

        int sock=socket(PF_INET, SOCK_STREAM, 0);
        struct ifreq ifr;
        ::strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

        if(-1==ioctl(sock, SIOCGIFHWADDR, &ifr))
        {
                ::close(sock);
                return -1;
        }
	::memcpy(out_mac_addr, ifr.ifr_hwaddr.sa_data, MAC_ADDR_LEN);
        ::close(sock);

	return 0;
}

int socket_util_t::s_convert_mac_addr(mac_addr_t mac_addr, bb_t& mac_addr_string, bool to_string_flag)
{
	if(!mac_addr) return -1;

	if(to_string_flag)
	{
		mac_addr_string.reserve(MAC_ADDR_STRING_LEN+1);
		int8 *mac_str=mac_addr_string.get_buf();
		mac_str[mac_addr_string.get_size() - 1]='\0';
		::sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", 
			(int)(uint32)(uint8)mac_addr[0], 
			(int)(uint32)(uint8)mac_addr[1],
			(int)(uint32)(uint8)mac_addr[2], 
			(int)(uint32)(uint8)mac_addr[3], 
			(int)(uint32)(uint8)mac_addr[4], 
			(int)(uint32)(uint8)mac_addr[5]); 
		mac_addr_string.set_filled_len(MAC_ADDR_STRING_LEN);
	}
	else
	{
		if(!mac_addr_string.get_buf() || mac_addr_string.get_filled_len() < MAC_ADDR_STRING_LEN)
			return -2;
		::sscanf(mac_addr_string.get_buf(), "%02X:%02X:%02X:%02X:%02X:%02X", 
			mac_addr, 
			mac_addr+1,
                        mac_addr+2, 
			mac_addr+3, 
			mac_addr+4, 
			mac_addr+5);
	}
	return 0;	
}

//uuid_util_t
critical_section_t uuid_util_t::_s_cs=critical_section_t(true);
int uuid_util_t::_s_state = 0;
mac_addr_t uuid_util_t::_s_mac_addr;

#ifdef __ENABLE_UUID__

void uuid_util_t::uuid_generate(uuid_t out_uuid)
{
	::uuid_generate(out_uuid);
}

void uuid_util_t::uuid_clear(uuid_t uu)
{
	::uuid_clear(uu);
}

void uuid_util_t::uuid_copy(uuid_t dst, const uuid_t src)
{
	::uuid_copy(dst, src);
}

int uuid_util_t::uuid_compare(const uuid_t uu1, const uuid_t uu2)
{
	return ::uuid_compare(uu1, uu2);
}

int uuid_util_t::uuid_is_null(const uuid_t uu)
{
	return ::uuid_is_null(uu);
}

int uuid_util_t::uuid_parse( char *in, uuid_t uu)
{
	return ::uuid_parse(in, uu);	
}

void uuid_util_t::uuid_unparse(uuid_t uu, char *out)
{
	uuid_unparse(uu, out);
}

#else // __ENABLE_UUID__

void uuid_util_t::uuid_generate(uuid_t out_uuid)
{
	struct timeval tv;
	::gettimeofday(&tv,0);	

	if(!_s_state) //uninitialized
	{
		smart_lock_t slock(&_s_cs);
		if(!_s_state) //uninitialized
		{
			::srand(tv.tv_sec); //intialize rand()

			//get MAC address
			if(socket_util_t::s_ifr_get_mac_addr("eth0", _s_mac_addr))
			{
				_s_state = -1; //fail to get MAC address	
			}
			else //succeeded
			{
				_s_state = 1;
			}
		}
	}
	//generate uuid
	MD5_CTX md5_ctx;

	::MD5_Init(&md5_ctx);
	::MD5_Update(&md5_ctx, (const void*)_s_mac_addr, sizeof(_s_mac_addr));
	::MD5_Update(&md5_ctx, (const void*)&tv, sizeof(tv));
	int r=::rand();
	::MD5_Update(&md5_ctx, (const void*)&r, sizeof(r));

	::MD5_Final(out_uuid, &md5_ctx); 	
}

void uuid_util_t::uuid_clear(uuid_t uu)
{
	::memset(uu, 0, UUID_LEN);
}

void uuid_util_t::uuid_copy(uuid_t dst, const uuid_t src)
{
	::memcpy(dst, src, UUID_LEN);
}

int uuid_util_t::uuid_compare(const uuid_t uu1, const uuid_t uu2)
{
	for(int i=0; i<UUID_LEN; ++i)
	{
		if(uu1[i] < uu2[i]) return -1;
		if(uu1[i] > uu2[i]) return 1;
	}
	return 0;
}

int uuid_util_t::uuid_is_null(const uuid_t uu)
{
	for(int i=0; i<UUID_LEN; ++i)
	{
		if(uu[i]) return 0;			
	}
	return 1;
}

int uuid_util_t::uuid_parse( char *in, uuid_t uu)
{       
	if(!in) return -1;

	int rst=::sscanf(in, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		uu, uu+1, uu+2, uu+3,
		uu+4, uu+5,
		uu+6, uu+7,
		uu+8, uu+9,
		uu+10, uu+11, uu+12, uu+13, uu+14, uu+15);

	if(rst == EOF && errno) return -1;

	return 0;	
}               
        
void uuid_util_t::uuid_unparse(uuid_t uu, char *out)
{
	if(!out) return;
	::sprintf(out, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		(int)(uint32)(uint8)uu[0], (int)(uint32)(uint8)uu[1], (int)(uint32)(uint8)uu[2], (int)(uint32)(uint8)uu[3],
		(int)(uint32)(uint8)uu[4], (int)(uint32)(uint8)uu[5],
		(int)(uint32)(uint8)uu[6], (int)(uint32)(uint8)uu[7],
		(int)(uint32)(uint8)uu[8], (int)(uint32)(uint8)uu[9],
		(int)(uint32)(uint8)uu[10],(int)(uint32)(uint8)uu[11],(int)(uint32)(uint8)uu[12], (int)(uint32)(uint8)uu[13],
		(int)(uint32)(uint8)uu[14], (int)(uint32)(uint8)uu[15]);
}

#endif //__ENABLE_UUID__

//socket_listener_t
sp_listener_t socket_listener_t::s_create(bool rcts_flag)
{
        socket_listener_t *raw_lisner=new socket_listener_t();
        if(rcts_flag) raw_lisner->set_lock(&(raw_lisner->_cs));

        return sp_listener_t(raw_lisner, true);
}

socket_listener_t::socket_listener_t() : _cs(true) 
{
	_ip_inaddr=0;
	_port=0;
	_sock_fd=INVALID_SOCKET;
	_max_incoming_cnt_inwin=DEFAULT_MAX_INCOMING_CNT_INWIN;
	_ctrl_window = 0; 
	_incoming_cnt_inwin=0;
	_ctrl_timer_is_active=false;
}

socket_listener_t::~socket_listener_t()
{
	_reset();
}

void socket_listener_t::set_ctrl_window(uint32 ctrl_window) 
{
	FY_XFUNC("set_ctrl_window");
	FY_XINFOD("set_ctrl_window,ctrl_window:"<<ctrl_window);
 
	if(ctrl_window == _ctrl_window) return; 

	_ctrl_window = ctrl_window; 
	if(_ctrl_timer_is_active) 
	{
		_post_remove_msg(MSG_SOKLISNER_CTRL_TIMER); //remove old timer

		//set new timer
		_post_msg_nopara(MSG_SOKLISNER_CTRL_TIMER, _ctrl_window, -1);
	}
}

int32 socket_listener_t::listen(sp_aiosap_t aio_sap, sp_aiosap_t dest_sap, in_addr_t listen_inaddr, uint16 port) 
{
	FY_XFUNC("listen");
	FY_XINFOI("listen,listen_inaddr:"<<(uint32)listen_inaddr<<",port:"<<port);

	if(INVALID_SOCKET != _sock_fd) return INVALID_SOCKET;

	FY_DECL_EXCEPTION_CTX_EX("listen");

	bool reg_fd_result=false; //indicate if socket fd has been registered to aio_provider 
	int32 tmp_sock_fd=INVALID_SOCKET;

	FY_TRY

	if(aio_sap.is_null()) FY_THROW_EX("soklsn-aio","no valid aio sap");

	smart_lock_t slock(&_cs); //thread-safe

        //create socket
        tmp_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(tmp_sock_fd == INVALID_SOCKET) FY_THROW_EX("soklsn-sok", "socket() fail");

        //this option must be set, otherwise, re-listen on same port will fail after closing former listening socket
        int32 opt_reuse=1;
        ::setsockopt(tmp_sock_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt_reuse, sizeof(int32));

	_ip_inaddr = listen_inaddr;
	_port=port;
        sockaddr_in sock_addr;
        if(0 == _ip_inaddr) //ANY_ADDR
                sock_addr.sin_addr.s_addr = ::htonl(INADDR_ANY);
        else
                sock_addr.sin_addr.s_addr = _ip_inaddr;

        sock_addr.sin_port = ::htons(_port);

	//register socket fd to aio service
	sp_aioeh_t tmp_aioeh(this,true);
	aio_sap_it *raw_dest_sap=(dest_sap.is_null()? 0: (aio_sap_it*)dest_sap->lookup(IID_aio_sap, PIN_aio_sap));

	reg_fd_result=aio_sap->register_fd(raw_dest_sap, tmp_sock_fd, tmp_aioeh);
	if(!reg_fd_result) FY_THROW_EX("soklsn-reg","register to aio service failure");

        int32 ret = ::bind(tmp_sock_fd, (const sockaddr*)&sock_addr, sizeof(sockaddr_in));
        if(ret) FY_THROW_EX("soklsn-bind", "bind error,err="<<(uint32)errno); 

	//start to listen
        ret=::listen(tmp_sock_fd, 0);
        if(ret) FY_THROW_EX("soklsn-lsn","listen error,err="<<(uint32)errno);

	bb_t nd_listen_addr;
	socket_util_t::s_in_addr_to_nd(listen_inaddr, nd_listen_addr);
        FY_XINFOI("listen, listen successfuly on: address:"<<nd_listen_addr<<",port:"<<port);

	_aio_sap=aio_sap;
	_sock_fd = tmp_sock_fd;

	if(_ctrl_window)
	{
		//trigger control check timer
		_ctrl_timer_is_active =true;
		_post_msg_nopara(MSG_SOKLISNER_CTRL_TIMER, _ctrl_window, -1);
	}
	FY_CATCH_N_THROW_AGAIN_EX("soklsn-ta","fail to listen", if(reg_fd_result) aio_sap->unregister_fd(tmp_sock_fd);\
				if(INVALID_SOCKET!=tmp_sock_fd) ::close(tmp_sock_fd); return INVALID_SOCKET;);	

        return _sock_fd;
}

void socket_listener_t::post_listen(sp_thd_t owner_thd, sp_aiosap_t dest_sap, in_addr_t listen_inaddr, uint16 port)
{
	FY_XFUNC("post_listen");
	FY_XINFOI("post_listen, listen_inaddr:"<<(uint32)listen_inaddr<<",port:"<<port);

	FY_DECL_EXCEPTION_CTX_EX("post_listen");

	FY_TRY

	FY_ASSERT(!dest_sap.is_null());

	if(owner_thd.is_null()) 
	{
		listen(dest_sap, dest_sap, listen_inaddr, port);
		return;
	}

	msg_proxy_t *raw_msg_proxy=owner_thd->get_msg_proxy();
	if(!raw_msg_proxy) FY_THROW_EX("sokplsn-thd", "owner thread hasn't enabled msg service");
	
	sp_msg_t msg=msg_t::s_create(MSG_SOKLISNER_POST_LISTEN, 3, true);
	lookup_it *obj_sap=(lookup_it*)dest_sap->lookup(IID_lookup, PIN_lookup);
	FY_ASSERT(obj_sap);

	variant_t v_obj(obj_sap, false); //add reference
	msg->set_para(0, v_obj);
	
	variant_t v_listen_addr((int32)listen_inaddr);
	msg->set_para(1, v_listen_addr);

	variant_t v_port((int16)port);
	msg->set_para(2, v_port);

	sp_msg_rcver_t rcver((msg_receiver_it*)this, true);
	msg->set_receiver(rcver);

	raw_msg_proxy->post_msg(msg);	
	
	FY_CATCH_N_THROW_AGAIN_EX("sokplsn-ta","fail to post_listen",);
}

void socket_listener_t::_reset()
{
	smart_lock_t slock(&_cs);

	if(INVALID_SOCKET != _sock_fd)
	{
		::close(_sock_fd);

		FY_ASSERT(!_aio_sap.is_null());
		_aio_sap->unregister_fd(_sock_fd);//unregister sock_fd from aio service.

		_sock_fd = INVALID_SOCKET;
	}
	_ip_inaddr = 0;
	_port= 0;
	_aio_sap.attach(0); 

	if(!_msg_proxy.is_null()) //remove all messages posted to listener
	{
        	//this msg is in same thread, thread-safe is unnecsssary
        	sp_msg_t msg= msg_util_t::s_build_remove_msg(0, (msg_receiver_it*)this, false);

        	_msg_proxy->post_msg(msg);

		_msg_proxy.attach(0);
		_ctrl_timer_is_active=false;	
	}
}

//lookup_it
void *socket_listener_t::lookup(uint32 iid, uint32 pin) throw()
{
        switch(iid)
        {
        case IID_self:
		if(PIN_self != pin) return 0;
        case IID_lookup:
		if(PIN_lookup != pin) return 0;
                return this;

        case IID_aio_event_handler:
		if(PIN_aio_event_handler != pin) return 0;
                return static_cast<aio_event_handler_it*>(this);

	case IID_msg_receiver:
		if(PIN_msg_receiver != pin) return 0;
		return static_cast<msg_receiver_it*>(this);

        case IID_object_id:
                return object_id_impl_t::lookup(iid, pin);

        default:
                return ref_cnt_impl_t::lookup(iid, pin);
        }
}

//aio_event_handler_it
void socket_listener_t::on_aio_events(int32 fd, uint32 aio_events, pointer_box_t ex_para)
{
	FY_XFUNC("on_aio_events,aio_events:"<<aio_events);

	FY_DECL_EXCEPTION_CTX_EX("on_aio_events");

	FY_TRY

	FY_ASSERT(fd == _sock_fd);

	if((aio_events & AIO_POLLIN) == AIO_POLLIN)
	{
		if(_incoming_cnt_inwin >= _max_incoming_cnt_inwin) return; //still on ceiling
		_accept();
	}
	if((aio_events & AIO_POLLERR) == AIO_POLLERR)
	{
		FY_XERROR("on_aio_events, receive an AIO_POLLERR event");
		_post_msg_nopara(MSG_SOKLISNER_POLLERR);
	}
	if((aio_events & AIO_POLLHUP) == AIO_POLLHUP)
	{
		FY_XERROR("on_aio_events, receive an AIO_POLLHUP event");
		_post_msg_nopara(MSG_SOKLISNER_POLLHUP);
	} 	

	FY_CATCH_N_THROW_AGAIN_EX("lsnaioe-ta","on_aio_events fail",);	
}

//msg_receiver_it
void socket_listener_t::on_msg(msg_t *msg)
{
	FY_XFUNC("on_msg,msg:"<<msg->get_msg());

	FY_DECL_EXCEPTION_CTX_EX("on_msg");

	FY_TRY

	FY_ASSERT(msg);

	switch(msg->get_msg())
	{
	case MSG_SOKLISNER_INCOMING_LIMIT:
		if(_incoming_cnt_inwin >= _max_incoming_cnt_inwin)
		{
			FY_ASSERT(!_msg_proxy.is_null());
		
			sp_msg_t sp_msg(msg, true);	
			_msg_proxy->post_msg(sp_msg);//check again
		}
		else //_incoming_cnt_inwin has been reset,accept subsequent incoming connections
		{
			_accept();
		}
		break;

	case MSG_SOKLISNER_CTRL_TIMER:
		if(_incoming_cnt_inwin >= _max_incoming_cnt_inwin)
		{
			FY_XINFOD("on_msg, full incoming count is reset:"<<_incoming_cnt_inwin);
		}

		_incoming_cnt_inwin = 0;
		break;

	case MSG_SOKLISNER_POST_LISTEN:
		{
			variant_t v_obj=msg->get_para(0);
			lookup_it *obj=v_obj.get_obj();
			FY_ASSERT(obj);
			
			sp_aiosap_t dest_sap((aio_sap_it*)obj->lookup(IID_aio_sap, PIN_aio_sap), true);
			
			variant_t v_listen_addr=msg->get_para(1);
			in_addr_t listen_addr=(in_addr_t)v_listen_addr.get_i32(); 	
		
			variant_t v_listen_port=msg->get_para(2);
			uint16 listen_port=(uint16)v_listen_port.get_i16();

			aio_proxy_t *raw_aio_proxy= aio_proxy_t::s_tls_instance();
			FY_ASSERT(raw_aio_proxy);
			sp_aiosap_t aio_proxy((aio_sap_it*)raw_aio_proxy, true);

			if(listen(aio_proxy, dest_sap, listen_addr, listen_port) == INVALID_SOCKET)
			{
				FY_XERROR("on_msg(MSG_SOKLISNER_POST_LISTEN), fail to listen on addr:"<<(int32)listen_addr
					<<",port:"<<listen_port);
			}
		}
		break;

	case MSG_SOKLISNER_POLLERR:
		_on_msg_pollerr();
		break;

	case MSG_SOKLISNER_POLLHUP:
		_on_msg_pollhup();
		break; 

        default:
                FY_XWARNING("on_msg, unknown msg type:"<<msg->get_msg());
	}

	FY_CATCH_N_THROW_AGAIN_EX("lsnmsg-ta","on_msg fail",);	
}

void socket_listener_t::_on_incoming(int32 incoming_fd, in_addr_t remote_addr, uint16 remote_port,
					in_addr_t local_addr, uint16 local_port)
{
	FY_XFUNC("_on_incoming");
	FY_XINFOD("_on_incoming, incoming_fd:"<<incoming_fd<<",remote addr:"<<(uint32)remote_addr<<",remote port:"<<remote_port\
             <<",local_addr:"<<(uint32)local_addr<<",local_port:"<<local_port);

	::close(incoming_fd);
}

void socket_listener_t::_lazy_init_object_id() throw()
{
        FY_TRY

        string_builder_t sb;
        sb<<"socket_listener_fd"<<_sock_fd<<"_"<<(void*)this;
        sb.build(_object_id);

        __INTERNAL_FY_EXCEPTION_TERMINATOR();
}

void socket_listener_t::_accept()
{
	FY_XFUNC("_accept");

	sockaddr sock_addr;
        socklen_t len = sizeof(sockaddr);

        int32 ret_sock = ::accept(_sock_fd, &sock_addr, &len);
        while(ret_sock != INVALID_SOCKET)
        {
        	//accepted an incoming connection request
                _on_incoming(ret_sock, ((sockaddr_in&)sock_addr).sin_addr.s_addr, ntohs(((sockaddr_in&)sock_addr).sin_port),
                                        _ip_inaddr, _port);

                if(_ctrl_window && ++_incoming_cnt_inwin >= _max_incoming_cnt_inwin)
                {
                	FY_XTRACE_IO("_accept, incoming count has reach ceiling:"<<_max_incoming_cnt_inwin);

                        //post a delay message
                        FY_ASSERT(!_msg_proxy.is_null());

                        //this msg is in same thread, thread-safe is unnecsssary
                        sp_msg_t msg=msg_t::s_create(MSG_SOKLISNER_INCOMING_LIMIT, 0, false);

                        msg->set_receiver(sp_msg_rcver_t((msg_receiver_it*)this,true));
                        uint32 tmp_delay=_ctrl_window>>2;
                        msg->set_tc_interval(tmp_delay? tmp_delay:1);

                        _msg_proxy->post_msg(msg);

                        return;
                }

                //try to accept next incoming connection
                ret_sock= ::accept(_sock_fd, &sock_addr, &len);
        }
}

void socket_listener_t::_post_remove_msg(int32 msg_type)
{
	FY_XFUNC("_post_remove_msg");
	FY_XINFOD("_post_remove_msg,msg_type:"<<msg_type);

	if(_msg_proxy.is_null()) return; //no pending msg

	sp_msg_t rm_msg = msg_util_t::s_build_remove_msg(msg_type, (msg_receiver_it*)this, false);
	_msg_proxy->post_msg(rm_msg);	
}

void socket_listener_t::_post_msg_nopara(uint32 msg_type, uint32 utc_interval, int32 repeat)
{
	FY_XFUNC("_post_msg_nopara");
	FY_XINFOD("_post_msg_nopara,msg_type:"<<msg_type<<", utc_interval:"<<utc_interval<<",repeat:"<<repeat);

        if(_msg_proxy.is_null())
        {
                msg_proxy_t *raw_msg_proxy=msg_proxy_t::s_tls_instance();
                FY_ASSERT(raw_msg_proxy);

                _msg_proxy=sp_msg_proxy_t(raw_msg_proxy, true);
        }

        //this msg is in same thread, thread-safe is unnecsssary
        sp_msg_t msg=msg_t::s_create(msg_type, 0, false);
	if(utc_interval) msg->set_tc_interval(utc_interval);
	if(repeat) msg->set_repeat(repeat);

        msg->set_receiver(sp_msg_rcver_t((msg_receiver_it*)this->lookup(IID_msg_receiver, PIN_msg_receiver),true));
        _msg_proxy->post_msg(msg);
}

void socket_listener_t::_on_msg_pollerr()
{
        FY_XFUNC("_on_msg_pollerr");
}

void socket_listener_t::_on_msg_pollhup()
{
        FY_XFUNC("_on_msg_pollhup");
}

//socket_connection_t
sp_conn_t socket_connection_t::s_create(bool rcts_flag)
{
        socket_connection_t *raw_conn=new socket_connection_t();
        if(rcts_flag) raw_conn->set_lock(&(raw_conn->_cs));

        return sp_conn_t(raw_conn, true);
}

socket_connection_t::socket_connection_t() : _cs(true)
{
	_fd = INVALID_SOCKET;
	_max_in_bytes_inwin = SOKCONN_DEF_MAX_IN_BYTES_INWIN;
	_max_out_bytes_inwin = SOKCONN_DEF_MAX_OUT_BYTES_INWIN;
	_ctrl_window = 0;
	_in_bytes_inwin=0;
	_out_bytes_inwin=0;
	_total_in_bytes=0;
	_total_out_bytes=0;
	_ctrl_timer_is_active = false;
#ifdef __FY_DEBUG_RECONNECT__

	__broken_utc = 0;

#endif //__FY_DEBUG_RECONNECT__
}

socket_connection_t::~socket_connection_t()
{
	_reset();
}

void socket_connection_t::set_max_bytes_inwin(uint32 max_bytes, bool in_flag)
{
	FY_XFUNC("set_max_bytes_inwin");
	FY_XINFOI("set_max_bytes_inwin,max_bytes:"<<max_bytes<<",in_flag:"<<(int8)in_flag);

	if(in_flag)
	{
		_max_in_bytes_inwin = max_bytes;
	}
	else
	{
		_max_out_bytes_inwin = max_bytes;
	}
	FY_XINFOI("set_max_bytes_inwin, in_flag:"<<(int8)in_flag<<", max_bytes:"<<max_bytes);
}

void socket_connection_t::set_ctrl_window(uint32 ctrl_window) 
{
	FY_XFUNC("set_ctrl_window");
	FY_XINFOI("set_ctrl_window,ctrl_window:"<<ctrl_window);

	if(_ctrl_window == ctrl_window) return;
 
	_ctrl_window = ctrl_window; 

	FY_XINFOI("set_ctrl_window, ctrl_window(utc):"<<ctrl_window);

        if(_ctrl_timer_is_active) 
        {
		FY_XTRACE_IO("set_ctrl_window, reset timer");

                _post_remove_msg(MSG_SOKCONN_CTRL_TIMER); //remove old timer

                //set new timer
                _post_msg_nopara(MSG_SOKCONN_CTRL_TIMER, _ctrl_window, -1);
        }
	else if(INVALID_SOCKET != _fd)
	{
		FY_XTRACE_IO("set_ctrl_window, enable timer");

		_ctrl_timer_is_active=true;
		_post_msg_nopara(MSG_SOKCONN_CTRL_TIMER, _ctrl_window, -1);
	}
}

int32 socket_connection_t::attach(sp_aiosap_t aio_sap, sp_aiosap_t dest_sap, int32 fd, in_addr_t local_addr, uint16 local_port, 
					in_addr_t remote_addr, uint16 remote_port)
{
	FY_XFUNC("attach");
	FY_XINFOD("attach,fd:"<<fd<<",local_addr:"<<(uint32)local_addr<<",local_port:"<<local_port<<",remote_addr:"
		<<(uint32)remote_addr<<",remote_port:"<<remote_port);

	FY_DECL_EXCEPTION_CTX_EX("attach");

	FY_TRY

	smart_lock_t slock(&_cs);

	if(INVALID_SOCKET != _fd) return INVALID_SOCKET;

	FY_ASSERT(fd != INVALID_SOCKET);
	FY_ASSERT(!aio_sap.is_null());

        //register socket fd to aio service
        sp_aioeh_t tmp_aioeh(this,true);
        aio_sap_it *raw_dest_sap=(dest_sap.is_null()? 0: (aio_sap_it*)dest_sap->lookup(IID_aio_sap,PIN_aio_sap));

	if(!aio_sap->register_fd(raw_dest_sap, fd, tmp_aioeh))
	{
		FY_XERROR("attach, register to aio service failure");
 
		return INVALID_SOCKET;
	}
	_fd =fd;
	_local_addr = local_addr;
	_local_port = local_port;
	_remote_addr = remote_addr;
	_remote_port = remote_port;
	_aio_sap = aio_sap;
	if(_ctrl_window)
	{
        	//trigger control check timer
		FY_XINFOI("attach, enable timer");

		_ctrl_timer_is_active=true;
		_post_msg_nopara(MSG_SOKCONN_CTRL_TIMER, _ctrl_window, -1);
	}

	_post_msg_nopara(MSG_SOKCONN_INIT_POLLINOUT,1,0); //2008-12-25

	FY_XINFOD("attach,socket connection has been attached to aio service,_fd="<<_fd<<",_local_addr:"<<(uint32)_local_addr
		<<":"<<_local_port<<",remote_addr:"<<(uint32)_remote_addr<<":"<<_remote_port);

	FY_CATCH_N_THROW_AGAIN_EX("scat-ta","fail to attach", return INVALID_SOCKET;);	

	return _fd;
}

void socket_connection_t::post_attach(sp_thd_t owner_thd, sp_aiosap_t dest_sap, int32 fd, in_addr_t local_addr, uint16 local_port,
                        in_addr_t remote_addr, uint16 remote_port)
{
	FY_XFUNC("post_attach");
	FY_XINFOD("post_attach, fd:"<<fd<<",local_addr:"<<(uint32)local_addr<<",local_port:"
            <<local_port<<",remote_addr:"<<(uint32)remote_addr<<",remote_port:"<<remote_port);

	FY_DECL_EXCEPTION_CTX_EX("post_attach");

	FY_TRY

        FY_ASSERT(!dest_sap.is_null());

        if(owner_thd.is_null())
        {
                attach(dest_sap, dest_sap, fd, local_addr, local_port, remote_addr, remote_port);

                return;
        }

        msg_proxy_t *raw_msg_proxy=owner_thd->get_msg_proxy();
        if(!raw_msg_proxy) FY_THROW_EX("pta-mp","owner thread hasn't enabled msg service");

        sp_msg_t msg=msg_t::s_create(MSG_SOKCONN_POST_ATTACH, 6, true);
        lookup_it *obj_sap=(lookup_it*)dest_sap->lookup(IID_lookup, PIN_lookup);
        FY_ASSERT(obj_sap);

        variant_t v_obj(obj_sap, false); //add reference
        msg->set_para(0, v_obj);

	variant_t v_fd(fd);
	msg->set_para(1, v_fd);

        variant_t v_local_addr((int32)local_addr);
        msg->set_para(2, v_local_addr);

        variant_t v_local_port((int16)local_port);
        msg->set_para(3, v_local_port);

        variant_t v_remote_addr((int32)remote_addr);
        msg->set_para(4, v_remote_addr);

        variant_t v_remote_port((int16)remote_port);
        msg->set_para(5, v_remote_port);

        sp_msg_rcver_t rcver((msg_receiver_it*)this, true);
        msg->set_receiver(rcver);
        
        raw_msg_proxy->post_msg(msg);

	FY_CATCH_N_THROW_AGAIN_EX("cpat-ta","fail to post_attach",);
}

void socket_connection_t::post_attach(sp_thd_t owner_thd, sp_aiosap_t dest_sap, int32 fd)
{
	post_attach(owner_thd, dest_sap, fd, _local_addr, _local_port, _remote_addr, _remote_port);	
}

int32 socket_connection_t::detach()
{
	FY_XFUNC("detach");

	FY_DECL_EXCEPTION_CTX_EX("detach");

	int32 origin_fd;

	FY_TRY

	smart_lock_t slock(&_cs);

	if(INVALID_SOCKET == _fd) return INVALID_SOCKET;

	FY_ASSERT(!_aio_sap.is_null());

	_aio_sap->unregister_fd(_fd);//unregister sock_fd from aio service.
	_aio_sap.attach(0);

        if(!_msg_proxy.is_null()) //remove all messages posted to connection
        {
                //this msg is in same thread, thread-safe is unnecsssary
                sp_msg_t msg= msg_util_t::s_build_remove_msg(0, (msg_receiver_it*)this, false);

                _msg_proxy->post_msg(msg);

                _msg_proxy.attach(0);
        }

	origin_fd = _fd;
	_fd = INVALID_SOCKET;

	FY_XINFOI("detach,detached, fd="<<origin_fd);

	FY_CATCH_N_THROW_AGAIN_EX("cdat-ta","fail to detach", return INVALID_SOCKET;);

	return origin_fd;	
}

int32 socket_connection_t::connect(sp_aiosap_t aio_sap, sp_aiosap_t dest_sap, in_addr_t server_addr, uint16 server_port,
		in_addr_t local_addr, uint16 local_port)
{
	FY_XFUNC("connect");
	FY_XINFOD("connect, server_addr:"<<(uint32)server_addr<<",server_port:"<<server_port<<",local_addr:"<<(uint32)local_addr\
                 <<",local_port:"<<local_port);

	FY_DECL_EXCEPTION_CTX_EX("connect");

	int32 conn_fd = INVALID_SOCKET;

	FY_TRY

	conn_fd=::socket(PF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET == conn_fd) FY_THROW_EX("sokcon-sok","socket() fail");

        struct sockaddr_in svr_addr;
        svr_addr.sin_family=PF_INET; //protocol family
        svr_addr.sin_port=htons(server_port); //listening port number--transfer short type to network sequence
        svr_addr.sin_addr.s_addr = server_addr;

	if(local_addr)
	{
        	struct sockaddr_in cnt_addr;
        	cnt_addr.sin_family=PF_INET; //protocol family
        	cnt_addr.sin_port=htons(local_port); //client port number--transfer short type to network sequence
        	cnt_addr.sin_addr.s_addr = local_addr;
	
        	int32 ret = ::bind(conn_fd, (const sockaddr*)&cnt_addr, sizeof(sockaddr_in));
        	if(ret) FY_THROW_EX("sokcon-bind", "bind error,err="<<(uint32)errno);
	}
	//attach socket to this object
	if(attach(aio_sap, dest_sap, conn_fd, local_addr, local_port, server_addr, server_port) == INVALID_SOCKET)
	{
		FY_THROW_EX("sokcon-ata","attach() fail");
	}
                      
	int ret=::connect(conn_fd, (struct sockaddr *)&svr_addr, sizeof(struct sockaddr));
	if(!local_addr)
	{
        	struct sockaddr_in cnt_addr;
        	socklen_t cnt_addr_len=sizeof(struct sockaddr);
        	::getsockname(conn_fd, (struct sockaddr *)&cnt_addr, &cnt_addr_len);
		_local_addr = cnt_addr.sin_addr.s_addr;
		_local_port = ::ntohs(cnt_addr.sin_port);
	}
        if(ret == -1)
        {
                switch(errno)
                {
                case EINPROGRESS://almost always return this value
                        break;

                default:
                        FY_XERROR("connect, ::connect error,errno="<<(uint32)errno<<",fd="<<conn_fd);
			detach();
			::close(conn_fd);
 
			return INVALID_SOCKET;
                        break;
                }
	}
        FY_XINFOI("connect, successfully connect to:"<<(uint32)server_addr<<":"<<server_port<<",fd:"<<conn_fd<<",local_addr:"
		<<(uint32)_local_addr<<":"<<_local_port);

	FY_CATCH_N_THROW_AGAIN_EX("ccn-ta","fail to connect",detach();\
                if(INVALID_SOCKET!=conn_fd) ::close(conn_fd);  return INVALID_SOCKET);

	return _fd;
}

void socket_connection_t::post_connect(sp_thd_t owner_thd, sp_aiosap_t dest_sap, in_addr_t server_addr, uint16 server_port,
                         in_addr_t local_addr, uint16 local_port)
{
	FY_XFUNC("post_connect");
	FY_XINFOD("post_connect,server_addr:"<<(uint32)server_addr<<",server_port:"\
          <<server_port);

	FY_DECL_EXCEPTION_CTX_EX("post_connect");

	FY_TRY

        FY_ASSERT(!dest_sap.is_null());

        if(owner_thd.is_null()) 
	{
		connect(dest_sap, dest_sap, server_addr, server_port, local_addr, local_port);

		return;
	}

        msg_proxy_t *raw_msg_proxy=owner_thd->get_msg_proxy();
        if(!raw_msg_proxy) FY_THROW_EX("cpcn-mp","owner thread hasn't enabled msg service");

        sp_msg_t msg=msg_t::s_create(MSG_SOKCONN_POST_CONNECT, 5, true);
        lookup_it *obj_sap=(lookup_it*)dest_sap->lookup(IID_lookup, PIN_lookup);
        FY_ASSERT(obj_sap);

        variant_t v_obj(obj_sap, false); //add reference
        msg->set_para(0, v_obj);

        variant_t v_svr_addr((int32)server_addr);
        msg->set_para(1, v_svr_addr);

        variant_t v_svr_port((int16)server_port);
        msg->set_para(2, v_svr_port);

	variant_t v_local_addr((int32)local_addr);
	msg->set_para(3, v_local_addr);

	variant_t v_local_port((int16)local_port);
	msg->set_para(4, v_local_port);

        sp_msg_rcver_t rcver((msg_receiver_it*)this, true);
        msg->set_receiver(rcver);
        
        raw_msg_proxy->post_msg(msg);		

	FY_CATCH_N_THROW_AGAIN_EX("cpcn-ta","fail to post_connect",);
}

//lookup_it
void *socket_connection_t::lookup(uint32 iid, uint32 pin) throw()
{
        switch(iid)
        {
        case IID_self:
		if(PIN_self != pin) return 0;
        case IID_lookup:
		if(PIN_lookup != pin) return 0;
                return this;

        case IID_aio_event_handler:
		if(PIN_aio_event_handler != pin) return 0;
                return static_cast<aio_event_handler_it*>(this);

        case IID_msg_receiver:
		if(PIN_msg_receiver != pin) return 0;
                return static_cast<msg_receiver_it*>(this);

	case IID_stream:
		if(PIN_stream != pin) return 0;
		return static_cast<stream_it*>(this);

	case IID_iovec:
		if(PIN_iovec != pin) return 0;
		return static_cast<iovec_it*>(this);

        case IID_object_id:
                return object_id_impl_t::lookup(iid, pin);

        default:
                return ref_cnt_impl_t::lookup(iid, pin);
        }
}

//aio_event_handler_it
void socket_connection_t::on_aio_events(int32 fd, uint32 aio_events, pointer_box_t ex_para)
{
	FY_XFUNC("on_aio_events,aio_events:"<<aio_events);

	FY_DECL_EXCEPTION_CTX_EX("on_aio_events");

	FY_TRY

	if((aio_events & AIO_POLLIN) == AIO_POLLIN)
	{
		if(_in_bytes_inwin >= _max_in_bytes_inwin) return; //still on ceiling
		_on_pollin();
	}
	if((aio_events & AIO_POLLOUT) == AIO_POLLOUT)
	{
		if(_out_bytes_inwin >= _max_out_bytes_inwin) return; //still on ceiling
		_on_pollout();
	}
        if((aio_events & AIO_POLLERR) == AIO_POLLERR)
        {
                FY_XERROR("on_aio_events, receive an AIO_POLLERR event");
                _post_msg_nopara(MSG_SOKCONN_POLLERR);
        }
        if((aio_events & AIO_POLLHUP) == AIO_POLLHUP)
        {
                FY_XERROR("on_aio_events, receive an AIO_POLLHUP event");
                _post_msg_nopara(MSG_SOKCONN_POLLHUP);
        }

	FY_CATCH_N_THROW_AGAIN_EX("caioe-ta","on_aio_events fail",);
}

//msg_receiver_it
void socket_connection_t::on_msg(msg_t *msg)
{
	FY_XFUNC("on_msg,msg:"<<msg->get_msg());

	FY_DECL_EXCEPTION_CTX_EX("on_msg");

	FY_TRY

        FY_ASSERT(msg);

        switch(msg->get_msg())
        {
	case MSG_SOKCONN_INCOMING_LIMIT:
		if(_in_bytes_inwin >= _max_in_bytes_inwin)
		{
                        FY_ASSERT(!_msg_proxy.is_null());

                        sp_msg_t sp_msg(msg, true);
                        _msg_proxy->post_msg(sp_msg);//check again
		}
		else //_in_bytes_inwin has been reset
		{
			_on_pollin();
		}
		break;

	case MSG_SOKCONN_OUTGOING_LIMIT:
		if(_out_bytes_inwin >= _max_out_bytes_inwin)
		{
                        FY_ASSERT(!_msg_proxy.is_null());

                        sp_msg_t sp_msg(msg, true);
                        _msg_proxy->post_msg(sp_msg);//check again
		}
		else //_out_bytes_inwin has been reset
		{
			_on_pollout();
		}
		break;

	case MSG_SOKCONN_CTRL_TIMER:

		if(_in_bytes_inwin >= _max_in_bytes_inwin)
		{
			FY_XTRACE_IO("on_msg, _in_bytes_inwin is reset:"<<_in_bytes_inwin);

			_in_bytes_inwin = 0;
			_on_pollin();
		}
		if(_out_bytes_inwin >= _max_out_bytes_inwin)
		{
			FY_XTRACE_IO("on_msg, _out_bytes_inwin is reset:"<<_out_bytes_inwin);
			
			_out_bytes_inwin = 0;
			_on_pollout();
		}
		break;

	case MSG_SOKCONN_POST_ATTACH:
		{
                        variant_t v_obj=msg->get_para(0);
                        lookup_it *obj=v_obj.get_obj();
                        FY_ASSERT(obj);

                        sp_aiosap_t dest_sap((aio_sap_it*)obj->lookup(IID_aio_sap,PIN_aio_sap), true);

			variant_t v_fd=msg->get_para(1);
			int32 fd=v_fd.get_i32();
	
                        variant_t v_local_addr=msg->get_para(2);
                        in_addr_t local_addr=(in_addr_t)v_local_addr.get_i32();

                        variant_t v_local_port=msg->get_para(3);
                        uint16 local_port=(uint16)v_local_port.get_i16();

			variant_t v_remote_addr=msg->get_para(4);
			in_addr_t remote_addr=(in_addr_t)v_remote_addr.get_i32();

			variant_t v_remote_port=msg->get_para(5);
			uint16 remote_port=(uint16)v_remote_port.get_i16();

                        aio_proxy_t *raw_aio_proxy= aio_proxy_t::s_tls_instance();
                        FY_ASSERT(raw_aio_proxy);
                        sp_aiosap_t aio_proxy((aio_sap_it*)raw_aio_proxy, true);

			if(attach(aio_proxy, dest_sap, fd, local_addr, local_port, remote_addr, remote_port) == INVALID_SOCKET)
                        {
                                FY_XERROR("on_msg(MSG_SOKCONN_POST_ATTACH), fail to attach");
                        }
		}
		break;

	case MSG_SOKCONN_POST_CONNECT:
		{
                        variant_t v_obj=msg->get_para(0);
                        lookup_it *obj=v_obj.get_obj();
                        FY_ASSERT(obj);

                        sp_aiosap_t dest_sap((aio_sap_it*)obj->lookup(IID_aio_sap,PIN_aio_sap), true);

                        variant_t v_svr_addr=msg->get_para(1);
                        in_addr_t svr_addr=(in_addr_t)v_svr_addr.get_i32();

                        variant_t v_svr_port=msg->get_para(2);
                        uint16 svr_port=(uint16)v_svr_port.get_i16();

                        variant_t v_local_addr=msg->get_para(3);
                        in_addr_t local_addr=(in_addr_t)v_local_addr.get_i32();

                        variant_t v_local_port=msg->get_para(4);
                        uint16 local_port=(uint16)v_local_port.get_i16();

                        aio_proxy_t *raw_aio_proxy= aio_proxy_t::s_tls_instance();
                        FY_ASSERT(raw_aio_proxy);
                        sp_aiosap_t aio_proxy((aio_sap_it*)raw_aio_proxy, true);

			if(connect(aio_proxy, dest_sap, svr_addr, svr_port, local_addr, local_port) == INVALID_SOCKET)
                        {
                                FY_XERROR("on_msg(MSG_SOKCONN_POST_CONNECT), fail to connect");
                        }
		}
		break;

	case MSG_SOKCONN_CLOSE_BY_PEER:
		_on_msg_close_by_peer(); 
		break;

	case MSG_SOKCONN_POLLERR:
		_on_msg_pollerr();
		break;

	case MSG_SOKCONN_POLLHUP:
		_on_msg_pollhup();
		break;

	case MSG_SOKCONN_INIT_POLLINOUT: //2008-12-25
		FY_XTRACE_IO("on_msg, MSG_SOKCONN_INIT_POLLINOUT");
		_on_pollin();
		_on_pollout();
		break;

        case MSG_SOKCONN_RWERROR: //2009-1-6
		FY_XTRACE_IO("on_msg, MSG_SOKCONN_RWERROR");
                {
                        variant_t v_errno=msg->get_para(0);
                        _on_msg_rwerror(v_errno.get_i32());
                }
                break;

	default:
		FY_XWARNING("on_msg, unknown msg type:"<<msg->get_msg());
		break; 
	}

	FY_CATCH_N_THROW_AGAIN_EX("cmsg-ta","on_msg fail",);
}

//stream_it
uint32 socket_connection_t::read(int8* buf, uint32 len)
{
	FY_XFUNC("read");

	if(INVALID_SOCKET == _fd) return 0; //change owner thread may cause it's true temporarily

	FY_DECL_EXCEPTION_CTX_EX("read");

	int read_cnt;

	FY_TRY

	FY_ASSERT(buf && len); //null buf is disallowed here

	if(SOKCONN_MAX_IN_BYTES_INWIN_UNLIMITED == _max_in_bytes_inwin)
	{
		read_cnt=::recv(_fd, buf, len, 0);
		if(-1 == read_cnt)
		{
			if(errno == EAGAIN || errno == EINTR) return 0;

			FY_XTRACE_IO("read,recv() fail, erro:"<<(int32)errno<<",_fd:"<<_fd);

			_post_msg_rwerror((int32)errno); //2009-1-6

			return 0;
		}
	}
	else //control input bandwidth
	{
		int32 max_readable_len = _max_in_bytes_inwin - _in_bytes_inwin;
		if(max_readable_len > (int32)len)
		{
			read_cnt=::recv(_fd, buf, len, 0);
			if(-1 == read_cnt)
			{
				if(errno == EAGAIN || errno == EINTR) return 0;

				FY_XTRACE_IO("read,recv() fail, erro:"<<(int32)errno<<",_fd:"<<_fd);
				
				_post_msg_rwerror((int32)errno); //2009-1-6

				return 0; 
			}
		}
		else
		{
			if(max_readable_len<1) return 0;
			read_cnt=::recv(_fd, buf, max_readable_len, 0);
			if(-1 == read_cnt)
			{
				if(errno == EAGAIN || errno == EINTR) return 0;
				
				FY_XTRACE_IO("read,recv() fail, erro:"<<(int32)errno<<",_fd:"<<_fd);
				
				_post_msg_rwerror((int32)errno); //2009-1-6

				return 0;
			}
			else if(read_cnt == max_readable_len)
			{
				FY_XTRACE_IO("read, in_bytes_inwin reaches ceiling:"<<_max_in_bytes_inwin<<",_fd:"<<_fd);

				_susppend_pollin();	
			}
		}
		_in_bytes_inwin+=read_cnt;
	}
	if(!read_cnt)
	{
		FY_XTRACE_IO("read, read zero bytes,_fd:"<<_fd); 
		_post_msg_nopara(MSG_SOKCONN_CLOSE_BY_PEER);
	}
	else
		_total_in_bytes += read_cnt;

#ifdef __FY_DEBUG_RECONNECT__

	uint32 utc_cur = user_clock_t::instance()->get_usr_tick();
	if(__broken_utc)
	{
		if(g_broke_side && datetime_t::is_over_tc_end(__broken_utc, 3000, utc_cur))
		{
			int32 ret=::shutdown(_fd, SHUT_RDWR);
			__broken_utc=utc_cur;

			FY_XINFOI("__FY_DEBUG_RECONNECT__, read, socket connection was shutdown forcely, ret="<<ret
				<<",_fd:"<<_fd);
		}
	}
	else
		__broken_utc = utc_cur;

#endif //__FY_DEBUG_RECONNECT__

#ifdef __FY_DEBUG_DUMP_IO__

	__dump_buf(buf, read_cnt,"dump read");

#endif //__FY_DEBUG_DUMP_IO__

	FY_CATCH_N_THROW_AGAIN_EX("cr-ta","fail to read", return 0;);

	return read_cnt;
}

uint32 socket_connection_t::write(const int8* buf, uint32 len)
{
	FY_XFUNC("write");

	if(INVALID_SOCKET == _fd) return 0;//change owner thread may cause it's true temporarily

	FY_DECL_EXCEPTION_CTX_EX("write");

        int send_cnt;

	FY_TRY

	FY_ASSERT(buf && len);//null buf is disallowed here

        if(SOKCONN_MAX_OUT_BYTES_INWIN_UNLIMITED == _max_out_bytes_inwin)
        {
                send_cnt=::send(_fd, buf, len,0);
                if(-1 == send_cnt)
                {
			if(errno == EAGAIN || errno == EINTR) return 0;

			FY_XTRACE_IO("write, send fail, erro:"<<(int32)errno<<",_fd:"<<_fd);
			
			_post_msg_rwerror((int32)errno); //2009-1-6

			return 0;
                }
        }
        else //control output bandwidth
        {
                int32 max_writable_len = _max_out_bytes_inwin - _out_bytes_inwin;
                if(max_writable_len > (int32)len)
                {
                        send_cnt=::send(_fd, buf, len, 0);
                        if(-1 == send_cnt)
			{
				if(errno == EAGAIN || errno == EINTR) return 0;
				
				FY_XTRACE_IO("write, send fail, erro:"<<(int32)errno<<",_fd:"<<_fd);

				_post_msg_rwerror((int32)errno); //2009-1-6

				return 0;
			}
                }
                else
                {
			if(max_writable_len<1) return 0;

                        send_cnt=::send(_fd, buf, max_writable_len, 0);

                        if(-1 == send_cnt)
			{
				if(errno == EAGAIN || errno == EINTR) return 0;

				FY_XTRACE_IO("write, send fail, erro:"<<(int32)errno<<",_fd:"<<_fd);

				_post_msg_rwerror((int32)errno); //2009-1-6

				return 0;
			}
                        else if(send_cnt == max_writable_len)
                        {
                                FY_XTRACE_IO("write, out_bytes_inwin reaches ceiling:"<<_max_out_bytes_inwin<<",_fd:"<<_fd);

                                _susppend_pollout();
                        }
                }
                _out_bytes_inwin+=send_cnt;
        }

	if(!send_cnt)
	{
		FY_XTRACE_IO("write, wrote zero bytes,_fd:"<<_fd); 
		_post_msg_nopara(MSG_SOKCONN_CLOSE_BY_PEER);
	}
	else
		_total_out_bytes += send_cnt;

#ifdef __FY_DEBUG_DUMP_IO__

	FY_XTRACE_IO("====dump write====");
	__dump_buf(buf, send_cnt);

#endif //__FY_DEBUG_DUMP_IO__

	FY_CATCH_N_THROW_AGAIN_EX("cw-ta","fail to write", return 0;);

        return send_cnt;
}

//iovec_it
uint32 socket_connection_t::readv(struct iovec *vector, int32 count)
{
	FY_XFUNC("readv,count:"<<count);

	if(INVALID_SOCKET == _fd) return 0; //change owner thread may cause it's true temporarily

	FY_DECL_EXCEPTION_CTX_EX("readv");

        int read_cnt;

	FY_TRY

	FY_ASSERT(vector && count);
	FY_ASSERT(vector[0].iov_len);

        if(SOKCONN_MAX_IN_BYTES_INWIN_UNLIMITED == _max_in_bytes_inwin)
        {
                read_cnt=::readv(_fd, vector, count);
                if(-1 == read_cnt)
                {
			if(errno == EAGAIN || errno == EINTR) return 0;
			
			FY_XTRACE_IO("readv, readv fail, erro:"<<(int32)errno<<",_fd:"<<_fd);

			_post_msg_rwerror((int32)errno); //2009-1-6

			return 0;
                }
        }
        else //control input bandwidth
        {
                int32 max_readable_len = _max_in_bytes_inwin - _in_bytes_inwin;
		if(max_readable_len<1) return 0;

		int32 tmp_iov_cnt=count;
		for(int i=0; i<count; ++i)
		{
			if(vector[i].iov_len > max_readable_len)
			{
				vector[i].iov_len = max_readable_len;
				tmp_iov_cnt = i+1;
				break;
			}
			else
			{
				max_readable_len -= vector[i].iov_len;
			}
		} 
                read_cnt=::readv(_fd, vector, tmp_iov_cnt);

                if(-1 == read_cnt)
		{
			if(errno == EAGAIN || errno == EINTR) return 0;

			FY_XTRACE_IO("readv, readv fail, erro:"<<(int32)errno<<",_fd:"<<_fd);

			_post_msg_rwerror((int32)errno); //2009-1-6

			return 0;
		}
                _in_bytes_inwin+=read_cnt;
                if(_in_bytes_inwin >= _max_in_bytes_inwin)
                {
                        FY_XTRACE_IO("readv, in_bytes_inwin reaches ceiling:"<<_max_in_bytes_inwin<<",_fd:"<<_fd);

                        _susppend_pollin();
                }
        }

	if(!read_cnt) 
	{
		FY_XTRACE_IO("readv, read zero bytes,_fd:"<<_fd);
		_post_msg_nopara(MSG_SOKCONN_CLOSE_BY_PEER);
	}
	else
		_total_in_bytes += read_cnt;


#ifdef __FY_DEBUG_DUMP_IO__

	FY_XTRACE_IO("====dump readv====");
	__dump_iovec(vector, count, read_cnt);

#endif //__FY_DEBUG_DUMP_IO__

	FY_CATCH_N_THROW_AGAIN_EX("crv-ta","fail to readv", return 0;);

        return read_cnt;
}

uint32 socket_connection_t::writev(struct iovec *vector, int32 count)
{
	FY_XFUNC("writev,count:"<<count);

	if(INVALID_SOCKET == _fd) return 0; //change owner thread may cause it's true temporarily

	FY_DECL_EXCEPTION_CTX_EX("writev");

        int write_cnt;

	FY_TRY

	FY_ASSERT(vector && count);
	FY_ASSERT(vector[0].iov_len);

        if(SOKCONN_MAX_OUT_BYTES_INWIN_UNLIMITED == _max_out_bytes_inwin)
        {
                write_cnt=::writev(_fd, vector, count);

                if(-1 == write_cnt)
                {
			if(errno == EAGAIN || errno == EINTR) return 0;
			
			FY_XTRACE_IO("writev, writev fail, erro:"<<(int32)errno<<",_fd:"<<_fd);

			_post_msg_rwerror((int32)errno); //2009-1-6

			return 0;
                }
        }
        else //control output bandwidth
        {
                int32 max_writable_len = _max_out_bytes_inwin - _out_bytes_inwin;
		if(max_writable_len<1) return 0;

                int32 tmp_iov_cnt=count;
                for(int i=0; i<count; ++i)
                {
                        if(vector[i].iov_len > max_writable_len)
                        {
                                vector[i].iov_len = max_writable_len;
                                tmp_iov_cnt = i+1;

                                break;
                        }
                        else
                        {
                                max_writable_len -= vector[i].iov_len;
                        }
                }
                write_cnt=::writev(_fd, vector, tmp_iov_cnt);
                if(-1 == write_cnt)
		{
			if(errno == EAGAIN || errno == EINTR) return 0;

			FY_XTRACE_IO("writev, writev fail, erro:"<<(int32)errno<<",_fd:"<<_fd);

			_post_msg_rwerror((int32)errno); //2009-1-6

			return 0;
		}
                _out_bytes_inwin+=write_cnt;
                if(_out_bytes_inwin >= _max_out_bytes_inwin)
                {
                        FY_XTRACE_IO("writev, out_bytes_inwin reaches ceiling:"<<_max_out_bytes_inwin<<",_fd:"<<_fd);

                        _susppend_pollout();
                }
        }

	if(!write_cnt)
	{
		FY_XTRACE_IO("writev, wrote zero bytes,_fd:"<<_fd); 
		_post_msg_nopara(MSG_SOKCONN_CLOSE_BY_PEER);
	}
	else
		_total_out_bytes += write_cnt;

#ifdef __FY_DEBUG_DUMP_IO__

	__dump_iovec(vector, count, write_cnt, "dump writev");

#endif //__FY_DEBUG_DUMP_IO__


	FY_CATCH_N_THROW_AGAIN_EX("cwv-ta","fail to writev", return 0;);

        return write_cnt;
}

void socket_connection_t::_lazy_init_object_id() throw()
{
        FY_TRY

        string_builder_t sb;
        sb<<"socket_connection_fd"<<_fd<<"_"<<(void*)this;
        sb.build(_object_id);

        __INTERNAL_FY_EXCEPTION_TERMINATOR();
}

void socket_connection_t::_reset(bool half_reset)
{
	FY_XFUNC("reset,half_reset:"<<(int8)half_reset);

	smart_lock_t slock(&_cs);

	if(INVALID_SOCKET != _fd)
	{
		::close(_fd);

                FY_ASSERT(!_aio_sap.is_null());
                _aio_sap->unregister_fd(_fd);//unregister _fd from aio service.

                _fd = INVALID_SOCKET;		
	}

        if(!_msg_proxy.is_null()) //remove all messages posted to this object 
        {
                //this msg is in same thread, thread-safe is unnecsssary
                sp_msg_t msg= msg_util_t::s_build_remove_msg(0, (msg_receiver_it*)this, false);

                _msg_proxy->post_msg(msg);

                _msg_proxy.attach(0);
		_ctrl_timer_is_active=false;
        }
        _aio_sap.attach(0);

	if(!half_reset) //for tp_connection re-connect
	{
        	_max_in_bytes_inwin = SOKCONN_DEF_MAX_IN_BYTES_INWIN;
        	_max_out_bytes_inwin = SOKCONN_DEF_MAX_OUT_BYTES_INWIN;
        	_ctrl_window = 0;
        	_in_bytes_inwin=0;
        	_out_bytes_inwin=0;
	}
}

void socket_connection_t::_susppend_pollin()
{
	FY_XFUNC("_susppend_pollin");

        uint32 tmp_delay=_ctrl_window>>2;
	if(!tmp_delay) tmp_delay = 1;

	FY_XTRACE_IO("_susppend_pollin, delay:"<<tmp_delay);

        _post_msg_nopara(MSG_SOKCONN_INCOMING_LIMIT, tmp_delay);
}

void socket_connection_t::_susppend_pollout()
{
	FY_XFUNC("_susppend_pollout");

        uint32 tmp_delay=_ctrl_window>>2;
	if(!tmp_delay) tmp_delay = 1;

	FY_XTRACE_IO("_susppend_pollout, delay:"<<tmp_delay);

	_post_msg_nopara(MSG_SOKCONN_OUTGOING_LIMIT, tmp_delay);        
}

void socket_connection_t::_post_remove_msg(int32 msg_type)
{
	FY_XFUNC("_post_remove_msg");

        if(_msg_proxy.is_null()) return; //no pending msg

        sp_msg_t rm_msg = msg_util_t::s_build_remove_msg(msg_type, (msg_receiver_it*)this, false);
        _msg_proxy->post_msg(rm_msg);
}

void socket_connection_t::_post_msg_nopara(uint32 msg_type, uint32 utc_interval, int32 repeat)
{
	FY_XFUNC("_post_msg_nopara");

        if(_msg_proxy.is_null())
        {
                msg_proxy_t *raw_msg_proxy=msg_proxy_t::s_tls_instance();
                FY_ASSERT(raw_msg_proxy);

                _msg_proxy=sp_msg_proxy_t(raw_msg_proxy, true);
        }

        //this msg is in same thread, thread-safe is unnecsssary
        sp_msg_t msg=msg_t::s_create(msg_type, 0, false);
	if(utc_interval) msg->set_tc_interval(utc_interval);
	if(repeat) msg->set_repeat(repeat);

        msg->set_receiver(sp_msg_rcver_t((msg_receiver_it*)this->lookup(IID_msg_receiver,PIN_msg_receiver),true));
        _msg_proxy->post_msg(msg);		
}

void socket_connection_t::_post_msg_rwerror(int32 error_num)
{
	FY_XFUNC("_post_msg_rwerror");

        if(_msg_proxy.is_null())
        {
                msg_proxy_t *raw_msg_proxy=msg_proxy_t::s_tls_instance();
                FY_ASSERT(raw_msg_proxy);

                _msg_proxy=sp_msg_proxy_t(raw_msg_proxy, true);
        }
        //this msg is in same thread, thread-safe is unnecsssary
        sp_msg_t msg=msg_t::s_create(MSG_SOKCONN_RWERROR, 1, false);
        variant_t v_errno(error_num);
        msg->set_para(0, v_errno);
        msg->set_receiver(sp_msg_rcver_t((msg_receiver_it*)this->lookup(IID_msg_receiver,PIN_msg_receiver),true));
        _msg_proxy->post_msg(msg);
}

void socket_connection_t::_on_msg_pollerr()
{
	FY_XFUNC("_on_msg_pollerr,_fd:"<<_fd);
}

void socket_connection_t::_on_msg_pollhup()
{
	FY_XFUNC("_on_msg_pollhup,_fd:"<<_fd);
}

void socket_connection_t::_on_msg_close_by_peer()
{
	FY_XFUNC("_on_msg_close_by_peer,_fd:"<<_fd);
}

void socket_connection_t::_on_msg_rwerror(int32 error_num)
{
	FY_XFUNC("_on_msg_rwerror, error_num:"<<error_num);
}

#ifdef __FY_DEBUG_DUMP_IO__

void socket_connection_t::__dump_buf(const int8 *buf, int32 byte_count, const int8 *hint_msg)
{
	string_builder_t sb;
	sb.prealloc_n(byte_count<<1);

	for(int32 i=0; i<byte_count; ++i)
	{ 
		if(i % 20 == 0) sb<<"\r\n";
		sb<<buf[i]<<",";
	}
	bb_t bb_bytes;
	sb.build(bb_bytes);

	FY_XTRACE_IO("__dump_buf--"<<hint_msg<<", byte_count:"<<byte_count<<",bytes:"<<bb_bytes);
}

void socket_connection_t::__dump_iovec(const struct iovec *vector, int32 count, int32 byte_count, const int8 *hint_msg)
{
	int vec_idx=0;
	int32 rest_bytes=byte_count;
	int32 byte_idx=0;
	int32 piece_len=0;

	string_builder_t sb;
	sb.prealloc_n(byte_count);

	while(rest_bytes)
	{
		int8 *tmp_buf=(int8*)(vector[vec_idx].iov_base);
		if(vector[vec_idx].iov_len >= rest_bytes)
			piece_len = rest_bytes;
		else
			piece_len = vector[vec_idx++].iov_len;
	
		for(int32 i=0; i<piece_len; ++i) 
		{
			if(byte_idx++ % 20 == 0) sb<<"\r\n";
			sb<<tmp_buf[i]<<","; 
		}
		rest_bytes -= piece_len;
	}
	bb_t bb_bytes;
	sb.build(bb_bytes);

	FY_XTRACE_IO("__dump_iovec--"<<hint_msg<<", byte_count:"<<byte_count<<",bytes:"<<bb_bytes);
}

#endif //__FY_DEBUG_DUMP_IO__

