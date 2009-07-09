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
#include "fy_basex.h"

USING_FY_NAME_SPACE

//variant_t
void variant_t::_reset()
{
		uint16 i;
        if(_vt & VT_ARRAY) //array
        {
                switch(_vt & VT_TYPE)
                {
                case VT_I8:
                        if(_val.pi8) delete [] (_val.pi8);
                        break;

                case VT_I16:
                        if(_val.pi16) delete [] (_val.pi16);
                        break;

                case VT_I32:
                        if(_val.pi32) delete [] (_val.pi32);
                        break;
		
		case VT_I64:
			if(_val.pi64) delete [] (_val.pi64);
			break;

                case VT_LOOKUP:
                        for(i=0;i<_size;i++)
                        {
                                if(!_val.objs[i]) continue;

                                ref_cnt_it *ref_obj=(ref_cnt_it*)(_val.objs[i]->lookup(IID_ref_cnt, PIN_ref_cnt));
                                if(ref_obj) ref_obj->release_reference();
                        }
                        delete [] (_val.objs);
                        break;

                default:
                        break;
                }
        }
        else if(_vt == VT_LOOKUP && _val.obj)
        {
                ref_cnt_it *ref_obj=(ref_cnt_it*)(_val.obj->lookup(IID_ref_cnt, PIN_ref_cnt));
                if(ref_obj) ref_obj->release_reference();
        }
        _vt=VT_NULL;
}

//must assure current object is uninitialized or resetted before calling this function
void variant_t::_copy(const variant_t& v, bool shallow_copy_obj)
{
        _vt=v._vt;
        _size=v._size;

        if(_vt & VT_ARRAY) //array
        {
                FY_ASSERT(_size);
                switch(_vt & VT_TYPE)
                {
                case VT_I8:
                        _val.pi8=new int8[_size];
                        memcpy(_val.pi8, v._val.pi8, _size);
                        break;

                case VT_I16:
                        _val.pi16=new int16[_size];
                        memcpy(_val.pi16, v._val.pi16, _size*sizeof(int16));
                        break;

                case VT_I32:
                        _val.pi32=new int32[_size];
                        memcpy(_val.pi32, v._val.pi32, _size*sizeof(int32));
                        break;

		case VT_I64:
			_val.pi64 = new int64[_size];
			memcpy(_val.pi64, v._val.pi64, _size*sizeof(int64));
			break;

                case VT_LOOKUP:
                        _val.objs=new lookup_it*[_size];
                        memcpy(_val.objs, v._val.objs, _size*sizeof(lookup_it*));

                        if(shallow_copy_obj)
                        {
                                for(uint16 i=0;i<_size;i++)
                                {
                                        if(!_val.objs[i]) continue;

                                        ref_cnt_it *ref_obj=(ref_cnt_it*)(_val.objs[i]->lookup(IID_ref_cnt, PIN_ref_cnt));
                                        if(ref_obj) ref_obj->add_reference();
                                }
                        }
                        else //deep copy
                        {
                                for(uint16 i=0;i<_size;i++)
                                {
                                        if(!_val.objs[i]) continue;

                                        clone_it *p_cln=(clone_it *)(v._val.objs[i]->lookup(IID_clone, PIN_clone));
                                        if(p_cln)
                                        {
                                                p_cln->clone((void **)&(_val.objs[i]));
                                        }
                                        else
                                        {
                                                ref_cnt_it *ref_obj=0;
                                                for(uint16 k=i-1;k<=0;k--)//free above cloned object
                                                {
                                                        if(!_val.objs[k]) continue;
                                                        ref_obj=(ref_cnt_it*)(_val.objs[k]->lookup(IID_ref_cnt,PIN_ref_cnt));
                                                        if(ref_obj) ref_obj->release_reference();
                                                }
                                                _vt=VT_NULL;

                                                __INTERNAL_FY_THROW("variant_t","_copy","vtdcpa",
                                                        "can't deep copy object which doesn't implement clone_i interface");
                                        }
                                }
                        }
                        break;
                default:
                        break;
                }
        }
        else //not array
        {

                switch(_vt & VT_TYPE)
                {
                case VT_I8:
                        _val.i8=v._val.i8;
                        break;

                case VT_I16:
                        _val.i16=v._val.i16;
                        break;

                case VT_I32:
                        _val.i32=v._val.i32;
                        break;

		case VT_I64:
			_val.i64=v._val.i64;
			break;

                case VT_LOOKUP:
                        if(shallow_copy_obj)
                        {
                                _val.obj=v._val.obj;
                                if(_val.obj)
                                {
                                        ref_cnt_it *ref_obj=(ref_cnt_it*)(_val.obj->lookup(IID_ref_cnt,PIN_ref_cnt));
                                        if(ref_obj) ref_obj->add_reference();
                                }
                                else
                                        _vt=VT_NULL;
                        }
                        else //deep copy
                        {
                                clone_it *p_cln=(clone_it *)(v._val.obj->lookup(IID_clone,PIN_clone));
                                if(p_cln)
                                {
                                        p_cln->clone((void **)&(_val.obj));
                                }
                                else
                                {
                                        _vt=VT_NULL;

                                        __INTERNAL_FY_THROW("variant_t","_copy","vtdcp",
                                                "can't deep copy object which doesn't implement clone_i interface");
                               }
                        }
                        break;

                default:
                        break;
                }
        }
}

