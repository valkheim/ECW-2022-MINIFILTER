#include "Lock.h"

namespace kl
{
    Mutex::Mutex() : mutex({})
    {}

    void Mutex::Init()
    {
        // Initializes a mutex object, setting it to a signaled state.
        KeInitializeMutex(&mutex, 0);
    }

    void Mutex::Lock()
    {
        // Puts the current thread into a wait state until the given dispatcher object is set to a signaled state
        KeWaitForSingleObject(&mutex, Executive, KernelMode, FALSE, nullptr);
    }

    void Mutex::Unlock()
    {
        // Releases a mutex object
        KeReleaseMutex(&mutex, FALSE);
    }

    FastMutex::FastMutex() : mutex({})
    {}

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void FastMutex::Init()
    {
        // Initializes a fast mutex variable, used to synchronize mutually exclusive access by a set of threads to a shared resource.
        ExInitializeFastMutex(&mutex);
    }

    _IRQL_raises_(APC_LEVEL)
    _IRQL_saves_global_(OldIrql, mutex)
    void FastMutex::Lock()
    {
        // ExAcquireFastMutex puts the caller into a wait state if the given fast mutex cannot be acquired immediately.
        // Otherwise, the caller is given ownership of the fast mutex with APCs to the current thread disabled until it releases the fast mutex.
        // Use TryLock/ExTryToAcquireFastMutex if the current thread can do other work before it waits on the acquisition of the given mutex.
        ExAcquireFastMutex(&mutex);
    }

    _IRQL_raises_(APC_LEVEL)
    _IRQL_saves_global_(OldIrql, mutex)
    _Success_(return == true)
    [[nodiscard]] auto FastMutex::TryLock() -> bool
    {
        // ExTryToAcquireFastMutex acquires the fast mutex, if possible, with APCs to the current thread disabled.
        // ExTryToAcquireFastMutex returns TRUE if the current thread is given ownership of the fast mutex.
        return ExTryToAcquireFastMutex(&mutex);
    }

    _IRQL_requires_(APC_LEVEL)
    _IRQL_restores_global_(OldIrql, mutex)
    void FastMutex::Unlock()
    {
        // ExReleaseFastMutex releases ownership of the given fast mutex and sets the IRQL to the value that the caller was running at before it called ExAcquireFastMutex.
        // If the previous IRQL was less than APC_LEVEL, the delivery of APCs to the current thread is reenabled.
        ExReleaseFastMutex(&mutex);
    }

    GuardedMutex::GuardedMutex() : mutex({})
    {}

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void GuardedMutex::Init()
    {
        // Initializes a guarded mutex variable, used to synchronize mutually exclusive access by a set of threads to a shared resource.
        KeInitializeGuardedMutex(&mutex);
    }

    _IRQL_requires_max_(APC_LEVEL)
    void GuardedMutex::Lock()
    {
        // KeAcquireGuardedMutex puts the caller into a wait state if the given guarded mutex cannot be acquired immediately.
        // Otherwise, the caller is given ownership of the guarded mutex with APCs to the current thread disabled until it releases the guarded mutex.
        // Use TryLock/ExTryToAcquireGuardedMutex if the current thread can do other work before it waits on the acquisition of the given mutex.
        KeAcquireGuardedMutex(&mutex);
    }

    _IRQL_raises_(APC_LEVEL)
    _IRQL_saves_global_(OldIrql, mutex)
    _Success_(return == true)
    [[nodiscard]] auto GuardedMutex::TryLock() -> bool
    {
        // KeTryToAcquireGuardedMutex acquires the guarded mutex, if possible, with APCs to the current thread disabled.
        // KeTryToAcquireGuardedMutex returns TRUE if the current thread is given ownership of the guarded mutex.
        return KeTryToAcquireGuardedMutex(&mutex);
    }

    _IRQL_requires_max_(APC_LEVEL)
    void GuardedMutex::Unlock()
    {
        // KeReleaseGuardedMutex releases ownership of the given guarded mutex and sets the IRQL to the value that the caller was running at before it called ExAcquireGuardedMutex.
        // If the previous IRQL was less than APC_LEVEL, the delivery of APCs to the current thread is reenabled.
        KeReleaseGuardedMutex(&mutex);
    }
    
    SpinLock::SpinLock() : lock(0), old_irql(KeGetCurrentIrql())
    {}

    void SpinLock::Init()
    {
        // Initializes the KSPIN_LOCK lock.
        // Callers of this routine can be running at any IRQL.
        KeInitializeSpinLock(&lock);
    }

