package com.faforever.iceadapter.util;

import java.util.HashMap;
import java.util.Map;

public class ArgumentParser {

    public static Map<String, String> parse(String[] args) {
        Map<String, String> res = new HashMap<>();
        for(int i = 0;i < args.length;i++) {
            if(! args[i].startsWith("--")) {
                throw new IllegalArgumentException("Wrong formatting of arguments. Expected: --");
            }

            String key = args[i].substring(2);
            res.put(key, null);

            if(i + 1 < args.length && (! args[i + 1].startsWith("--"))) {
                i++;
                res.put(key, args[i]);
            }
        }
        return res;
    }
}
