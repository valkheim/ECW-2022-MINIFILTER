#pragma once

#include "main.h"

namespace kl
{
    class ILock
    {
    public:
        virtual void Init() = 0;
        virtual void Lock() = 0;
        virtual void Unlock() = 0;
    };

    /*class AutoLock final
    {
        ILock& lock;

    public:
        AutoLock(ILock& lock) : lock(lock)
        {
            lock.Lock();
        }

        ~AutoLock()
        {
            lock.Unlock();
        }
    };*/

    // For better performance, use fast mutexes or guarded mutexes.
    class Mutex final : public ILock
    {
        ALIGN KMUTEX mutex;

    public:
        Mutex();
        Mutex(Mutex const&) = delete;
        Mutex(Mutex&&) = delete;
        Mutex& operator = (Mutex const&) = delete;
        Mutex& operator = (Mutex&&) = delete;
        ~Mutex() = default;

        void Init();
        void Lock();
        void Unlock();
    };

    // Starting with Windows 2000, drivers can use fast mutexes
    // if they require a low-overhead form of mutual exclusion for code that runs at IRQL <= APC_LEVEL.
    // https://docs.microsoft.com/windows-hardware/drivers/kernel/fast-mutexes-and-guarded-mutexes#fast-mutexes
    class FastMutex final : public ILock
    {
        ALIGN FAST_MUTEX mutex;

    public:
        FastMutex();
        FastMutex(FastMutex const&) = delete;
        FastMutex(FastMutex&&) = delete;
        FastMutex& operator = (FastMutex const&) = delete;
        FastMutex& operator = (FastMutex&&) = delete;
        ~FastMutex() = default;

        _IRQL_requires_max_(DISPATCH_LEVEL)
        void Init();

        _IRQL_raises_(APC_LEVEL)
        _IRQL_saves_global_(OldIrql, mutex)
        void Lock();

        _IRQL_raises_(APC_LEVEL)
        _IRQL_saves_global_(OldIrql, mutex)
        _Success_(return == true)
        [[nodiscard]] auto TryLock() -> bool;

        _IRQL_requires_(APC_LEVEL)
        _IRQL_restores_global_(OldIrql, mutex)
        void Unlock();
    };

    // Guarded mutexes, which are available starting with Windows Server 2003,
    // perform the same function as fast mutexes but with higher performance.
    // https://docs.microsoft.com/windows-hardware/drivers/kernel/fast-mutexes-and-guarded-mutexes#guarded-mutexes
    class GuardedMutex final : public ILock
    {
        ALIGN KGUARDED_MUTEX mutex;

    public:
        GuardedMutex();
        GuardedMutex(GuardedMutex const&) = delete;
        GuardedMutex(GuardedMutex&&) = delete;
        GuardedMutex& operator = (GuardedMutex const&) = delete;
        GuardedMutex& operator = (GuardedMutex&&) = delete;
        ~GuardedMutex() = default;

        _IRQL_requires_max_(DISPATCH_LEVEL)
        void Init();

        _IRQL_requires_max_(APC_LEVEL)
        void Lock();

        _IRQL_raises_(APC_LEVEL)
        _IRQL_saves_global_(OldIrql, mutex)
        _Success_(return == true)
        [[nodiscard]] auto TryLock() -> bool;

        _IRQL_requires_max_(APC_LEVEL)
        void Unlock();
    };

    // Spin locks are kernel-defined, kernel-mode-only synchronization mechanisms,
    // exported as an opaque type: KSPIN_LOCK.
    // A spin lock can be used to protect shared data or resources from simultaneous access
    // by routines that can execute concurrently and at IRQL >= DISPATCH_LEVEL in SMP machines.
    // https://docs.microsoft.com/windows-hardware/drivers/kernel/spin-locks
    class SpinLock final : public ILock
    {
        ALIGN KSPIN_LOCK lock;
        KIRQL old_irql;

    public:
        SpinLock();
        SpinLock(SpinLock const&) = delete;
        SpinLock(SpinLock&&) = delete;
        SpinLock& operator = (SpinLock const&) = delete;
        SpinLock& operator = (SpinLock&&) = delete;
        ~SpinLock() = default;

        void Init();
        
        _IRQL_saves_global_(SpinLock, old_irql)
        _IRQL_raises_(DISPATCH_LEVEL)
        void Lock();

        _IRQL_requires_(DISPATCH_LEVEL)
        _IRQL_restores_global_(SpinLock, old_irql)
        void Unlock();
    };

    // Queued spin locks are a variant of spin locks that are more efficient for high contention locks on multiprocessor machines.
    // On multiprocessor machines, using queued spin locks guarantees that processors acquire the spin lock on a first - come first - served basis.
    // Drivers for Windows XP and later versions of Windows should use queued spin locks instead of ordinary spin locks.
    // https://docs.microsoft.com/windows-hardware/drivers/kernel/queued-spin-locks
    class QueuedSpinLock final : public ILock
    {
        ALIGN KSPIN_LOCK lock;
        KLOCK_QUEUE_HANDLE handle;

    public:
        QueuedSpinLock();
        QueuedSpinLock(QueuedSpinLock const&) = delete;
        QueuedSpinLock(QueuedSpinLock&&) = delete;
        QueuedSpinLock& operator = (QueuedSpinLock const&) = delete;
        QueuedSpinLock& operator = (QueuedSpinLock&&) = delete;
        ~QueuedSpinLock() = default;

        void Init();

        _IRQL_requires_max_(DISPATCH_LEVEL)
        _IRQL_saves_global_(QueuedSpinLock, handle)
        _IRQL_raises_(DISPATCH_LEVEL)
        void Lock();

        _IRQL_requires_(DISPATCH_LEVEL)
        _IRQL_restores_global_(QueuedSpinLock, handle)
        void Unlock();

        _IRQL_requires_(DISPATCH_LEVEL)
        void LockAtDpc();

        _IRQL_requires_(DISPATCH_LEVEL)
        void UnlockFromDpc();
    };

    // The resource variable can be used for synchrosization by a set of threads
    // The ERESOURCE structure is opaque.
    class ExecutiveResource : public ILock 
    {
        ERESOURCE resource;

    public:
        ExecutiveResource();
        ExecutiveResource(const ExecutiveResource&) = delete;
        ExecutiveResource(ExecutiveResource&&) = delete;
        ExecutiveResource& operator=(const ExecutiveResource&) = delete;
        ExecutiveResource& operator=(ExecutiveResource&&) = delete;

        _IRQL_requires_max_(APC_LEVEL)
         ~ExecutiveResource();

        _IRQL_requires_max_(DISPATCH_LEVEL)
        void Init() override;

        _IRQL_requires_max_(DISPATCH_LEVEL)
        auto Reinitialize() -> NTSTATUS;
        
        _IRQL_requires_max_(APC_LEVEL)
        void Lock() override;

        _IRQL_requires_max_(APC_LEVEL)
        auto LockShared() -> BOOLEAN;

        _IRQL_requires_max_(DISPATCH_LEVEL)
        void Unlock() override;
    };
}