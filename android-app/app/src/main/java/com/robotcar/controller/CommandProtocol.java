package com.robotcar.controller;

/**
 * Single source of truth for the Bluetooth command bytes sent to the
 * Arduino firmware. Frames are "%X#" where X is one of the command
 * characters the firmware accepts (F, B, L, R, S, T, A).
 */
public final class CommandProtocol {

    public static final char CMD_FORWARD    = 'F';
    public static final char CMD_BACKWARD   = 'B';
    public static final char CMD_LEFT       = 'L';
    public static final char CMD_RIGHT      = 'R';
    public static final char CMD_STOP       = 'S';
    public static final char CMD_TRACE      = 'T';
    public static final char CMD_AVOIDANCE  = 'A';

    public static final String FORWARD    = frame(CMD_FORWARD);
    public static final String BACKWARD   = frame(CMD_BACKWARD);
    public static final String LEFT       = frame(CMD_LEFT);
    public static final String RIGHT      = frame(CMD_RIGHT);
    public static final String STOP       = frame(CMD_STOP);
    public static final String TRACE      = frame(CMD_TRACE);
    public static final String AVOIDANCE  = frame(CMD_AVOIDANCE);

    private static final String FRAME_PREFIX = "%";
    private static final String FRAME_SUFFIX = "#";

    private CommandProtocol() {}

    public static String frame(char command) {
        if (!isValidCommand(command)) {
            throw new IllegalArgumentException("Unknown command char: " + command);
        }
        return FRAME_PREFIX + command + FRAME_SUFFIX;
    }

    public static boolean isValidCommand(char command) {
        switch (command) {
            case CMD_FORWARD:
            case CMD_BACKWARD:
            case CMD_LEFT:
            case CMD_RIGHT:
            case CMD_STOP:
            case CMD_TRACE:
            case CMD_AVOIDANCE:
                return true;
            default:
                return false;
        }
    }
}
