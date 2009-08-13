/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2009-4-29
 * ====================================================================
 */
#include "fy_trace.h"

USING_FY_NAME_SPACE

//trace_provider_t::_piece_t
trace_provider_t::_piece_t::_piece_t(uint8 trace_level, int8 *piece, uint32 piece_len)
{
        m_trace_level = trace_level;
        m_piece = piece;
        m_piece_len = piece_len;
}

//trace_provider_t::_std_trace_stream_t
sp_trace_stream_t trace_provider_t::_std_trace_stream_t::s_create()
{
        _std_trace_stream_t *stm=new _std_trace_stream_t();

        return sp_trace_stream_t(stm, true);
}

uint32 trace_provider_t::_std_trace_stream_t::write(const int8* buf, uint32 len, bool trace_start)
{
#ifdef POSIX
        return ::write(STDOUT_FILENO, buf, len);
#elif defined(WIN32)
		printf("%s",buf);
		return len;
#endif
}

//lookup_it
void *trace_provider_t::_std_trace_stream_t::lookup(uint32 iid, uint32 pin) throw()
{
        switch(iid)
        {
        case IID_self:
		if(pin != PIN_self) return 0;		
               	return this;

        case IID_trace_stream:
		if(pin != PIN_trace_stream) return 0;
               	return static_cast<trace_stream_it *>(this);

        default:
               return ref_cnt_impl_t::lookup(iid, pin);
        }
}

//trace_provider_t::tracer_t
trace_provider_t::tracer_t::tracer_t(   uint32 pipe_size,
                                        uint32 max_queued_size,
                                        event_slot_t *event_slot_notfull,
                                        uint16 event_slot_index,
                                        event_t *event_incoming) throw()
{
        _sp_pipe=oneway_pipe_t::s_create(pipe_size*TRACE_ITEM_NLP_SIZE + 1);
        _sb.prealloc_n(TRACE_DESC_BUILDER_BUF_SIZE);
        _stop_w_flag=false;
        _max_q_size=max_queued_size;
        _es_notfull=event_slot_notfull;
        _esi_notfull=event_slot_index;
        _e_incoming=event_incoming;
        _loss_cnt=0;
}

//prepare prefix of one piece of trace info to _sb,the format of prefix is as fllowing:
//<level tag><file name tag><line number tag>--
string_builder_t& trace_provider_t::tracer_t::prepare_trace_prefix(uint8 trace_level,const int8 *src_file, int32 src_line) throw()
{
        //prepare trace level tag
        switch(trace_level)
        {
                case TRACE_LEVEL_ERROR:
                        _sb<<"<E>";
                        break;

                case TRACE_LEVEL_WARNI:
                        _sb<<"<W>";
                        break;

                case TRACE_LEVEL_INFOI:
                        _sb<<"<I>";
                        break;

                case TRACE_LEVEL_INFOD:
                        _sb<<"<D>";
                        break;

                case TRACE_LEVEL_FUNC:
                        _sb<<"<F>";
                        break;

                default: //non-predefined trace level
                        _sb<<"<X"<<trace_level<<">";
                        break;
        }
        //prepare <file name tag> and <line umber tag>
        _sb<<"<f="<<src_file<<"><l="<<src_line<<">--";

        return _sb;
}

//write a piece of trace to pipe,the format of piece in pipe is as following
//<(uint8)trace_level><(uint32)piece_length><(int8*)piece_heap_pointer>
//--length of a trace piece:TRACE_ITEM_NLP_SIZE(1+4+4=9)
void trace_provider_t::tracer_t::write_trace(uint8 trace_level) throw()
{
        trace_provider_t::_piece_t piece;
        uint32 piece_len=0;
        while(!_q.empty())
        {
                piece=_q.front();
                //ensure piece isn't null
                if(!_write_trace_piece(piece)) //fail to send queued piece
                {
                        if(_q.size() >= _max_q_size) //_q is full
                        {
                                _sb.reset(); //abandon current pending trace piece
                                ++_loss_cnt;

                                return;
                        }

                        //_q isn't full, build piece
                        _build_trace_piece(trace_level, piece);
                        if(0 == piece.m_piece) return;
                        _q.push_back(piece); //queued piece into _q

                        return;
                }
                _q.pop_front();
        }

        //no queued piece to send, build piece
        _build_trace_piece(trace_level, piece);
        if(0 == piece.m_piece) return; //no trace piece to send

        if(!_write_trace_piece(piece)) //fail to send current piece
                _q.push_back(piece);

        return;
}

