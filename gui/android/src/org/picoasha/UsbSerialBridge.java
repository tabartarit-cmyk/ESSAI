package org.picoasha;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.util.Log;

import java.util.Arrays;

/**
 * Liaison série USB (CDC-ACM) avec la Pico, utilisée par la partie C++ (seriallink.cpp).
 * Toutes les méthodes sont statiques : il n'y a qu'une Pico à la fois.
 */
public class UsbSerialBridge {
    private static final String TAG = "PicoASHA-USB";
    private static final String ACTION_PERMISSION = "org.picoasha.USB_PERMISSION";
    private static final int VENDOR_ID = 0xCAFE;
    private static final int PRODUCT_ID = 0x4011;

    // Implémentées en C++ (seriallink.cpp)
    public static native void nativeOnData(byte[] data, int len);
    public static native void nativeOnClosed();

    private static UsbManager sManager;
    private static UsbDevice sDevice;
    private static UsbDeviceConnection sConnection;
    private static UsbInterface sControlIf;
    private static UsbInterface sDataIf;
    private static UsbEndpoint sIn;
    private static UsbEndpoint sOut;
    private static volatile boolean sRunning;
    private static boolean sReceiverRegistered;
    private static String sAskedDeviceName;   // l'autorisation n'est demandée qu'une fois par branchement

