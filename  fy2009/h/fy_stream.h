/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2006-6-19 
 * ====================================================================
 */
#ifndef __FENGYI2009_STREAM_DREAMFREELANCER_20090520_H__
#define __FENGYI2009_STREAM_DREAMFREELANCER_20090520_H__ 

#include "fy_base.h"

#ifdef LINUX
#include <sys/uio.h>
#endif //LINUX

DECL_FY_NAME_SPACE_BEGIN

/*[tip]class declaration
 */
class memory_stream_t;
typedef smart_pointer_tt<memory_stream_t> sp_mstream_t;

class fast_memory_stream_t;
typedef smart_pointer_tt<fast_memory_stream_t> sp_fmstream_t;

class oneway_pipe_t;
typedef smart_pointer_tt<oneway_pipe_t> sp_owp_t;

/*[tip] stream interface
 *[desc] generally describe a kind of bytes string medium(eg. file,memory or other etc).
 *[history] 
 * Initialize: 2006-6-19
 */
class stream_it : public lookup_it
{
public:
        //read bytes from current position of stream and move current position after reading
        //parameters: buf,hold bytes to read; len, expected length to read,
        //buf is null but len isn't zero indicates to skip specified length of bytes
        //return actually readed length of bytes on success, throw exception on error
        //in fact, buf can be almost any type, e.g. int16* ,int32*,etc
        virtual uint32 read(int8* buf, uint32 len)=0;

        //write bytes to current position of stream and move current position after writing
        //parameters: buf,ready for writing bytes; len, writable length
        //buf is null but len isn't zero indicates to skip specified length of bytes
        //return actually written length of bytes on success, throw exception on error
        virtual uint32 write(const int8* buf, uint32 len)=0;
};

typedef smart_pointer_lu_tt<stream_it> sp_stream_t;

/*[tip] Interface Random Stream
 *[desc]support random read/write stream medium
 *[history] 
 * Initialize: 2006-6-19
 */

const int8 STM_SEEK_BEGIN=0; //calc new position from beginning of stream
const int8 STM_SEEK_CUR=1; //calc new position from current position
const int8 STM_SEEK_END=2; //calc new position from end of stream

class random_stream_it : public stream_it
{
public:
        //change current position of stream
        //return new current position
	//parameters: option,base position to calculate new position;
        //Note: disallow to seek over the end of written content
        virtual uint32 seek(int8 option, int32 offset)=0;
};

typedef smart_pointer_lu_tt<random_stream_it> sp_rstream_t;

/*[tip]
 *[desc] to adapt writev, readv api
 *[history] 
 * Initialize: 2008-10-28
 */
 #ifdef POSIX

class iovec_it : public lookup_it
{
public:
        //read bytes from current position of stream and move current position after reading
        //parameters: vector,to hold bytes to read; count, vector item count,
        //return actually length of bytes read on success, throw exception on error
        virtual uint32 readv(struct iovec *vector, int32 count)=0;

        //write bytes to current position of stream and move current position after writing
        //parameters: vector,ready for writting bytes; count, vector item count,
        //buf is null but len isn't zero indicates to skip specified length of bytes
        //return actually written length of bytes on success, throw exception on error
        virtual uint32 writev(struct iovec *vector, int32 count)=0;
};

typedef smart_pointer_lu_tt<iovec_it> sp_iovec_t;

#endif //POSIX

/*[tip] adaptor to stream_it
 *[desc] change access interface of stream_it,always use it as stack object but never heap object
 *[history] 
 * Initialize: 2006-6-19
 */
const int8 BO_UNKNOWN=0;
const int8 BO_LITTLE_ENDIAN=1;//i386 host byte order
const int8 BO_BIG_ENDIAN=2; //network byte order

const int8 BO_UPPERBOUND = BO_BIG_ENDIAN;

//stream_adaptor option
const int8 STM_ADP_OPT_COPY=0;//adapt to stream_it and add its reference,release its reference on destruction
const int8 STM_ADP_OPT_ATTACH=1; //adapt to stream_it but not add its reference,release its reference on destruction
const int8 STM_ADP_OPT_STANDALONE=2; //adapt to stream_it, not add its reference, not release its reference on destruction