bool trace_provider_t::tracer_t::_write_trace_piece(_piece_t piece) throw()
{
        //ensure trace_piece isn't null and piece_len isn't 0

        if( _sp_pipe->write((const int8*)&(piece.m_trace_level),sizeof(piece.m_trace_level),false) \
                ==sizeof(piece.m_trace_level) &&
            _sp_pipe->write((const int8*)&(piece.m_piece_len),sizeof(piece.m_piece_len),false) \
                ==sizeof(piece.m_piece_len) &&
            _sp_pipe->write((const int8*)&(piece.m_piece),sizeof(int8*),false)==sizeof(int8*))
        {
                _sp_pipe->commit_w();
                //ensure it's not a null pointer
                _e_incoming->signal(); //notify trace reading thread this pipe is no longer empty

                return true;
        }
        _sp_pipe->rollback_w();

        return false;
}

void trace_provider_t::tracer_t::_build_trace_piece(uint8 trace_level, _piece_t& piece) throw()
{
        piece.m_piece_len=_sb.get_result_size();
        if(!piece.m_piece_len)
        {
                piece.m_piece=0;

                return;
        }
        piece.m_trace_level=trace_level;
        piece.m_piece=new int8[++piece.m_piece_len];
        bb_t bb(piece.m_piece, piece.m_piece_len, false, 0);

        _sb.build(bb);
        _sb.reset();
        bb.detach();
}

void trace_provider_t::tracer_t::on_destroy(const int8 *buf, uint32 buf_len)
{
        if(!buf || !buf_len) return;

        __INTERNAL_FY_TRACE("some trace info still existed in pipe on destroying\n");

        _piece_t piece;
        for(uint32 i=0; i<buf_len;)
        {
                //read a piece of trace info
                //format of piece:<(uint8)trace_level><(uint32)piece_length><(int8*)piece_heap_pointer>
                //skip trace level
                i+=sizeof(uint8);
                //skip piece length
                i+=sizeof(uint32);

                //read piece heap pointer
                memcpy(&(piece.m_piece), buf+i, sizeof(int8*));
                if(piece.m_piece) delete [] piece.m_piece;
                piece.m_piece = 0;
                i+=sizeof(int8*);
        }
}

void *trace_provider_t::tracer_t::lookup(uint32 iid, uint32 pin) throw()
{
        switch(iid)
        {
        case IID_self:
		if(pin != PIN_self) return 0;
               	return this;

        case IID_oneway_pipe_sink:
		if(pin != PIN_oneway_pipe_sink) return 0;
               return static_cast<oneway_pipe_sink_it *>(this);

        default:
               return 0;
        }
}

trace_provider_t::tracer_t::~tracer_t()
{
        _piece_t piece;
        while(!_q.empty())
        {
                piece=_q.front();
                if(piece.m_piece) delete [] piece.m_piece;
                piece.m_piece=0;

                _q.pop_front();
        }
}

//trace_provider_t
trace_provider_t *trace_provider_t::_s_inst=0;
critical_section_t trace_provider_t::_s_cs=critical_section_t();

trace_provider_t *trace_provider_t::instance()
{
        if(_s_inst)
        {
                return _s_inst;
        }

        smart_lock_t slock(&_s_cs);

        if(_s_inst) return _s_inst;

        trace_provider_t *tmp_inst=0;

        FY_TRY

        tmp_inst=new trace_provider_t();

        //default trace device is standard output file
        for(uint8 i=0; i<MAX_TRACE_LEVEL_COUNT; ++i)
        {
                tmp_inst->_stm_slot[i].push_back(trace_provider_t::_std_trace_stream_t::s_create());
        }

        __INTERNAL_FY_EXCEPTION_TERMINATOR(;)

        _s_inst=tmp_inst; //it must be last statement to make sure thread safe,2007-3-5

        return _s_inst;
}

//poll to read trace from pipes and write it to trace file
#ifdef POSIX

void *trace_provider_t::_thd_r_f(void *arg)

#elif defined(WIN32)