variant_t::variant_t()
{
        _vt=VT_NULL;
}

variant_t::variant_t(int8 i8)
{
        _vt=VT_I8;
        _val.i8=i8;
}

variant_t::variant_t(int16 i16)
{
        _vt=VT_I16;
        _val.i16=i16;
}

variant_t::variant_t(int32 i32)
{
        _vt=VT_I32;
        _val.i32=i32;
}

variant_t::variant_t(int64 i64)
{
        _vt=VT_I64;
        _val.i64=i64;
}

variant_t::variant_t(pointer_box_t ptb)
{
#ifdef __OS64__
       
        _vt=VT_I64;
        _val.i64=ptb;

#else //OS32

	_vt=VT_I32;
	_val.i32=ptb;

#endif
}           

variant_t::variant_t(lookup_it *obj, bool attach_opt)
{
        if(obj)
        {
                _vt=VT_LOOKUP;
                _val.obj=obj;

                //shallow copy
                if(!attach_opt)
                {
                        ref_cnt_it *ref_obj=(ref_cnt_it*)(_val.obj->lookup(IID_ref_cnt,PIN_ref_cnt));
                        if(ref_obj) ref_obj->add_reference();
                }
        }
        else
                _vt=VT_NULL;
}

//shallow copy if type is VT_LOOKUP
variant_t::variant_t(const variant_t& v)
{
        _copy(v);
}

variant_t::~variant_t()
{
        _reset();
}

//shallow copy if type is VT_LOOKUP
const variant_t& variant_t::operator =(const variant_t& v)
{
        _reset();
        _copy(v);

        return v;
}

void variant_t::clone_from(const variant_t& src_v)
{
        _reset();
        _copy(src_v, false);//deep copy if type is VT_LOOKUP
}

void variant_t::set_null()
{
        _reset();
}

bool variant_t::is_null() const throw()
{
        return _vt == VT_NULL;
}

void variant_t::set_i8(int8 i8)
{
        _reset();
        _vt=VT_I8;
        _val.i8=i8;
}

int8 variant_t::get_i8() const
{
        if(_vt != VT_I8)
        {
                __INTERNAL_FY_THROW("variant_t","get_i8","vtgi8",
                        "type isn't wanted,current type is "<<_vt);
        }
        return _val.i8;
}

void variant_t::set_i8s(int8 *pi8, uint16 size, bool attach_opt)
{
        _reset();

        if(pi8 && size)
        {
                _size=size;
                _vt=VT_ARRAY | VT_I8;
                if(attach_opt)
                        _val.pi8=pi8; //attach
                else
                {
                        _val.pi8=new int8[_size];
                        memcpy(_val.pi8, pi8, _size*sizeof(int8));
                }
        }
}

int8 *variant_t::get_i8s(uint16 *psize) const
{
        if(_vt != (VT_ARRAY | VT_I8))
        {
                __INTERNAL_FY_THROW("variant_t","get_i8s","vtgi8s",
                        "type isn't wanted,current type is "<<_vt);
        }
        if(psize) *psize=_size;

        return _val.pi8;
}

