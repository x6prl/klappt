package org.libsdl.app;

import android.content.Context;
import android.text.Editable;
import android.text.Selection;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;

class SDLInputConnection extends BaseInputConnection {
    public static SDLInputConnection sInstance;
    protected EditText mEditText;
    protected View mTargetView;

    SDLInputConnection(View targetView, boolean fullEditor) {
        super(targetView, fullEditor);
        mTargetView = targetView;
        mEditText = new EditText(SDL.getContext());
        sInstance = this;
    }

    @Override
    public Editable getEditable() {
        return mEditText != null ? mEditText.getEditableText() : null;
    }

    @Override
    public boolean sendKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_ENTER) {
            if (SDLActivity.onNativeSoftReturnKey()) {
                return true;
            }
        }
        return super.sendKeyEvent(event);
    }

    // --- Forwarding: IME -> Native C++ ---

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        super.commitText(text, newCursorPosition);
        nativeCommitText(text.toString(), newCursorPosition);
        return true;
    }

    @Override
    public boolean setComposingRegion(int start, int end) {
        super.setComposingRegion(start, end);
        nativeSetComposingRegion(start, end);
        return true;
    }

    @Override
    public boolean deleteSurroundingText(int beforeLength, int afterLength) {
        super.deleteSurroundingText(beforeLength, afterLength);
        nativeDeleteSurroundingText(beforeLength, afterLength, false); // UTF-16 code units
        return true;
    }

    @Override
    public boolean deleteSurroundingTextInCodePoints(int beforeLength, int afterLength) {
        super.deleteSurroundingTextInCodePoints(beforeLength, afterLength);
        nativeDeleteSurroundingText(beforeLength, afterLength, true); // Codepoints
        return true;
    }

    public static native void nativeSetComposingRegion(int start, int end);

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        super.setComposingText(text, newCursorPosition);
        nativeSetComposingText(text.toString(), newCursorPosition);
        return true;
    }

    @Override
    public boolean finishComposingText() {
        super.finishComposingText();
        nativeFinishComposingText();
        return true;
    }

    @Override
    public boolean setSelection(int start, int end) {
        super.setSelection(start, end);
        nativeSetSelection(start, end);
        return true;
    }

    // --- Reverse Synchronization: Native C++ -> Android IME ---

    public static void updateState(String text, int selStart, int selEnd, int compStart, int compEnd) {
        SDLActivity.mSingleton.runOnUiThread(() -> {
            if (sInstance != null) {
                if (sInstance.mEditText != null) {
                    Editable editable = sInstance.mEditText.getText();
                    if (editable != null && !editable.toString().equals(text)) {
                        editable.replace(0, editable.length(), text);
                    }
                    if (editable != null) {
                        int len = editable.length();
                        int s = Math.max(0, Math.min(selStart, len));
                        int e = Math.max(0, Math.min(selEnd, len));
                        Selection.setSelection(editable, s, e);
                    }
                }
                InputMethodManager imm = (InputMethodManager) SDL.getContext()
                        .getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null && sInstance.mTargetView != null) {
                    imm.updateSelection(sInstance.mTargetView, selStart, selEnd, compStart, compEnd);
                }
            }
        });
    }

    // SDL3 RegisterNatives in SDL_android.c
    public static native void nativeCommitText(String text, int newCursorPosition);
    public static native void nativeGenerateScancodeForUnichar(char c);

    // hooks in line_input.cpp
    public static native void nativeSetComposingText(String text, int newCursorPosition);
    public static native void nativeFinishComposingText();
    public static native void nativeDeleteSurroundingText(int before, int after, boolean isCodePoints);
    public static native void nativeSetSelection(int start, int end);
}
