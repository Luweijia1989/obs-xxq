#pragma once

#include <QCoreApplication>
#include <QQmlEngine>

#define QML_REGISTER_TYPE(T) \
    static QmlRegister::Type<T> s_qmlRegister;

#define QML_REGISTER_CREATABLE_TYPE(T, N) \
    template<> \
    const char * const QmlRegister::CreatableType<T>::NAME = #N; \
    static QmlRegister::CreatableType<T> s_qmlRegister;

#define QML_REGISTER_UNCREATABLE_TYPE(T, N) \
    template<> \
    const char * const QmlRegister::UncreatableType<T>::NAME = #N; \
    static QmlRegister::UncreatableType<T> s_qmlRegister;

#define QML_REGISTER_CPP_SINGLETON_TYPE(T, N) \
    template<> \
    const char * const QmlRegister::CppSingletonType<T>::NAME = #N; \
    static QmlRegister::CppSingletonType<T> s_qmlRegister;

#define QML_REGISTER_JAVASCRIPT_SINGLETON_TYPE(T, N) \
    template<> \
    const char * const QmlRegister::JavaScriptSingletonType<T>::NAME = #N; \
    static QmlRegister::JavaScriptSingletonType<T> s_qmlRegister;

namespace QmlRegister
{
    static constexpr const char * const URI = "com.ypp";
    static constexpr int MAJOR = 1;
    static constexpr int MINOR = 0;

    template <class T>
    struct Type
    {
        Type()
        {
            qAddPreRoutine(call);
        }

        static void call()
        {
            qmlRegisterType<T>();
        }
    };

    template <class T>
    struct CreatableType
    {
        static const char * const NAME;

        CreatableType()
        {
            qAddPreRoutine(call);
        }

        static void call()
        {
            qmlRegisterType<T>(URI, MAJOR, MINOR, NAME);
        }
    };

    template <class T>
    struct UncreatableType
    {
        static const char * const NAME;

        UncreatableType()
        {
            qAddPreRoutine(call);
        }

        static void call()
        {
            qmlRegisterUncreatableType<T>(URI, MAJOR, MINOR, NAME, QStringLiteral("%1 is not available.").arg(NAME));
        }
    };

    template <class T>
    struct CppSingletonType
    {
        static const char * const NAME;

        CppSingletonType()
        {
            qAddPreRoutine(call);
        }

        static void call()
        {
            static auto callback = [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject*
            {
                QObject* ptr = &T::instance();
                engine->setObjectOwnership(ptr, QQmlEngine::ObjectOwnership::CppOwnership);
                return ptr;
            };

            qmlRegisterSingletonType<T>(URI, MAJOR, MINOR, NAME, callback);
        }
    };

    template <class T>
    struct JavaScriptSingletonType
    {
        static const char * const NAME;

        JavaScriptSingletonType()
        {
            qAddPreRoutine(call);
        }

        static void call()
        {
            static auto callback = [](QQmlEngine* engine, QJSEngine* scriptEngine) -> QObject*
            {
                QObject* ptr = new T();
                engine->setObjectOwnership(ptr, QQmlEngine::ObjectOwnership::JavaScriptOwnership);
                return ptr;
            };

            qmlRegisterSingletonType<T>(URI, MAJOR, MINOR, NAME, callback);
        }
    };
}
