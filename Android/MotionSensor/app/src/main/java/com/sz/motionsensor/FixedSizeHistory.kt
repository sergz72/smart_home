package com.sz.motionsensor

import kotlinx.coroutines.sync.Semaphore

@Suppress("UNCHECKED_CAST")
open class FixedSizeHistory<T>(protected val capacity: Int) {
    protected val mHistory = arrayOfNulls<Any?>(capacity) as Array<T?>
    protected var mSize: Int = 0
    protected var mHead: Int = 0
    protected val lock = Semaphore(1)

    suspend fun addItems(l: List<T>) {
        lock.acquire()
        l.forEach { internalAddItem(it) }
        lock.release()
    }

    protected fun internalAddItem(l: T) {
        if (mSize < capacity) {
            mSize++
        }
        mHistory[mHead] = l
        mHead++
        if (mHead == capacity) {
            mHead = 0
        }
    }

    suspend fun addItem(i: T) {
        lock.acquire()
        internalAddItem(i)
        lock.release()
    }

    suspend fun getItems(l: MutableList<T>) {
        lock.acquire()
        for (i in 0 until mSize) {
            var idx = mHead - mSize + i
            if (idx < 0) {
                idx += capacity
            }
            l.add(mHistory[idx]!!)
        }
        lock.release()
    }
}