    private static final BroadcastReceiver sReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(intent.getAction())) {
                UsbDevice d = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if (d != null && d.getVendorId() == VENDOR_ID && d.getProductId() == PRODUCT_ID) {
                    sAskedDeviceName = null;
                    closeInternal(true);
                }
            }
        }
    };

    /** 1 = ouvert, 0 = pas de Pico ou autorisation en attente, -1 = erreur. */
    public static synchronized int open(Context ctx) {
        try {
            if (sConnection != null) {
                return 1;
            }
            if (sManager == null) {
                sManager = (UsbManager) ctx.getSystemService(Context.USB_SERVICE);
            }
            if (!sReceiverRegistered) {
                IntentFilter filter = new IntentFilter(ACTION_PERMISSION);
                filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
                if (Build.VERSION.SDK_INT >= 33) {
                    ctx.registerReceiver(sReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
                } else {
                    ctx.registerReceiver(sReceiver, filter);
                }
                sReceiverRegistered = true;
            }

            UsbDevice found = null;
            for (UsbDevice d : sManager.getDeviceList().values()) {
                if (d.getVendorId() == VENDOR_ID && d.getProductId() == PRODUCT_ID) {
                    found = d;
                    break;
                }
            }
            if (found == null) {
                sAskedDeviceName = null;
                return 0;
            }

            if (!sManager.hasPermission(found)) {
                if (!found.getDeviceName().equals(sAskedDeviceName)) {
                    sAskedDeviceName = found.getDeviceName();
                    int flags = Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0;
                    Intent intent = new Intent(ACTION_PERMISSION).setPackage(ctx.getPackageName());
                    PendingIntent pi = PendingIntent.getBroadcast(ctx, 0, intent, flags);
                    sManager.requestPermission(found, pi);
                }
                return 0;   // le prochain essai aura lieu une fois l'autorisation donnée
            }

            // Interfaces CDC : contrôle (classe 2) et données (classe 10, deux points d'accès « bulk »)
            UsbInterface ctrl = null;
            UsbInterface data = null;
            UsbEndpoint in = null;
            UsbEndpoint out = null;
            for (int i = 0; i < found.getInterfaceCount(); i++) {
                UsbInterface itf = found.getInterface(i);
                if (itf.getInterfaceClass() == UsbConstants.USB_CLASS_COMM && ctrl == null) {
                    ctrl = itf;
                } else if (itf.getInterfaceClass() == UsbConstants.USB_CLASS_CDC_DATA && data == null) {
                    UsbEndpoint ei = null;
                    UsbEndpoint eo = null;
                    for (int e = 0; e < itf.getEndpointCount(); e++) {
                        UsbEndpoint ep = itf.getEndpoint(e);
                        if (ep.getType() == UsbConstants.USB_ENDPOINT_XFER_BULK) {
                            if (ep.getDirection() == UsbConstants.USB_DIR_IN) {
                                ei = ep;
                            } else {
                                eo = ep;
                            }
                        }
                    }
                    if (ei != null && eo != null) {
                        data = itf;
                        in = ei;
                        out = eo;
                    }
                }
            }
            if (ctrl == null || data == null) {
                Log.e(TAG, "CDC interfaces not found");
                return -1;
            }

            UsbDeviceConnection conn = sManager.openDevice(found);
            if (conn == null) {
                Log.e(TAG, "openDevice failed");
                return -1;
            }
            conn.claimInterface(ctrl, true);
            conn.claimInterface(data, true);

            // 115200 8N1 puis DTR + RTS : la Pico considère alors la liaison série « connectée »
            byte[] lineCoding = {(byte) 0x00, (byte) 0xC2, (byte) 0x01, (byte) 0x00, 0x00, 0x00, 0x08};
            conn.controlTransfer(0x21, 0x20, 0, ctrl.getId(), lineCoding, lineCoding.length, 1000);
            conn.controlTransfer(0x21, 0x22, 0x03, ctrl.getId(), null, 0, 1000);

            sDevice = found;
            sConnection = conn;
            sControlIf = ctrl;
            sDataIf = data;
            sIn = in;
            sOut = out;
            sRunning = true;

            final UsbDeviceConnection readConn = conn;
            final UsbEndpoint readEp = in;
            final String name = found.getDeviceName();
            Thread t = new Thread(new Runnable() {
                @Override
                public void run() {
                    readLoop(readConn, readEp, name);
                }
            }, "PicoASHA-USB-read");
            t.setDaemon(true);
            t.start();
            return 1;
        } catch (Exception e) {
            Log.e(TAG, "open failed", e);
            return -1;
        }
    }

    private static void readLoop(UsbDeviceConnection conn, UsbEndpoint in, String deviceName) {
        byte[] buf = new byte[4096];
        int misses = 0;
        while (sRunning) {
            int n = conn.bulkTransfer(in, buf, buf.length, 100);
            if (n > 0) {
                misses = 0;
                nativeOnData(Arrays.copyOf(buf, n), n);
            } else if (n < 0) {
                // -1 = délai dépassé (normal quand la Pico n'envoie rien) ou erreur :
                // toutes les ~2 s, on vérifie que le câble est toujours là
                if (++misses >= 20) {
                    misses = 0;
                    if (sManager == null || !sManager.getDeviceList().containsKey(deviceName)) {
                        closeInternal(true);
                        return;
                    }
                }
            }
        }
    }

    /** Renvoie le nombre d'octets écrits, ou -1. */
    public static int write(byte[] data, int len) {
        UsbDeviceConnection conn;
        UsbEndpoint out;
        synchronized (UsbSerialBridge.class) {
            conn = sConnection;
            out = sOut;
        }
        if (conn == null || out == null) {
            return -1;
        }
        int off = 0;
        while (off < len) {
            int chunk = Math.min(len - off, 4096);
            int n = conn.bulkTransfer(out, data, off, chunk, 1000);
            if (n < 0) {
                return -1;
            }
            off += n;
        }
        return len;
    }

    public static void close() {
        closeInternal(false);
    }

    private static synchronized void closeInternal(boolean notify) {
        if (sConnection == null) {
            return;
        }
        sRunning = false;
        try {
            sConnection.controlTransfer(0x21, 0x22, 0x00, sControlIf.getId(), null, 0, 200);   // DTR/RTS off
        } catch (Exception ignored) {
        }
        try {
            sConnection.releaseInterface(sDataIf);
            sConnection.releaseInterface(sControlIf);
        } catch (Exception ignored) {
        }
        sConnection.close();
        sConnection = null;
        sDevice = null;
        sIn = null;
        sOut = null;
        if (notify) {
            nativeOnClosed();
        }
    }

    /** Relance l'application (utilisé après un changement de langue). */
    public static void restartApp(Context ctx) {
        Intent launch = ctx.getPackageManager().getLaunchIntentForPackage(ctx.getPackageName());
        if (launch == null) {
            return;
        }
        launch.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        PendingIntent pi = PendingIntent.getActivity(ctx, 0, launch,
                PendingIntent.FLAG_CANCEL_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        AlarmManager am = (AlarmManager) ctx.getSystemService(Context.ALARM_SERVICE);
        am.set(AlarmManager.RTC, System.currentTimeMillis() + 300, pi);
        System.exit(0);
    }
}
