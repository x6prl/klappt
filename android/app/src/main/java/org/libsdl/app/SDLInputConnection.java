package org.libsdl.app;

import android.content.Context;
import android.text.Editable;
import android.text.Selection;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;

/** Android's UTF-16 Editable mirrored to LineInput's UTF-32 buffer. */
public final class SDLInputConnection extends BaseInputConnection {
    private static volatile SDLInputConnection active;
    private static volatile String pendingText = "";
    private static volatile int pendingStart, pendingEnd;

    private final View target;
    private final EditText editor;
    private boolean closed, enterHandled;

    public SDLInputConnection(View target, boolean fullEditor) {
        super(target, fullEditor);
        this.target = target;
        this.editor = new EditText(target.getContext());
        active = this;
        applyNativeState(pendingText, pendingStart, pendingEnd);
    }

    @Override public Editable getEditable() { return editor.getEditableText(); }

    private void post(Runnable task) {
        target.post(() -> { if (!closed && active == this) task.run(); });
    }

    private static int clamp(int value, int min, int max) {
        return Math.max(min, Math.min(value, max));
    }

    private static int offset(CharSequence text, int value) {
        value = clamp(value, 0, text.length());
        return value > 0 && value < text.length()
                && Character.isHighSurrogate(text.charAt(value - 1))
                && Character.isLowSurrogate(text.charAt(value)) ? value - 1 : value;
    }

    private static int utf16(CharSequence text, int utf32) {
        return Character.offsetByCodePoints(text, 0, clamp(utf32, 0,
                Character.codePointCount(text, 0, text.length())));
    }

    private static int utf32(CharSequence text, int utf16) {
        return Character.codePointCount(text, 0, offset(text, utf16));
    }

    public static void setNativeBufferState(String text, int start, int end) {
        pendingText = text != null ? text : "";
        pendingStart = start;
        pendingEnd = end;
        SDLInputConnection connection = active;
        if (connection != null) connection.applyNativeState(pendingText, start, end);
    }

    private void applyNativeState(String text, int start, int end) {
        post(() -> {
            Editable content = getEditable();
            BaseInputConnection.removeComposingSpans(content);
            if (!content.toString().equals(text)) content.replace(0, content.length(), text);
            Selection.setSelection(content, utf16(content, start), utf16(content, end));
            notifyIme();
        });
    }

    private boolean hasComposition(Editable content) {
        return BaseInputConnection.getComposingSpanStart(content) >= 0;
    }

    private void sendComposition() {
        Editable content = getEditable();
        int start = BaseInputConnection.getComposingSpanStart(content);
        int end = BaseInputConnection.getComposingSpanEnd(content);
        if (start < 0 || end < start) {
            nativeSetComposingText("", -1);
            return;
        }
        int cursor = offset(content, clamp(Selection.getSelectionEnd(content), start, end));
        nativeSetComposingText(content.subSequence(start, end).toString(),
                Character.codePointCount(content, start, cursor));
    }

    private void sendSelection() {
        Editable content = getEditable();
        int start = Selection.getSelectionStart(content);
        int end = Selection.getSelectionEnd(content);
        if (start < 0 || end < 0) return;
        if (hasComposition(content)) sendComposition();
        else nativeSetSelection(utf32(content, start), utf32(content, end));
    }

    @Override public boolean commitText(CharSequence text, int newCursorPosition) {
        Editable content = getEditable();
        boolean composing = hasComposition(content);
        int start = Selection.getSelectionStart(content), end = Selection.getSelectionEnd(content);
        int nativeStart = start < 0 ? -1 : utf32(content, start);
        int nativeEnd = end < 0 ? -1 : utf32(content, end);
        if (!super.commitText(text, newCursorPosition)) return false;
        if (!composing && nativeStart >= 0 && nativeEnd >= 0)
            nativeSetSelection(nativeStart, nativeEnd);
        nativeCommitText(text != null ? text.toString() : "", newCursorPosition);
        sendSelection();
        notifyIme();
        return true;
    }

    @Override public boolean setComposingText(CharSequence text, int newCursorPosition) {
        Editable content = getEditable();
        boolean composing = hasComposition(content);
        int start = Selection.getSelectionStart(content), end = Selection.getSelectionEnd(content);
        int nativeStart = start < 0 ? -1 : utf32(content, start);
        int nativeEnd = end < 0 ? -1 : utf32(content, end);
        if (!super.setComposingText(text, newCursorPosition)) return false;
        if (!composing && nativeStart >= 0 && nativeEnd >= 0)
            nativeSetSelection(nativeStart, nativeEnd);
        sendComposition();
        notifyIme();
        return true;
    }

