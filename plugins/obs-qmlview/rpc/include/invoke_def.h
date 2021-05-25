#pragma once
#include <QJsonDocument>
#include <QJsonObject>

inline void GetParam(bool& result, QJsonValue jsonValue)
{
	result = jsonValue.toBool();
}
inline void GetParam(int& result, QJsonValue jsonValue)
{
	result = jsonValue.toInt();
}
inline void GetParam(double& result, QJsonValue jsonValue)
{
	result = jsonValue.toDouble();
}
inline void GetParam(QString& result,QJsonValue jsonValue)
{
	result = jsonValue.toString();
}

#define ON_INVOKE(invoke_function) \
{\
	auto funcName = m_funcPackObj["function"].toString();\
	if(#invoke_function == funcName)\
    {\
		invoke_function();\
	}\
}

#define ON_INVOKE_1(invoke_function,TYPE) \
{\
	auto funcName = m_funcPackObj["function"].toString();\
	if(#invoke_function == funcName)\
	{\
		TYPE p1; \
		GetParam(p1, m_funcPackObj["0"]); \
        invoke_function(p1);\
	}\
}

#define ON_INVOKE_2(invoke_function,TYPE1,TYPE2) \
{\
   auto funcName = m_funcPackObj["function"].toString();\
   if(#invoke_function == funcName)\
   {\
	   TYPE1 p1;TYPE2 p2;\
       GetParam(p1, m_funcPackObj["0"]);\
       GetParam(p2, m_funcPackObj["1"]);\
       invoke_function(p1,p2);\
   }\
}

#define ON_INVOKE_3(invoke_function,TYPE1,TYPE2,TYPE3) \
{\
   auto funcName = m_funcPackObj["function"].toString();\
   if(#invoke_function == funcName)\
   {\
	   TYPE1 p1;TYPE2 p2;TYPE3 p3;\
       GetParam(p1, m_funcPackObj["0"]);\
       GetParam(p2, m_funcPackObj["1"]);\
	   GetParam(p3, m_funcPackObj["2"]);\
       invoke_function(p1,p2,p3);\
   }\
}

#define BEGIN_INVOKE(CLASS) \
void CLASS::ParseInvoke()\
{\

#define END_INVOKE \
}

#define DEF_PROCESS_INVOKE(CLASS) \
void CLASS::ParseInvoke();
