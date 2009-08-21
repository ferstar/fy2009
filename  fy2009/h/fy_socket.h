/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2009-8-7
 * ====================================================================
 */
#ifndef __FENGYI2009_SOCKET_DREAMFREELANCER_20080926_H__ 
#define __FENGYI2009_SOCKET_DREAMFREELANCER_20080926_H__

#include "fy_thread.h"

#ifdef LINUX

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <openssl/md5.h>

#endif

DECL_FY_NAME_SPACE_BEGIN

/*[tip]
 *[declare]
 */
class socket_listener_t;
typedef smart_pointer_tt<socket_listener_t> sp_listener_t;

class socket_connection_t;
typedef smart_pointer_tt<socket_connection_t> sp_conn_t;

#ifndef INVALID_SOCKET

#define INVALID_SOCKET INVALID_FD

#endif
/*[tip]
 *[desc] tool functions for socket
 *[history]
 * Initialize 2008-9-26
 */
const uint16 MAC_ADDR_LEN = 6;
typedef int8 mac_addr_t[MAC_ADDR_LEN];

//readable MAC address format: AA:BB:CC:DD:EE:FF
const uint16 MAC_ADDR_STRING_LEN = 17;

#ifdef WIN32
typedef uint32 in_addr_t;
#endif

class socket_util_t 
{
public:
        //decide if an address string is "number dot" ip string or not
        //format:0-255.0-255.0-255.0-255
        //--to meet inet_ntoa() needs, prefix white space and middle white space are not allowed,
        //  surfix white space is allowed, and prefix 0 isn't also allowed, on the other hand,
        //ip address must followed by ':','/' or '\0'
        //--pass test,2007-2-13
        static bool is_nd_ip(const int8 *ip_addr);

	//convert in_addr_t ip address to number-dot address string, inet_ntoa can do it, but it isn't thread-safe
	//this function is thread safe
	static void s_in_addr_to_nd(in_addr_t in_addr_ip, bb_t& nd_addr);

        //ip address format: <url or ip address string>[:<port number>][/host path]
        //parse ip_addr into ip and port and return the unparsed rest string start position, if any
        //--pass test,2007-2-13
        static const int8 *s_ip_address_parse_inplace(const int8 *ip_addr, in_addr_t *p_ip, uint16 *p_port);

#ifdef LINUX
	//get network interface card name list
	//list all network interface cards installed and filled out_ifnames with all found (or as many as 
	//out_ifnames_size) device's name, return count of all found network cards 
	static uint16 s_ifr_get_name_list(bb_t *out_ifnames, uint16 out_ifnames_size);

	//get MTU (Maximum Transfer Unit) of specified device, return -1 on error
	static int32 s_ifr_get_mtu(const int8 *if_name);
	//get MAC address of specified device, return 0 for success, otherwise non-zero
	static int s_ifr_get_mac_addr(const int8 *if_name, mac_addr_t out_mac_addr);

	//convert MAC address to readable format or reverse
	//return 0 for success, otheerwise non-zero
	static int s_convert_mac_addr(mac_addr_t mac_addr, bb_t& mac_addr_string, bool to_string_flag);
#endif
};

/*[tip]
 *[desc] uuid util
 *[history] 
 *Initialize 2008-11-20
 */
#ifdef LINUX
#ifdef __ENABLE_UUID__

#include <uuid/uuid.h>

#else //__ENABLE_UUID__

#define UUID_LEN 16
typedef unsigned char uuid_t[UUID_LEN];

#endif //__ENABLE_UUID__

#elif defined(WIN32)

#include <rpc.h>
#pragma comment(lib,"Rpcrt4.lib")
#pragma comment(lib,"ws2_32.lib")

#endif //LINUX
class uuid_util_t
{
public:
	static void uuid_generate(uuid_t& out_uuid);	
	static void uuid_clear(uuid_t& uu);
	static void uuid_copy(uuid_t& dst, const uuid_t& src);
	static int uuid_compare(const uuid_t& uu1, const uuid_t& uu2);

	//is null, return true, otherwise, return 0
	static int uuid_is_null(const uuid_t& uu);

	//convert uuid string into uuid_t	
	static int uuid_parse( char *in, uuid_t& uu);

	//convert uuid into readable uuid string format
	static void uuid_unparse(const uuid_t& uu, char *out);
private:
	static critical_section_t _s_cs;
	static int _s_state;
	static mac_addr_t _s_mac_addr; 
};
//888
#ifdef LINUX
/*[tip]
 *[desc] provide asynchronous tcp listening service for any upper layer based on tcp,integrate with aio service, message service
 *       and provide control strategy support, concrete upper layer implementation based on socket can iherit from this and
 *       overwrite _on_incoming and _on_msg_pollerr, _on_msg_pollhup if needed
 *[history] 
 * Initialize 2008-9-26
 */