class stream_adaptor_t : public object_id_impl_t
{
public:
        //adaptor c-style byte buffer(prealloced size) to operator << and >>
        //if decode length > _buf_len, if _auto_truncate is true, extra data will be abandoned silently,
        //if _auto_truncate is false, will throw exception
        class raw_buf_t
        {
                friend class stream_adaptor_t;
        public:
                raw_buf_t(int8 *raw_buf, uint32 buf_len, bool auto_truncate=false)
                {
                        _raw_buf= raw_buf;
                        _buf_len= buf_len;
                        _data_size=0;
                        _auto_truncate = auto_truncate;
                }

                inline uint32 get_data_size() const throw() { return _data_size; }
        private:
                int8 *_raw_buf;
                uint32 _buf_len;
                uint32 _data_size;
                bool _auto_truncate;
        };

        //adaptor c-style string to operator >>, it will auto add '\0' terminator
        //even if truncation occurs, '\0' terminator is properly appended as well
        class raw_str_t : public raw_buf_t
        {
        public:
                raw_str_t(int8 *raw_buf, uint32 buf_len, bool auto_truncate=false)
                        : raw_buf_t(raw_buf, buf_len, auto_truncate)
                {
                }

        };

public:
        //detect and return the host byte order
        static int8 s_test_host_byte_order() throw();

        //reverse bytes in-place,for translation between host byte order and network byte order
        //--modify swap algrithm, can increase 35% performance,2006-10-3,usa
        //--change algrithm from swap to htonl or htons, later efficiency is about 100% more than the former,2007-8-14
        //but can't replace this function with htonl and htons, if little endian byte order is selected as transportation byte
        //order, the htonl and htons can do nothing on a machine with big endian host byte order--2008-3-25
        static void s_reverse_bytes(uint8* data, uint8* dest_buf, uint16 len);

        //calculate char* persisted size
        static inline uint32 s_persist_size_cstr(const int8* cstr)
        {
                if(cstr)
                        return sizeof(uint32) + strlen(cstr);
                else
                        return sizeof(uint32);
        }

        //calculate string_t persisted size
        static inline uint32 s_persist_size_str(const string_t& str) { return sizeof(uint32) + str.size(); }

        //calculate int8v_t persisted size
        static inline uint32 s_persist_size_int8v(const int8v_t& i8v) { return sizeof(uint32) + i8v.size(); }

        //calculate bb_t persisted size,2008-4-14
        static inline uint32 s_persist_size_bb(const bb_t& bb) { return sizeof(uint32) + bb.get_filled_len(); }

        //calculate raw_buf_t persisted size
        static inline uint32 s_persist_size_raw_buf(const raw_buf_t& raw_buf)
        {
                return sizeof(raw_buf._buf_len) + raw_buf._buf_len;
        }

        //calculate raw_str_t persisted size
        static inline uint32 s_persist_size_raw_str(const raw_str_t& raw_str)
        {
                return sizeof(raw_str._buf_len) + raw_str._buf_len;
        }
public:
        stream_adaptor_t();
        //attach an stream_it to this adaptor
        stream_adaptor_t(stream_it *stm, int8 option=STM_ADP_OPT_COPY);
        ~stream_adaptor_t();

        //set byte order flag for attached stream--because byte order conversion will cause some performce decrease,
        //so it's reasonable to select the most common byte order(maybe different from network byte order) as 'standard'
        //availabe para:BO_UNKNOWN--consistent with host byte order;BO_LITTLE_ENDIAN(i386 byte order);
        //BO_BIG_ENDIAN(network byte order)
        void set_byte_order(int8 flag) throw();

        //get network byte order flag
        inline int8 get_byte_order() const throw(){ return _bo_flag; }

        //set byte order to network byte order
        inline void set_nbo() throw() {_bo_flag=BO_BIG_ENDIAN;}

        //attach an stream_it to this adaptor
        void attach(stream_it *stm, int8 option=STM_ADP_OPT_ATTACH) throw();
        inline stream_it *detach() throw() { return _sp_stm.detach(); }
        inline stream_it *get_attached_stream() throw() { return (stream_it*)_sp_stm; }

