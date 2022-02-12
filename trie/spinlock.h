#pragma once

/*! \file spinlock.h

Implements a simple spinlock, based on 
https://rigtorp.se/spinlock/
*/

namespace speedex {

//! Mutex based on a spinlock
class SpinMutex {
	mutable std::atomic<bool> flag;
public:
	void lock() const {
		while(true) {
			bool res = flag.load(std::memory_order_relaxed);

			if (!res) {
				if (!flag.exchange(
						true, std::memory_order_acquire)) {
					return;
				}
			}
			__builtin_ia32_pause();
		}
	}

	void unlock() const {
		flag.store(false, std::memory_order_release);
	}
};

//! Automatically unlocking wrapper around SpinMutex
class SpinLockGuard {
	const SpinMutex& mtx;
public:
	SpinLockGuard(const SpinMutex& mtx) 
		: mtx(mtx) {
			mtx.lock();
		}

	SpinLockGuard(const SpinLockGuard&) = delete;
	SpinLockGuard(SpinLockGuard&&) = delete;

	~SpinLockGuard() {
		mtx.unlock();
	}
};

//! Automatically unlocking unique lock wrapper around SpinMutex
class SpinUniqueLock {
	const SpinMutex* mtx;
	bool locked = false;
public:
	SpinUniqueLock(const SpinMutex& mtx_) 
		: mtx(&mtx_) {
			mtx->lock();
			locked=true;
		}

	SpinUniqueLock(const SpinUniqueLock&) = delete;
	SpinUniqueLock(SpinUniqueLock&&) = delete;
	
	SpinUniqueLock&
	operator=(SpinUniqueLock&& other) {
		if (locked) {
			mtx->unlock();
			locked = false;
		}
		mtx = other.mtx;
		locked = other.locked;

		other.mtx = nullptr;
		other.locked = false;

		return *this;
	}

	~SpinUniqueLock() {
		if (locked) {
			mtx->unlock();
		}
	}
};


} /* speedex */