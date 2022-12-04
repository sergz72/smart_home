package com.sz.motionsensor

import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Test

class FixedSizeHistoryTest {
    @Test
    fun fixedSizeHistoryTest() {
        val h = FixedSizeHistory<Int>(3)
        runBlocking {  h.addItems(listOf(1,2,3,4)) }
        val l = mutableListOf<Int>()
        runBlocking { h.getItems(l) }
        assertEquals(3, l.size)
        assertEquals(2, l[0])
        assertEquals(4, l[2])
    }
}