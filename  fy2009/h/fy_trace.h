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
#ifndef __FENGYI2009_TRACE_DREAMFREELANCER_20080318_H__
#define __FENGYI2009_TRACE_DREAMFREELANCER_20080318_H__

#include "fy_stream.h"
#include <queue>
#include <vector>

DECL_FY_NAME_SPACE_BEGIN

/*[tip]
 *[desc] trace can be outputed to various media which realizes this interface
 *[history] 
 * Initialize: 2008-5-4
 */
class trace_stream_it : public lookup_it
{
public:
        //trace_start is true if current buf is start of a new trace piece
        virtual uint32 write(const int8* buf, uint32 len, bool trace_start)=0;
};

typedef smart_pointer_lu_tt<trace_stream_it> sp_trace_stream_t;

/*[tip]trace info string builder work buffer size
 *[desc] this buffer used as trace info string builder work buffer to reduce memory allocation operation,
 *[note that] --this size can't be changed on run-time
 *--test shows system flushes file as frequent as you expected in general, and to keep trace service as efficient as
 *  possible, it's not necessary to implement flush files on checkpoint
 *--forbidden any function throw exception
 */
const uint32 TRACE_DESC_BUILDER_BUF_SIZE=128;
         
//1(trace level)+4(trace item length)+4(trace buffer pointer)
const uint32 TRACE_ITEM_NLP_SIZE=9;
        
/*[tip]the size of oneway_pipe_t to transfer trace info
 *[desc] trace pipe size only can by changed by request parameter.
 *because only pointer of trace string be written to pipe, practical one-way-pipe size(bytes) is
 *TRACE_DEF_NLP_SIZE*TRACE_ITEM_NLP_SIZE + 1
 */ 
const uint32 TRACE_DEF_NLP_SIZE=256; 
    
/*[tip]max queued trace size at trace source side
 *[desc] if trace oneway-pipe is full, sequential trace will be queued till oneway-pipe becomes not full again,
 *but total queued size is limitted, if it's exceeded, sequential trace will be abandoned silently
 */ 
const uint32 TRACE_DEF_MAX_QUEUED_SIZE=1024; //1024 pieces of trace but not bytes

//provider capacity,means the count of simultaneously supported writers,can't be changed on the fly
const uint32 TRACE_PROVIDER_CAPACITY=128;

//max valid trace level,user defined trace level should be less than it, can't be changed on the fly
const uint8 MAX_TRACE_LEVEL_COUNT=32;

//trace level index, it mush be less than MAX_TRACE_LEVEL_COUNT
const uint8 TRACE_LEVEL_ERROR=0;
const uint8 TRACE_LEVEL_WARNI=1;
const uint8 TRACE_LEVEL_INFOI=2;
const uint8 TRACE_LEVEL_INFOD=3;
const uint8 TRACE_LEVEL_FUNC =4; //2009-1-19

//--it must be modified as adding new predefined trace level
const uint8 TRACE_LEVEL_MAX_PREDEFINED = TRACE_LEVEL_FUNC;

//register trace stream option
const uint8 REG_TRACE_STM_OPT_NONE=0;
const uint8 REG_TRACE_STM_OPT_EQ=0x01;
const uint8 REG_TRACE_STM_OPT_LT=0x02;
const uint8 REG_TRACE_STM_OPT_GT=0x04;

const uint8 REG_TRACE_STM_OPT_ALL = REG_TRACE_STM_OPT_EQ | REG_TRACE_STM_OPT_LT | REG_TRACE_STM_OPT_GT;

/*[tip]trace service,singleton
 *[desc] it shoulde be openned before using and closed before shutting down. A trace read thread will be created
 *       on openning to read traces which come from a thread-specific oneway pipe, and writting to registered trace streams,
 *       by reistering proper trace stream, you can redirect trace info to anywhere you wanted, any trace-enabled thread should
 *       register trace service before using it
 *[history] 
 * Initialize: 2008-4-28
 *[memo] 
 * default, trace info will be outputted to standard output device, you can change it by calling register_trace_stream
 */
class trace_provider_t
{
#ifdef POSIX
private:
#elif defined(WIN32)
public:
#endif
        //a piece of trace info
        //it isn't responsible for lifecycle of m_piece
        class _piece_t
        {
        public:
                _piece_t() : m_trace_level(0), m_piece(0), m_piece_len(0){}

                //piece must be a heap pointer string
                _piece_t(uint8 trace_level, int8 *piece, uint32 piece_len);
        public:
                uint8 m_trace_level;
                int8 *m_piece;
                uint32 m_piece_len;
        };

        typedef std::deque<_piece_t> piece_q_t;