void variant_t::set_i16(int16 i16)
{
        _reset();
        _vt=VT_I16;
        _val.i16=i16;
}

int16 variant_t::get_i16() const
{
        if(_vt != VT_I16)
        {
                __INTERNAL_FY_THROW("variant_t","get_i16","vtgi16",
                        "type isn't wanted,current type is "<<_vt);
        }
        return _val.i16;
}

void variant_t::set_i16s(int16 *pi16, uint16 size, bool attach_opt)
{
        _reset();

        if(pi16 && size)
        {
                _size=size;
                _vt=VT_ARRAY | VT_I16;
                if(attach_opt)
                        _val.pi16=pi16; //attach
                else
                {
                        _val.pi16=new int16[_size];
                        memcpy(_val.pi16, pi16, _size*sizeof(int16));
                }
        }
}

int16 *variant_t::get_i16s(uint16 *psize) const
{
        if(_vt != (VT_ARRAY | VT_I16))
        {
                __INTERNAL_FY_THROW("variant_t","get_i16s","vtgi16s",
                        "type isn't wanted,current type is "<<_vt);
        }

        if(psize) *psize=_size;

        return _val.pi16;
}

void variant_t::set_i32(int32 i32)
{
        _reset();
        _vt=VT_I32;
        _val.i32=i32;
}

int32 variant_t::get_i32() const
{
        if(_vt != VT_I32)
        {
                __INTERNAL_FY_THROW("variant_t","get_i32","vtgi32",
                        "type isn't wanted,current type is "<<_vt);
        }
        return _val.i32;
}

void variant_t::set_i32s(int32 *pi32, uint16 size, bool attach_opt)
{
        _reset();

        if(pi32 && size)
        {
                _size=size;
                _vt=VT_ARRAY | VT_I32;
                if(attach_opt)
                        _val.pi32=pi32; //attach
                else
                {
                        _val.pi32=new int32[_size];
                        memcpy(_val.pi32, pi32, _size*sizeof(int32));
                }
        }
}

int32 *variant_t::get_i32s(uint16 *psize) const
{
        if(_vt != (VT_ARRAY | VT_I32))
        {
                __INTERNAL_FY_THROW("variant_t","get_i32s","vtgi32s",
                        "type isn't wanted,current type is "<<_vt);
        }
        if(psize) *psize=_size;

        return _val.pi32;
}

void variant_t::set_i64(int64 i64)
{
        _reset();
        _vt=VT_I64;
        _val.i64=i64;
}

int64 variant_t::get_i64() const
{
        if(_vt != VT_I64)
        {
                __INTERNAL_FY_THROW("variant_t","get_i64","vtgi64",
                        "type isn't wanted,current type is "<<_vt);
        }
        return _val.i64;
}

void variant_t::set_i64s(int64 *pi64, uint16 size, bool attach_opt)
{
        _reset();

        if(pi64 && size)
        {
                _size=size;
                _vt=VT_ARRAY | VT_I64;
                if(attach_opt)
                        _val.pi64=pi64; //attach
                else
                {
                        _val.pi64=new int64[_size];
                        memcpy(_val.pi64, pi64, _size*sizeof(int64));
                }
        }
}

int64 *variant_t::get_i64s(uint16 *psize) const
{
        if(_vt != (VT_ARRAY | VT_I64))
        {
                __INTERNAL_FY_THROW("variant_t","get_i64s","vtgi64s",
                        "type isn't wanted,current type is "<<_vt);
        }
        if(psize) *psize=_size;

        return _val.pi64;
}

void variant_t::set_ptb(pointer_box_t ptb)
{
#ifdef __OS64__
	set_i64((int64)ptb);
#else
	set_i32((int32)ptb);
#endif
}

pointer_box_t variant_t::get_ptb() const
{
#ifdef __OS64__
	return (pointer_box_t)get_i64();
#else
	return (pointer_box_t)get_i32();
#endif
}

void variant_t::set_ptbs(pointer_box_t *pptb, uint16 size, bool attach_opt)
{
#ifdef __OS64__
	set_i64s((int64*)pptb, size, attach_opt);
#else
	set_i32s((int32*)pptb, size, attach_opt);
#endif
}