        stream_adaptor_t& operator <<(int8 data);
        stream_adaptor_t& operator <<(uint8 data);
        stream_adaptor_t& operator <<(int16 data);
        stream_adaptor_t& operator <<(uint16 data);
        stream_adaptor_t& operator <<(int32 data);
        stream_adaptor_t& operator <<(uint32 data);
	stream_adaptor_t& operator <<(int64 data);
	stream_adaptor_t& operator <<(uint64 data);
        stream_adaptor_t& operator <<(const int8* cstr);
        stream_adaptor_t& operator <<(const string_t& str);
        stream_adaptor_t& operator <<(const int8v_t& i8v);
        stream_adaptor_t& operator <<(const bb_t& bb);//2008-4-14
        stream_adaptor_t& operator <<(const raw_buf_t& raw_buf);
        stream_adaptor_t& operator <<(const raw_str_t& raw_str);//use raw buffer to output decoded data

        stream_adaptor_t& operator >>(int8& data);
        stream_adaptor_t& operator >>(uint8& data);
        stream_adaptor_t& operator >>(int16& data);
        stream_adaptor_t& operator >>(uint16& data);
        stream_adaptor_t& operator >>(int32& data);
        stream_adaptor_t& operator >>(uint32& data);
	stream_adaptor_t& operator >>(int64& data);
	stream_adaptor_t& operator >>(uint64& data);
        stream_adaptor_t& operator >>(string_t& str);
        stream_adaptor_t& operator >>(int8v_t& i8v);
        stream_adaptor_t& operator >>(bb_t& bb);//2008-4-14
        stream_adaptor_t& operator >>(raw_buf_t& raw_buf);
        stream_adaptor_t& operator >>(raw_str_t& raw_str);//ensure c-style string with proper '\0' terminator

protected:
        //determine current host byte order is identical with destination byte order or not
        inline bool _is_correct_bo() const throw(){ return (_bo_flag==BO_UNKNOWN || s_test_host_byte_order()==_bo_flag); }

        //check if dest byte order is network byte order
        inline bool _is_nbo() const throw() { return BO_BIG_ENDIAN == _bo_flag; }
        void _lazy_init_object_id() throw(){ OID_DEF_IMP("stream_adaptor"); }
protected:
        static int8 _s_bo_host;//host byte order
protected:
        smart_pointer_lu_tt<stream_it> _sp_stm;
        int8 _bo_flag;//write/read data to/from stream with specific byte order
        int8 _adp_option;
};

/*[tip] persist interface
 *[desc] support an object to save to or load from an stream_it medium
 *[history] 
 * Initialize: 2006-6-19
 * to reduce complexibility, removing the support for non-binary format,2008-5-28
 * change stream_it *para to stream_adaptor_t& to support byte order feature,2008-5-28
 */
class persist_it : public lookup_it
{
public:
        //which indentify a class, used for prototype registration and object activation/deactivation
        //, zero means not applicable
        virtual uint32 get_persist_id() const throw()=0;

	//add pin to avoid potential persist id definition confilict, 2009-6-10
	virtual uint32 get_persist_pin() const throw()=0;

        //calculate persistent size of this object,allow the size calulated is a little greater than actual
        //persistent size,but disallowed it's smaller, zero means not applicable
        virtual uint32 get_persist_size() const throw()=0;

        //save this object to parameter stream,success, throw exception on error
        virtual void save_to(stream_adaptor_t& stm_adp)=0;

        //load an object from parameter stream,success, trhow exception on error
        virtual void load_from(stream_adaptor_t& stm_adp)=0;
};

typedef smart_pointer_lu_tt<persist_it> sp_persist_t;

/*[tip]
 *[desc] load/save object which is registered in prototype_manager_t from/to stream media
 *       persisted pattern: persist_id(uint32) + object's persited bytes
 *[history] 
 * Initialize: 2008-5-28
 */