        //trace to std output device
        //after register, it's only be called by trace thread, so thread-safe isn't necessary for it,
        //except ref_cnt_it
        class _std_trace_stream_t : public trace_stream_it,
                                    public ref_cnt_impl_t //thread-safe
        {
        public:
                static sp_trace_stream_t s_create();
                //trace_stream_it
                uint32 write(const int8* buf, uint32 len, bool trace_flag);

                //lookup_it
                void *lookup(uint32 iid, uint32 pin) throw();
        private:
                _std_trace_stream_t() : _cs(true), ref_cnt_impl_t(&_cs){} //forbiden ctor is called directly
                critical_section_t _cs;
        };
public:
        //offer trace service for specific thread.
        //all trace desc must be allocated on heap,and just write its heap pointer to _pipe to improve performance
        class tracer_t : public oneway_pipe_sink_it
        {
                friend class trace_provider_t;
        public:
                tracer_t(uint32 pipe_size=TRACE_DEF_NLP_SIZE,
                        uint32 max_queued_size=TRACE_DEF_MAX_QUEUED_SIZE,
                        event_slot_t *event_slot_notfull=0,
                        uint16 event_slot_index=0,
                        event_t *event_incoming=0) throw();

                ~tracer_t();

                inline sp_owp_t get_pipe() const throw() { return _sp_pipe; }

                //prepare trace piece prefix into _sb
                string_builder_t& prepare_trace_prefix(uint8 trace_level, const int8 *src_file,int32 src_line) throw();

                //write current _sb to pipe
                void write_trace(uint8 trace_level) throw();

                //notify read thread writting has been stopped
                inline void stop_write() throw() { _stop_w_flag=true; }
                //read thread call it to decide writting has been stopped or not
                inline bool is_stopped_writing() const throw() { return _stop_w_flag; }

                //oneway_pipe_sink_it
                void on_destroy(const int8 *buf, uint32 buf_len);

                //lookup_it
                void *lookup(uint32 iid, uint32 pin) throw();

        private:
                //build current content in _sb into para piece, piece.m_piece isn't null on success,
                //otherwise on error
                inline void _build_trace_piece(uint8 trace_level, _piece_t& piece) throw();

                //write parameter piece to _sp_pipe, return true on success, false on error
                inline bool _write_trace_piece(_piece_t piece) throw();

                //it is used for write/read thread for performance
                inline oneway_pipe_t *_get_raw_pipe() throw() { return (oneway_pipe_t*)_sp_pipe; }
        private:
                sp_owp_t _sp_pipe;
                string_builder_t _sb; //for building trace desc
                bool _stop_w_flag; //notify read thread that writing has been stopped

                //if _sp_pipe is full as writting, para trace piece can be saved to _q temporarily
                //to wait for _sp_pipe is changed to not full,2008-4-21
                piece_q_t _q;

                //max queued trace piece count , if it's reached, sequential trace will be abandoned
                //and call __INTERNAL_FY_TRACE to write a basic log
                uint32 _max_q_size;

                //if it has been registered, it will be signalled with slot index= _esi_notfull after trace reading thread
                // has read a piece of trace from _sp_pipe, trace writting thread can wait for this event slot on idle to
                //write trace info in _q to _sp_pipe,2008-4-21
                //caller must ensure _es_notfull is always valid during whole lifecycle of this object
                event_slot_t *_es_notfull;
                uint16 _esi_notfull;

                //notify reading thread the pipe is no longer empty
                event_t *_e_incoming; //pointer to trace_provider._e_incoming
                uint32 _loss_cnt; //lost piece count for _q is full
        };
public:
        static trace_provider_t *instance();

        ~trace_provider_t();

        //must call it before accessing any tracer service,called by main thread
        void open();

        //must call it before main thread exits
        void close();

        //thread must call it to register trace service before write trace info
        tracer_t *register_tracer(uint32 pipe_size=TRACE_DEF_NLP_SIZE,
                                uint32 max_queued_size=TRACE_DEF_MAX_QUEUED_SIZE,
                                event_slot_t *event_slot_notfull=0,
                                uint16 event_slot_index=0);

        //thread must call it to match with register call before thread exit
        void unregister_tracer();

        //get thread-specific tracer
        tracer_t *get_thd_tracer();

        //register trace stream for specific trace level, strace_stm.is_null() can be true to mean unreg all registered stream
        //it only works before openning to eliminate thread confilict
        void register_trace_stream(uint8 trace_level, sp_trace_stream_t trace_stm, uint8 option=REG_TRACE_STM_OPT_EQ);