pointer_box_t *variant_t::get_ptbs(uint16 *psize) const
{
#ifdef __OS64__
	return (pointer_box_t*)get_i64s(psize);
#else
	return (pointer_box_t*)get_i32s(psize);
#endif
}

void variant_t::set_obj(lookup_it *obj, bool attach_opt)
{
        _reset();
        if(obj)
        {
                _vt=VT_LOOKUP;
                _val.obj=obj;
                if(!attach_opt)
                {
                        ref_cnt_it *ref_obj=(ref_cnt_it*)(_val.obj->lookup(IID_ref_cnt,PIN_ref_cnt));
                        if(ref_obj) ref_obj->add_reference();
                }
        }
}

lookup_it *variant_t::get_obj() const
{
        if(_vt != VT_LOOKUP)
        {
                __INTERNAL_FY_THROW("variant_t","get_obj","vtgobj",
                        "type isn't wanted,current type is "<<_vt);
        }
        return _val.obj;
}

void variant_t::set_objs(lookup_it **objs, uint16 size, bool attach_opt)
{
        _reset();

        if(objs && size)
        {
                _size=size;
                _vt=VT_ARRAY | VT_LOOKUP;
                if(attach_opt)
                        _val.objs=objs; //attach
                else
                {
                        _val.objs=new lookup_it*[_size];

                        ref_cnt_it *ref_obj=0;
                        for(uint16 i=0;i<_size;i++)
                        {
                                ref_obj=(ref_cnt_it*)(objs[i]->lookup(IID_ref_cnt,PIN_ref_cnt));
                                if(ref_obj) ref_obj->add_reference();
                        }
                        memcpy(_val.objs, objs, _size*sizeof(lookup_it*));
                }
        }
}

lookup_it **variant_t::get_objs(uint16 *psize) const
{
        if(_vt != (VT_ARRAY | VT_LOOKUP))
        {
                __INTERNAL_FY_THROW("variant_t","get_objs","vtgobjs",
                        "type isn't wanted,current type is "<<_vt);
        }
        if(psize) *psize=_size;

        return _val.objs;
}

//persist
//persist bit pattern:
//<1byte:_vt>[2bytes:size of array][...: union]
uint32 variant_t::calc_persist_size() const
{
        uint32 psize=sizeof(_vt);
		uint16 i;
        if(_vt & VT_ARRAY)
        {
                psize+=sizeof(_size);
                switch(_vt & VT_TYPE)
                {
                case VT_I8:
                        psize+=_size*sizeof(int8);
                        break;

                case VT_I16:
                        psize+=_size*sizeof(int16);
                        break;

                case VT_I32:
                        psize+=_size*sizeof(int32);
                        break;

		case VT_I64:
			psize+=_size*sizeof(int64);
			break;

                case VT_LOOKUP:
                        for(i=0;i<_size;i++)
                        {
                                persist_it *per_obj=(persist_it*)(_val.objs[i]->lookup(IID_persist, PIN_persist));
                                psize += persist_helper_t::get_persist_size(per_obj);
                        }
                        break;

                default:
                        __INTERNAL_FY_THROW("variant_t","calc_persist_size","vtkpsizeta",
                                "invalid type:"<<_vt);
                        break;
                }
        }
        else //not array
        {
                switch(_vt & VT_TYPE)
                {
                case VT_NULL:
                        break;

                case VT_I8:
                        psize+=sizeof(int8);
                        break;

                case VT_I16:
                        psize+=sizeof(int16);
                        break;

                case VT_I32:
                        psize+=sizeof(int32);
                        break;

		case VT_I64:
			psize+=sizeof(int64);
			break;

                case VT_LOOKUP:
                        {
                                persist_it *per_obj=(persist_it*)(_val.obj->lookup(IID_persist,PIN_persist));
                                psize += persist_helper_t::get_persist_size(per_obj);
                        }
                        break;

                default:
                        __INTERNAL_FY_THROW("variant_t","calc_persist_size","vtkpsizet",
                                "invalid type:"<<_vt);
                        break;
                }
        }
        return psize;
}