class persist_helper_t
{
public:
        //sizeof(ptid) + object's persist size
        //zero means not applicable
        inline static uint32 get_persist_size(persist_it *obj)
        {
                return sizeof(uint32) + (obj? obj->get_persist_size() : 0);
        }

        //obj can be zero, then zero persist_id will be saved to stream adaptor
        static void save_to(persist_it *obj, stream_adaptor_t& stm_adp);
        static sp_persist_t load_from(stream_adaptor_t& stm_adp);
};

/*[tip] memory-based random stream
 *[desc] an random_stream_it implementation on memory buffer,caller can attach an 'out' memory block to this stream,this block can
 *      be on stack or heap,in this occasion,caller must ensure that block is big enough and stream will be on only one memory
 *      block, if caller doesn't attach memory block,stream will allocated new heap block on demand and stream can be on numerous
 *      blocks
 *[history] 
 * Initialize: 2006-6-19
 *1.attach buffer feature is removed from this class--2006-7-12
 *2.change implementation from above base-list to base-char array, performance coefficient increase from above 0.33 to 0.65.
 *      make a great progress --2006-7-19
 *3.test reveals that algorithm of _alloc_buffer have great impact on efficiency and test proves that current algorithm is almost
 *      as efficient as pre-allocated buffer,but pre-allocated buffer maybe can save some memory --2006-7-19
 *4.pass purify memory check,2006-7-20
 *5. new revision add more validation check but leads to some performance decrease 2008-3-25
*/

const uint32 MSTM_DEFAULT_MIN_MBLK_SIZE=32;

class memory_stream_t : public random_stream_it,
                        public object_id_impl_t,
                        public ref_cnt_impl_t
{
public:
        //para rcts_flag(ref_cnt thread-safe flag) indicate whether its ref_cnt_it should be thread-safe
        static sp_mstream_t s_create(bool rcts_flag=false);
public:

#ifdef POSIX

        //wrapp this object's memory buffers to struct iovec for socket fucntion writev to avoid extra data copy,2008-3-25
        class iovec_box_t
        {
                friend class memory_stream_t;
        public:
                iovec_box_t();
                ~iovec_box_t();
                void clear();

                inline struct iovec *get_iovec() throw() { return _iovec; }
                inline uint32 get_vec_size() const throw() { return _vec_size; }
        private:
                //connect this object with relevant memory stream object, it's only called by relevant memory_stream_t object
                void _set_ref_mstm(ref_cnt_it *ref_mstm);
        private:
                struct iovec *_iovec;
                uint32 _vec_size;
                ref_cnt_it *_ref_mstm; //referred memory_stream_t object
        };

#endif //POSIX

public:
        virtual ~memory_stream_t();

        //specify min memory allocation size
        void set_min_memory_blk_size(uint32 min_blk_size) throw();
        inline uint32 get_min_memory_blk_size() const throw() { return _min_blk_size; }

        uint32 copy_to(bb_t& bb);//2008-4-14
        uint32 copy_to(int8v_t& i8v);

#ifdef POSIX

        //map memory blocks to struct iovec for writev() socket function
        //return total bytes,if detach_flag is true, memory blocks will be detached from memory stream object,
        uint32 get_iovec(iovec_box_t& iov_box, bool detach_flag=false);

#endif //POSIX

        //explicitly pre-alloc buffer for stream for better performance
        void prealloc_buffer(uint32 siz);

        //lookup_it
        void *lookup(uint32 iid, uint32 pin) throw();

        //stream_it
        //buf is null but len isn't zero,will cause current position to jump,2006-7-20
        uint32 read(int8* buf, uint32 len);
        uint32 write(const int8* buf, uint32 len);

        //random_stream_it
        //this implementation disallow to seek over the end of written content
        uint32 seek(int8 option,int32 offset);
protected:
        //memory block nested type
        class _mblk_t
        {
        public:
                _mblk_t()
                {
                        _blk=0;
                        _size=0;
                        _pos_offset=0;
                        _pos=0;
                }
                _mblk_t(int8 *blk, uint32 blk_size, uint32 offset_from_start, uint32 offset_in_blk)
                {
                        _blk=blk;
                        _size=blk_size;
                        _pos_offset=offset_from_start;
                        _pos=offset_in_blk;
                }
                _mblk_t(const _mblk_t& mblk)
                {
                        _blk=mblk._blk;
                        _size=mblk._size;
                        _pos_offset=mblk._pos_offset;
                        _pos=mblk._pos;
                }
                const _mblk_t& operator =(const _mblk_t& mblk)
                {
                        _blk=mblk._blk;
                        _size=mblk._size;
                        _pos_offset=mblk._pos_offset;
                        _pos=mblk._pos;

                        return mblk;
                }
        public:
                int8 *_blk;//a block of memory
                uint32 _size;//size of block;
                uint32 _pos_offset;//this block beginning position from the stream beginning
                uint32 _pos; //next read or write position from block beginning
        };
protected:
        memory_stream_t();//disallow instantiating directly
        void _lazy_init_object_id() throw() { OID_DEF_IMP("memory_stream"); }
        //new a new buffer and append to  _blk_v
        void _alloc_buffer(uint32 want_size);
        void _clear();
protected:
        _mblk_t * _blk_v;//memory block array
        uint16 _len_v; //lengh of _blk_v
        uint16 _idx_next_alloc;//next allocated _blk_v item index
        uint16 _idx_next_rw;//next read or write _blk_v item index
        uint32 _len_written;//length(bytes) of which has been written
        uint32 _min_blk_size;
        critical_section_t _cs; //can be used to syn ref_cnt_it if needed
};

