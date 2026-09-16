package org.viktorfilinkov.klappt;

import android.os.Handler;
import android.os.Message;
import java.lang.reflect.Field;
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
}
