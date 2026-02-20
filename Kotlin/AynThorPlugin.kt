package org.godot.plugins.aynthor

import android.app.Presentation
import android.content.Context
import android.graphics.Rect
import android.hardware.display.DisplayManager
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.Display
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowManager
import org.godotengine.godot.Godot
import org.godotengine.godot.plugin.GodotPlugin
import org.godotengine.godot.plugin.SignalInfo
import org.godotengine.godot.plugin.UsedByGodot

class AynThorPlugin(godot: Godot) : GodotPlugin(godot) {
    private val TAG = "AynThor"
    private var presentation: SecondScreenPresentation? = null
    private var isScreenActive = false

    private external fun nativeSetSurface(surface: Surface)
    private external fun nativeRemoveSurface()

    companion object {
        init {
            System.loadLibrary("aynthor_native")
        }
    }

    override fun getPluginName() = "AynThor"

    override fun getPluginSignals(): Set<SignalInfo> {
        return setOf(
            SignalInfo("second_screen_connected"),
            SignalInfo("second_screen_disconnected"),
            SignalInfo(
                "second_screen_input",
                Int::class.javaObjectType,
                Float::class.javaObjectType,
                Float::class.javaObjectType,
                Int::class.javaObjectType
            )
        )
    }

    private fun applyImmersiveLogic(targetView: View) {
        val uiOptions = (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN)

        targetView.systemUiVisibility = uiOptions

        targetView.setOnSystemUiVisibilityChangeListener { visibility ->
            if ((visibility and View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                targetView.systemUiVisibility = uiOptions
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            targetView.post {
                val exclusionWidth = 200
                val exclusionRects = listOf(
                    Rect(0, 0, exclusionWidth, targetView.height),
                    Rect(targetView.width - exclusionWidth, 0, targetView.width, targetView.height)
                )
                targetView.systemGestureExclusionRects = exclusionRects
            }
        }
    }

    @UsedByGodot
    fun init_screen() {
        isScreenActive = true
        activity?.runOnUiThread {
            activity?.window?.decorView?.let { applyImmersiveLogic(it) }

            val displayManager = activity?.getSystemService(Context.DISPLAY_SERVICE) as? DisplayManager
            if (displayManager == null) return@runOnUiThread

            val displays = displayManager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)
            val targetDisplay = if (displays.isNotEmpty()) {
                displays[0]
            } else {
                displayManager.displays.firstOrNull { it.displayId != Display.DEFAULT_DISPLAY }
            }

            if (targetDisplay != null) {
                showPresentation(targetDisplay)
            }
        }
    }

    private fun showPresentation(display: Display) {
        presentation?.dismiss()
        presentation = SecondScreenPresentation(activity!!, display).apply {
            window?.setFlags(
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
            )
            show()
        }
        emitSignal("second_screen_connected")
    }

    @UsedByGodot
    fun destroy_screen() {
        isScreenActive = false
        activity?.runOnUiThread {
            presentation?.dismiss()
            presentation = null
            emitSignal("second_screen_disconnected")
        }
    }

    override fun onMainPause() {
        super.onMainPause()
        activity?.runOnUiThread {
            if (presentation != null) {
                presentation?.dismiss()
                presentation = null
            }
        }
    }

    override fun onMainResume() {
        super.onMainResume()
        if (isScreenActive) {
            init_screen()
        }
    }

    private inner class SecondScreenPresentation(
        outerContext: Context,
        display: Display
    ) : Presentation(outerContext, display) {

        override fun onCreate(savedInstanceState: Bundle?) {
            super.onCreate(savedInstanceState)

            val surfaceView = SurfaceView(context)

            surfaceView.setOnTouchListener { _, event ->
                val action = event.actionMasked

                if (action == MotionEvent.ACTION_MOVE) {
                    for (i in 0 until event.pointerCount) {
                        emitSignal("second_screen_input", 2, event.getX(i), event.getY(i), event.getPointerId(i))
                    }
                } else {
                    val idx = event.actionIndex
                    emitSignal("second_screen_input", action, event.getX(idx), event.getY(idx), event.getPointerId(idx))
                }
                true
            }

            surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
                override fun surfaceCreated(holder: SurfaceHolder) {
                    nativeSetSurface(holder.surface)
                }

                override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {}

                override fun surfaceDestroyed(holder: SurfaceHolder) {
                    nativeRemoveSurface()
                }
            })

            setContentView(surfaceView)
            window?.decorView?.let { applyImmersiveLogic(it) }
        }
    }

    override fun onMainDestroy() {
        destroy_screen()
        super.onMainDestroy()
    }
}