DWORD WINAPI trace_provider_t::_thd_r_f(LPVOID arg)

#endif
{
        FY_TRY

        trace_provider_t *trp=(trace_provider_t *)arg;

        trp->_s_cs_rtf_read.unlock();//means read thread is ready, it should be just before while(true)
        uint16 readed_cnt=0;
        while(true)
        {
                readed_cnt=trp->_poll_once();

                if(trp->_stop_flag) break; //provider request stop read thread

                //read trace thread is idle
                if(!readed_cnt) trp->_e_incoming.wait(1000); //time-out is 1 second
        }

        //try to empty all pipes--continous three trip read no data will think that all pipes is empty
        //thread will sleep 10 tc between the last three trip to make sure all pending write completed
        int16 final_trip=3;
        do{
                int16 ret=trp->_poll_once();
                if(ret<0) break;//no pipe,exit immediately

                if(ret>0) //not empty
                        final_trip=3;

                else //no data
                {
                        --final_trip;
			fy_msleep(10);
                }

        }while(final_trip>0);

        //unregister from all pipes
        for(uint16 i=0;i<trp->_filled_len;i++)
        {
                if(trp->_vec_tracer[i]==0) continue;//empty slot

                sp_owp_t sp_pipe=trp->_vec_tracer[i]->get_pipe();
                if(sp_pipe.is_null()) continue;

                sp_pipe->unregister_read();
        }

        __INTERNAL_FY_EXCEPTION_TERMINATOR(;)

        return 0;
}

int16 trace_provider_t::_poll_once()
{
        int16 readed_cnt=0;
        bool has_pipe=false;
        _piece_t piece;

        //get timestamp
        user_clock_t *clk=user_clock_t::instance();


        for(uint16 i=0;i<_filled_len;i++)
        {
                if(_vec_tracer[i]==0)
                {
                        if(_dead_vec_tracer[i])
                        {
                                delete _dead_vec_tracer[i];
                                _dead_vec_tracer[i]=0;
                        }
                        continue;//empty slot
                }
                oneway_pipe_t *pipe=_vec_tracer[i]->_get_raw_pipe();//pipe never null,for performnace, not add_reference
                has_pipe=true;

                if(!(pipe->is_read_registered())) pipe->register_read(); //lazy register this thread as read thread

                //read a piece of trace info
                //format of piece:<(uint8)trace_level><(uint32)piece_length><(int8*)piece_heap_pointer>
                //trace write must commit piece by piece to avoid partial piece written
                //read trace level
                uint32 ret=pipe->read((int8*)&(piece.m_trace_level),sizeof(piece.m_trace_level));
                if(ret==0)//current no data to read
                {
                        //free slot of unregistered pipe after read it to empty
                        if(_vec_tracer[i]->is_stopped_writing())
                        {
                                //except constructor/destructor,only read thread can reset _vec_tracer[] to 0,
                                //but forbidden this thread to set it to nonzero,so lock isn't needed
                                delete _vec_tracer[i]; //free this slot
                                _vec_tracer[i]=0;
                                if(i==_filled_len-1) _filled_len--; //shrinkage _filled_len
                        }
                        continue;
                }
                //read piece length
                ret=pipe->read((int8*)&(piece.m_piece_len), sizeof(piece.m_piece_len));

                //read piece heap pointer
                if(piece.m_piece_len) ret=pipe->read((int8*)&(piece.m_piece),sizeof(int8*));

                ++readed_cnt;

                //notify writing thread pipe isn't full,2008-6-10
                if(_vec_tracer[i]->_es_notfull) _vec_tracer[i]->_es_notfull->signal(_vec_tracer[i]->_esi_notfull);

                if(piece.m_trace_level >= MAX_TRACE_LEVEL_COUNT)
                {
                        __INTERNAL_FY_TRACE("trace_provider_t, invalid trace level\n");

                        if(piece.m_piece) delete [] piece.m_piece;
                        piece.m_piece = 0;
                        piece.m_piece_len=0;

                        continue;
                }

                if(!piece.m_piece) continue;

                struct tm ts;
                uint32 tc=clk->get_localtime(&ts);
                int8 tmp_buf[128];
                sprintf(tmp_buf, "<ts=%04d-%02d-%02d %02d:%02d:%02d.%03d><tid=%lu>",
                        ts.tm_year,ts.tm_mon,ts.tm_mday,
                        ts.tm_hour,ts.tm_min,ts.tm_sec,tc,(uint32)(pipe->get_w_thd()));

                uint32 len_prefix=::strlen(tmp_buf);

                _stream_vec_t& stm_vec= _stm_slot[ piece.m_trace_level ];
                int vec_size=stm_vec.size();
                for(int i=0; i<vec_size; ++i)
                {
                        sp_trace_stream_t& sp_stm=stm_vec[i];
                        if(sp_stm.is_null()) continue;

                        if(sp_stm->write(tmp_buf, len_prefix, true) != len_prefix)
                                __INTERNAL_FY_TRACE("trace_provider_t, fail to write entire trace to stream\n");

                        uint32 trace_len=piece.m_piece_len - 1;
                        if(sp_stm->write(piece.m_piece, trace_len, false) != trace_len)
                                __INTERNAL_FY_TRACE("trace_provider_t, fail to write entire trace to stream\n");
                }

                if(piece.m_piece)
                {
                        delete [] piece.m_piece; //free trace piece allocated by write thread
                        piece.m_piece=0;
                        piece.m_piece_len=0;
                }
        }
        return (has_pipe? readed_cnt : -1);
}

