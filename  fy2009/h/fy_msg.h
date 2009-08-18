/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2009-6-1
 * ====================================================================
 */
#ifndef __FENGYI2009_MESSAGE_DREAMFREELANCER_20080603_H__
#define __FENGYI2009_MESSAGE_DREAMFREELANCER_20080603_H__

#include "fy_msgtypes.h"
#include "fy_basex.h"
#include "fy_trace.h"

#include <queue>

DECL_FY_NAME_SPACE_BEGIN

/*[tip]
 *[declare]
 */
//concrete message receiver should realize ref_cnt_i with thread-safe mode, this interface can be realized as
//thread-safe or not, if not, message poster must ensure all relavent message to be send to its owner thread
class msg_receiver_it;
typedef smart_pointer_lu_tt<msg_receiver_it> sp_msg_rcver_t;

class msg_t;
typedef smart_pointer_tt<msg_t> sp_msg_t;

class msg_proxy_t;
typedef smart_pointer_tt<msg_proxy_t> sp_msg_proxy_t;

/*[tip] message sink interface
 *[desc] sometime, there can be some heap pointer(changed to other type) exists in _pv_para on msg_t destroying,
 *   these parameters should be destroyed correctly on msg_t's destroying, otherwise, memory leak will occur.
 *   this interface used to clear these parameters on msg_t destroying
 *[history] 
 * Initialize: 2007-3-27
 */
class msg_sink_it : public lookup_it 
{
public:
        virtual void on_msg_destroy(uint32 msg, const variant_t *pv_para, uint16 para_count)=0;
};
typedef smart_pointer_lu_tt<msg_sink_it> sp_msg_sink_t;

/*[tip] generic message
 *[desc] generic message pattern: msg type(uint32), variable parameter list(variant_t list) 
 *[history] 
 * Initialize: 2007-3-21
 */
//_flag value:
int8 const MSG_FLAG_POSTED=0x01;

class msg_t : public object_id_impl_t,
              public ref_cnt_impl_t	
{
	friend class msg_proxy_t;
public:
	//msg:message type; para_count: count of parameters,rcts:ref_cnt thread-safe
	//pin: avoid message type definition conflict
	static sp_msg_t s_create(uint32 msg, uint32 pin, uint16 para_count, bool rcts_flag=false);
public:
	~msg_t();

	inline uint32 get_msg() const throw() { return _msg; }
	//add pin to avoid potential msg type definition conflict, 2009-6-8
	inline uint32 get_pin() const throw() { return _pin; }
	inline uint16 get_para_count() const throw() { return _pv_len; }

	void set_para(uint16 para_index, const variant_t& para);
	const variant_t& get_para(uint16 para_index);

	//record current user tick-count as message was posted to message delivery service,
	//it's used for message delay doing
	inline uint32 get_utc_posted() const throw() { return _utc_posted; }

	//specify interval(unit:ms) in which message will be delayed doing or repeated doing
        void set_tc_interval(uint32 tc_interval) throw();
	uint32 get_tc_interval() const throw(); //unit:ms 
        inline uint32 get_utc_interval() const throw() { return _utc_interval; } 

	//indicate this message will be done specified times or infinitely(_repeat == -1),
	//time interval between consective doing is specified by set_tc_interval
        inline void set_repeat(int32 repeat) throw() 
	{
                if((_flag & MSG_FLAG_POSTED) == MSG_FLAG_POSTED)
                {
                        FY_XERROR("set_repeat, it is posted, not allow change");
                        return;
                } 
		_repeat = repeat; 
	}
        inline int32 get_repeat() const throw() { return _repeat; }

	//specify subject who will do on this message
	inline void set_receiver(const sp_msg_rcver_t& smt_receiver) 
	{
                if((_flag & MSG_FLAG_POSTED) == MSG_FLAG_POSTED)
                {
                        FY_XERROR("set_receiver, it is posted, not allow change");
                        return;
                } 
		_smt_receiver = smt_receiver; 
	}
	inline const sp_msg_rcver_t& get_receiver() const throw() { return _smt_receiver; }

	//specify message destroyer, which is responsible for freeing parameter list of message on destroying	
	inline void register_sink(const sp_msg_sink_t& smt_on_destroy) 
	{
                if((_flag & MSG_FLAG_POSTED) == MSG_FLAG_POSTED)
                {
                        FY_XERROR("register_sink, it is posted, not allow change");
                        return;
                } 
		_smt_on_destroy = smt_on_destroy; 
	}
	
        //lookup_it
        void *lookup(uint32 iid, uint32 pin) throw();
protected:	
	msg_t();
	msg_t(uint32 msg, uint32 pin, uint16 para_count);
	void _lazy_init_object_id() throw(){ OID_DEF_IMP("msg"); }

	//it is only called by friend msg_proxy_t
	inline void _set_utc_posted(uint32 posted_utc) { _utc_posted = posted_utc; }
protected:
	int8 _flag; 
	uint32 _msg;
	uint32 _pin; //avoid message type definition conflict
	uint32 _utc_posted;
        uint32 _utc_interval; //user tick-count
        int32 _repeat;      //repeat times:0 always
	variant_t *_pv_para; //parameter array;
	uint16 _pv_len; //length of _pv_para
	sp_msg_sink_t _smt_on_destroy; //clear _pv_para on destroying
	sp_msg_rcver_t _smt_receiver;
	critical_section_t _cs; //syn add_reference/release_reference
};

