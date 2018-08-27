package com.faforever.iceadapter.util;

import java.io.File;

public class Util {

    public static boolean mkdir(File file) {
        if (!file.exists()) {
            return file.mkdir();
        }
        return false;
    }

}
