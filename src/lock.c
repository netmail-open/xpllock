#include <config.h>
#include <xpllock.h>

#if defined(LINUX) || defined(S390RH) || defined(MACOSX)
#define __USE_GNU
#include <pthread.h>
#undef __USE_GNU
#endif

#define	RWLOCK_INITIALIZED		(1<<0)
#define	RWLOCK_READ					0
#define	RWLOCK_WRITE				1

EXPORT XplBool XplRWLockInit(XplRWLock *RWLock)
{
	if (RWLock == NULL) {
		return(FALSE);
	}
	RWLock->RWState = RWLOCK_INITIALIZED;
	RWLock->RWReaders = 0;
	XplSemaInit(RWLock->RWLock, 1);
	XplSemaInit(RWLock->RWMutexRead, 1);
	XplSemaInit(RWLock->RWMutexReadWrite, 1);

	return(TRUE);
}

EXPORT XplBool XplRWLockDestroy(XplRWLock *RWLock)
{
	if (RWLock == NULL) {
		return(FALSE);
	}

	XplRWWriteLockAcquire(RWLock);
	RWLock->RWState=0;		/* Void the structure */
	XplSemaDestroy(RWLock->RWLock);
	XplSemaDestroy(RWLock->RWMutexRead);
	XplSemaDestroy(RWLock->RWMutexReadWrite);
	return(TRUE);
}

EXPORT XplBool XplRWReadLockAcquire(XplRWLock *RWLock)
{
	/* consistency checks */
	if (RWLock == NULL) {
		return(FALSE);
	}

	if (!(RWLock->RWState & RWLOCK_INITIALIZED)) {
		return(FALSE);
	}

	XplSemaWait(RWLock->RWLock);
	XplSemaPost(RWLock->RWLock);

	/* acquire lock */
	XplSemaWait(RWLock->RWMutexRead);

	RWLock->RWReaders++;

	if (RWLock->RWReaders == 1) {
		XplSemaWait(RWLock->RWMutexReadWrite);
	}
	RWLock->RWMode = RWLOCK_READ;
	XplSemaPost(RWLock->RWMutexRead);
	return(TRUE);
}

EXPORT XplBool XplRWWriteLockAcquire(XplRWLock *RWLock)
{
	/* consistency checks */
	if (RWLock == NULL) {
		return(FALSE);
	}

	if (!(RWLock->RWState & RWLOCK_INITIALIZED)) {
		return(FALSE);
	}

	/* acquire lock */
	XplSemaWait(RWLock->RWLock);
	XplSemaWait(RWLock->RWMutexReadWrite);

	RWLock->RWMode = RWLOCK_WRITE;
	return(TRUE);
}

EXPORT XplBool XplRWReadLockRelease(XplRWLock *RWLock)
{
	/* consistency checks */
	if (RWLock == NULL) {
		return(FALSE);
	}

	if (!(RWLock->RWState & RWLOCK_INITIALIZED)) {
		return(FALSE);
	}

	/* release lock */
	XplSemaWait(RWLock->RWMutexRead);

	RWLock->RWReaders--;
	if (RWLock->RWReaders == 0) {
		if (XplSemaPost(RWLock->RWMutexReadWrite)!=0) {
			RWLock->RWReaders++;
			XplSemaPost(RWLock->RWMutexRead);
			return(FALSE);
		}
	}
	RWLock->RWMode = RWLOCK_READ;
	XplSemaPost(RWLock->RWMutexRead);
	return(TRUE);
}

EXPORT XplBool XplRWWriteLockRelease(XplRWLock *RWLock)
{
	/* consistency checks */
	if (RWLock == NULL) {
		return(FALSE);
	}

	if (!(RWLock->RWState & RWLOCK_INITIALIZED)) {
		return(FALSE);
	}

	/* release lock */
	if (XplSemaPost(RWLock->RWMutexReadWrite)!=0) {
		return FALSE;
	}
	XplSemaPost(RWLock->RWLock);
	return(TRUE);
}

#if defined(LINUX) || defined(MACOSX)

#ifdef DEBUG
void _XplSafeInit( XplAtomic *variable, unsigned long value, char *id, const char *file, const int line )
#else
void _XplSafeInit( XplAtomic *variable, unsigned long value )
#endif
{
#ifdef DEBUG
	if((variable->signature == ATOMIC_SIG) && id
		&& (variable->id == id)
	) {
		fprintf(stderr, "%s:%d: Warning: XplSafeInit() called twice on the same XplAtomic.\n", file, line);
	}
	variable->flags = 0;
	variable->signature = ATOMIC_SIG;
	variable->id = id;
#endif

	XplLockInit( &variable->lock );
	variable->value = value;
}

#if defined(DEBUG)
void _XplSafeFlag(XplAtomic *variable, unsigned long flags, const char *file, const int line)
{
	if (variable->signature != ATOMIC_SIG) {
		fprintf(stderr, "%s:%d: Warning: XplSafeFlag() called before XplSafeInit()\n.", file, line);
		DebugAssert(0);
	}

	XplLockAcquire(&variable->lock);
	variable->flags = flags;
	XplLockRelease(&variable->lock);

	return;
}
#endif

#define MIN_SIGNED_LONG		(1UL << (sizeof(unsigned long) * 8 - 1))

