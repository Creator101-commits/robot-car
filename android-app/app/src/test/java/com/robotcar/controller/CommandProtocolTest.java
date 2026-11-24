package com.robotcar.controller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import org.junit.Test;

/**
 * Asserts the same command table the firmware host tests pin:
 * F forward, B backward, L left, R right, S stop, T trace, A avoidance,
 * each sent as the framed "%X#" byte string.
 */
public class CommandProtocolTest {

    @Test
    public void framesMatchFirmwareCommandTable() {
        assertEquals("%F#", CommandProtocol.FORWARD);
        assertEquals("%B#", CommandProtocol.BACKWARD);
        assertEquals("%L#", CommandProtocol.LEFT);
        assertEquals("%R#", CommandProtocol.RIGHT);
        assertEquals("%S#", CommandProtocol.STOP);
        assertEquals("%T#", CommandProtocol.TRACE);
        assertEquals("%A#", CommandProtocol.AVOIDANCE);
    }

    @Test
    public void frameWrapsValidCommands() {
        for (char c : new char[]{'F', 'B', 'L', 'R', 'S', 'T', 'A'}) {
            assertEquals("%" + c + "#", CommandProtocol.frame(c));
            assertTrue(CommandProtocol.isValidCommand(c));
        }
    }

    @Test
    public void rejectsInvalidCommands() {
        for (char c : new char[]{'X', 'f', ' ', '0', '%', '#'}) {
            assertFalse(CommandProtocol.isValidCommand(c));
            try {
                CommandProtocol.frame(c);
                fail("expected IllegalArgumentException for '" + c + "'");
            } catch (IllegalArgumentException expected) {
            }
        }
    }
}