void variant_t::save_to(stream_adaptor_t& stm_adp)
{
		uint16 i;
       stm_adp<<_vt;
        if(_vt & VT_ARRAY)
        {
                stm_adp<<_size;
                switch(_vt & VT_TYPE)
                {
                case VT_I8:
                        for(i=0;i<_size;i++) stm_adp<<_val.pi8[i];
                        break;

                case VT_I16:
                        for(i=0;i<_size;i++) stm_adp<<_val.pi16[i];
                        break;

                case VT_I32:
                        for(i=0;i<_size;i++) stm_adp<<_val.pi32[i];
                        break;

		case VT_I64:
			for(i=0;i<_size;i++) stm_adp<<_val.pi64[i];
			break;

                case VT_LOOKUP:
                        {
                                persist_it *per_obj=0;
                                for(i=0;i<_size;i++)
                                {
                                        per_obj=(persist_it*)(_val.objs[i]->lookup(IID_persist,PIN_persist));
                                        persist_helper_t::save_to(per_obj, stm_adp);
                               }
                        }
                        break;
                default:
                        __INTERNAL_FY_THROW("variant_t","save_to","vtstta",
                                "invalid type:"<<_vt);
                        break;
                }
        }
        else //not array
        {
                switch(_vt & VT_TYPE)
                {
                case VT_NULL:
                        break;

                case VT_I8:
                        stm_adp<<_val.i8;
                        break;

                case VT_I16:
                        stm_adp<<_val.i16;
                        break;

                case VT_I32:
                        stm_adp<<_val.i32;
                        break;

		case VT_I64:
			stm_adp<<_val.i64;
			break;

                case VT_LOOKUP:
                        {
                                persist_it *per_obj=(persist_it*)(_val.obj->lookup(IID_persist,PIN_persist));
                                persist_helper_t::save_to(per_obj, stm_adp);
                        }
                        break;

                default:
                        __INTERNAL_FY_THROW("variant_t","save_to","vtstt",
                                "invalid type:"<<_vt);
                        break;
                }
        }
}

void variant_t::load_from(stream_adaptor_t& stm_adp)
{
       _reset();

		uint16 i;
        stm_adp>>_vt;
        if(_vt & VT_ARRAY)
        {
                stm_adp>>_size;
                switch(_vt & VT_TYPE)
                {
                case VT_I8:
                        _val.pi8=new int8[_size];
                        for(i=0;i<_size;++i) stm_adp>>(_val.pi8[i]);
                        break;

                case VT_I16:
                        _val.pi16=new int16[_size];
                        for(i=0;i<_size;++i) stm_adp>>(_val.pi16[i]);
                        break;

                case VT_I32:
                        _val.pi32=new int32[_size];
                        for(i=0;i<_size;++i) stm_adp>>(_val.pi32[i]);
                        break;

		case VT_I64:
			_val.pi64=new int64[_size];
			for(i=0;i<_size;++i) stm_adp>>(_val.pi64[i]);
			break;

                case VT_LOOKUP:
                        {
                                _val.objs=new lookup_it*[_size];
                                for(i=0;i<_size;++i)
                                {
                                        sp_persist_t sp_pst=persist_helper_t::load_from(stm_adp);
                                        _val.objs[i]=(lookup_it*)(sp_pst.detach());
                                }
                        }
                        break;

                default:
                        __INTERNAL_FY_THROW("variant_t","load_from","vtlfta","invalid type:"<<_vt);
                        break;
                }
        }
        else
        {
                switch(_vt & VT_TYPE)
                {
                case VT_NULL:
                        break;

                case VT_I8:
                        stm_adp>>(_val.i8);
                        break;

                case VT_I16:
                        stm_adp>>(_val.i16);
                        break;

                case VT_I32:
                        stm_adp>>(_val.i32);
                        break;

		case VT_I64:
			stm_adp>>(_val.i64);
			break;

                case VT_LOOKUP:
                        {
                                sp_persist_t sp_pst=persist_helper_t::load_from(stm_adp);
                                _val.obj=(lookup_it*)(sp_pst.detach());
                        }
                        break;
                default:
                        __INTERNAL_FY_THROW("variant_t","load_from","vtlft","invalid type:"<<_vt);
                        break;
                }
        }
}

