#ifndef BE_DEFINES_H
#define BE_DEFINES_H

#include <stdio.h>


typedef enum BE_LOG_LEVEL
{
    BYTE_LOG_LEVEL_DEBUG = 0x0,    /* debug-level								   */
    BYTE_LOG_LEVEL_INFO = 0x1,    /* informational 							   */    
    BYTE_LOG_LEVEL_WARNING = 0x2,    /* warning conditions						   */
    BYTE_LOG_LEVEL_ERROR = 0x3,    /* error conditions							   */    
    BYTE_LOG_LEVEL_FATAL = 0x4,	   /* just for compatibility with previous version */
}BE_LOG_LEVEL;


#define BYTE_TIME_COST_PRINT(func,time) do{ \
    if(1){\
        printf("Profile %s estimation time %lu\n", func, time);\
    }\
} while(0)

//#define BYTE_PRINTF(LevelStr,Msg, ...) do {\
// fprintf(stderr,"[Level]:%s,[Func]:%s [Line]:%d [Info]:"Msg,LevelStr, __FUNCTION__, __LINE__,## __VA_ARGS__); } while (0)
//
//#define BYTE_PRINTF_RED(LevelStr,Msg, ...) do { fprintf(stderr,"\033[0;31m [Level]:%s,[Func]:%s [Line]:%d [Info]:"Msg"\033[0;39m\n",LevelStr, __FUNCTION__, __LINE__,## __VA_ARGS__); } while (0)
//
//#define BYTE_PRINTF_YELLOW(LevelStr,Msg, ...) do { fprintf(stderr,"\033[0;43m [Level]:%s,[Func]:%s [Line]:%d [Info]:"Msg"\033[0;39m\n",LevelStr, __FUNCTION__, __LINE__,## __VA_ARGS__); } while (0)
//
//#define BYTE_PRINTF_GREEN(LevelStr,Msg, ...)  do { fprintf(stderr,"\033[0;42m [Level]:%s,[Func]:%s [Line]:%d [Info]:"Msg"\033[0;39m\n",LevelStr, __FUNCTION__, __LINE__,## __VA_ARGS__); } while (0)
//
///* system is unusable	*/
//#define BYTE_LOG_FATAL(Msg,...)   BYTE_PRINTF_RED("Fatal",Msg,##__VA_ARGS__)
///* error conditions */
//#define BYTE_LOG_ERR(Msg,...)     BYTE_PRINTF_RED("Error",Msg,##__VA_ARGS__)
///* warning conditions */
//#define BYTE_LOG_WARN(Msg,...)    BYTE_PRINTF_YELLOW("Warning",Msg,##__VA_ARGS__)
///* normal but significant condition  */
//#define BYTE_LOG_INFO(Msg,...)    BYTE_PRINTF("Info",Msg,##__VA_ARGS__)
///* debug-level messages  */
//#define BYTE_LOG_DEBUG(Msg, ...)  BYTE_PRINTF_GREEN("Debug",Msg,##__VA_ARGS__)
//
//#define BYTE_LOG(Level,Msg, ...)\
//do\
//{\
//	switch(Level){\
//		case BYTE_LOG_LEVEL_DEBUG:\
//		    BYTE_LOG_DEBUG(Msg,##__VA_ARGS__);\
//			break;\
//		case BYTE_LOG_LEVEL_INFO:\
//		    BYTE_LOG_INFO(Msg,##__VA_ARGS__);\
//			break;\
//		case BYTE_LOG_LEVEL_WARNING:\
//		    BYTE_LOG_WARN(Msg,##__VA_ARGS__);\
//			break;\
//		case BYTE_LOG_LEVEL_ERROR:\
//			BYTE_LOG_ERR(Msg,##__VA_ARGS__);\
//			break;\
//		case BYTE_LOG_LEVEL_FATAL:\
//			BYTE_LOG_FATAL(Msg,##__VA_ARGS__);\
//			break;\
//		default:\
//			break;\
//		}\
//}while(0)
//


#endif//BE_DEFINES_H