/*[tip] message receiver
 *[desc] it defines the subject who will do something on this message
 *[history] 
 * Initialize: 2007-3-22
 */
class msg_receiver_it : public lookup_it
{
public:
	virtual void on_msg(msg_t *msg)=0;
};

/*[tip] message proxy
 *[desc] tls singleton. it provide message service for a specific thread, any thread-specific message
 *       should be delivered to thread-related message proxy, which is based on a semi-lock oneway_pipe, i.e. send message
 *       operation should lock/unlock, but receive message from proxy should not lock/unlock
 *[history] 
 * Initialize: 2007-3-23
 * revise 2008-6-4
 */
uint32 const MPXY_LOCAL_MQ_CNT=10;
uint32 const MPXY_DEF_MP_SIZE=128;

//max slice of once heart_beat,unit:user tick-count(10ms)
uint32 const MPXY_HB_MAX_SLICE=8; 

class msg_proxy_t : public heart_beat_it,
		    public oneway_pipe_sink_it,
		    public object_id_impl_t,
		    public ref_cnt_impl_t //thread-safe 
{
public:
	//local msg queue
	typedef std::deque< sp_msg_t > mq_t;
public:
        //mp_size: message pipe size,unit: message piece
	//only first request takes effect whithin a thread
	//return same instance for same thread
        static msg_proxy_t *s_tls_instance(uint32 mp_size=MPXY_DEF_MP_SIZE,
						event_slot_t *es_notfull=0, uint16 esi_notfull=0,
						event_slot_t *es_notempty=0, uint16 esi_notempty=0);
        static void s_delete_tls_instance(); //destroyed instance attached to current thread	
public:
	inline fy_thread_t get_owner_thread() const throw() { return _thd; }

	//heart_beat_it
	//periodically try to receive messages
	//--each call must not last too long
	int8 heart_beat();
	//unit:user tick-count
	void set_max_slice(uint32 max_slice){ _max_slice = max_slice; }
	uint32 get_max_slice() const throw() { return _max_slice; }

	//it expects to be called by owner thread as soon as possible
	uint32 get_expected_interval() const throw() { return 0; }

	//it's thread-safe, can be called by multi thread simultaneously
	void post_msg(sp_msg_t& msg);//post message to owner thread

	//unit:ms
	uint32 get_min_delay_interval() const throw();

	//oneway_pipe_sink_it
	void on_destroy(const int8 *buf, uint32 buf_len);

        //lookup_it
        void *lookup(uint32 iid, uint32 pin) throw();
protected:
 	void _lazy_init_object_id() throw();
private:
	//mp_size=0 means message pipe will be intialized with default size
	msg_proxy_t(uint32 mp_size=MPXY_DEF_MP_SIZE,
                    event_slot_t *es_notfull=0, uint16 esi_notfull=0,
                    event_slot_t *es_notempty=0, uint16 esi_notempty=0 );

	//check and run local message, it's interruptable  
	int8 _poll_local_mq(uint32 utc_start, uint8 start_idx, uint8 end_idx);

	//remove local message specified by para rcver and msg from _local_mq
	//para msg==0 means remove all recer matched msg
	void _remove_local_msg(const sp_msg_rcver_t& rcver, uint32 msg);

	//push msg to proper _local_mq
	void _push_to_local_mq(sp_msg_t& msg);
private:
	static fy_thread_key_t _s_tls_key;
	static bool _s_key_created;
	static critical_section_t _s_cs;
	//define message interval bound which determine message will be divided into which _local_mq
	static uint32 _s_local_mq_bound[MPXY_LOCAL_MQ_CNT];
private:
	sp_owp_t _mp;
	//concerning performance, local message will be divided into different _local_mq according its interval  
	mq_t _local_mq[MPXY_LOCAL_MQ_CNT];
	mq_t _post_wait_q; //post_msg fail to send msg to _mp, it will be saved to this queue

	//if owner thread is post_msg destination, message will be push to this queue, but not _mp or _post_wait_q concerning
	//performance, also not directly to _local_mq to avoid post_msg is called from msg_sink_i.on_msg which may destroy
        // _local_mq intergrity
	mq_t _post_owner_q;  	
	uint32 _utc_lastpoll[MPXY_LOCAL_MQ_CNT]; //record last poll user tick-count for each _local_mq
	uint8 _idx_nextpoll;//hold _local_mq suffix from which  next _poll_local_mq will start 
	uint32 _max_slice; //max slice length expected once heart_beat calling,unit:user tick-count
	fy_thread_t _thd; //owner thread
	critical_section_t _cs; //lock/unlock post_msg

        //signal it with _esi_notfull after reading a message from _mp, notify one of message writer threads to write
	//message in _post_wait_q into _mp, writer thread can wait on _es_notfull if it's idle
	//caller should ensure it's always idle during whole lifecycle of message proxy
	event_slot_t *_es_notfull;
        uint16 _esi_notfull;

	//signal it with _esi_notempty after a message are written into _mp, notify owner thread to read message from
	//_mp, owner thread can wait on _es_notempty if it's idle
	//caller should ensure it's always idle during whole lifecycle of message proxy
	event_slot_t *_es_notempty;
	uint16 _esi_notempty;
	//by checking _es_notempty, owner thread can respond to instant message immediately, but for delay/repeat message
	//no any signal can be probed, a proper event_slot_t::wait timeout is needed to avoid too late response or too
	//much cpu waste,2009-8-17
	uint32 _utc_min_delay_interval;
};

/*[tip]
 *[desc] build some general messages
 *[history] 
 * Initialize: 2008-11-27
 */
class msg_util_t
{
public:
	static inline sp_msg_t s_build_remove_msg(int32 msg_type, msg_receiver_it *raw_msg_recver, bool rcts_flag=false)
	{
        	sp_msg_t msg=msg_t::s_create(MSG_REMOVE_MSG, MSG_PIN_REMOVE_MSG, 1, rcts_flag);
        	msg->set_receiver(sp_msg_rcver_t(raw_msg_recver,true));
        	variant_t vpara(msg_type);
        	msg->set_para(0, vpara);

        	return msg;
	}
};

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_MESSAGE_DREAMFREELANCER_20080603_H__

