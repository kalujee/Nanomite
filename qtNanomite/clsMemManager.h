#ifndef CLSMEMMANAGER_H
#define CLSMEMMANAGER_H
//#define USE_NEW_POOL

#include <Windows.h>

#ifdef USE_NEW_POOL
	#include "clsMemPoolNew.h"
#else
	#include "clsMemPool.h"
#endif

class clsMemManager
{
public:
    clsMemManager();
    ~clsMemManager();
    
    void* Alloc(unsigned long ulSize, bool bUseMemPool);
    void Free(void* p);

	static void* CAlloc(unsigned long ulSize, bool bUseMemPool = true);
	static void CFree(void* p);

private:
#ifdef USE_NEW_POOL
	clsMemPoolNew *pPool_200;
#else
	clsMemPool *pPool_200;
#endif
	DWORD64 PoolBufferBase_200;
	DWORD64 PoolBufferEnd_200;
	DWORD64 PoolBufferSize_200;
	DWORD64 PoolUnitCount_200;
	DWORD64 PoolUnitSize_200;

#ifdef USE_NEW_POOL
	clsMemPoolNew *pPool_50;
#else
	clsMemPool *pPool_50;
#endif
	DWORD64 PoolBufferBase_50;
	DWORD64 PoolBufferEnd_50;
	DWORD64 PoolBufferSize_50;
	DWORD64 PoolUnitCount_50;
	DWORD64 PoolUnitSize_50;

	static clsMemManager *pThis;
};
#endif //CLSMEMMANAGER_H