trace_provider_t::trace_provider_t()
{
        memset(_vec_tracer,0,TRACE_PROVIDER_CAPACITY*sizeof(tracer_t*));
        _filled_len=0;
        _stop_flag=false;
        _r_thd=0;

        memset(_enable_flag, 0, MAX_TRACE_LEVEL_COUNT*sizeof(bool));
        //follow level trace service is enabled by default
        for(uint8 i=TRACE_LEVEL_ERROR; i<=TRACE_LEVEL_INFOD; ++i) _enable_flag[i] = true;
}

//tf_r_f must make sure all pipes have been read empty before destructor,otherwise,memory leak will occur
//for this destructor always occurred when process exited,so possible memory leak isn't harmful.
trace_provider_t::~trace_provider_t()
{
        close();
        for(uint16 i=0;i<_filled_len;i++)
        {
                if(_vec_tracer[i]) delete _vec_tracer[i]; //free tracer_t *
                if(_dead_vec_tracer[i]) delete _dead_vec_tracer[i];
        }
}

void trace_provider_t::open()
{
        smart_lock_t slock(&_s_cs);

        if(_r_thd) return;
        int32 ret=fy_thread_key_create(&_tls_key,0);
        if(ret)
        {
                __INTERNAL_FY_TRACE("trace_provider_t::open create TLS key error\n");
                return;
        }
        //create read thread
        _s_cs_rtf_read.lock();//wait for read thread unlock it while ready to read

        fy_thread_t tmp_thd=0;
#ifdef POSIX
        if(pthread_create(&tmp_thd,0,_thd_r_f,(void *)this))
#elif defined(WIN32)
	tmp_thd=::CreateThread(NULL, 0, _thd_r_f, (void*)this, 0, NULL);
	if(tmp_thd == NULL)
#endif
        {
                _s_cs_rtf_read.unlock();
                __INTERNAL_FY_TRACE("trace_provider_t::open create read thread error\n");
        }
        else //succeeded in creating read thread
        {
                _s_cs_rtf_read.lock();//wait for read thread is ready
                _s_cs_rtf_read.unlock();
                _r_thd=tmp_thd;//announce trace provider has been opened
        }
}

void trace_provider_t::close()
{
        smart_lock_t slock(&_s_cs);

        if(!_r_thd) return;

        fy_thread_key_delete(_tls_key);//disable later write try
        _tls_key=0;

        _stop_flag=true;//notify read thread to stop
        fy_thread_join(_r_thd,0);//wait read thread exit
        _r_thd=0;
}

