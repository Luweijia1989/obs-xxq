#ifndef __AMF_H__
#define __AMF_H__

#include <errno.h>
#include <stdint.h>

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        AMF_NUMBER = 0, AMF_BOOLEAN, AMF_STRING, AMF_OBJECT,
        AMF_MOVIECLIP,		/* reserved, not used */
        AMF_NULL, AMF_UNDEFINED, AMF_REFERENCE, AMF_ECMA_ARRAY, AMF_OBJECT_END,
        AMF_STRICT_ARRAY, AMF_DATE, AMF_LONG_STRING, AMF_UNSUPPORTED,
        AMF_RECORDSET,		/* reserved, not used */
        AMF_XML_DOC, AMF_TYPED_OBJECT,
        AMF_AVMPLUS,		/* switch to AMF3 */
        AMF_INVALID = 0xff
    }AMFDataType;

    typedef enum {
	    AMF3_UNDEFINED = 0,
	    AMF3_NULL,
	    AMF3_FALSE,
	    AMF3_TRUE,
	    AMF3_INTEGER,
	    AMF3_DOUBLE,
	    AMF3_STRING,
	    AMF3_XML_DOC,
	    AMF3_DATE,
	    AMF3_ARRAY,
	    AMF3_OBJECT,
	    AMF3_XML,
	    AMF3_BYTE_ARRAY
    } AMF3DataType;

    typedef struct AVal
    {
        char *av_val;
        int av_len;
    } AVal;

    char *AMF_EncodeString(char *output, char *outend, const AVal * str);
    char *AMF_EncodeNumber(char *output, char *outend, double dVal);
    char *AMF_EncodeInt16(char *output, char *outend, short nVal);
    char *AMF_EncodeInt24(char *output, char *outend, int nVal);
    char *AMF_EncodeInt32(char *output, char *outend, int nVal);
    char *AMF_EncodeBoolean(char *output, char *outend, int bVal);

    /* Shortcuts for AMFProp_Encode */
    char *AMF_EncodeNamedString(char *output, char *outend, const AVal * name, const AVal * value);
    char *AMF_EncodeNamedNumber(char *output, char *outend, const AVal * name, double dVal);
    char *AMF_EncodeNamedBoolean(char *output, char *outend, const AVal * name, int bVal);

#ifdef __cplusplus
}
#endif

#endif				/* __AMF_H__ */