#ifdef DEBUG
unsigned long _XplSafeRead( XplAtomic *variable, const char *file, const int line )
#else
unsigned long _XplSafeRead( XplAtomic *variable )
#endif
{
	unsigned long r;

#if defined(DEBUG)
	unsigned long f;

	if( variable->signature != ATOMIC_SIG )
	{
		fprintf(stderr, "%s:%d: Warning: XplSafeRead() called before XplSafeInit()\n.", file, line);
		DebugAssert(0);
	}
#endif

	XplLockAcquire( &variable->lock );
	r = variable->value;

#if defined(DEBUG)
	f = variable->flags;
#endif

	XplLockRelease( &variable->lock );

#ifdef WARN_ON_LOCK_READ
	if ((r & MIN_SIGNED_LONG) && !(f & XPL_ATOMIC_FLAG_IGNORE_HIGHBIT)) {
		fprintf(stderr, "%s:%d: Warning: XplSafeRead() read a value that appears to have rolled over (0x%08lx).\n", file, line, r);
	}
#endif

	return r;
}

#ifdef DEBUG
unsigned long _XplSafeWrite( XplAtomic *variable, unsigned long value, const char *file, const int line )
#else
unsigned long _XplSafeWrite( XplAtomic *variable, unsigned long value )
#endif
{
	unsigned long r;

#ifdef DEBUG
	unsigned long f;

	if( variable->signature != ATOMIC_SIG )
	{
		fprintf(stderr, "%s:%d: Warning: XplSafeWrite() called before XplSafeInit()\n.", file, line);
		DebugAssert(0);
	}
#endif

	XplLockAcquire( &variable->lock );
	r = variable->value;

#if defined(DEBUG)
	f = variable->flags;
#endif

	variable->value = value;
	XplLockRelease( &variable->lock );

#ifdef DEBUG
	if (!(f & XPL_ATOMIC_FLAG_IGNORE_HIGHBIT) &&
		(r & MIN_SIGNED_LONG) != (value & MIN_SIGNED_LONG)
	) {
		fprintf(stderr, "%s:%d: Warning: XplSafeWrite() potentially rolled over. (0x%08lx -> 0x%08lx)\n", file, line, r, value);
	}
#endif

	return r;
}

#ifdef DEBUG
unsigned long _XplSafeAdd( XplAtomic *variable, unsigned long value, const char *file, const int line )
#else
unsigned long _XplSafeAdd( XplAtomic *variable, unsigned long value )
#endif
{
	unsigned long r;

#ifdef DEBUG
	unsigned long f;

	if( variable->signature != ATOMIC_SIG )
	{
		fprintf(stderr, "%s:%d: Warning: XplSafeAdd() called before XplSafeInit()\n.", file, line);
		DebugAssert(0);
	}
#endif

	XplLockAcquire( &variable->lock );
#ifdef DEBUG
		if( variable->id ) {
			XplConsolePrintf( "TRACE_ATOMIC:%p ADD %02lu+%02lu=%02lu file: %s Line: %d  _XplSafeAdd(%s) \n", (void *) variable, variable->value, value, variable->value + value, file, line, variable->id );
		}
#endif
	variable->value += value;
	r = variable->value;

#if defined(DEBUG)
	f = variable->flags;
#endif

	XplLockRelease( &variable->lock );

#ifdef DEBUG
	if (!(f & XPL_ATOMIC_FLAG_IGNORE_HIGHBIT) &&
		(r & MIN_SIGNED_LONG) != ((r - value) & MIN_SIGNED_LONG)
	) {
		fprintf(stderr, "%s:%d: Warning: XplSafeAdd() potentially rolled over. (0x%08lx -> 0x%08lx)\n", file, line, (r - value), r);
	}
#endif

	return r;
}

#ifdef DEBUG
unsigned long _XplSafeAnd( XplAtomic *variable, unsigned long value, const char *file, const int line )
#else
unsigned long _XplSafeAnd( XplAtomic *variable, unsigned long value )
#endif
{
	unsigned long r;

#if defined(DEBUG)
	if( variable->signature != ATOMIC_SIG )
	{
		fprintf(stderr, "%s:%d: Warning: XplSafeAnd() called before XplSafeInit()\n.", file, line);
		DebugAssert(0);
	}
#endif

	XplLockAcquire( &variable->lock );
	r = variable->value;
	variable->value &= value;
	XplLockRelease( &variable->lock );

	return r;
}

#ifdef DEBUG
unsigned long _XplSafeOr( XplAtomic *variable, unsigned long value, const char *file, const int line )
#else
unsigned long _XplSafeOr( XplAtomic *variable, unsigned long value )
#endif
{
	unsigned long r;

#if defined(DEBUG)
	if( variable->signature != ATOMIC_SIG )
	{
		fprintf(stderr, "%s:%d: Warning: XplSafeOr() called before XplSafeInit()\n.", file, line);
		DebugAssert(0);
	}
#endif

	XplLockAcquire( &variable->lock );
	r = variable->value;
	variable->value |= value;
	XplLockRelease( &variable->lock );

	return r;
}

#endif

#ifdef WIN32

EXPORT void *Win32GetFunction( XplPluginHandle handle, const char *function )
{
#ifdef __WATCOMC__
	char f_name[1024];
	sprintf( f_name, "_%s", function );
	return GetProcAddress( handle, f_name );
#else
	return GetProcAddress( handle, function);
#endif
}

EXPORT int _ReturnZero( void )
{
	return 0;
}

#endif