        //indicate specific level trace service is enabled or not, which means nothing to this service itself, only indicate
        //trace service caller if to trace or not
        void set_enable_flag(uint8 trace_level, bool enable_flag);
        bool get_enable_flag(uint8 trace_level);
private:
        typedef std::vector<sp_trace_stream_t> _stream_vec_t;
private:
        //thread function,to read trace from pipes and write them to trace file
#ifdef POSIX
        static void *_thd_r_f(void * arg);
#elif defined(WIN32)
		static DWORD WINAPI _thd_r_f(LPVOID arg);
#endif
		trace_provider_t(); //forbiden caller create this object directly
        void _start();//start read thread
        //poll read one trip and return readed pieces of trace info,or return 0 if no data,or return -1 if no pipe
        int16 _poll_once();
private:
        static trace_provider_t *_s_inst;
        static critical_section_t _s_cs;
        event_t _e_incoming;//2008-4-14, trace thread will wait on it if all trace pipes are empty
#ifdef POSIX
        pthread_key_t _tls_key;
#elif defined(WIN32)
	DWORD _tls_key;
#endif
        tracer_t * _vec_tracer[TRACE_PROVIDER_CAPACITY];
        uint16 _filled_len; //filled length of _vec_tracer

        //unregister_tracer will move tracer from _vec_tracer to here,
        //until tracer thread removes tracer prcatically later, to avoid tracer attached oneway pipe is freed during tracer
        //thread is accessing it,2008-5-4
        tracer_t * _dead_vec_tracer[TRACE_PROVIDER_CAPACITY];

        bool _stop_flag; //notify read thread to stop
#ifdef POSIX
        pthread_t _r_thd;//read thread
#elif defined(WIN32)
	HANDLE _r_thd;
#endif
        critical_section_t _s_cs_rtf_read;//when read thread is ready will unlock it

        //trace dispatching table,suffix is trace level
        _stream_vec_t _stm_slot[ MAX_TRACE_LEVEL_COUNT ];
        bool _enable_flag[ MAX_TRACE_LEVEL_COUNT ];
};

/*[tip]trace to file 
 *[desc] in general, server traces to some files, in terms of trace level, trace will respectively be written to erro, warning,
 *   infoi and infod trace files,which name include executable file name, process id and date, each type file will be splitted
 *   to specified count files, old file may be overwritten on wrapping occurs. By specifying max trace file count and max trace file *   for each file to limit max disk space usage to eliminate the possibility of using disk space up.
 *[memo]
 *1.after register, it's only be called by trace thread, so thread-safe isn't necessary for it
 *[history] 
 * Initialize: 2008-4-28
 */
const uint32 MAX_EXE_CMDLINE_SIZE=256;

class trace_file_t : public trace_stream_it,
                     public ref_cnt_impl_t //it must be thread-safe
{
public:
        static sp_trace_stream_t s_create(uint8 trace_level, uint32 max_file_cnt_per_day=10, uint32 max_size_per_file=1048576);
public:
        ~trace_file_t();

        //trace_stream_it
        uint32 write(const int8* buf, uint32 len, bool trace_start);

        //lookup_it
        void *lookup(uint32 iid, uint32 pin) throw();
private:
        static void _s_lazy_init();
private:
        trace_file_t(uint8 trace_level, uint32 max_file_cnt_per_day, uint32 max_size_per_file);
        void _check_file(uint32 want_size);
private:
        static critical_section_t _s_cs;
#ifdef POSIX
        static uint32 _s_pid; //process id
#elif defined(WIN32)
		static DWORD _s_pid;
#endif
        static int8 _s_exe_name[MAX_EXE_CMDLINE_SIZE]; //executable file name
private:
        uint8 _trace_level;
        FILE *_fp;
        uint32 _max_file_cnt_per_day;
        uint32 _max_size_per_file;
        uint32 _cur_file_idx;
        int32 _tm_mday;//current log file related day of month
        uint32 _filled_size;
        critical_section_t _cs;
};

#ifdef WIN32

/*[tip]trace to DebugView for windows
 *[desc]
 *[history]
 * Initialize: 2009-6-3
 */
class trace_debugview_t : public trace_stream_it,
                          public ref_cnt_impl_t //it must be thread-safe
{
public:
        static sp_trace_stream_t s_create();
public:
        ~trace_debugview_t(){}

        //trace_stream_it
        uint32 write(const int8* buf, uint32 len, bool trace_start);

        //lookup_it
        void *lookup(uint32 iid, uint32 pin) throw();
private:
        trace_debugview_t(){}
private:
        static critical_section_t _s_cs;
};

#endif //WIN32

/*[tip]
 *[desc] some maros about trace service
 *[history] 
 * Initialize: 2008-4-29
 */
//macro to output trace info
#define __INTERNAL_FY_TRACE_SERVICE(level,desc) trace_provider_t* prvd=trace_provider_t::instance();\
 if(prvd->get_enable_flag(level)){\
 trace_provider_t::tracer_t *tcer=prvd->get_thd_tracer();\
 if(tcer){\
        tcer->prepare_trace_prefix(level, __FILE__, __LINE__)<<desc<<"\r\n";\
        tcer->write_trace(level);}}

