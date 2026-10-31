package ir.consumerirmanager;

import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;

import devtitans.ir.IIr;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;

/** System library used by Titan Beam to communicate with the DevTitans IR AIDL HAL. */
public final class ConsumerIrManager {
    private static final String TAG = "TitanBeam.ConsumerIrManager";
    private static final String SERVICE_NAME = IIr.DESCRIPTOR + "/default";
    private static final int DEFAULT_CARRIER_HZ = 38_000;
    private static final int MAX_LEARN_TIMEOUT_MILLIS = 10_000;
    private static final int MAX_SYMBOLS = 256;

    private static ConsumerIrManager instance;
    private IIr service;

    private ConsumerIrManager() { connect(); }

    public static synchronized ConsumerIrManager getInstance() {
        if (instance == null) instance = new ConsumerIrManager();
        return instance;
    }

    /** Connects to the VINTF-declared DevTitans IR AIDL service. */
    public synchronized boolean connect() {
        try {
            IBinder binder = ServiceManager.checkService(SERVICE_NAME);
            service = IIr.Stub.asInterface(binder);
            return service != null;
        } catch (RuntimeException error) {
            Log.e(TAG, "Unable to connect to the IR HAL.", error);
            service = null;
            return false;
        }
    }

    /** Sends a pattern through the DevTitans AIDL HAL. Durations are in microseconds. */
    public synchronized boolean transmit(int carrierFreq, int[] pattern) throws RemoteException {
        if (service == null && !connect()) return false;
        if (pattern == null || pattern.length == 0) return false;
        try {
            return service.transmit(carrierFreq, pattern);
        } catch (RemoteException error) {
            service = null;
            throw error;
        }
    }

    /**
     * Requests one RAW capture through the DevTitans AIDL HAL.
     *
     * @return a decoded signal, or {@code null} when no frame was captured before the timeout.
     */
    public synchronized IrSignal learn(int timeoutMillis) throws IOException {
        if (timeoutMillis <= 0 || timeoutMillis > MAX_LEARN_TIMEOUT_MILLIS) {
            throw new IllegalArgumentException("Learning timeout must be between 1 and 10000 ms.");
        }
        if (service == null && !connect()) {
            throw new IOException("IR HAL is unavailable.");
        }

        int timeoutSeconds = (timeoutMillis + 999) / 1000;
        try {
            byte[] payload = service.read(timeoutSeconds);
            return payload == null || payload.length == 0 ? null : decodeSignal(payload);
        } catch (RemoteException error) {
            service = null;
            throw new IOException("Failed to read an IR signal from the HAL.", error);
        }
    }

    /** Returns the [minimumHz, maximumHz] carrier-frequency range reported by the HAL. */
    public synchronized int[] getCarrierFreqs() throws RemoteException {
        if (service == null && !connect()) return new int[0];
        try {
            return service.getCarrierFreqs();
        } catch (RemoteException error) {
            service = null;
            throw error;
        }
    }

    private static IrSignal decodeSignal(byte[] payload) throws IOException {
        if (payload.length < 12) throw new IOException("Captured IR payload is empty.");
        ByteBuffer data = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN);
        int symbols = data.getShort() & 0xffff;
        int flags = data.getShort() & 0xffff;
        int resolution = data.getInt();
        int carrier = data.getInt();
        if (symbols == 0 || symbols > MAX_SYMBOLS || flags != 0 || resolution != 1_000_000
                || payload.length != 12 + symbols * 4) {
            throw new IOException("Invalid RAW IR payload format.");
        }
        int[] pattern = new int[symbols * 2];
        for (int index = 0; index < pattern.length; index++) {
            pattern[index] = data.getShort() & 0xffff;
        }
        return new IrSignal(carrier == 0 ? DEFAULT_CARRIER_HZ : carrier, pattern);
    }

    /** Immutable RAW infrared code returned by {@link #learn(int)}. */
    public static final class IrSignal {
        private final int carrierFrequencyHz;
        private final int[] pattern;

        IrSignal(int carrierFrequencyHz, int[] pattern) {
            this.carrierFrequencyHz = carrierFrequencyHz;
            this.pattern = Arrays.copyOf(pattern, pattern.length);
        }

        public int getCarrierFrequencyHz() { return carrierFrequencyHz; }
        public int[] getPattern() { return Arrays.copyOf(pattern, pattern.length); }
    }
}
