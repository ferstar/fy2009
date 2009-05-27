/* ====================================================================
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 The FengYi2009 Project, All rights reserved.
 *
 * Author: DreamFreelancer, zhangxb66@2008.sina.com
 *
 * [History]
 * initialize: 2009-5-21
 * ====================================================================
 */
#include "fy_stream.h"

USING_FY_NAME_SPACE

//stream_adaptor_t
//initialize static member variable
int8 stream_adaptor_t::_s_bo_host=BO_UNKNOWN;
    
int8 stream_adaptor_t::s_test_host_byte_order() throw()
{   
        //to improve efficiency,2006-10-18,usa
        if(_s_bo_host == BO_UNKNOWN)
        {
                uint16 tmp= 0x55aa;
                if(*(uint8*)&tmp == 0xaa)
                        _s_bo_host=BO_LITTLE_ENDIAN;
                else
                        _s_bo_host=BO_BIG_ENDIAN;
        }
        return _s_bo_host;
}

void stream_adaptor_t::s_reverse_bytes(uint8* data, uint8* dest_buf, uint16 len)
{
        if(_s_bo_host == BO_UNKNOWN) s_test_host_byte_order(); //fix bug of big-endian host byte order,2008-12-23
        switch(len)
        {
        case 2:
                if(BO_LITTLE_ENDIAN == _s_bo_host)//fix bug of big-endian host byte order,2008-12-23
                {
                        //htons equivalent algrithm,pass test 2008-3-24
                        *((uint16*)dest_buf)=((uint16)(data[0]) << 8) | ((uint16)data[1]);
                }
                else
                {
                        *((uint16*)dest_buf)=(uint16)(data[0]) | ((uint16)(data[1]) << 8);
                }
                break;
        case 4:
                if(BO_LITTLE_ENDIAN == _s_bo_host)//fix bug of big-endian host byte order,2008-12-23
                {
                        //htonl equivalent algrithm,pass test 2008-3-24
                        *((uint32*)dest_buf)=((uint32)(data[0]) << 24) | ((uint32)(data[1]) << 16) | ((uint32)(data[2]) << 8) |
                                                (uint32)data[3];
                }
                else
                {
                        *((uint32*)dest_buf)=(uint32)data[0] | ((uint32)(data[1]) << 8) | ((uint32)(data[2]) << 16) |
                                                ((uint32)(data[3]) << 24);
                }
                break;

        default:
                __INTERNAL_FY_THROW("stream_adaptor_t","s_reverse_bytes",
                         "sasrb", "only 2,4 bytes type length is supported");
                break;
        }
}

stream_adaptor_t::stream_adaptor_t()
{
        _bo_flag=BO_UNKNOWN; //default byte order is host byte order
        _adp_option=STM_ADP_OPT_COPY;
}

stream_adaptor_t::stream_adaptor_t(stream_it *stm, int8 option)
{
        _adp_option=option;
        switch(_adp_option)
        {
        case STM_ADP_OPT_COPY:
                _sp_stm.copy_from(stm);
                break;
        case STM_ADP_OPT_ATTACH:
        case STM_ADP_OPT_STANDALONE:
                _sp_stm.attach(stm);
                break;
        default:
                break;
        }

        _bo_flag=BO_UNKNOWN; //default byte order is host byte order
}

stream_adaptor_t::~stream_adaptor_t()
{
        if(_adp_option == STM_ADP_OPT_STANDALONE) _sp_stm.detach();
}

void stream_adaptor_t::set_byte_order(int8 flag) throw()
{
        switch(flag)
        {
        case BO_UNKNOWN:
        case BO_LITTLE_ENDIAN:
        case BO_BIG_ENDIAN:
                _bo_flag=flag;
                break;
        default:
                //error
                break;
        }
}

void stream_adaptor_t::attach(stream_it *stm, int8 option) throw()
{
        _adp_option=option;
        switch(_adp_option)
        {
        case STM_ADP_OPT_COPY:
                _sp_stm.copy_from(stm);
                break;
        case STM_ADP_OPT_ATTACH:
        case STM_ADP_OPT_STANDALONE:
                _sp_stm.attach(stm);
                break;
        default:
                break;
        }
}

//to keep the following operator as efficient as possible, don't use throw catch marcros within them to eliminate
//function name local variable dfinition and assignment

