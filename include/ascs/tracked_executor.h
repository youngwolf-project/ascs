/*
 * stracked_executor.h
 *
 *  Created on: 2018-4-19
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * the top class
 */

#ifndef _ASCS_TRACKED_EXECUTOR_H_
#define _ASCS_TRACKED_EXECUTOR_H_

#include "config.h"
#include "executor.h"

namespace ascs
{
	
#if 0 == ASCS_DELAY_CLOSE
using tracked_executor = executor_base<async_call_tracker>;
#else
typedef executor tracked_executor;
#endif

} //namespace

#endif /* _ASCS_TRACKED_EXECUTOR_H_ */
