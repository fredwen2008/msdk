#ifndef _X_MUTEX_H_
#define _X_MUTEX_H_

/////////////////////////////////////////////////////////////////////////////
class XMutex
{
public:
	XMutex();
	virtual ~XMutex();

	VOID Lock();
	VOID Unlock();

protected:
	CRITICAL_SECTION m_cs;
};

/////////////////////////////////////////////////////////////////////////////
#endif