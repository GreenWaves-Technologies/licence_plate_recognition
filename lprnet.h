
#ifndef __VERGESENSE_H__
#define __VERGESENSE_H__

#define __PREFIX(x) china_ocr ## x

#include "Gap.h"

#ifdef __EMUL__
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>
#include <string.h>
#include "helpers.h"
#endif

extern AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash);

#endif
