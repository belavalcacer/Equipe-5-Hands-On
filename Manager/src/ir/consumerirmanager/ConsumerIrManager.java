package ir.consumerirmanager;

import android.hardware.ir.V1_0.ConsumerIrFreqRange;
import android.hardware.ir.V1_0.IConsumerIr;
import android.os.RemoteException;
import android.util.Log;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.zip.CRC32;

/** System library used by Titan Beam to communicate with the IR HAL and receiver. */
public final class ConsumerIrManager {
    private static final String TAG = "TitanBeam.ConsumerIrManager";
    private static final String DEVICE_PATH = "/dev/devtitans_ir0";
    private static final int HEADER_SIZE = 16;
    private static final int MAX_PAYLOAD_SIZE = 1036;
    private static final byte VERSION = 1;
    private static final byte COMMAND_READ = 0x01;
    private static final byte RESPONSE_FLAG = 0x01;
    private static final byte STATUS_OK = 0x00;
    private static final int DEFAULT_CARRIER_HZ = 38_000;

    private static ConsumerIrManager instance;
    private final AtomicInteger sequence = new AtomicInteger();
    private IConsumerIr service;

    private ConsumerIrManager() { connect(); }

    public static synchronized ConsumerIrManager getInstance() {
        if (instance == null) instance = new ConsumerIrManager();
        return instance;
    }

    public synchronized boolean connect() {
        try {
            service = IConsumerIr.getService("default");
            return service != null;
        } catch (RemoteException | java.util.NoSuchElementException error) {
            Log.e(TAG, "Não foi possível conectar ao HAL de IR.", error);
            service = null;
            return false;
        }
    }

    /** Sends a pattern through the Consumer IR HAL. Durations are in microseconds. */
    public synchronized boolean transmit(int carrierFreq, int[] pattern) throws RemoteException {
        if (service == null && !connect()) return false;
        if (pattern == null || pattern.length == 0) return false;
        ArrayList<Integer> hidlPattern = new ArrayList<>(pattern.length);
        for (int duration : pattern) hidlPattern.add(duration);
        try {
            return service.transmit(carrierFreq, hidlPattern);
        } catch (RemoteException error) {
            service = null;
            throw error;
        }
    }

    /**
     * Requests one RAW capture from the receiver. The firmware owns the capture timeout;
     * this parameter is validated to keep the app and service contract explicit.
     */
    public synchronized IrSignal learn(int timeoutMillis) throws IOException {
        if (timeoutMillis <= 0 || timeoutMillis > 10_000) {
            throw new IllegalArgumentException("O timeout de aprendizado deve ser entre 1 e 10000 ms.");
        }
        int requestSequence = nextSequence();
        byte[] request = buildPacket(COMMAND_READ, (byte) 0, (byte) 0, requestSequence, new byte[0]);

        try (RandomAccessFile device = new RandomAccessFile(DEVICE_PATH, "rw")) {
            device.write(request);
            byte[] header = new byte[HEADER_SIZE];
            device.readFully(header);
            Header response = parseHeader(header);
            if (response.command != COMMAND_READ || (response.flags & RESPONSE_FLAG) == 0 || response.sequence != requestSequence) {
                throw new IOException("Resposta IR inválida.");
            }
            if (response.payloadLength < 0 || response.payloadLength > MAX_PAYLOAD_SIZE) {
                throw new IOException("Tamanho de resposta IR inválido.");
            }
            byte[] payload = new byte[response.payloadLength];
            device.readFully(payload);
            verifyChecksum(header, payload, response.checksum);
            if (response.status != STATUS_OK) {
                throw new IOException("A captura IR falhou (status " + (response.status & 0xff) + ").");
            }
            return decodeSignal(payload);
        }
    }

    public synchronized ArrayList<ConsumerIrFreqRange> getCarrierFreqs() throws RemoteException {
        if (service == null && !connect()) return new ArrayList<>();
        ArrayList<ConsumerIrFreqRange> ranges = new ArrayList<>();
        final boolean[] success = {false};
        try {
            service.getCarrierFreqs((result, supportedRanges) -> {
                success[0] = result;
                if (result && supportedRanges != null) ranges.addAll(supportedRanges);
            });
            if (!success[0]) ranges.clear();
            return ranges;
        } catch (RemoteException error) {
            service = null;
            throw error;
        }
    }

    private int nextSequence() { return sequence.updateAndGet(value -> (value + 1) & 0xffff); }

    private static byte[] buildPacket(byte command, byte flags, byte status, int sequence, byte[] payload) {
        ByteBuffer packet = ByteBuffer.allocate(HEADER_SIZE + payload.length).order(ByteOrder.LITTLE_ENDIAN);
        packet.put((byte) 'I').put((byte) 'R').put(VERSION).put(command).put(flags).put(status);
        packet.putShort((short) sequence).putInt(payload.length).putInt(0).put(payload);
        byte[] bytes = packet.array();
        ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).putInt(12, checksum(bytes, payload.length));
        return bytes;
    }

    private static Header parseHeader(byte[] header) throws IOException {
        ByteBuffer data = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN);
        if (data.get() != 'I' || data.get() != 'R' || data.get() != VERSION) throw new IOException("Cabeçalho IR inválido.");
        return new Header(data.get(), data.get(), data.get(), data.get() & 0xffff, data.getInt(), data.getInt());
    }

    private static void verifyChecksum(byte[] header, byte[] payload, int expected) throws IOException {
        byte[] packet = Arrays.copyOf(header, HEADER_SIZE + payload.length);
        System.arraycopy(payload, 0, packet, HEADER_SIZE, payload.length);
        if (checksum(packet, payload.length) != expected) throw new IOException("CRC da resposta IR inválido.");
    }

    private static int checksum(byte[] packet, int payloadLength) {
        CRC32 crc = new CRC32();
        crc.update(packet, 0, 12);
        crc.update(packet, HEADER_SIZE, payloadLength);
        return (int) crc.getValue();
    }

    private static IrSignal decodeSignal(byte[] payload) throws IOException {
        if (payload.length < 12) throw new IOException("Captura IR vazia.");
        ByteBuffer data = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN);
        int symbols = data.getShort() & 0xffff;
        int flags = data.getShort() & 0xffff;
        int resolution = data.getInt();
        int carrier = data.getInt();
        if (symbols == 0 || flags != 0 || resolution != 1_000_000 || payload.length != 12 + symbols * 4) {
            throw new IOException("Formato RAW de IR inválido.");
        }
        int[] pattern = new int[symbols * 2];
        for (int index = 0; index < pattern.length; index++) {
            pattern[index] = data.getShort() & 0xffff;
        }
        return new IrSignal(carrier == 0 ? DEFAULT_CARRIER_HZ : carrier, pattern);
    }

    private static final class Header {
        final byte command, flags, status;
        final int sequence, payloadLength, checksum;
        Header(byte command, byte flags, byte status, int sequence, int payloadLength, int checksum) {
            this.command = command; this.flags = flags; this.status = status;
            this.sequence = sequence; this.payloadLength = payloadLength; this.checksum = checksum;
        }
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
