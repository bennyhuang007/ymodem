#ifndef _COMMON_H_
#define _COMMON_H_

#define CMD_SET_GPIO_RESET   0x1000

#define RETRY_CNT			 (3)
#define TIME_TIMEOUT		 (50)

#define ERR_CODE_NONE        (0)
#define ERR_CODE_PARAM       (-1)
#define ERR_CODE_TTY         (-2)
#define ERR_CODE_GPIO        (-3)
#define ERR_CODE_FILE        (-4)
#define ERR_CODE_NORSP       (-5)
#define ERR_CODE_TTYTIMEOUT  (-6)
#define ERR_CODE_MALLOC      (-7)
#define ERR_CODE_FILENAME    (-8)
#define ERR_CODE_POPEN       (-9)

#define ERR_CODE_OTHERS		 (-255)

#endif