trace_provider_t::tracer_t *trace_provider_t::register_tracer(uint32 pipe_size,
                                                        uint32 max_queued_size,
                                                        event_slot_t *event_slot_notfull,
                                                        uint16 event_slot_index)
{
        bool locked_flag=false;
        tracer_t *tcer=0;

        FY_TRY
        void *ret=fy_thread_getspecific(_tls_key);//read tracer pointer from Thread Local Storage
        if(ret) return (tracer_t *)ret; //current thread register repeatedly
        if(!_r_thd)
        {
                __INTERNAL_FY_TRACE("try to register_tracer before opening tracer serivce\n");
                return 0;
        }

        //current thread first register
        tcer=new tracer_t(pipe_size, max_queued_size, event_slot_notfull, event_slot_index, &_e_incoming);
        tcer->get_pipe()->register_write();//register current thread as write thread
        tcer->get_pipe()->register_sink(static_cast<oneway_pipe_sink_it*>(tcer));

        //if there is 'hole' in _vec_tracer,tcer will be inserted into this 'hole',
        //otherwise, be appended to the tail of _vec_tracer
        bool insert_flag=false;

		smart_lock_t slock(&_s_cs);

        //only this function can set a null _vec_tracer[] to nonzero,but forbidden to set a nonzero _vec_tracer[]
        //to zero,so lock isn't needed
        for(uint16 i=0;i<_filled_len;i++)
        {
                if(_vec_tracer[i] || _dead_vec_tracer[i]) continue;//not 'hole'
                _vec_tracer[i]=tcer;
                insert_flag=true;
                break;
        }
        if(!insert_flag)
        {
                if(_filled_len==TRACE_PROVIDER_CAPACITY)//capacity is full
                {
                        delete tcer;
                        __INTERNAL_FY_TRACE("register_tracer fail for reaching capacity\n");

                        return 0;
                }
                else
                        _vec_tracer[_filled_len++]=tcer;//append to tail
        }

        slock.unlock();
        int ret_set_tls=fy_thread_setspecific(_tls_key,(void *)tcer);//set tracer to Thread Local Storage
        if(ret_set_tls)
        {
                delete tcer;
                __INTERNAL_FY_TRACE("register_tracer,fy_thread_setspecific fail\n");

                return 0;
        }

        __INTERNAL_FY_EXCEPTION_TERMINATOR( if(tcer) delete tcer;)

        return tcer;
}

void trace_provider_t::unregister_tracer()
{
        FY_TRY

        void *ret=fy_thread_getspecific(_tls_key);
        if(!ret) return; //never registered
        fy_thread_setspecific(_tls_key,(void *)0);//reset thread local storage

        fy_thread_t thd=fy_thread_self();
        smart_lock_t slock(&_s_cs);

        for(uint16 i=0;i<_filled_len;i++)
        {
                if(!_vec_tracer[i]) continue; //empty slot

                sp_owp_t sp_pip=_vec_tracer[i]->get_pipe();
                if(sp_pip->get_w_thd()!=thd) continue;//not attached to this thread

                //found current thread matched tracer
                _vec_tracer[i]->stop_write();//notify read thread writting has been stopped

                //wait for read thread read the rest data in related pipe
                uint16 icnt=20; //time out = 2s
                while(icnt--)
                {
                        if(sp_pip->get_r_size()==0) break; //no rest data to read
			fy_msleep(100);
                }
                if(!icnt) __INTERNAL_FY_TRACE("trace_provider_t::unregister_tracer lost some traces\n");

                //must after no rest data in pipe,otherwise,read thread will has no idea of written thread
                sp_pip->unregister_write();

                _dead_vec_tracer[i] = _vec_tracer[i];
                _vec_tracer[i]=0;

                break;
        }

        __INTERNAL_FY_EXCEPTION_TERMINATOR(;)
}

trace_provider_t::tracer_t *trace_provider_t::get_thd_tracer()
{
        void *ret=fy_thread_getspecific(_tls_key);
	uint32 tmp_thd=(uint32)fy_thread_self();
        if(ret) return (tracer_t *)ret;

        __INTERNAL_FY_TRACE_EX("trace_provider_t::get_thd_tracer,error, thread(tid="<<tmp_thd \
                <<") hasn't register for tracer\n");

        return 0;
}