stream_adaptor_t& stream_adaptor_t::operator <<(int8 data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_sp_stm->write(&data,sizeof(data)) != sizeof(data))
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator<<(int8)", "sasi8",
                        "fail to write all or part data into stream");
        }
        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator <<(uint8 data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_sp_stm->write((const int8*)(&data),sizeof(data)) != sizeof(data))
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator<<(uint8)", "sasui8",
                        "fail to write all or part data into stream");
        }
        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator <<(int16 data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_is_correct_bo())
        {
                if(_sp_stm->write((const int8*)(&data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator<<(int16)", "sasi16",
                                "fail to write all or part data into stream");
                }
        }
        else
        {
                int16 r_data;
                s_reverse_bytes((uint8*)&data,(uint8*)&r_data,sizeof(data));
                if(_sp_stm->write((const int8*)(&r_data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator<<(int16)", "sasi16-2",
                                "fail to write all or part data into stream");
                }
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator <<(uint16 data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_is_correct_bo())
        {
                if(_sp_stm->write((const int8*)(&data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator<<(uint16)", "sasui16",
                                "fail to write all or part data into stream");
                }
        }
        else
        {
                uint16 r_data;
                s_reverse_bytes((uint8*)&data,(uint8*)&r_data,sizeof(data));
                if(_sp_stm->write((const int8*)(&r_data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator<<(uint16)", "sasui16-2",
                                "fail to write all or part data into stream");
                }
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator <<(int32 data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_is_correct_bo())
        {
                if(_sp_stm->write((const int8*)(&data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator<<(int32)", "sasi32",
                                "fail to write all or part data into stream");
                }
        }
        else
        {
                int32 r_data;
                s_reverse_bytes((uint8*)&data,(uint8*)&r_data,sizeof(data));
                if(_sp_stm->write((const int8*)(&r_data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator<<(int32)", "sasi32-2",
                                "fail to write all or part data into stream");
                }
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator <<(uint32 data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_is_correct_bo())
        {
                if(_sp_stm->write((const int8*)(&data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator<<(uint32)", "sasui32",
                                "fail to write all or part data into stream");
                }
        }
        else
        {
                uint32 r_data;
                s_reverse_bytes((uint8*)&data,(uint8*)&r_data,sizeof(data));
                if(_sp_stm->write((const int8*)(&r_data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator<<(uint32)", "sasui32-2",
                                "fail to write all or part data into stream");
                }
        }

        return *this;
}

//bit pattern:length_of_string(uint32)|sequence of characters,not include '\0'
stream_adaptor_t& stream_adaptor_t::operator <<(const int8* cstr)
{
        FY_ASSERT(!_sp_stm.is_null());

        uint32 len=(cstr==0?0: ::strlen(cstr));//not include '\0'
        *this<<len;
        if(0 == len) return *this;

        if(_sp_stm->write(cstr,len) != len) //not include '\0'
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator<<(tchar_t*)", "sascstr",
                        "fail to write all or part data into stream");
        }
        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator <<(const string_t& str)
{
        FY_ASSERT(!_sp_stm.is_null());

        uint32 len=str.size();
        *this<<len;
        if(0 == len) return *this;
        if(_sp_stm->write(str.c_str(),len) != len) //not include '\0'
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator<<(const string_t&)", "sasstr",
                        "fail to write all or part data into stream");
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator <<(const int8v_t& i8v)
{
        FY_ASSERT(!_sp_stm.is_null());

        uint32 len=i8v.size();
        *this<<len;
        if(0 == len) return *this;
        if(_sp_stm->write((const int8*)(&i8v[0]), len) != len)
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator<<(const int8v_t&)", "sasi8v",
                        "fail to write all or part data into stream");
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator <<(const bb_t& bb)
{
        FY_ASSERT(!_sp_stm.is_null());

        uint32 len=bb.get_filled_len();
        *this<<len;
        if(0 == len) return *this;
        if(_sp_stm->write(bb.get_buf(), len) != len)
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator<<(const bb_t&)", "sasbb",
                        "fail to write all or part data into stream");
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator <<(const raw_buf_t& raw_buf)
{
        FY_ASSERT(!_sp_stm.is_null());

        *this<<raw_buf._buf_len;
        if(0 == raw_buf._buf_len) return *this;
        if(_sp_stm->write(raw_buf._raw_buf, raw_buf._buf_len) != raw_buf._buf_len)
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator<<(const raw_buf_t&)", "sasrbuf",
                        "fail to write all or part data into stream");
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator <<(const raw_str_t& raw_str)
{
        FY_ASSERT(!_sp_stm.is_null());

        *this<<raw_str._buf_len;
        if(0 == raw_str._buf_len) return *this;
        if(_sp_stm->write(raw_str._raw_buf, raw_str._buf_len) != raw_str._buf_len)
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator<<(const raw_str_t&)", "sasrstr",
                        "fail to write all or part data into stream");
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator >>(int8& data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_sp_stm->read((int8*)(&data),sizeof(data)) != sizeof(data))
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator>>(int8&)", "sadi8",
                        "fail to read all or part data from stream");
        }
        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator >>(uint8& data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_sp_stm->read((int8*)(&data),sizeof(data)) != sizeof(data))
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator>>(uint8&)", "sadui8",
                        "fail to read all or part data from stream");
        }
        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator >>(int16& data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_is_correct_bo())
        {
                if(_sp_stm->read((int8*)(&data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator>>(int16&)", "sadi16",
                                "fail to read all or part data from stream");
                }
        }
        else
        {
                int16 r_data;
                if(_sp_stm->read((int8*)(&r_data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator>>(int16&)", "sadi16-2",
                                "fail to read all or part data from stream");
                }
                s_reverse_bytes((uint8*)&r_data,(uint8*)&data,sizeof(data));
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator >>(uint16& data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_is_correct_bo())
        {
                if(_sp_stm->read((int8*)(&data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator>>(uint16&)", "sadu16",
                                "fail to read all or part data from stream");
                }
        }
        else
        {
                uint16 r_data;
                if(_sp_stm->read((int8*)(&r_data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator>>(uint16&)", "sadu16-2",
                                "fail to read all or part data from stream");
                }
                s_reverse_bytes((uint8*)&r_data,(uint8*)&data,sizeof(data));
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator >>(int32& data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_is_correct_bo())
        {
                if(_sp_stm->read((int8*)(&data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator>>(int32&)", "sadi32",
                                "fail to read all or part data from stream");
                }
        }
        else
        {
                int32 r_data;
                if(_sp_stm->read((int8*)(&r_data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator>>(int32&)", "sadi32-2",
                                "fail to read all or part data from stream");
                }
                s_reverse_bytes((uint8*)&r_data,(uint8*)&data,sizeof(data));
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator >>(uint32& data)
{
        FY_ASSERT(!_sp_stm.is_null());

        if(_is_correct_bo())
        {
                if(_sp_stm->read((int8*)(&data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator>>(uint32&)", "sadui32",
                                "fail to read all or part data from stream");
                }
        }
        else
        {
                uint32 r_data;
                if(_sp_stm->read((int8*)(&r_data),sizeof(data)) != sizeof(data))
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator>>(uint32&)", "sadui32-2",
                                "fail to read all or part data from stream");
                }
                s_reverse_bytes((uint8*)&r_data,(uint8*)&data,sizeof(data));
        }

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator >>(string_t& str)
{
        FY_ASSERT(!_sp_stm.is_null());

        uint32 len=0;

        FY_TRY

        *this>>len;
        if(!len)
        {
                string_t str_tmp;
                str.swap(str_tmp); //clear isn't supported by VC
                return *this;
        }
        int8 *buf=new int8[len];
        if(_sp_stm->read(buf, len) != len)
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator>>(string_t&)", "sadstr",
                          "fail to read all or part data from stream");
        }
        str.assign(buf,len);
        delete [] buf;

        __INTERNAL_FY_CATCH_N_THROW_AGAIN(get_object_id(), "operator >>(string_t&)", "sadstr-2", "pass",;)

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator >>(int8v_t& i8v)
{
        FY_ASSERT(!_sp_stm.is_null());

        uint32 len=0;

        FY_TRY

        *this>>len;
        i8v.resize(len);
        if(_sp_stm->read(&i8v[0], len) != len)
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator>>(int8v_t&)", "sadi8v",
                          "fail to read all or part data from stream");
        }

        __INTERNAL_FY_CATCH_N_THROW_AGAIN(get_object_id(), "operator >>(int8v_t&)", "sadi8v-2", "pass",;)

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator >>(bb_t& bb)
{
        FY_ASSERT(!_sp_stm.is_null());

        uint32 len=0;

        FY_TRY

        *this>>len;
        bb.reserve(len);
        if(_sp_stm->read(bb.get_buf(), len) != len)
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator>>(bb_t&)", "sadbb",
                          "fail to read all or part data from stream");
        }

        __INTERNAL_FY_CATCH_N_THROW_AGAIN(get_object_id(), "operator >>(bb_t&)", "sadbb-2", "pass",;)

        bb.set_filled_len(len);

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator >>(raw_buf_t& raw_buf)
{
        FY_ASSERT(!_sp_stm.is_null());

        uint32 len=0;

        FY_TRY

        *this>>len;

        if(len > raw_buf._buf_len)//buffer size isn't enough
        {
                if(!raw_buf._auto_truncate)
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator>>(raw_buf_t&)", "sadrbuf",
                                "object buffer isn't enough to hold all data");
                }
                raw_buf._data_size=_sp_stm->read(raw_buf._raw_buf, raw_buf._buf_len);
                if(raw_buf._data_size != raw_buf._buf_len)
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator>>(raw_buf_t&)", "sadrbuf-2",
                                "fail to read all or part data from stream");
                }
                //skip rest data
                uint32 rest_len= len - raw_buf._buf_len;
                if(_sp_stm->read(0, rest_len) != rest_len)
                {
                        __INTERNAL_FY_THROW(get_object_id(), "operator>>(raw_buf_t&)", "sadrbuf-3",
                                "fail to skip truncated data from stream");
                }
                return *this;
        }
        //buffer size is big enough
        raw_buf._data_size=_sp_stm->read(raw_buf._raw_buf, len);
        if(raw_buf._data_size != len)
        {
                __INTERNAL_FY_THROW(get_object_id(), "operator>>(raw_buf_t&)", "sadrbuf-4",
                        "fail to read all or part data from stream");
        }

        __INTERNAL_FY_CATCH_N_THROW_AGAIN(get_object_id(), "operator >>(raw_buf_t&)", "sadrbuf-4", "pass",;)

        return *this;
}

stream_adaptor_t& stream_adaptor_t::operator >>(raw_str_t& raw_str)
{
        FY_ASSERT(!_sp_stm.is_null());

        FY_TRY

        raw_buf_t raw_buf(raw_str._raw_buf, (raw_str._buf_len? raw_str._buf_len - 1 : 0), raw_str._auto_truncate);
        *this>>raw_buf;
        raw_str._data_size=raw_buf._data_size;
        raw_str._raw_buf[raw_str._data_size]=0;

        __INTERNAL_FY_CATCH_N_THROW_AGAIN(get_object_id(), "operator >>(raw_str_t&)", "sadrstr", "pass",;)

        return *this;
}

#ifdef POSIX

//memory_stream_t::iovec_box_t
memory_stream_t::iovec_box_t::iovec_box_t()
{
        _iovec=0;
        _vec_size=0;
        _ref_mstm=0;
}

memory_stream_t::iovec_box_t::~iovec_box_t()
{
        clear();
}

void memory_stream_t::iovec_box_t::clear()
{
        if(_ref_mstm)//memory blocks are controlled by referred memory stream
        {
                _ref_mstm->release_reference();
                _ref_mstm=0;
        }
        else//memory blocks are controlled by itself
        {
                for(int i=0; i<_vec_size; ++i)
                {
                        delete [] static_cast<int8*>(_iovec[i].iov_base);
                }
        }
        delete [] _iovec;
        _iovec=0;
        _vec_size=0;
}

void memory_stream_t::iovec_box_t::_set_ref_mstm(ref_cnt_it *ref_mstm)
{
        if(_ref_mstm) _ref_mstm->release_reference();
        _ref_mstm=ref_mstm;
        if(_ref_mstm) _ref_mstm->add_reference();
}

#endif //POSIX

//memory_stream_t
sp_mstream_t memory_stream_t::s_create(bool rcts_flag)
{
        memory_stream_t *pstm=new memory_stream_t();
        if(rcts_flag) pstm->set_lock(&(pstm->_cs));

        return sp_mstream_t(pstm,true);
}

memory_stream_t::memory_stream_t() : _cs(true)
{
        _len_v=_idx_next_rw=_idx_next_alloc=_len_written=0;
        _blk_v=0;
        _min_blk_size=MSTM_DEFAULT_MIN_MBLK_SIZE;
}

memory_stream_t::~memory_stream_t()
{
        _clear();
}

void memory_stream_t::set_min_memory_blk_size(uint32 min_blk_size) throw()
{
        _min_blk_size=(min_blk_size?min_blk_size : MSTM_DEFAULT_MIN_MBLK_SIZE);
}

uint32 memory_stream_t::copy_to(bb_t& bb)
{
        if(!_len_written) return 0;//empty stream
        bb.reserve(_len_written);
        int8 *rest_buf=bb.get_buf();
        for(uint16 i=0;i<_idx_next_alloc;i++)
        {
                if(i==(_idx_next_alloc - 1))//last written block
                {
                        memcpy(rest_buf,_blk_v[i]._blk,_len_written - _blk_v[i]._pos_offset);
                        break;
                }
                else//not last written block
                {
                        memcpy(rest_buf,_blk_v[i]._blk,_blk_v[i]._size);
                        rest_buf += _blk_v[i]._size;
                }
        }
        bb.set_filled_len(_len_written);

        return _len_written;
}

uint32 memory_stream_t::copy_to(int8v_t& i8v)
{
        if(!_len_written) return 0;//empty stream
        i8v.resize(_len_written);
        int8 *rest_buf=&i8v[0];
        for(uint16 i=0;i<_idx_next_alloc;i++)
        {
                if(i==(_idx_next_alloc - 1))//last written block
                {
                        memcpy(rest_buf,_blk_v[i]._blk,_len_written - _blk_v[i]._pos_offset);
                        break;
                }
                else//not last written block
                {
                        memcpy(rest_buf,_blk_v[i]._blk,_blk_v[i]._size);
                        rest_buf += _blk_v[i]._size;
                }
        }
        return _len_written;
}

#ifdef POSIX

uint32 memory_stream_t::get_iovec(iovec_box_t& iov_box, bool detach_flag)
{
        if(!_len_written)
        {
                iov_box._iovec=0;
                iov_box._vec_size=0;

                return 0;//empty stream
        }

        struct iovec *raw_iov=new struct iovec[_idx_next_alloc];
        if(iov_box._iovec) iov_box.clear();
        iov_box._iovec=raw_iov;
        iov_box._vec_size=_idx_next_alloc;

        for(uint16 i=0;i<_idx_next_alloc;i++)
        {
                if(i==(_idx_next_alloc - 1))//last written block
                {
                        raw_iov[i].iov_base=_blk_v[i]._blk;
                        raw_iov[i].iov_len=_len_written - _blk_v[i]._pos_offset;
                        break;
                }
                else//not last written block
                {
                        raw_iov[i].iov_base=_blk_v[i]._blk;
                        raw_iov[i].iov_len=_blk_v[i]._size;
                }
        }

        if(detach_flag) //memory blocks will be controlled by iov_box
        {
                _clear();
        }
        else //memory blocks is still controlled by memory stream object
        {
                //keep a reference of memory stream to iov_box to avoid memory blocks are deleted by
                //memory stream on destruction as long as iov_box destroy
                iov_box._set_ref_mstm(static_cast<ref_cnt_it*>(this));
        }

        return _len_written;
}

#endif //POSIX

void memory_stream_t::prealloc_buffer(uint32 siz)
{
        _alloc_buffer(siz);
}

//lookup_it
void *memory_stream_t::lookup(uint32 iid) throw()
{
        switch(iid)
        {
        case IID_self:
        case IID_lookup:
                return this;

        case IID_stream:
                return static_cast<stream_it *>(this);

        case IID_random_stream:
                return static_cast<random_stream_it *>(this);

        case IID_object_id:
                return object_id_impl_t::lookup(iid);

        default:
                return ref_cnt_impl_t::lookup(iid);
        }
}

//stream_it
uint32 memory_stream_t::read(int8* buf,uint32 len)
{
        if(!len || !_len_written) return 0;//invalid acceptable buffer or empty stream

        uint32 want_len=len;
        int8 *next_read_to=buf;
        uint32 old_idx_rw=_idx_next_rw;
        for(;_idx_next_rw<_idx_next_alloc;_idx_next_rw++)
        {
                uint32 read_pos=(_idx_next_rw==old_idx_rw?_blk_v[_idx_next_rw]._pos:0);
                uint32 written_size=(_blk_v[_idx_next_rw]._pos_offset + _blk_v[_idx_next_rw]._size > _len_written?
                                        _len_written - _blk_v[_idx_next_rw]._pos_offset:_blk_v[_idx_next_rw]._size);

                uint32 readable_size=written_size - read_pos;

                if(readable_size>=want_len)//this block is enough
                {
                        if(buf) memcpy(next_read_to,_blk_v[_idx_next_rw]._blk+read_pos,want_len);
                        if(readable_size==want_len)//this block has been read to end
                        {
                                if(++_idx_next_rw<_idx_next_alloc) _blk_v[_idx_next_rw]._pos=0;
                        }
                        else //this block hasn't been read to end
                        {
                                _blk_v[_idx_next_rw]._pos=read_pos+want_len;
                        }
                        want_len=0;//read to buf full
                        return len;
                }
                else //this block is not enough
                {
                        if(buf) memcpy(next_read_to,_blk_v[_idx_next_rw]._blk+read_pos,readable_size);
                        next_read_to+=readable_size;
                        want_len -=readable_size;
                }
        }
        //read to end but not enough
        return len - want_len;
}

uint32 memory_stream_t::write(const int8* buf,uint32 len)
{
        if(!len) return 0;//no data to write

        //if no buffer to write,will expand new buffer space to write
        if(_idx_next_rw==_idx_next_alloc)
        {
                _alloc_buffer(len);
        }

        uint32 rest_len=len;
        const int8 *rest_buf=buf;
        bool bFirstLoop=true;//first writting loop
        while(true)
        {
                uint32 write_pos;
                if(bFirstLoop)
                        write_pos=_blk_v[_idx_next_rw]._pos;
                else
                        write_pos=0;

                bFirstLoop=false;

                uint32 free_len=_blk_v[_idx_next_rw]._size - write_pos;
                if(rest_len<=free_len) //buffer space is enough
                {
                        if(buf) memcpy(_blk_v[_idx_next_rw]._blk + write_pos,rest_buf,rest_len);
                        uint32 cur_len=_blk_v[_idx_next_rw]._pos_offset + write_pos + rest_len;
                        if(_len_written<cur_len) _len_written=cur_len;

                        if(rest_len==free_len)//just full
                        {
                                _idx_next_rw++;
                                if(_idx_next_rw<_idx_next_alloc) _blk_v[_idx_next_rw]._pos=0;
                        }
                        else //not full
                                _blk_v[_idx_next_rw]._pos=write_pos+rest_len;

                        break;
                }
                else //not enough
                {
                        if(buf) memcpy(_blk_v[_idx_next_rw]._blk + write_pos,rest_buf,free_len);
                        uint32 cur_len=_blk_v[_idx_next_rw]._pos_offset + write_pos + free_len;
                        if(_len_written<cur_len) _len_written=cur_len;

                        rest_len -= free_len;
                        rest_buf += free_len;
                        _alloc_buffer(rest_len);
                        _idx_next_rw++;
                }
        }
        return len;
}

//random_stream_t
uint32 memory_stream_t::seek(int8 option,int32 offset)
{
        if(!_len_written)//empty stream
        {
                _idx_next_rw=0;
                return 0;
        }
        switch(option)
        {
        case STM_SEEK_BEGIN:
                {
                        if(offset<=0)
                        {
                                _blk_v[_idx_next_rw=0]._pos=0;
                                return 0;
                        }
                        uint32 max_offset=(offset>_len_written?_len_written:offset);
                        for(_idx_next_rw=0;_idx_next_rw<_idx_next_alloc;_idx_next_rw++)
                        {
                                //new pos within this block
                                if(_blk_v[_idx_next_rw]._pos_offset+_blk_v[_idx_next_rw]._size>max_offset)
                                {
                                        _blk_v[_idx_next_rw]._pos=max_offset - _blk_v[_idx_next_rw]._pos_offset;
                                        return max_offset;
                                }
                        }
                        //to end
                        return _len_written;
                }

                break;

        case STM_SEEK_CUR:
                {
                        uint32 cur_pos=(_idx_next_rw==_idx_next_alloc?_len_written:
                                        (_blk_v[_idx_next_rw]._pos_offset+_blk_v[_idx_next_rw]._pos));
                        if(offset==0)
                        {
                                return cur_pos;
                        }
                        else if(offset<0)
                        {
                                uint32 new_pos=(offset*(-1)>cur_pos?0:(cur_pos+offset));
                                if(!new_pos) return _blk_v[_idx_next_rw=0]._pos=0;//to begin

                                for(uint16 i=_idx_next_rw;;i--)
                                {
                                        if(i==_idx_next_alloc) continue;//at end
                                        if(_blk_v[i]._pos_offset<=new_pos)
                                        {
                                                _idx_next_rw=i;
                                                _blk_v[_idx_next_rw]._pos=new_pos - _blk_v[_idx_next_rw]._pos_offset;
                                                return new_pos;
                                        }
                                        if(i==0) break;
                                }
                                return _blk_v[_idx_next_rw=0]._pos=0;
                        }
                        else//offset>0
                        {
                                uint32 new_pos=(cur_pos+offset>_len_written?_len_written:cur_pos+offset);
                                if(_idx_next_rw==_idx_next_alloc) return _len_written;
                                for(uint16 i=_idx_next_rw;i<_idx_next_alloc;i++)
                                {
                                        if(_blk_v[i]._pos_offset + _blk_v[i]._size > new_pos)//new pos within this block
                                        {
                                                _idx_next_rw=i;
                                                _blk_v[_idx_next_rw]._pos=new_pos - _blk_v[_idx_next_rw]._pos_offset;
                                                return new_pos;
                                        }
                                }
                                _idx_next_rw=_idx_next_alloc;
                                return _len_written;
                        }
                }
                break;

        case STM_SEEK_END:
                {
                        if(offset<=0)
                        {
                                _idx_next_rw=_idx_next_alloc - 1;
                                if((_blk_v[_idx_next_rw]._pos_offset + _blk_v[_idx_next_rw]._size) > _len_written)
                                        _blk_v[_idx_next_rw]._pos= _len_written - _blk_v[_idx_next_rw]._pos_offset;
                                else
                                        _idx_next_rw++;

                                return _len_written;
                        }
                        if(offset>=_len_written)//new pos is begin
                        {
                                _blk_v[_idx_next_rw=0]._pos=0;
                                return 0;
                        }
                        uint32 new_len=_len_written - offset;
                        for(_idx_next_rw=_idx_next_alloc - 1;;_idx_next_rw--)
                        {
                                if(_blk_v[_idx_next_rw]._pos_offset<=new_len)//new pos within this block
                                {
                                        _blk_v[_idx_next_rw]._pos=new_len - _blk_v[_idx_next_rw]._pos_offset;
                                        return new_len;
                                }
                                if(_idx_next_rw==0) break;
                        }
                        _blk_v[_idx_next_rw]._pos=0;
                        return 0;
                }
                break;
        }
        return 0;
}

//first block size is 2 times of requested size and next block size is 2 times of current block size
//or requested size
//--optimized following algrithm can balance performance and memory usage,2006-7-19
void memory_stream_t::_alloc_buffer(uint32 want_size)
{
        if(!want_size) return;
        if(want_size < _min_blk_size)
                want_size=_min_blk_size; //to avoid too small memory block,2008-3-25

        if(!_blk_v || _idx_next_alloc==_len_v)
        {
                uint16 new_len_v=_len_v+4;
                _mblk_t *v=new _mblk_t[new_len_v];
                _mblk_t *old_v=_blk_v;
                if(_len_v) memcpy(v,_blk_v,_len_v*sizeof(_mblk_t));

                _blk_v=v;
                _len_v=new_len_v;
                delete [] old_v;

        }
        if(!_idx_next_alloc) //alloc first block
        {
                _blk_v[_idx_next_alloc++]=_mblk_t(new int8[want_size],want_size,0,0);
        }
        else//not first block
        {
                uint32 blk_size=(want_size<<_idx_next_alloc);
                _blk_v[_idx_next_alloc]=_mblk_t(new int8[blk_size],blk_size,
                                        _blk_v[_idx_next_alloc - 1 ]._pos_offset+_blk_v[_idx_next_alloc - 1 ]._size,0);
                _idx_next_alloc++;

        }
}

void memory_stream_t::_clear()
{
       if(!_blk_v) return;
       for(uint16 i=0;i<_idx_next_alloc;++i) delete [] (_blk_v[i]._blk);
       delete [] _blk_v;
       _blk_v=0;
       _len_v=_idx_next_rw=_idx_next_alloc=_len_written=0;
}

//fast_memory_stream_t
sp_fmstream_t fast_memory_stream_t::s_create(bool rcts_flag)
{
        fast_memory_stream_t *pstm=new fast_memory_stream_t();
        if(rcts_flag) pstm->set_lock(&(pstm->_cs));

        return sp_fmstream_t(pstm,true);
}

sp_fmstream_t fast_memory_stream_t::s_create(int8* buf,uint32 len,uint32 filled_len, bool rcts_flag)
{
        fast_memory_stream_t *pstm=new fast_memory_stream_t(buf, len, filled_len);
        if(rcts_flag) pstm->set_lock(&(pstm->_cs));

        return sp_fmstream_t(pstm,true);
}

fast_memory_stream_t::fast_memory_stream_t() : _cs(true)
{
        _buf=0;
        _len=0;
        _pos=0;
        _filled_len=0;
}

fast_memory_stream_t::fast_memory_stream_t(int8* buf,uint32 len,uint32 filled_len) : _cs(true)
{
        FY_ASSERT(filled_len<=len);

        if(!buf || !len) return;
        _filled_len=filled_len;
        _len=len;
        _buf=buf;
        _pos=0;
}

void fast_memory_stream_t::attach(int8* buf,uint32 len,uint32 filled_len)
{
        FY_ASSERT(filled_len<=len);

        if(!buf || !len) return;
        _filled_len=filled_len;
        _len=len;
        _buf=buf;
        _pos=0;
}

uint32 fast_memory_stream_t::detach(int8** ppBuf,uint32 *pFilledLen) throw()
{
        if(!ppBuf || !_buf || !_len) return 0;
        if(pFilledLen) *pFilledLen=_filled_len;
        *ppBuf=_buf;
        uint32 ret_len=_len;

        //reset
        _buf=0;
        _len=0;
        _pos=0;
        _filled_len=0;

        return ret_len;
}

//lookup_it
void *fast_memory_stream_t::lookup(uint32 iid) throw()
{
        switch(iid)
        {
        case IID_self:
        case IID_lookup:
                return this;

        case IID_stream:
                return static_cast<stream_it *>(this);

        case IID_random_stream:
                return static_cast<random_stream_it *>(this);

        case IID_object_id:
                return object_id_impl_t::lookup(iid);

        default:
                return ref_cnt_impl_t::lookup(iid);
        }
}

//stream_it
uint32 fast_memory_stream_t::read(int8* buf,uint32 len)
{
        //buf is null but len isn't zero,will cause current position to jump,2006-7-20
        if(!len || !_buf || !_filled_len || (_pos==_filled_len)) return 0;
        uint32 max_readable_len=_filled_len - _pos;
        uint32 will_read_len=(len>max_readable_len?max_readable_len:len);
        if(buf) memcpy(buf,_buf+_pos,will_read_len);
        _pos+=will_read_len;

        return will_read_len;
}

uint32 fast_memory_stream_t::write(const int8* buf,uint32 len)
{
        //buf is null but len isn't zero,will cause current position to jump,2006-7-20
        if(!len || !_buf || (_pos==_len)) return 0;
        uint32 max_writable_len=_len - _pos;
        uint32 will_write_len=(len>max_writable_len?max_writable_len:len);
        if(buf) memcpy(_buf + _pos,buf,will_write_len);
        _pos+=will_write_len;
        if(_pos>_filled_len) _filled_len=_pos;

        return will_write_len;
}

//random_stream_it
uint32 fast_memory_stream_t::seek(int8 option,int32 offset)
{
        if(!_buf || !_filled_len) return 0;
        switch(option)
        {
        case STM_SEEK_BEGIN:
                if(offset<=0)
                        _pos=0;
                else
                {
                        _pos=(offset>_filled_len?_filled_len:offset);
                }
                break;

        case STM_SEEK_CUR:
                if(offset<0)
                {
                        if(offset*(-1)>_pos)
                                _pos=0;
                        else
                                _pos+=offset;
                }
                else
                {
                        _pos+=offset;
                        if(_pos>_filled_len) _pos=_filled_len;
                }
                break;

        case STM_SEEK_END:
                if(offset<=0)
                        _pos=_filled_len;
                else
                {
                        if(offset>_filled_len)
                                _pos=0;
                        else
                                _pos=_filled_len - offset;
                }
                break;

        default:
                //error
                break;
        }
        return _pos;
}

//oneway_pipe_t
sp_owp_t oneway_pipe_t::s_create(uint32 pipe_size, bool rcts_flag)
{
        oneway_pipe_t *owp=new oneway_pipe_t(pipe_size);
        if(rcts_flag) owp->set_lock(&(owp->_cs_reg));

        return sp_owp_t(owp,true);
}

oneway_pipe_t::oneway_pipe_t(uint32 buf_size) : _cs_reg(true), _cs_r(true), _cs_w(true) //recursive critical section
{
        _len_buf=(buf_size?buf_size:DEFAULT_NOLOCK_PIPE_SIZE);
        if(_len_buf > MAX_NOLOCK_PIPE_SIZE) _len_buf = MAX_NOLOCK_PIPE_SIZE;
        if(_len_buf) _buf=new int8[_len_buf];
        _r_pos=0;
        _r_pos_c=0;
        _r_thd=0;
        _w_pos=0;
        _w_pos_c=0;
        _w_thd=0;

        this->set_lock(&_cs_reg); //ensure add/release reference thread-safe
}

oneway_pipe_t::~oneway_pipe_t()
{
        if(!_sp_sink.is_null() && _w_pos_c != _r_pos_c && _buf)
        {
                if(_w_pos_c > _r_pos_c)
                {
                        _sp_sink->on_destroy(_buf+_r_pos_c, _w_pos_c - _r_pos_c);
                }
                else
                {
                        bb_t bb;
                        bb.reserve(_len_buf - _r_pos_c + _w_pos_c);
                        bb.fill_from(_buf+_r_pos_c, _len_buf - _r_pos_c, 0);
                        bb.fill_from(_buf, _w_pos_c);

                        _sp_sink->on_destroy(bb.get_buf(), bb.get_filled_len());
                }
        }

        if(_buf) delete [] _buf;
}


//lookup_it
void *oneway_pipe_t::lookup(uint32 iid) throw()
{
        switch(iid)
        {
        case IID_self:
        case IID_lookup:
                return this;

        case IID_object_id:
                return object_id_impl_t::lookup(iid);

        default:
                return ref_cnt_impl_t::lookup(iid);
        }
}

bool oneway_pipe_t::register_read()
{
        smart_lock_t slock(&_cs_reg);
#ifdef POSIX
        pthread_t thd_self=pthread_self();
#elif defined(WIN32)
		HANDLE thd_self=GetCurrentThread();
#endif
        if(_r_thd)
        {
                if(_r_thd==thd_self)
                        return true;
                else
                        return false;
        }
        else //_r_thd==0
        {
                _r_thd=thd_self;
                return true;
        }
        return false;
}

void oneway_pipe_t::unregister_read()
{
        smart_lock_t slock(&_cs_reg);
#ifdef POSIX
        if(_r_thd==pthread_self()) _r_thd=0;
#elif defined(WIN32)
		if(_r_thd == GetCurrentThread()) _r_thd=0;
#endif
}

bool oneway_pipe_t::register_write()
{
        smart_lock_t slock(&_cs_reg);
#ifdef POSIX
        pthread_t thd_self=pthread_self();
#elif defined(WIN32)
		HANDLE thd_self=GetCurrentThread();
#endif
        if(_w_thd)
        {
                if(_w_thd==thd_self)
                        return true;
                else
                        return false;
        }
        else //_w_thd==0
        {
                _w_thd=thd_self;
                return true;
        }
        return false;
}

void oneway_pipe_t::unregister_write()
{
        smart_lock_t slock(&_cs_reg);
#ifdef POSIX
        if(_w_thd==pthread_self()) _w_thd=0;
#elif defined(WIN32)
		if(_w_thd==GetCurrentThread()) _w_thd=0;
#endif
}

uint32 oneway_pipe_t::get_w_size() const
{
        //snap for thread-safe
        uint32 snap_r_pos_c=_r_pos_c;
        uint32 snap_w_pos=_w_pos;

        return (snap_r_pos_c > snap_w_pos ?
                snap_r_pos_c - snap_w_pos - 1 : _len_buf - snap_w_pos + snap_r_pos_c - 1);
}

uint32 oneway_pipe_t::get_r_size() const
{
        //snap for thread-safe
        uint32 snap_r_pos=_r_pos;
        uint32 snap_w_pos_c=_w_pos_c;

        return (snap_w_pos_c>=snap_r_pos?
                snap_w_pos_c - snap_r_pos : _len_buf - snap_r_pos + snap_w_pos_c);
}

uint32 oneway_pipe_t::read(int8* buf,uint32 len, bool auto_commit)
{
        FY_ASSERT(buf && len);
#ifdef POSIX
		FY_ASSERT(0==_r_thd ||  _r_thd==pthread_self());
#elif defined(WIN32)
		FY_ASSERT(0==_r_thd ||  _r_thd==GetCurrentThread());
#endif
        smart_lock_t slock(_r_thd? 0: &_cs_r);

        //keep a snap shot of _w_pos_c to avoid multi-thread conflict
        uint32 snap_w_pos_c=_w_pos_c;

        if(snap_w_pos_c==_r_pos) return 0;//empty buffer
        if(snap_w_pos_c>_r_pos)// no write wrapp
        {
                uint32 read_len=snap_w_pos_c - _r_pos;
                read_len=(read_len>len?len:read_len);
                memcpy(buf,_buf + _r_pos,read_len);
                _r_pos+=read_len;
                if(auto_commit) _r_pos_c = _r_pos;

                return read_len;
        }
        //write wrapped
        uint32 end_len=_len_buf - _r_pos;
        if(end_len>=len)//end part is enough
        {
                memcpy(buf,_buf + _r_pos,len);
                _r_pos+=len;
                if(_r_pos==_len_buf) _r_pos=0;
                if(auto_commit) _r_pos_c = _r_pos;

                return len;
        }
        //end part isn't enough
        //read end part
        memcpy(buf,_buf + _r_pos,end_len);
        //read start part
        uint32 start_len=len - end_len;
        start_len=(start_len>snap_w_pos_c?snap_w_pos_c:start_len);
        if(start_len) memcpy(buf+end_len,_buf,start_len);
        _r_pos=start_len;
        if(auto_commit) _r_pos_c = _r_pos;

        return end_len+start_len;
}

uint32 oneway_pipe_t::write(const int8* buf,uint32 len,bool auto_commit)
{
        FY_ASSERT(buf && len );
#ifdef POSIX		
		FY_ASSERT(0 == _w_thd || _w_thd == pthread_self());
#elif defined(WIN32)
		FY_ASSERT(0 == _w_thd || _w_thd == GetCurrentThread());
#endif
        smart_lock_t slock(_w_thd? 0 : &_cs_w);

        //keep a snap shot of _r_pos_c to avoid multi-thread conflict
        uint32 snap_r_pos_c=_r_pos_c;
        if(snap_r_pos_c>_w_pos)//write wrapped
        {
                //1byte interval must be kept if write reach read after wrapping,otherwise,_r_pos_c==_w_pos_c won't
                //mean buffer is empty
                uint32 write_len=snap_r_pos_c - _w_pos - 1;
                write_len=(write_len>len?len:write_len);
                memcpy(_buf + _w_pos,buf,write_len);
                _w_pos+=write_len;
                if(auto_commit) _w_pos_c=_w_pos;
                return write_len;
        }
        //no write wrapp
        uint32 end_len=_len_buf - _w_pos;
        if(!snap_r_pos_c) end_len -=1;
        if(end_len>=len)//end part is enough
        {
                memcpy(_buf + _w_pos,buf,len);
                _w_pos+=len;
                if(_w_pos==_len_buf) _w_pos=0;
                if(auto_commit) _w_pos_c=_w_pos;
                return len;
        }
        //end part isn't enough
        //write end part,correct following '+ _r_pos' to '+ _w_pos',2006-8-30
        if(end_len) memcpy(_buf + _w_pos,buf,end_len);
        if(!snap_r_pos_c)
        {
                _w_pos+=end_len;
                if(auto_commit) _w_pos_c=_w_pos;
                return end_len;
        }
        //write start part
        //1byte interval must be kept if write reach read after wrapping,otherwise,_r_pos_c==_w_pos won't
        //mean buffer is empty
        uint32 start_len=len - end_len;
        start_len=(start_len>(snap_r_pos_c - 1)?snap_r_pos_c - 1:start_len);
        if(start_len) memcpy(_buf,buf+end_len,start_len);
        _w_pos=start_len;
        if(auto_commit) _w_pos_c=_w_pos;
        return end_len+start_len;
}

//only write thread can commit write
void oneway_pipe_t::commit_w()
{
#ifdef POSIX
        FY_ASSERT(0 == _w_thd || _w_thd == pthread_self());
#elif defined(WIN32)
		FY_ASSERT(0 == _w_thd || _w_thd == GetCurrentThread());
#endif
        smart_lock_t slock(_w_thd? 0 : &_cs_w);

        _w_pos_c=_w_pos;
}

//only write thread can rollback write
void oneway_pipe_t::rollback_w()
{
#ifdef POSIX
        FY_ASSERT(0 == _w_thd || _w_thd == pthread_self());
#elif defined(WIN32)
		FY_ASSERT(0 == _w_thd || _w_thd == GetCurrentThread());
#endif

        smart_lock_t slock(_w_thd? 0 : &_cs_w);

        _w_pos=_w_pos_c;
}

//only read thread can commit read
void oneway_pipe_t::commit_r()
{
#ifdef POSIX
        FY_ASSERT(0 == _w_thd || _w_thd == pthread_self());
#elif defined(WIN32)
		FY_ASSERT(0 == _w_thd || _w_thd == GetCurrentThread());
#endif

        smart_lock_t slock(_r_thd? 0 : &_cs_r);

        _r_pos_c=_r_pos;
}

//only read thread can rollback read
void oneway_pipe_t::rollback_r()
{
#ifdef POSIX
        FY_ASSERT(0 == _w_thd || _w_thd == pthread_self());
#elif defined(WIN32)
		FY_ASSERT(0 == _w_thd || _w_thd == GetCurrentThread());
#endif

        smart_lock_t slock(_r_thd? 0 : &_cs_r);

        _r_pos=_r_pos_c;
}