const uint32 DEFAULT_MAX_INCOMING_CNT_INWIN=0xffffffff;

//socket listener reachs incoming rate limit
uint32 const MSG_SOKLISNER_INCOMING_LIMIT=MSG_USER;

//socket listener incoming control timer
uint32 const MSG_SOKLISNER_CTRL_TIMER=MSG_USER + 1;

//post listen message
uint32 const MSG_SOKLISNER_POST_LISTEN=MSG_USER + 2;
//para_0:aio_sap_it *, dest aio_provider
//para_1:int32(in_addr_t), listen address
//para_2:int16, listen port

//post a msg to notify upper layer that an AIO_POLLERR is received
uint32 const MSG_SOKLISNER_POLLERR = MSG_USER + 3;

//post a msg to notify upper layer that an AIO_POLLHUP is received
uint32 const MSG_SOKLISNER_POLLHUP = MSG_USER + 4;

//--add any socket listener message, it should be changed at the same time
//2009-1-6
uint32 const MSG_SOKLISNER_MAX_RANGE = MSG_SOKLISNER_POLLHUP;

class socket_listener_t : public aio_event_handler_it,
			  public msg_receiver_it,
			  public object_id_impl_t,
			  public ref_cnt_impl_t  
{
public:
	static sp_listener_t s_create(bool rcts_flag=false);	
public:
	~socket_listener_t();

	//listener will do incoming rate check periodically, here, set check interval user tick-count	
	//ctrl window == 0 means no incoming rate control check
	//--it must be called before listening, otherwise, it does nothing
	void set_ctrl_window(uint32 ctrl_window);
	uint32 get_ctrl_window() const throw() { return _ctrl_window; }
 
	//set incoming rate threshold
	void set_max_incoming_cnt_inwin(uint32 max_cnt) 
	{ 
		_max_incoming_cnt_inwin = (max_cnt? max_cnt: DEFAULT_MAX_INCOMING_CNT_INWIN);
	}
	uint32 get_max_incoming_cnt_inwin() const throw() { return _max_incoming_cnt_inwin;}
		
	//listen on ip address and socket port,return socket fd
	//aio_sap can be aio_provider_t or aio_proxy_t depending on thread-mode,
	//dest_sap is destination aio_provider_t, if aio_sap is aio_proxy_t, it's useful, otherwise it's ignored
	//--it's thread safe, i.e. it can be called from listener's owner thread or not
	int32 listen(sp_aiosap_t aio_sap, sp_aiosap_t dest_sap, in_addr_t listen_addr, uint16 port);
	
	//post a listen command to aio_proxy-aware destination thread, then, listen() will be called within this thread
	void post_listen(sp_thd_t owner_thd, sp_aiosap_t dest_sap, in_addr_t listen_addr, uint16 port);

	//it should be called explicity to avoid memory leak caused by circular refering
	//stop listen and uncouple circular reference between this object and aio _aio_sap
	//--it's thread safe
	inline void stop_listen(){ _reset(); }  

	//lookup_it
	void *lookup(uint32 iid, uint32 pin) throw();

	//aio_event_handler_it
	void on_aio_events(int32 fd, uint32 aio_events, pointer_box_t ex_para=0);

	//msg_receiver_it
	void on_msg(msg_t *msg);
protected:
	socket_listener_t();
	void _reset();
	void _lazy_init_object_id() throw();

	void _accept();

	inline void _post_remove_msg(int32 msg_type);

	//post a msg without parameter,
	void _post_msg_nopara(uint32 msg_type, uint32 utc_interval=1, int32 repeat=0);

	//descendent can overwrite it
	virtual void _on_incoming(int32 incoming_fd, in_addr_t remote_addr, uint16 remote_port,
				 	in_addr_t local_addr, uint16 local_port); 

	//in general, upper layer will call stop_listen within them
	//--transfer aio event to msg to avoid potential calling a aio_proxy method within another aio_proxy method 
	//->
	virtual void _on_msg_pollerr();
	virtual void _on_msg_pollhup();
	//<-
protected:
        in_addr_t _ip_inaddr; //network byte order binary ip address
        uint16 _port;
        int32 _sock_fd; //isn't INVALID_SOCKET means is listening
	sp_aiosap_t _aio_sap;
	critical_section_t _cs;
	uint32 _max_incoming_cnt_inwin; //max allowable incoming count within control window
	uint32 _ctrl_window; //incoming control check interval
	bool _ctrl_timer_is_active;

	//accumulate incoming count within control window, it will be reset by timer
	uint32 _incoming_cnt_inwin; 
	sp_msg_proxy_t _msg_proxy;
};

/*[bullet]
 *[desc] provide asynchronous tcp connection service for any upper layer based on tcp, integrate with aio service, message
 *       service and provide data flow control strategy, it can be used at both client and server side
 *[history] 
 * Initialize 2008-10-9
 */