//macro to output error level trace, it's enabled by default
//--X version is used within member functions of class which realizes object_id_it
#if defined(FY_TRACE_DISABLE_ERROR)

#       define FY_ERROR(desc)
#       define FY_XERROR(desc)

#else

#       define FY_ERROR(desc)  do{ __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_ERROR, desc) }while(0)
#       define FY_XERROR(desc) do{ __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_ERROR, get_object_id()<<": "<<desc) }while(0)

#endif

//macro to output warning level trace, it's enabled by default
//--X version is used within member functions of class which realizes object_id_it
#if defined(FY_TRACE_DISABLE_WARNING)

#       define FY_WARNING(desc)
#       define FY_XWARNING(desc)

#else

#       define FY_WARNING(desc)  do{ __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_WARNI, desc) }while(0)
#       define FY_XWARNING(desc) do{ __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_WARNI, get_object_id()<<": "<<desc) }while(0)

#endif

//macro to output infoi level trace, it's enabled by default
//--X version is used within member functions of class which realizes object_id_it
#if defined(FY_TRACE_DISABLE_INFOI)

#       define FY_INFOI(desc)
#       define FY_XINFOI(desc)

#else

#       define FY_INFOI(desc)  do{ __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_INFOI, desc) }while(0)
#       define FY_XINFOI(desc) do{ __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_INFOI, get_object_id()<<": "<<desc) }while(0)

#endif

//macro to output infod level trace, it's enabled by default
//--X version is used within member functions of class which realizes object_id_it
#if defined(FY_TRACE_DISABLE_INFOD)

#       define FY_INFOD(desc)
#       define FY_XINFOD(desc)

#else

#       define FY_INFOD(desc)  do{ __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_INFOD, desc) }while(0)
#       define FY_XINFOD(desc) do{ __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_INFOD, get_object_id()<<": "<<desc) }while(0)

#endif

//macro to output function call track trace, it's disabled by default
//--X version is used within member functions of class which realizes object_id_it
#if defined(FY_TRACE_ENABLE_FUNC)

class __func_tracker_t
{
public:
        __func_tracker_t() {}

        ~__func_tracker_t()
        {
                __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_FUNC, "Leave: "<<bb_func_desc)
        }
public:
        bb_t bb_func_desc;
};

#       define FY_FUNC(desc) __func_tracker_t __func_tracker_20090119_klv; do{\
  string_builder_t sb; sb<<desc; sb.build(__func_tracker_20090119_klv.bb_func_desc);\
  __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_FUNC, "Enter: "<<__func_tracker_20090119_klv.bb_func_desc) }while(0)

#       define FY_XFUNC(desc) __func_tracker_t __func_tracker_20090119_klv; do{\
  string_builder_t sb; sb<<get_object_id()<<": "<<desc; sb.build(__func_tracker_20090119_klv.bb_func_desc);\
  __INTERNAL_FY_TRACE_SERVICE(TRACE_LEVEL_FUNC, "Enter: "<<__func_tracker_20090119_klv.bb_func_desc) }while(0)

#else

#       define FY_FUNC(desc)
#       define FY_XFUNC(desc)

#endif

/*[tip]exception terminator 
 *[desc] catch exception and call FY_ERROR to log it to trace service
 *[history] 
 * Initialize: 2008-6-13
 */
#define FY_EXCEPTION_TERMINATOR(ctx_desc,final_logic) }catch(exception_t& e){\
         bb_t bb;\
         e.to_string(bb);\
         int8 *str=bb.get_buf();\
         if(str) { FY_ERROR(ctx_desc<<str);}\
         final_logic;\
       }catch(std::exception& e){\
         FY_ERROR(ctx_desc<<e.what());\
         final_logic;\
       }catch(...){\
         FY_ERROR(ctx_desc<<"unknown exception");\
         final_logic; }

/*[tip]exception terminator extend
 *[desc] catch exception and call FY_XERROR to log it to trace service, which is
 * used within member function of class which realizes object_id_it
 *[history] 
 * Initialize: 2008-6-13
 */
#define FY_EXCEPTION_XTERMINATOR(final_logic) }catch(exception_t& e){\
         bb_t bb;\
         e.to_string(bb);\
         char *str=bb.get_buf();\
         if(str) FY_XERROR(str);\
         final_logic;\
       }catch(std::exception& e){\
         FY_XERROR(e.what());\
         final_logic;\
       }catch(...){\
         FY_XERROR("unknown exception");\
         final_logic; }

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_TRACE_DREAMFREELANCER_20080318_H__

