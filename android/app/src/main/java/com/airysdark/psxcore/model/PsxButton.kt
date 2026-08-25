package com.airysdark.psxcore.model

enum class PsxButton(val bit: Int) {
    SELECT(0),
    L3(1),
    R3(2),
    START(3),
    UP(4),
    RIGHT(5),
    DOWN(6),
    LEFT(7),
    L2(8),
    R2(9),
    L1(10),
    R1(11),
    TRIANGLE(12),
    CIRCLE(13),
    CROSS(14),
    SQUARE(15),
    ANALOG(16) // Added for PSXCore
}

object PsxButtonMapping {
    fun isPressed(mask: Int, button: PsxButton): Boolean {
        return (mask and (1 shl button.bit)) != 0
    }
}