const uint32 SOKCONN_MAX_IN_BYTES_INWIN_UNLIMITED=0xffffffff;
const uint32 SOKCONN_MAX_OUT_BYTES_INWIN_UNLIMITED=0xffffffff;
const uint32 SOKCONN_DEF_MAX_IN_BYTES_INWIN=SOKCONN_MAX_IN_BYTES_INWIN_UNLIMITED;
const uint32 SOKCONN_DEF_MAX_OUT_BYTES_INWIN=SOKCONN_MAX_IN_BYTES_INWIN_UNLIMITED;

//socket connection bandwidth control timer
uint32 const MSG_SOKCONN_CTRL_TIMER=MSG_USER;

//incoming bytes in control widnow reached ceiling
uint32 const MSG_SOKCONN_INCOMING_LIMIT=MSG_USER+1;

//outgoing bytes in control window reached ceiling
uint32 const MSG_SOKCONN_OUTGOING_LIMIT=MSG_USER+2;

//post an attach command to dest thread
uint32 const MSG_SOKCONN_POST_ATTACH =MSG_USER+3;
//para_0: aio_sap_it*, dest aio provider
//para_1: int32, fd
//para_2: int32(in_addr_t), local address
//para_3: uint16, local port
//para_4: int32(in_addr_t), remote address
//para_5: uint16, remote port

//post a connect command to dest thread
uint32 const MSG_SOKCONN_POST_CONNECT = MSG_USER+4;
//para_0: aio_sap_it*, dest aio provider
//para_1: int32(in_addr_t), server address
//para_2: uint16, server port
//para_3: int32(in_addr_t), local address
//para_4: uint16, local port

//post a message to notify upper layer that the socket has been closed by peer
uint32 const MSG_SOKCONN_CLOSE_BY_PEER = MSG_USER+5;

//post a message to notify upper layer that an AIO_POLLERR has been received
uint32 const MSG_SOKCONN_POLLERR = MSG_USER+6;

//post a message to notify upper layer that an AIO_POLLHUP has been received
uint32 const MSG_SOKCONN_POLLHUP = MSG_USER+7;

//init data receiving and sedning
uint32 const MSG_SOKCONN_INIT_POLLINOUT = MSG_USER+8;

//not all socket/write error can by catched by pollerr,e.g. EPIPE(32)--broken pipe
//, this msg is intent to report this error to upper layer,2009-1-6
uint32 const MSG_SOKCONN_RWERROR = MSG_USER + 9;
//para_0:int32, error number

//--add any socket connection message, it should be changed at the same time
//2008-12-25
uint32 const MSG_SOKCONN_MAX_RANGE = MSG_SOKCONN_RWERROR;

