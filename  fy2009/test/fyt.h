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
#ifndef __FENGYI2009_TEST_DREAMFREELANCER_20090429_H__
#define __FENGYI2009_TEST_DREAMFREELANCER_20090429_H__

#include "fy_base.h"
#include <string>
#include <vector>

DECL_FY_NAME_SPACE_BEGIN

/*[tip] command agruments vector
 *[desc] make it easy to handle command arguments
 */ 
class argv_t
{
public:
	argv_t(){}
	argv_t(int argc, char **argv)
	{
		for(int i=0; i<argc; ++i) _strv.push_back(std::string(argv[i]));
	}

	int get_size() { return _strv.size(); }

	std::string get_arg(int idx)
	{
		if(idx < 0 || idx >= _strv.size()) return std::string();
		return _strv[idx];
	}
	
	bool has_arg(const std::string arg)
	{
		for(_strv_t::iterator it = _strv.begin(); it != _strv.end(); ++it)
		{
			if(arg.compare(*it) == 0) return true;
		}
		return false;
	}

	bool has_arg(const char *raw_arg)
	{
		for(_strv_t::iterator it = _strv.begin(); it != _strv.end(); ++it)
		{
			if(it->compare(raw_arg) == 0) return true;
		}
		return false;
	}	
private:
	typedef std::vector< std::string > _strv_t;
private:
	_strv_t _strv;	
};

DECL_FY_NAME_SPACE_END

#endif //__FENGYI2009_TEST_DREAMFREELANCER_20090429_H__
