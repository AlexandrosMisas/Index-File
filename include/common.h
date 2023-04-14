#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include "bf.h"

#define sizeof_field(type, field) sizeof(((type*)0)->field)
#define array_size(x) (sizeof(x) / sizeof((x)[0]))


#define COPY(src, dest, src_size, dest_size) { 	\
	memset(dest, 0, dest_size); 				\
	memcpy(dest, src, src_size); 				\
}



#define CALL_BF(call, label)				\
	do {									\
		BF_ErrorCode code = call;			\
		if (code != BF_OK) {				\
			BF_PrintError(code);			\
			goto label;						\
		}									\
	} while (0)

#define INSERTED(handle, call) 				\
	({ 										\
		int _rec_cnt = handle->rec_count;	\
		assert(!call);						\
		_rec_cnt < handle->rec_count;		\
	})

#define DELETED(handle, call) 				\
	({ 										\
		int _rec_cnt = handle->rec_count;	\
		assert(!call);						\
		handle->rec_count < _rec_cnt;		\
	})

#define TMP_LIST _tmp_dll

#define GET_NUM_ENTRIES(call) 						\
	({						  						\
		Dl_list TMP_LIST = list_create(free);		\
		assert(!call);								\
		int tmp = list_size(TMP_LIST);				\
		list_destroy(TMP_LIST);						\
		tmp;										\
	})


#endif /* COMMON_H */