class socket_connection_t : public aio_event_handler_it,
                            public msg_receiver_it,
			    public stream_it,
			    public iovec_it, 
                            public object_id_impl_t,
                            public ref_cnt_impl_t  
{
public:
	static sp_conn_t s_create(bool rcts_flag=false);
public:
	~socket_connection_t();
	//statistics
	uint32 get_total_in_bytes() const throw() { return _total_in_bytes; }
	uint32 get_total_out_bytes() const throw() { return _total_out_bytes; }
	//control incoming/outgoing bandwidth
	//--can change it on the fly	
	void set_max_bytes_inwin(uint32 max_bytes, bool in_flag);
	uint32 get_max_bytes_inwin(bool in_flag) const throw() { return (in_flag? _max_in_bytes_inwin : _max_out_bytes_inwin); }

        //connection will do in/out bandwidth check periodically, here, set check interval user tick-count
	//if smaller delay is expected, smaller ctrl window is prefered
	//zero control window means no bandwidth check
	//--can change it on the fly
        void set_ctrl_window(uint32 ctrl_window); 
        uint32 get_ctrl_window() const throw() { return _ctrl_window; }

	in_addr_t get_addr(bool local_flag) const throw() { return (local_flag? _local_addr : _remote_addr); }
	uint16 get_port(bool local_flag) const throw() { return (local_flag? _local_port : _remote_port); }

	//attach this object to an open socket fd and register it to aio service and enable msg service,from either
	//server or client side, return FY_RESULT_OK on success 
	//--it's thread-safe
	int32 attach(sp_aiosap_t aio_sap, sp_aiosap_t dest_sap, int32 fd, in_addr_t local_addr, uint16 local_port, 
			in_addr_t remote_addr, uint16 remote_port);

	//used to change owner thread
	int32 attach(sp_aiosap_t aio_sap, sp_aiosap_t dest_sap, int32 fd)
	{
		return attach(aio_sap, dest_sap, fd, _local_addr, _local_port, _remote_addr, _remote_port);
	}

	//post an attach command to dest thread
	void post_attach(sp_thd_t owner_thd, sp_aiosap_t dest_sap, int32 fd, in_addr_t local_addr, uint16 local_port,
                        in_addr_t remote_addr, uint16 remote_port);

	//post attach version 2, used to change owner thread--attach connection to new owner thread
	void post_attach(sp_thd_t owner_thd, sp_aiosap_t dest_sap, int32 fd);

	//detach _fd from this object and unregister it from aio service and disable current attached thread msg service
	//return original _fd
	//--it's thread-safe
	int32 detach();

	//connect to server and attach this object to aio service, it is called from client side
	//you can specify local address for connection, local_addr==0 means decided by system,
	//--it's thread-safe, you can connect from this object's owner thread or not
	int32 connect(sp_aiosap_t aio_sap, sp_aiosap_t dest_sap, in_addr_t server_addr, uint16 server_port,
			 in_addr_t local_addr=0, uint16 local_port=0);

	//post a connect command to dest thread
	void post_connect(sp_thd_t owner_thd, sp_aiosap_t dest_sap, in_addr_t server_addr, uint16 server_port,
                         in_addr_t local_addr=0, uint16 local_port=0);

        //it should be called explicity to avoid memory leak caused by circular refering
	//close connection and uncouple circular reference between this object and aio _aio_sap
	//for tp_connection re-connect
        inline void close(bool half_close=false){ _reset(half_close); } 

        //lookup_it
        void *lookup(uint32 iid, uint32 pin) throw();

        //aio_event_handler_it
        void on_aio_events(int32 fd, uint32 aio_events, pointer_box_t ex_para=0);

        //msg_receiver_it
        void on_msg(msg_t *msg);
	
	//stream_it
	uint32 read(int8* buf, uint32 len);
	uint32 write(const int8* buf, uint32 len);

	//iovec_it
	uint32 readv(struct iovec *vector, int32 count);
	uint32 writev(struct iovec *vector, int32 count);
protected:
	socket_connection_t();
	void _lazy_init_object_id() throw();

	void _reset(bool half_reset=false);

	//post a MSG_SOKCONN_INCOMING_LIMIT to msg service as incoming bytes reachs ceiling 
	void _susppend_pollin();
	
	//post a MSG_SOKCONN_OUTGOING_LIMIT to msg service as outgoing bytes reachs ceiling
	void _susppend_pollout();

	inline void _post_remove_msg(int32 msg_type);

	//post a msg without parameter,
	void _post_msg_nopara(uint32 msg_type, uint32 utc_interval=1, int32 repeat=0);

        //post an read/write error message,2009-1-6
        void _post_msg_rwerror(int32 error_num);

	//descendant overwrite it to receive data from socket.
	//--either stream_it or iovec_it interface SHOULD be used
	virtual void _on_pollin(){}

        //descendant overwrite it to receive data from socket.
	//--either stream_it or iovec_it interface SHOULD be used
	virtual void _on_pollout(){}

	//in gerneral, upper layer will call detach() within them
	//--transfer aio event to msg to avoid potential calling a aio_proxy method within another aio_proxy method
	//->
        virtual void _on_msg_pollerr();
        virtual void _on_msg_pollhup(); 
	virtual void _on_msg_close_by_peer();
	virtual void _on_msg_rwerror(int32 error_num); //other socket read/write error event excluding pollerr event,2009-1-6
	//<-
private:

#ifdef __FY_DEBUG_DUMP_IO__

	void __dump_buf(const int8 *buf, int32 byte_count, const int8 *hint_msg=0);
	void __dump_iovec(const struct iovec *vector, int32 count, int32 byte_count, const int8 *hint_msg=0);

#endif //__FY_DEBUG_DUMP_IO__

protected:
	int32 _fd;
	
	//bandwidth control
	//->
	uint32 _max_in_bytes_inwin;
	uint32 _max_out_bytes_inwin;
	uint32 _ctrl_window; //user tick-count
	uint32 _in_bytes_inwin;//accumulate incoming bytes in control window
	uint32 _out_bytes_inwin;//accumulate outgoing bytes in control window
	bool _ctrl_timer_is_active;
	//<-

	//statistics
	//->
	uint32 _total_in_bytes;
	uint32 _total_out_bytes;
	//<-

	in_addr_t _local_addr;
	uint16 _local_port;
	in_addr_t _remote_addr;
	uint16 _remote_port;
	sp_aiosap_t _aio_sap;
	sp_msg_proxy_t _msg_proxy;
	critical_section_t _cs;
private:

#ifdef __FY_DEBUG_RECONNECT__

	uint32 __broken_utc;	

#endif //__FY_DEBUG_RECONNECT__
};
//999
#endif //LINUX
DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_SOCKET_DREAMFREELANCER_20080926_H__
