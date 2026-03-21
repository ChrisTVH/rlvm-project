package org.libsdl.app;


import android.content.Context;
import android.content.pm.ActivityInfo;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;


/**
 * SDLSurface. This is what we draw on, so we need to know when it's created
 * in order to do anything useful.
 * <p>
 * Because of this, that's where we set up the SDL thread
 */
public class SDLSurface extends SurfaceView implements SurfaceHolder.Callback,
        View.OnKeyListener, View.OnTouchListener, SensorEventListener {

    // Android bitmask for primary (left) and secondary (right) buttons.
    // SDL JNI maps BUTTON_PRIMARY (1) -> SDL_BUTTON_LEFT (1),
    //              BUTTON_SECONDARY (2) -> SDL_BUTTON_RIGHT (3).
    private static final int ANDROID_BUTTON_PRIMARY = 1;
    private static final int ANDROID_BUTTON_SECONDARY = 2;
    private static final int RIGHT_CLICK_MAX_MS = 400;
    private static final float RIGHT_CLICK_MAX_MOVE_PX = 35f;
    // Is SurfaceView ready for rendering
    public boolean mIsSurfaceReady;

    // ---------------------------------------------------------------
    // Touch-to-mouse emulation
    //
    // SDL's built-in touch-to-mouse synthesis is DISABLED via
    // SDL_HINT_TOUCH_MOUSE_EVENTS=0 (set in GameActivity).
    // This gives us full control and avoids conflicts where SDL's
    // synthetic LEFT CLICK from finger 0 would interfere with the
    // two-finger right-click gesture.
    //
    // Single finger:
    //   - ACTION_DOWN  → MOVE + LEFT DOWN
    //   - ACTION_MOVE  → MOVE
    //   - ACTION_UP    → LEFT UP
    //
    // Two-finger tap (sequential or simultaneous):
    //   - Second finger down  → start gesture window
    //   - Both fingers lift quickly without moving → fire RIGHT CLICK
    //     at finger-0's pixel position
    //   - If the two-finger gesture fires, the remaining finger's
    //     lift is consumed (no ghost LEFT UP).
    // ---------------------------------------------------------------
    // Sensors
    protected SensorManager mSensorManager;
    protected Display mDisplay;
    // Keep track of the surface size
    protected float mWidth, mHeight;
    private boolean mTwoFingerActive = false;
    private boolean mTwoFingerFired = false;
    private long mTwoFingerStartMs = 0;
    private float mTwoFingerX = 0f;   // raw pixels of finger 0 when gesture started
    private float mTwoFingerY = 0f;
    private float mTwoFingerRef0X = 0f;   // drift reference
    private float mTwoFingerRef0Y = 0f;

    // Startup
    public SDLSurface(Context context) {
        super(context);
        getHolder().addCallback(this);

        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();
        setOnKeyListener(this);
        setOnTouchListener(this);

        mDisplay = ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
        mSensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);

        setOnGenericMotionListener(SDLActivity.getMotionListener());

        // Some arbitrary defaults to avoid a potential division by zero
        mWidth = 1.0f;
        mHeight = 1.0f;

        mIsSurfaceReady = false;
    }

    private void fireRightClick() {
        mTwoFingerFired = true;
        // Cancel the synthetic left-click that SDL generated from
        // finger 0's touch-down.  The C++ engine will filter the
        // resulting synthetic SDL_BUTTON_LEFT, but we send an explicit
        // LEFT UP via onNativeMouse (mouseID 0, not filtered) so the
        // game's button1_state_ is released before the right-click.
        SDLActivity.onNativeMouse(ANDROID_BUTTON_PRIMARY, MotionEvent.ACTION_UP,
                mTwoFingerX, mTwoFingerY, false);
        // Move cursor to finger-0's position and fire RIGHT CLICK.
        SDLActivity.onNativeMouse(0, MotionEvent.ACTION_MOVE,
                mTwoFingerX, mTwoFingerY, false);
        SDLActivity.onNativeMouse(ANDROID_BUTTON_SECONDARY, MotionEvent.ACTION_DOWN,
                mTwoFingerX, mTwoFingerY, false);
        SDLActivity.onNativeMouse(ANDROID_BUTTON_SECONDARY, MotionEvent.ACTION_UP,
                mTwoFingerX, mTwoFingerY, false);
    }

    public void handlePause() {
        enableSensor(Sensor.TYPE_ACCELEROMETER, false);
    }

    public void handleResume() {
        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();
        setOnKeyListener(this);
        setOnTouchListener(this);
        enableSensor(Sensor.TYPE_ACCELEROMETER, true);
    }

    public Surface getNativeSurface() {
        return getHolder().getSurface();
    }

    // Called when we have a valid drawing surface
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.v("SDL", "surfaceCreated()");
        SDLActivity.onNativeSurfaceCreated();
    }

    // Called when we lose the surface
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.v("SDL", "surfaceDestroyed()");

        // Transition to pause, if needed
        SDLActivity.mNextNativeState = SDLActivity.NativeState.PAUSED;
        SDLActivity.handleNativeState();

        mIsSurfaceReady = false;
        SDLActivity.onNativeSurfaceDestroyed();
    }

    // Called when the surface is resized
    @Override
    public void surfaceChanged(SurfaceHolder holder,
                               int format, int width, int height) {
        Log.v("SDL", "surfaceChanged()");

        if (SDLActivity.mSingleton == null) {
            return;
        }

        mWidth = width;
        mHeight = height;
        int nDeviceWidth = width;
        int nDeviceHeight = height;
        try {
            if (Build.VERSION.SDK_INT >= 17 /* Android 4.2 (JELLY_BEAN_MR1) */) {
                DisplayMetrics realMetrics = new DisplayMetrics();
                mDisplay.getRealMetrics(realMetrics);
                nDeviceWidth = realMetrics.widthPixels;
                nDeviceHeight = realMetrics.heightPixels;
            }
        } catch (Exception ignored) {
        }

        synchronized (SDLActivity.getContext()) {
            // In case we're waiting on a size change after going fullscreen, send a notification.
            SDLActivity.getContext().notifyAll();
        }

        Log.v("SDL", "Window size: " + width + "x" + height);
        Log.v("SDL", "Device size: " + nDeviceWidth + "x" + nDeviceHeight);
        SDLActivity.nativeSetScreenResolution(width, height, nDeviceWidth, nDeviceHeight, mDisplay.getRefreshRate());
        SDLActivity.onNativeResize();

        // Prevent a screen distortion glitch,
        // for instance when the device is in Landscape and a Portrait App is resumed.
        boolean skip = false;
        int requestedOrientation = SDLActivity.mSingleton.getRequestedOrientation();

        if (requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT || requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT) {
            if (mWidth > mHeight) {
                skip = true;
            }
        } else if (requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE || requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE) {
            if (mWidth < mHeight) {
                skip = true;
            }
        }

        // Special Patch for Square Resolution: Black Berry Passport
        if (skip) {
            double min = Math.min(mWidth, mHeight);
            double max = Math.max(mWidth, mHeight);

            if (max / min < 1.20) {
                Log.v("SDL", "Don't skip on such aspect-ratio. Could be a square resolution.");
                skip = false;
            }
        }

        // Don't skip in MultiWindow.
        if (skip) {
            if (Build.VERSION.SDK_INT >= 24 /* Android 7.0 (N) */) {
                if (SDLActivity.mSingleton.isInMultiWindowMode()) {
                    Log.v("SDL", "Don't skip in Multi-Window");
                    skip = false;
                }
            }
        }

        if (skip) {
            Log.v("SDL", "Skip .. Surface is not ready.");
            mIsSurfaceReady = false;
            return;
        }

        /* If the surface has been previously destroyed by onNativeSurfaceDestroyed, recreate it here */
        SDLActivity.onNativeSurfaceChanged();

        /* Surface is ready */
        mIsSurfaceReady = true;

        // Signal to native code that the surface is ready
        Log.v("SDL", "Calling nativeReady()");
        try {
            SDLActivity.nativeReady();
            Log.v("SDL", "nativeReady() returned successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e("SDL", "nativeReady() failed: " + e.getMessage());
        }

        SDLActivity.mNextNativeState = SDLActivity.NativeState.RESUMED;
        SDLActivity.handleNativeState();
    }

    // Key events
    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        return SDLActivity.handleKeyEvent(v, keyCode, event, null);
    }

    // Touch events
    @Override
    public boolean onTouch(View v, MotionEvent event) {
        /* Ref: http://developer.android.com/training/gestures/multi.html */
        int touchDevId = event.getDeviceId();
        final int pointerCount = event.getPointerCount();
        int action = event.getActionMasked();
        int pointerFingerId;
        int i = -1;
        float x, y, p;

        /*
         * Prevent id to be -1, since it's used in SDL internal for synthetic events
         * Appears when using Android emulator, eg:
         *  adb shell input mouse tap 100 100
         *  adb shell input touchscreen tap 100 100
         */
        if (touchDevId < 0) {
            touchDevId -= 1;
        }

        // 12290 = Samsung DeX mode desktop mouse
        // 12290 = 0x3002 = 0x2002 | 0x1002 = SOURCE_MOUSE | SOURCE_TOUCHSCREEN
        // 0x2   = SOURCE_CLASS_POINTER
        if (event.getSource() == InputDevice.SOURCE_MOUSE || event.getSource() == (InputDevice.SOURCE_MOUSE | InputDevice.SOURCE_TOUCHSCREEN)) {
            int mouseButton = 1;
            try {
                Object object = event.getClass().getMethod("getButtonState").invoke(event);
                if (object != null) {
                    mouseButton = (Integer) object;
                }
            } catch (Exception ignored) {
            }

            // We need to check if we're in relative mouse mode and get the axis offset rather than the x/y values
            // if we are.  We'll leverage our existing mouse motion listener
            SDLGenericMotionListener_API12 motionListener = SDLActivity.getMotionListener();
            x = motionListener.getEventX(event);
            y = motionListener.getEventY(event);

            SDLActivity.onNativeMouse(mouseButton, action, x, y, motionListener.inRelativeMode());
        } else {
            // ---------------------------------------------------------
            // Two-finger right-click overlay.
            //
            // ALL events pass through to SDL unchanged so cursor movement
            // and left-click work with zero latency.
            //
            // When a second finger arrives while the first is held down,
            // we immediately fire a RIGHT CLICK at finger-0's position.
            // The C++ engine filters synthetic LEFT CLICK (from
            // SDL_TOUCH_MOUSEID) while g_two_finger_active is true.
            // ---------------------------------------------------------
            switch (action) {

                case MotionEvent.ACTION_DOWN:
                    if (pointerCount >= 2) {
                        // Both fingers arrived simultaneously
                        mTwoFingerX = event.getX(0);
                        mTwoFingerY = event.getY(0);
                        mTwoFingerActive = true;
                        mTwoFingerFired = false;
                        SDLActivity.nativeSetTwoFingerActive(true);
                        fireRightClick();
                    }
                    break;

                case MotionEvent.ACTION_POINTER_DOWN:
                    if (pointerCount == 2) {
                        mTwoFingerX = event.getX(0);
                        mTwoFingerY = event.getY(0);
                        mTwoFingerActive = true;
                        mTwoFingerFired = false;
                        SDLActivity.nativeSetTwoFingerActive(true);
                        fireRightClick();
                    }
                    break;

                case MotionEvent.ACTION_UP:
                    if (mTwoFingerActive) {
                        mTwoFingerActive = false;
                        mTwoFingerFired = false;
                        SDLActivity.nativeSetTwoFingerActive(false);
                    }
                    break;

                case MotionEvent.ACTION_CANCEL:
                    mTwoFingerActive = false;
                    mTwoFingerFired = false;
                    SDLActivity.nativeSetTwoFingerActive(false);
                    break;

                default:
                    break;
            }

            // Always pass the event to SDL (no blocking, no delay)
            switch (action) {
                case MotionEvent.ACTION_MOVE:
                    for (i = 0; i < pointerCount; i++) {
                        pointerFingerId = event.getPointerId(i);
                        x = event.getX(i) / mWidth;
                        y = event.getY(i) / mHeight;
                        p = event.getPressure(i);
                        if (p > 1.0f) {
                            p = 1.0f;
                        }
                        SDLActivity.onNativeTouch(touchDevId, pointerFingerId, action, x, y, p);
                    }
                    break;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_DOWN:
                    i = 0;
                    /* fallthrough */
                case MotionEvent.ACTION_POINTER_UP:
                case MotionEvent.ACTION_POINTER_DOWN:
                    if (i == -1) {
                        i = event.getActionIndex();
                    }
                    pointerFingerId = event.getPointerId(i);
                    x = event.getX(i) / mWidth;
                    y = event.getY(i) / mHeight;
                    p = event.getPressure(i);
                    if (p > 1.0f) {
                        p = 1.0f;
                    }
                    SDLActivity.onNativeTouch(touchDevId, pointerFingerId, action, x, y, p);
                    break;

                case MotionEvent.ACTION_CANCEL:
                    for (i = 0; i < pointerCount; i++) {
                        pointerFingerId = event.getPointerId(i);
                        x = event.getX(i) / mWidth;
                        y = event.getY(i) / mHeight;
                        p = event.getPressure(i);
                        if (p > 1.0f) {
                            p = 1.0f;
                        }
                        SDLActivity.onNativeTouch(touchDevId, pointerFingerId,
                                MotionEvent.ACTION_UP, x, y, p);
                    }
                    break;

                default:
                    break;
            }
        }

        return true;
    }

    // Sensor events
    public void enableSensor(int sensortype, boolean enabled) {
        // TODO: This uses getDefaultSensor - what if we have >1 accels?
        if (enabled) {
            mSensorManager.registerListener(this,
                    mSensorManager.getDefaultSensor(sensortype),
                    SensorManager.SENSOR_DELAY_GAME, null);
        } else {
            mSensorManager.unregisterListener(this,
                    mSensorManager.getDefaultSensor(sensortype));
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        // TODO
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER) {

            // Since we may have an orientation set, we won't receive onConfigurationChanged events.
            // We thus should check here.
            int newOrientation;

            float x, y;
            switch (mDisplay.getRotation()) {
                case Surface.ROTATION_90:
                    x = -event.values[1];
                    y = event.values[0];
                    newOrientation = SDLActivity.SDL_ORIENTATION_LANDSCAPE;
                    break;
                case Surface.ROTATION_270:
                    x = event.values[1];
                    y = -event.values[0];
                    newOrientation = SDLActivity.SDL_ORIENTATION_LANDSCAPE_FLIPPED;
                    break;
                case Surface.ROTATION_180:
                    x = -event.values[0];
                    y = -event.values[1];
                    newOrientation = SDLActivity.SDL_ORIENTATION_PORTRAIT_FLIPPED;
                    break;
                case Surface.ROTATION_0:
                default:
                    x = event.values[0];
                    y = event.values[1];
                    newOrientation = SDLActivity.SDL_ORIENTATION_PORTRAIT;
                    break;
            }

            if (newOrientation != SDLActivity.mCurrentOrientation) {
                SDLActivity.mCurrentOrientation = newOrientation;
                SDLActivity.onNativeOrientationChanged(newOrientation);
            }

            SDLActivity.onNativeAccel(-x / SensorManager.GRAVITY_EARTH,
                    y / SensorManager.GRAVITY_EARTH,
                    event.values[2] / SensorManager.GRAVITY_EARTH);


        }
    }

    // Captured pointer events for API 26.
    public boolean onCapturedPointerEvent(MotionEvent event) {
        int action = event.getActionMasked();

        float x, y;
        switch (action) {
            case MotionEvent.ACTION_SCROLL:
                x = event.getAxisValue(MotionEvent.AXIS_HSCROLL, 0);
                y = event.getAxisValue(MotionEvent.AXIS_VSCROLL, 0);
                SDLActivity.onNativeMouse(0, action, x, y, false);
                return true;

            case MotionEvent.ACTION_HOVER_MOVE:
            case MotionEvent.ACTION_MOVE:
                x = event.getX(0);
                y = event.getY(0);
                SDLActivity.onNativeMouse(0, action, x, y, true);
                return true;

            case MotionEvent.ACTION_BUTTON_PRESS:
            case MotionEvent.ACTION_BUTTON_RELEASE:

                // Change our action value to what SDL's code expects.
                if (action == MotionEvent.ACTION_BUTTON_PRESS) {
                    action = MotionEvent.ACTION_DOWN;
                } else { /* MotionEvent.ACTION_BUTTON_RELEASE */
                    action = MotionEvent.ACTION_UP;
                }

                x = event.getX(0);
                y = event.getY(0);
                int button = event.getButtonState();

                SDLActivity.onNativeMouse(button, action, x, y, true);
                return true;
        }

        return false;
    }
}
