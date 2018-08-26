package client.nativeAccess;

import com.sun.jna.Native;
import com.sun.jna.win32.W32APIOptions;

public interface Kernel32 extends com.sun.jna.platform.win32.Kernel32 {
    Kernel32 INSTANCE = Native.loadLibrary("kernel32", Kernel32.class, W32APIOptions.DEFAULT_OPTIONS);

    boolean DebugActiveProcess(int dwProcessId);
    boolean DebugActiveProcessStop(int dwProcessId);
    boolean DebugSetProcessKillOnExit(boolean KillOnExit);
}
