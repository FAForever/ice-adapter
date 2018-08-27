package client.nativeAccess;

import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.platform.win32.WinNT;

public class NativeAccess {

    private static final boolean windows;
    private static final boolean arch64;//x86 or x86_64, windows will lie if this application is running in a 32 bit JVM
    private static final int SYS_KILL;//EAX

    public static final int SIGKILL = 9;
    public static final int SIGCONT = 18;
    public static final int SIGSTOP = 19;

    private static CStdLib c;

    static {
        windows = System.getProperty("os.name").contains("Windows");
        arch64 = System.getProperty("os.arch").endsWith("64");

        //Setup posix syscall id's
        if(arch64) {
            SYS_KILL = 0x3E;
        } else {
            SYS_KILL = 0x25;
        }

        if(windows) {
            //TODO
        } else {
            c = (CStdLib)Native.loadLibrary("c", CStdLib.class);
        }
    }

    public interface CStdLib extends Library {
        int syscall(int number, Object... args);
    }



    public static void sendSignal(int pid, int sig) {
        c.syscall(SYS_KILL, pid, sig);
    }

    public static void suspendWin32(long handle) {
        Kernel32 kernel = Kernel32.INSTANCE;

        WinNT.HANDLE hand = new WinNT.HANDLE();
        hand.setPointer(Pointer.createConstant(handle));

        int processId = kernel.GetProcessId(hand);

        kernel.DebugActiveProcess(processId);
    }

    public static void resumeWin32(long handle) {
        Kernel32 kernel = Kernel32.INSTANCE;

        WinNT.HANDLE hand = new WinNT.HANDLE();
        hand.setPointer(Pointer.createConstant(handle));

        int processId = kernel.GetProcessId(hand);

        kernel.DebugSetProcessKillOnExit(false);
        kernel.DebugActiveProcessStop(processId);
    }
}