    _IRQL_saves_global_(SpinLock, old_irql)
    _IRQL_raises_(DISPATCH_LEVEL)
    void SpinLock::Lock()
    {
        // KeAcquireSpinLock first resets the IRQL to DISPATCH_LEVEL and then acquires the lock
        // The previous IRQL is written to OldIrql after the lock is acquired.
        KeAcquireSpinLock(&lock, &old_irql);
    }

    _IRQL_requires_(DISPATCH_LEVEL)
    _IRQL_restores_global_(SpinLock, old_irql)
    void SpinLock::Unlock()
    {
        // KeReleaseSpinLock releases a spin lock and restores the original IRQL at which the caller was running.
        // This routine raises the IRQL level to DISPATCH_LEVEL when acquiring the spin lock.
        // If the caller is guaranteed to already be running at DISPATCH_LEVEL, it is more efficient to call KeAcquireInStackQueuedSpinLockAtDpcLevel.
        KeReleaseSpinLock(&lock, old_irql);
    }

    QueuedSpinLock::QueuedSpinLock() : lock(0), handle({})
    {}

    void QueuedSpinLock::Init()
    {
        // Initializes the KSPIN_LOCK lock.
        // Callers of this routine can be running at any IRQL.
        KeInitializeSpinLock(&lock);
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _IRQL_saves_global_(QueuedSpinLock, handle)
    _IRQL_raises_(DISPATCH_LEVEL)
    void QueuedSpinLock::Lock()
    {
        // KeAcquireInStackQueuedSpinLock acquires a spin lock as a queued spin lock.
        // If the caller is guaranteed to already be running at DISPATCH_LEVEL, it is more efficient to call LockAtDpc/KeAcquireInStackQueuedSpinLockAtDpcLevel.
        KeAcquireInStackQueuedSpinLock(&lock, &handle);
    }

    _IRQL_requires_(DISPATCH_LEVEL)
    _IRQL_restores_global_(QueuedSpinLock, handle)
    void QueuedSpinLock::Unlock()
    {
        // KeReleaseInStackQueuedSpinLock restores the original IRQL that
        // the operating system saved at the beginning of the KeAcquireInStackQueuedSpinLock call.
        KeReleaseInStackQueuedSpinLock(&handle);
    }

    _IRQL_requires_(DISPATCH_LEVEL)
    void QueuedSpinLock::LockAtDpc()
    {
        // KeAcquireInStackQueuedSpinLockAtDpcLevel acquires a queued spin lock
        // when the caller is already running at IRQL >= DISPATCH_LEVEL.
        KeAcquireInStackQueuedSpinLockAtDpcLevel(&lock, &handle);
    }

    _IRQL_requires_(DISPATCH_LEVEL)
    void QueuedSpinLock::UnlockFromDpc()
    {
        // KeReleaseInStackQueuedSpinLockFromDpcLevel releases a queued spin lock acquired by KeAcquireInStackQueuedSpinLockAtDpcLevel.
        KeReleaseInStackQueuedSpinLockFromDpcLevel(&handle);
    }

    ExecutiveResource::ExecutiveResource() : resource({})
    {}

    _IRQL_requires_max_(APC_LEVEL)
    ExecutiveResource::~ExecutiveResource()
    {
        ExDeleteResourceLite(&resource);
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void ExecutiveResource::Init()
    {
        // Initializes the resource variable
        ExInitializeResourceLite(&resource);
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    auto ExecutiveResource::Reinitialize() -> NTSTATUS
    {
        // Reinitializes the resource. Replaces the three following calls:
        // 1. ExDeleteResourceLite
        // 2. ExAllocatePool
        // 3. ExInitializeResourceLite
        return ExReinitializeResourceLite(&resource);
    }

    _IRQL_requires_max_(APC_LEVEL)
    void ExecutiveResource::Lock()
    {
        // Disables the execution of normal kernel-mode APCs (but does not prevent special kernel APCs from running)
        KeEnterCriticalRegion();
        // Acquires the resource for exclusive access by the calling thread
        static_cast<void>(ExAcquireResourceExclusiveLite(&resource, TRUE));
    }

    _IRQL_requires_max_(APC_LEVEL)
    auto ExecutiveResource::LockShared() -> BOOLEAN
    {
        // Disables the execution of normal kernel-mode APCs (but does not prevent special kernel APCs from running)
        KeEnterCriticalRegion();
        // Returns TRUE if (or when) the resource is acquired
        // Returns FALSE if the shared access cannot be granted immediately (or if Wait parameter is FALSE)
        return ExAcquireResourceSharedLite(&resource, TRUE);
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void ExecutiveResource::Unlock()
    {
        // Releases the resource owned by the current thread
        ExReleaseResourceLite(&resource);
        // Reenables the delivery of normal kernel-mode APCs
        KeLeaveCriticalRegion();
    }
}