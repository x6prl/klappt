package org.viktorfilinkov.klappt;

import java.lang.reflect.Field;

import android.os.Handler;
import android.os.Message;
import android.os.Build;

import android.view.View;
import android.view.Window;
import android.view.WindowInsetsController;

import org.libsdl.app.SDLActivity;

public class KlapptActivity extends SDLActivity {

    private Handler mCmdHandler = null;

    @Override
    protected boolean sendCommand(int command, Object data) {
        // Bypass the hardcoded getContext().wait(500) inside SDLActivity.java
        if (command == COMMAND_CHANGE_WINDOW_STYLE) {
            try {
                if (mCmdHandler == null) {
                    Field field = SDLActivity.class.getDeclaredField("commandHandler");
                    field.setAccessible(true);
                    mCmdHandler = (Handler) field.get(this);
                }
                if (mCmdHandler != null) {
                    Message msg = mCmdHandler.obtainMessage();
                    msg.arg1 = command;
                    msg.obj = data;
                    return mCmdHandler.sendMessage(msg);
                }
            } catch (Throwable ignored) {}
        }
        return super.sendCommand(command, data);
    }

    public void setSystemBarIconsDark(final boolean dark) {
        runOnUiThread(() -> {
            Window window = getWindow();

            if (Build.VERSION.SDK_INT >= 30) {
                WindowInsetsController controller =
                    window.getInsetsController();

                if (controller == null)
                    return;

                final int mask =
                    WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS |
                    WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;

                controller.setSystemBarsAppearance(
                    dark ? mask : 0,
                    mask
                );
            } else {
                View decor = window.getDecorView();
                int flags = decor.getSystemUiVisibility();
                int mask = 0;

                if (Build.VERSION.SDK_INT >= 23)
                    mask |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;

                if (Build.VERSION.SDK_INT >= 26)
                    mask |= View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;

                decor.setSystemUiVisibility(
                    dark ? (flags | mask) : (flags & ~mask)
                );
            }
        });
    }
}