/*[tip]
 *[desc] a very simple implementation of memory stream to keep performance as high as posssible,
 *this stream can only be attached a prepared buffer block and never manage the block's life cycle
 *[history] 
 * Initialize: 2006-7-11
 *1. pass purify memory check,2006-7-20
 */

class fast_memory_stream_t : public random_stream_it,
                             public object_id_impl_t,
                             public ref_cnt_impl_t
{
public:
        static sp_fmstream_t s_create(bool rcts_flag=false);
        static sp_fmstream_t s_create(int8* buf,uint32 len,uint32 filled_len=0, bool rcts_flag=false);
public:
        //attach a caller pre-allocated memory buffer(stack or heap) to this stream
        void attach(int8* buf, uint32 len, uint32 filled_len=0);

        //detach memory buffer from stream,which has been attached before,return buffer size,otherwise,it return 0
        uint32 detach(int8** ppBuf, uint32 *pFilledLen) throw();//return size of buffer

        inline int8 *get_buf() throw() { return _buf; }
        inline uint32 get_len() const throw() { return _len; }
        inline uint32 get_cur_pos() const throw() { return _pos; }
        inline uint32 get_filled_len() const throw() { return _filled_len; }

        //lookup_it
        void *lookup(uint32 iid, uint32 pin) throw();

        //stream_it
        uint32 read(int8* buf, uint32 len);
        uint32 write(const int8* buf, uint32 len);

        //random_stream_it
        //this implementation disallow to seek over the end of written content
        uint32 seek(int8 option, int32 offset);
protected:
        fast_memory_stream_t();
        fast_memory_stream_t(int8* buf,uint32 len,uint32 filled_len=0);
        void _lazy_init_object_id() throw(){ OID_DEF_IMP("fast_memory_stream"); }
protected:
        int8* _buf;
        uint32 _len;
        uint32 _pos;
        uint32 _filled_len;
        critical_section_t _cs;
};

/*[tip]
 *[desc] one-way pipe. it provides an impressive inter-thread data exchaning mechanism, if only one thread to read/write,
 *       read/write operation will not need lock/unlock, so depending on different cases, this pipe can work as no-lock,
 *       semi-no-lock or lock mode
 *       it's usually expensive that inter-thread data exchanging based on lock/unlock. this class makes data exchanging
 *       more efficient by eliminating lock/unlock from read/write.this class is a kind of stream,but to make it as efficient as
 *       possible,it doesn't implement stream_it interface to avoid virtual function performance fall.
 *[memo] when buffer is full,write will fail,this class doesn't offer wait and wake mechanism,the caller has to be responsible
 *       for it or spin or sleep to wait for buffer free
 *[history]
 * Initialize: 2006-8-16
 *1. benchmark reveals that it's a very efficient itc mechanism(about 33 times than lock/unlock mechanism if data
 *   exchanging one byte by one byte
 *2. test shows that throw...exception macro decrease performance dramatically, it leads to read/write is
 *   most slower than nolock/unlock, for performance, never apply throw..exception framework in this class--2008-4-3
 *3. enhance this pipe to allow multi-threads read and/or write simultaneously, if there are more than one reading threads,
 *   reading operation will have to share a read-lock, it's also true for multi writing threads.--2008-6-4
 */