void trace_provider_t::register_trace_stream(uint8 trace_level, sp_trace_stream_t trace_stm, uint8 option)
{
        if(_r_thd)
        {
                __INTERNAL_FY_TRACE("trace_provider_t::register_trace_stream must be called before opening\n");
                return;
        }

        if(REG_TRACE_STM_OPT_NONE == option) return;

        smart_lock_t slock(&_s_cs);

        if((option & REG_TRACE_STM_OPT_EQ) == REG_TRACE_STM_OPT_EQ)
        {
                if(trace_stm.is_null())
                        _stm_slot[trace_level].clear();
                else
                        _stm_slot[trace_level].push_back(trace_stm);
        }

        if((option & REG_TRACE_STM_OPT_LT) == REG_TRACE_STM_OPT_LT && trace_level > 0)
        {
                for(uint8 i=0; i<trace_level; ++i)
                {
                        if(trace_stm.is_null())
                                _stm_slot[i].clear();
                        else
                                _stm_slot[i].push_back(trace_stm);
                }
        }

        if((option & REG_TRACE_STM_OPT_GT) == REG_TRACE_STM_OPT_GT && trace_level < MAX_TRACE_LEVEL_COUNT - 1)
        {
                for(uint8 i=trace_level + 1; i<MAX_TRACE_LEVEL_COUNT; ++i)
                {
                        if(trace_stm.is_null())
                                _stm_slot[i].clear();
                        else
                                _stm_slot[i].push_back(trace_stm);
                }
        }
}

void trace_provider_t::set_enable_flag(uint8 trace_level, bool enable_flag)
{
        if(trace_level >= MAX_TRACE_LEVEL_COUNT) return;

        _enable_flag[ trace_level ] = enable_flag;
}

bool trace_provider_t::get_enable_flag(uint8 trace_level)
{
        if(trace_level >= MAX_TRACE_LEVEL_COUNT) return false;

        return _enable_flag[ trace_level ];
}

//trace_file_t
critical_section_t trace_file_t::_s_cs = critical_section_t();
uint32 trace_file_t::_s_pid=0;
int8 trace_file_t::_s_exe_name[MAX_EXE_CMDLINE_SIZE]={0};

void trace_file_t::_s_lazy_init()
{
        if(_s_pid) return; //has initialized

        smart_lock_t slock(&_s_cs);
#ifdef POSIX
        uint32 pid=(uint32)::getpid();
#elif defined(WIN32)
		DWORD pid=::GetCurrentProcessId();
#endif
        //get execute file name
#ifdef LINUX
        int8 pid_file_path[32];
        sprintf(pid_file_path, "/proc/%d/cmdline", pid);
        FILE *fp=fopen(pid_file_path,"rb");

        int16 ret=fread(_s_exe_name, 1, MAX_EXE_CMDLINE_SIZE, fp);
        if(ret>=MAX_EXE_CMDLINE_SIZE)
        {
                __INTERNAL_FY_TRACE("current process related command line is too long too exceed MAX_EXE_CMDLINE_SIZE\n");
                return;
        }
#elif defined(WIN32)

		int16 ret=::GetModuleFileName(NULL, _s_exe_name, MAX_EXE_CMDLINE_SIZE);
#endif
        _s_exe_name[ret]='\0';

        for(int16 i=ret; i>=0; i--)
        {
#ifdef POSIX
                if(_s_exe_name[i]=='/')
#elif defined(WIN32)
				if(_s_exe_name[i] == '\\')
#endif
                {
                        memmove(_s_exe_name, _s_exe_name+i+1, ret - i);
                        break;
                }
        }
        _s_pid=pid; //it must be last statement
}

sp_trace_stream_t trace_file_t::s_create(uint8 trace_level, uint32 max_file_cnt_per_day, uint32 max_size_per_file)
{
        _s_lazy_init();//initialize _pid and _exe_name

        trace_file_t *p=new trace_file_t(trace_level, max_file_cnt_per_day, max_size_per_file);

        return sp_trace_stream_t(p, true);
}

trace_file_t::trace_file_t(uint8 trace_level, uint32 max_file_cnt_per_day, uint32 max_size_per_file)
        : _cs(true),ref_cnt_impl_t(&_cs)
{
        _trace_level=trace_level;
        _fp=0;
        _max_file_cnt_per_day = max_file_cnt_per_day;
        _max_size_per_file = max_size_per_file;
        _cur_file_idx=0;
        _tm_mday=0;
        _filled_size=0;
}

trace_file_t::~trace_file_t()
{
        if(_fp) ::fclose(_fp);
}

