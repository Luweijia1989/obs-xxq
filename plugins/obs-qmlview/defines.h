#pragma once

#define DEFINE_PROPERTY(type, name)                                          \
                                                                             \
private:                                                                     \
	Q_PROPERTY(type name READ name WRITE set##name NOTIFY name##Changed) \
                                                                             \
private:                                                                     \
	type m_##name;                                                       \
                                                                             \
public:                                                                      \
	void set##name(const type &a)                                        \
	{                                                                    \
		if (a != m_##name) {                                         \
			m_##name = a;                                        \
			emit name##Changed();                                \
		}                                                            \
	}                                                                    \
                                                                             \
public:                                                                      \
	type name() const { return m_##name; }                               \
                                                                             \
public:                                                                      \
Q_SIGNALS:                                                                   \
	void name##Changed();

#define READONLY_PROPERTY(TYPE, NAME)               \
	Q_PROPERTY(TYPE NAME READ NAME CONSTANT)    \
                                                    \
public:                                             \
	TYPE NAME() const { return m_##NAME; }      \
                                                    \
private:                                            \
	void NAME(TYPE value) { m_##NAME = value; } \
	TYPE m_##NAME;
