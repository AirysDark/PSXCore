package com.airysdark.psxcore.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.airysdark.psxcore.model.ControllerInputState

private const val BASE_WIDTH = 1000f
private const val BASE_HEIGHT = 600f

@Composable
fun Ps2ControllerVisualizer(
    inputState: ControllerInputState,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.aspectRatio(BASE_WIDTH / BASE_HEIGHT)) {
        val scale = size.width / BASE_WIDTH

        drawControllerBody(scale)

        // D-Pad
        drawDpad(
            x = 200f * scale,
            y = 250f * scale,
            scale = scale,
            up = inputState.dpadUp,
            down = inputState.dpadDown,
            left = inputState.dpadLeft,
            right = inputState.dpadRight
        )

        // Face Buttons
        drawFaceButtons(
            x = 800f * scale,
            y = 250f * scale,
            scale = scale,
            triangle = inputState.triangle,
            circle = inputState.circle,
            cross = inputState.cross,
            square = inputState.square
        )

        // Sticks
        drawStick(
            centerX = 350f * scale,
            centerY = 450f * scale,
            scale = scale,
            stickX = inputState.leftStickX,
            stickY = inputState.leftStickY,
            pressed = inputState.l3
        )
        drawStick(
            centerX = 650f * scale,
            centerY = 450f * scale,
            scale = scale,
            stickX = inputState.rightStickX,
            stickY = inputState.rightStickY,
            pressed = inputState.r3
        )

        // Shoulder Buttons
        drawShoulderButton(150f * scale, 50f * scale, 150f * scale, 60f * scale, scale, inputState.l2)
        drawShoulderButton(150f * scale, 120f * scale, 150f * scale, 60f * scale, scale, inputState.l1)
        drawShoulderButton(700f * scale, 50f * scale, 150f * scale, 60f * scale, scale, inputState.r2)
        drawShoulderButton(700f * scale, 120f * scale, 150f * scale, 60f * scale, scale, inputState.r1)

        // Center Buttons
        drawCenterButton(400f * scale, 280f * scale, 80f * scale, 40f * scale, scale, inputState.select)
        drawCenterButton(520f * scale, 280f * scale, 80f * scale, 40f * scale, scale, inputState.start)
        
        // Analog Mode Button
        drawControllerButton(
            center = Offset(500f * scale, 380f * scale),
            radius = 30f * scale,
            pressed = inputState.analogButton,
            color = if (inputState.analogMode) Color.Red else Color.DarkGray
        )
    }
}

private fun DrawScope.drawControllerBody(scale: Float) {
    val bodyColor = Color(0xFF333333)
    val path = Path().apply {
        moveTo(300f * scale, 180f * scale)
        lineTo(700f * scale, 180f * scale)
        quadraticTo(950f * scale, 180f * scale, 950f * scale, 350f * scale)
        lineTo(900f * scale, 550f * scale)
        lineTo(700f * scale, 500f * scale)
        lineTo(300f * scale, 500f * scale)
        lineTo(100f * scale, 550f * scale)
        lineTo(50f * scale, 350f * scale)
        quadraticTo(50f * scale, 180f * scale, 300f * scale, 180f * scale)
        close()
    }
    drawPath(path, color = bodyColor)
    drawPath(path, color = Color.Black, style = Stroke(width = 4f * scale))
}

private fun DrawScope.drawDpad(x: Float, y: Float, scale: Float, up: Boolean, down: Boolean, left: Boolean, right: Boolean) {
    val size = 50f * scale
    val color = Color(0xFF222222)
    val activeColor = Color.Cyan

    drawRect(if (up) activeColor else color, Offset(x - size / 2, y - size * 1.5f), Size(size, size))
    drawRect(if (down) activeColor else color, Offset(x - size / 2, y + size * 0.5f), Size(size, size))
    drawRect(if (left) activeColor else color, Offset(x - size * 1.5f, y - size / 2), Size(size, size))
    drawRect(if (right) activeColor else color, Offset(x + size * 0.5f, y - size / 2), Size(size, size))
}

private fun DrawScope.drawFaceButtons(x: Float, y: Float, scale: Float, triangle: Boolean, circle: Boolean, cross: Boolean, square: Boolean) {
    val radius = 35f * scale
    val dist = 80f * scale
    drawControllerButton(Offset(x, y - dist), radius, triangle, Color.Green)
    drawControllerButton(Offset(x + dist, y), radius, circle, Color.Red)
    drawControllerButton(Offset(x, y + dist), radius, cross, Color.Blue)
    drawControllerButton(Offset(x - dist, y), radius, square, Color.Magenta)
}

private fun DrawScope.drawControllerButton(center: Offset, radius: Float, pressed: Boolean, color: Color) {
    drawCircle(
        color = if (pressed) color else Color(0xFF222222),
        radius = radius,
        center = center
    )
    drawCircle(
        color = Color.Black,
        radius = radius,
        center = center,
        style = Stroke(width = 2f * (radius / 35f))
    )
}

private fun DrawScope.drawStick(centerX: Float, centerY: Float, scale: Float, stickX: Int, stickY: Int, pressed: Boolean) {
    val outerRadius = 90f * scale
    val innerRadius = 55f * scale
    val maxTravel = 45f * scale
    drawCircle(Color.Black, outerRadius, Offset(centerX, centerY))
    val normX = (stickX - 128f) / 128f
    val normY = (stickY - 128f) / 128f 
    val thumbX = centerX + normX * maxTravel
    val thumbY = centerY + normY * maxTravel
    drawCircle(
        color = if (pressed) Color.Cyan else Color(0xFF555555),
        radius = innerRadius,
        center = Offset(thumbX, thumbY)
    )
    drawCircle(
        color = Color.Black,
        radius = innerRadius,
        center = Offset(thumbX, thumbY),
        style = Stroke(width = 2f * scale)
    )
}

private fun DrawScope.drawShoulderButton(x: Float, y: Float, w: Float, h: Float, scale: Float, pressed: Boolean) {
    drawRoundRect(
        color = if (pressed) Color.Cyan else Color(0xFF222222),
        topLeft = Offset(x, y),
        size = Size(w, h),
        cornerRadius = CornerRadius(10f * scale)
    )
    drawRoundRect(
        color = Color.Black,
        topLeft = Offset(x, y),
        size = Size(w, h),
        cornerRadius = CornerRadius(10f * scale),
        style = Stroke(width = 2f * scale)
    )
}

private fun DrawScope.drawCenterButton(x: Float, y: Float, w: Float, h: Float, scale: Float, pressed: Boolean) {
    drawRoundRect(
        color = if (pressed) Color.Cyan else Color(0xFF222222),
        topLeft = Offset(x, y),
        size = Size(w, h),
        cornerRadius = CornerRadius(5f * scale)
    )
}

@Preview(showBackground = true)
@Composable
fun PreviewControllerVisualizer() {
    Ps2ControllerVisualizer(
        inputState = ControllerInputState(
            triangle = true,
            cross = true,
            dpadLeft = true,
            l1 = true,
            r2 = true,
            leftStickX = 50,
            leftStickY = 200,
            analogMode = true
        ),
        modifier = Modifier.fillMaxWidth().padding(16.dp)
    )
}