    @Override public boolean finishComposingText() {
        Editable content = getEditable();
        int start = BaseInputConnection.getComposingSpanStart(content);
        int end = BaseInputConnection.getComposingSpanEnd(content);
        String text = start >= 0 && end >= start ? content.subSequence(start, end).toString() : "";
        if (!super.finishComposingText()) return false;
        if (text.isEmpty()) nativeSetComposingText("", -1);
        else nativeCommitText(text, 1);
        sendSelection();
        notifyIme();
        return true;
    }

    @Override public boolean setSelection(int start, int end) {
        if (!super.setSelection(start, end)) return false;
        sendSelection();
        notifyIme();
        return true;
    }

    private boolean deleteRange(int start, int end) {
        Editable content = getEditable();
        start = offset(content, start);
        end = offset(content, end);
        if (start < 0 || end < start || end > content.length()) return false;
        int nativeStart = utf32(content, start), nativeEnd = utf32(content, end);
        if (nativeStart == nativeEnd) return true;
        boolean composing = hasComposition(content);
        content.delete(start, end);
        if (composing) sendComposition();
        else nativeDeleteRange(nativeStart, nativeEnd);
        sendSelection();
        notifyIme();
        return true;
    }

    private boolean deleteSurrounding(int before, int after, boolean codePoints) {
        Editable content = getEditable();
        int start = Selection.getSelectionStart(content), end = Selection.getSelectionEnd(content);
        if (start < 0 || end < 0) return false;
        if (start > end) { int swap = start; start = end; end = swap; }
        if (codePoints) {
            start = Character.offsetByCodePoints(content, offset(content, start),
                    -Math.min(before, Character.codePointCount(content, 0, start)));
            end = Character.offsetByCodePoints(content, offset(content, end),
                    Math.min(after, Character.codePointCount(content, end, content.length())));
        } else {
            start = offset(content, start - before);
            end = offset(content, end + after);
        }
        return deleteRange(start, end);
    }

    @Override public boolean deleteSurroundingText(int before, int after) {
        return deleteSurrounding(before, after, false);
    }

    @Override public boolean deleteSurroundingTextInCodePoints(int before, int after) {
        return deleteSurrounding(before, after, true);
    }

    @Override public boolean sendKeyEvent(KeyEvent event) {
        int key = event.getKeyCode();
        if (key == KeyEvent.KEYCODE_DEL || key == KeyEvent.KEYCODE_FORWARD_DEL) {
            if (event.getAction() != KeyEvent.ACTION_DOWN) return true;
            Editable content = getEditable();
            int start = Selection.getSelectionStart(content), end = Selection.getSelectionEnd(content);
            if (start < 0 || end < 0) return false;
            if (start != end) return deleteRange(Math.min(start, end), Math.max(start, end));
            if (key == KeyEvent.KEYCODE_DEL)
                return start == 0 || deleteRange(Character.offsetByCodePoints(content, start, -1), start);
            return end == content.length() || deleteRange(end, Character.offsetByCodePoints(content, end, 1));
        }
        if (key == KeyEvent.KEYCODE_ENTER) {
            if (event.getAction() == KeyEvent.ACTION_DOWN && (enterHandled || SDLActivity.onNativeSoftReturnKey())) {
                enterHandled = true;
                return true;
            }
            if (event.getAction() == KeyEvent.ACTION_UP && enterHandled) {
                enterHandled = false;
                return true;
            }
        }
        return super.sendKeyEvent(event);
    }

    private void notifyIme() {
        Editable content = getEditable();
        int start = Selection.getSelectionStart(content), end = Selection.getSelectionEnd(content);
        if (start < 0 || end < 0) return;
        InputMethodManager imm = (InputMethodManager) target.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) imm.updateSelection(target, start, end,
                BaseInputConnection.getComposingSpanStart(content),
                BaseInputConnection.getComposingSpanEnd(content));
    }

    @Override public void closeConnection() {
        try { super.closeConnection(); }
        finally {
            closed = true;
            if (active == this) active = null;
        }
    }

    public static native void nativeCommitText(String text, int newCursorPosition);
    public static native void nativeSetComposingText(String text, int cursorPosition);
    public static native void nativeSetSelection(int start, int end);
    public static native void nativeDeleteRange(int start, int end);
    public static native void nativeGenerateScancodeForUnichar(char c);
}