/*
 *[desc] sometime, there can be some data still exists in oneway_pipe_t on its destroying,these data may be some object
 *       pointer, which must be destroyed correctly on oneway_pipe_t destroying, otherwise, memory leak will occur.
 *       this interface used to clear rest data existing in oneway_pipe_t on its destroying,
 *[memo]
 *1.it's add/release reference thread-safe
 *
 *[history] 
 * Initialize: 2007-3-27
 */
class oneway_pipe_sink_it : public lookup_it
{
public:
        //buf:rest data start pointer; buf_len: rest data length
        virtual void on_destroy(const int8 *buf, uint32 buf_len)=0;
};

const uint32 DEFAULT_NOLOCK_PIPE_SIZE=256;
//24bits, range of atomic_t type
const uint32 MAX_NOLOCK_PIPE_SIZE=0x00ffffff;

class oneway_pipe_t : public object_id_impl_t,
                      public ref_cnt_impl_t
{
public:
        static sp_owp_t s_create(uint32 pipe_size=DEFAULT_NOLOCK_PIPE_SIZE, bool rcts_flag=false);
public:
        ~oneway_pipe_t();

        inline uint32 get_buf_size() const throw() { return _len_buf; }
        uint32 get_w_size() const; //writable size
        uint32 get_r_size() const; //readable size

        //register current thread for read, which means reading operation only be allowed from this thread
        bool register_read();
        void unregister_read();

        //register current thread for write, which means writing operation only be allowed from this thread
        bool register_write();
        void unregister_write();

        inline bool is_write_registered() const throw() { return _w_tid!=0; }
        inline bool is_read_registered() const throw() { return _r_tid!=0; }
        inline uint32 get_r_tid() const throw(){ return _r_tid; }
        inline uint32 get_w_tid() const throw(){ return _w_tid; }
        uint32 read(int8* buf,uint32 len, bool auto_commit=true);
        uint32 write(const int8* buf,uint32 len, bool auto_commit=true);
        void commit_w();//commit write,uncommited data will cann't be read
        void rollback_w();//rollback write,rollback last uncommitted writen data

        void commit_r();//commit read, uncommited data will cann't be overwritten
        void rollback_r(); //rollback read, rollback last uncommitted read data

        inline void register_sink(oneway_pipe_sink_it *sink){ _sp_sink.copy_from(sink); }

        //lookup_it
        void *lookup(uint32 iid, uint32 pin) throw();
protected:
        //to avoid overhead of lock/unlock, buffer size will disallowed changing after construct
        oneway_pipe_t(uint32 buf_size=DEFAULT_NOLOCK_PIPE_SIZE);

        void _lazy_init_object_id() throw(){ OID_DEF_IMP("oneway_pipe"); }
private:
        typedef smart_pointer_lu_tt<oneway_pipe_sink_it> _sp_sink_t;
private:
        int8 *_buf;
        uint32 _len_buf;
        volatile uint32 _r_pos; //next read position
        volatile uint32 _r_pos_c; //committed read position,2008-4-1
        uint32 _r_tid; //read thread id
        uint32 _w_tid; //write thread id
        volatile uint32 _w_pos; //next write position
        volatile uint32 _w_pos_c;//committed write position
        critical_section_t _cs_reg;//for register read/write
        _sp_sink_t _sp_sink; //destroy rest data on destructor
        critical_section_t _cs_r; //read lock
        critical_section_t _cs_w; //write lock
};

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_STREAM_DREAMFREELANCER_20090520_H__