uint32 trace_file_t::write(const int8* buf, uint32 len, bool trace_start)
{
        //splitting trace file is only allowed as trace_start is true to avoid split one ppiece of trace to two files
        if(trace_start) _check_file(len);

        uint32 ret=fwrite(buf, 1, len, _fp);
        if(ret != len)
        {
                __INTERNAL_FY_TRACE_EX("fwrite trace file(level="<<_trace_level<<") entirely or partially fail\r\n");
                return ret;
        }
        _filled_size += ret;

        return ret;
}

//lookup_it
void *trace_file_t::lookup(uint32 iid, uint32 pin) throw()
{
        switch(iid)
        {
        case IID_self:
		if(pin != PIN_self) return 0;
               	return this;

        case IID_trace_stream:
		if(pin != PIN_trace_stream) return 0;
               	return static_cast<trace_stream_it *>(this);

        default:
               return ref_cnt_impl_t::lookup(iid, pin);
        }
}

void trace_file_t::_check_file(uint32 want_size)
{
        //get timestamp
        user_clock_t *clk=user_clock_t::instance();
        struct tm ts;
        uint32 tc=clk->get_localtime(&ts);

        //check day change or not
        if(ts.tm_mday != _tm_mday)//create different log files for every day
        {
                _tm_mday=ts.tm_mday;
                if(_fp) fclose(_fp);
                _fp=0;
                _cur_file_idx=0;
                _filled_size=0;

                int8 msg[64];
                sprintf(msg, "current log date(level=%d):%04d-%02d-%02d\r\n",
                                _trace_level, ts.tm_year, ts.tm_mon, ts.tm_mday);

                __INTERNAL_FY_TRACE(msg);
        }

        //check if trace file reached max size
        if(_filled_size + want_size > _max_size_per_file)
        {
                if(_fp) fclose(_fp);
                _fp=0;
                _filled_size=0;

                if(_cur_file_idx == _max_file_cnt_per_day - 1)
                {
                        _cur_file_idx=0;
                        __INTERNAL_FY_TRACE_EX("trace file(level="<<_trace_level<<") has wrapped\r\n");
                }
                else
                {
                        ++_cur_file_idx;
                        __INTERNAL_FY_TRACE_EX("trace file(level="<<_trace_level<<") has changed index to "<<_cur_file_idx\
                                                <<"\r\n");
                }
        }

        //create and open file
        if(!_fp)
        {
                int8 fm[256];
                int8 level_tok[16];
                switch(_trace_level)
                {
                case TRACE_LEVEL_ERROR:
                        sprintf(level_tok, "%s", "e");
                        break;

                case TRACE_LEVEL_WARNI:
                        sprintf(level_tok, "%s", "w");
                        break;

                case TRACE_LEVEL_INFOI:
                        sprintf(level_tok, "%s", "i");
                        break;

                case TRACE_LEVEL_INFOD:
                        sprintf(level_tok, "%s", "d");
                        break;

                case TRACE_LEVEL_FUNC:
                        sprintf(level_tok, "%s", "f");
                        break;

                default:
                        sprintf(level_tok, "x%d", _trace_level);
                        break;
                }

                sprintf(fm, "%s_%lu_%04d%02d%02d_%s_%d.log",(int8*)trace_file_t::_s_exe_name,
                        trace_file_t::_s_pid,
                        ts.tm_year, ts.tm_mon, ts.tm_mday, level_tok, _cur_file_idx);

                _fp=::fopen(fm,"w");
        }
}

#ifdef WIN32

//trace_debugview_t
critical_section_t trace_debugview_t::_s_cs = critical_section_t();

sp_trace_stream_t trace_debugview_t::s_create()
{
        trace_debugview_t *p=new trace_debugview_t();

        return sp_trace_stream_t(p, true);
}

uint32 trace_debugview_t::write(const int8* buf, uint32 len, bool trace_start)
{
	::OutputDebugString(buf);

        return len;
}

//lookup_it
void *trace_debugview_t::lookup(uint32 iid, uint32 pin) throw()
{
        switch(iid)
        {
        case IID_self:
		if(pin != PIN_self) return 0;
               	return this;

        case IID_trace_stream:
		if(pin != PIN_trace_stream) return 0;
               	return static_cast<trace_stream_it *>(this);

        default:
               return ref_cnt_impl_t::lookup(iid, pin);
        }
}

#endif //WIN32

