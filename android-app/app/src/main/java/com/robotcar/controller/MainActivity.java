package com.robotcar.controller;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class MainActivity extends AppCompatActivity {

    // SPP UUID for HC-05 / HC-06
    private static final UUID BT_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
    private static final int  PERM_REQUEST_CODE  = 100;
    // How often (ms) to re-send a direction command while a button is held
    private static final long REPEAT_INTERVAL_MS = 320L;

    private BluetoothAdapter btAdapter;
    private BluetoothSocket  btSocket;
    private OutputStream     outStream;
    private boolean          connected = false;

    // null = manual, 'A' = avoidance, 'T' = tracking
    private Character activeMode = null;

    private final Handler  handler   = new Handler(Looper.getMainLooper());
    private       String   repeatCmd = null;

    private final Runnable repeatRunnable = new Runnable() {
        @Override
        public void run() {
            if (repeatCmd != null) transmit(repeatCmd);
            handler.postDelayed(this, REPEAT_INTERVAL_MS);
        }
    };

    private TextView tvStatus;
    private Button   btnConnect;
    private Button   btnForward;
    private Button   btnLeft;
    private Button   btnStop;
    private Button   btnRight;
    private Button   btnBackward;
    private Button   btnAvoidance;
    private Button   btnTracking;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        BluetoothManager manager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        if (manager != null) {
            btAdapter = manager.getAdapter();
        }

        bindViews();
        requestBtPermissions();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        doDisconnect();
    }

    @SuppressLint("ClickableViewAccessibility")
    private void bindViews() {
        tvStatus     = findViewById(R.id.tv_status);
        btnConnect   = findViewById(R.id.btn_connect);
        btnForward   = findViewById(R.id.btn_forward);
        btnLeft      = findViewById(R.id.btn_left);
        btnStop      = findViewById(R.id.btn_stop);
        btnRight     = findViewById(R.id.btn_right);
        btnBackward  = findViewById(R.id.btn_backward);
        btnAvoidance = findViewById(R.id.btn_avoidance);
        btnTracking  = findViewById(R.id.btn_tracking);

        btnConnect.setOnClickListener(v -> {
            if (connected) doDisconnect();
            else pickDevice();
        });

        // Hold to drive, release to stop
        attachHoldDrive(btnForward,  CommandProtocol.FORWARD);
        attachHoldDrive(btnBackward, CommandProtocol.BACKWARD);
        attachHoldDrive(btnLeft,     CommandProtocol.LEFT);
        attachHoldDrive(btnRight,    CommandProtocol.RIGHT);

        btnStop.setOnClickListener(v -> {
            cancelRepeat();
            setMode(null);
            transmit(CommandProtocol.STOP);
        });

        // Tap to activate, tap again to stop
        btnAvoidance.setOnClickListener(v -> toggleMode('A', CommandProtocol.AVOIDANCE));
        btnTracking.setOnClickListener(v  -> toggleMode('T', CommandProtocol.TRACE));

        setControlsEnabled(false);
    }

    @SuppressLint("ClickableViewAccessibility")
    private void attachHoldDrive(Button btn, String cmd) {
        btn.setOnTouchListener((v, event) -> {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    setMode(null);
                    startRepeat(cmd);
                    break;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    cancelRepeat();
                    transmit(CommandProtocol.STOP);
                    v.performClick();
                    break;
                default:
                    break;
            }
            return true;
        });
    }

    private void toggleMode(char mode, String cmd) {
        if (activeMode != null && activeMode == mode) {
            setMode(null);
            transmit(CommandProtocol.STOP);
        } else {
            cancelRepeat();
            setMode(mode);
            transmit(cmd);
        }
    }

    private void setMode(Character mode) {
        activeMode = mode;
        refreshModeButtons();
    }

    // Full alpha on active mode, dimmed otherwise
    private void refreshModeButtons() {
        btnAvoidance.setAlpha((activeMode != null && activeMode == 'A') ? 1f : 0.50f);
        btnTracking.setAlpha( (activeMode != null && activeMode == 'T') ? 1f : 0.50f);
    }

    private void startRepeat(String cmd) {
        cancelRepeat();
        repeatCmd = cmd;
        handler.post(repeatRunnable);
    }

    private void cancelRepeat() {
        handler.removeCallbacks(repeatRunnable);
        repeatCmd = null;
    }

    private void pickDevice() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                        != PackageManager.PERMISSION_GRANTED) {
            requestBtPermissions();
            return;
        }

        List<BluetoothDevice> paired = new ArrayList<>();
        if (btAdapter != null && btAdapter.getBondedDevices() != null) {
            paired.addAll(btAdapter.getBondedDevices());
        }

        if (paired.isEmpty()) {
            Toast.makeText(this, getString(R.string.toast_no_paired), Toast.LENGTH_LONG).show();
            return;
        }

        String[] names = new String[paired.size()];
        for (int i = 0; i < paired.size(); i++) {
            BluetoothDevice device = paired.get(i);
            names[i] = (device.getName() != null) ? device.getName() : device.getAddress();
        }

        new AlertDialog.Builder(this)
            .setTitle(R.string.dialog_pick_device)
            .setItems(names, (dialog, which) -> doConnect(paired.get(which)))
            .setNegativeButton(R.string.dialog_cancel, null)
            .show();
    }

    // Opens RFCOMM socket on a background thread
    private void doConnect(BluetoothDevice device) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                        != PackageManager.PERMISSION_GRANTED) {
            requestBtPermissions();
            return;
        }

        String deviceName = (device.getName() != null) ? device.getName() : device.getAddress();

        tvStatus.setText(getString(R.string.status_connecting));
        tvStatus.setTextColor(ContextCompat.getColor(this, R.color.status_connecting));
        btnConnect.setEnabled(false);

        new Thread(() -> {
            try {
                BluetoothSocket sock = device.createRfcommSocketToServiceRecord(BT_UUID);
                sock.connect();

                btSocket  = sock;
                outStream = sock.getOutputStream();
                connected = true;

                runOnUiThread(() -> {
                    tvStatus.setText(getString(R.string.status_connected_named, deviceName));
                    tvStatus.setTextColor(ContextCompat.getColor(this, R.color.status_connected));
                    btnConnect.setText(getString(R.string.btn_disconnect));
                    btnConnect.setEnabled(true);
                    setControlsEnabled(true);
                });

            } catch (IOException e) {
                runOnUiThread(() -> {
                    tvStatus.setText(getString(R.string.status_disconnected));
                    tvStatus.setTextColor(ContextCompat.getColor(this, R.color.status_disconnected));
                    btnConnect.setEnabled(true);
                    Toast.makeText(
                        this,
                        getString(R.string.toast_connect_failed) + ": " + e.getMessage(),
                        Toast.LENGTH_SHORT
                    ).show();
                });
            }
        }).start();
    }

    // Stops the car, closes the socket, resets UI
    private void doDisconnect() {
        cancelRepeat();
        transmit(CommandProtocol.STOP);

        try {
            if (outStream != null) outStream.close();
            if (btSocket  != null) btSocket.close();
        } catch (IOException ignored) {}

        outStream  = null;
        btSocket   = null;
        connected  = false;
        activeMode = null;

        tvStatus.setText(getString(R.string.status_disconnected));
        tvStatus.setTextColor(ContextCompat.getColor(this, R.color.status_disconnected));
        btnConnect.setText(getString(R.string.btn_connect));

        setControlsEnabled(false);
        refreshModeButtons();
    }

    // Sends a command to the Arduino over Bluetooth (format: %X#)
    private void transmit(String cmd) {
        if (!connected) return;

        new Thread(() -> {
            try {
                if (outStream != null) {
                    outStream.write(cmd.getBytes(StandardCharsets.US_ASCII));
                    outStream.flush();
                }
            } catch (IOException e) {
                runOnUiThread(() -> {
                    Toast.makeText(this, getString(R.string.toast_lost), Toast.LENGTH_SHORT).show();
                    doDisconnect();
                });
            }
        }).start();
    }

    private void setControlsEnabled(boolean enabled) {
        Button[] controls = {
            btnForward, btnLeft, btnStop, btnRight,
            btnBackward, btnAvoidance, btnTracking
        };
        for (Button btn : controls) {
            btn.setEnabled(enabled);
            btn.setAlpha(enabled ? 1f : 0.35f);
        }
        if (enabled) refreshModeButtons();
    }

    private void requestBtPermissions() {
        // Only BLUETOOTH_CONNECT is a runtime permission (API 31+); the
        // legacy BLUETOOTH/BLUETOOTH_ADMIN permissions are install-time and
        // ACCESS_FINE_LOCATION is only needed for device discovery, which
        // this app does not use.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return;

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(
                this,
                new String[]{Manifest.permission.BLUETOOTH_CONNECT},
                PERM_REQUEST_CODE
            );
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String[] permissions,
                                           int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERM_REQUEST_CODE) {
            boolean allGranted = true;
            for (int result : grantResults) {
                if (result != PackageManager.PERMISSION_GRANTED) {
                    allGranted = false;
                    break;
                }
            }
            if (!allGranted) {
                Toast.makeText(
                    this,
                    "Bluetooth permission is required to control the car.",
                    Toast.LENGTH_LONG
                ).show();
            }
        }
    }
}
