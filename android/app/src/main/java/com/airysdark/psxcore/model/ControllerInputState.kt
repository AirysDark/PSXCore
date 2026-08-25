package com.airysdark.psxcore.model

data class ControllerInputState(
    val dpadUp: Boolean = false,
    val dpadDown: Boolean = false,
    val dpadLeft: Boolean = false,
    val dpadRight: Boolean = false,

    val triangle: Boolean = false,
    val circle: Boolean = false,
    val cross: Boolean = false,
    val square: Boolean = false,

    val l1: Boolean = false,
    val l2: Boolean = false,
    val l3: Boolean = false,

    val r1: Boolean = false,
    val r2: Boolean = false,
    val r3: Boolean = false,

    val start: Boolean = false,
    val select: Boolean = false,

    val analogButton: Boolean = false,
    val analogMode: Boolean = false,

    val leftStickX: Int = 128,
    val leftStickY: Int = 128,
    val rightStickX: Int = 128,
    val rightStickY: Int = 128,

    val lastInputTimestamp: Long? = null,
    val packetCount: Long = 0
)
