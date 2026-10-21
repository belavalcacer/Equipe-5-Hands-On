package ir.consumerirmanager;

import android.hardware.ir.V1_0.ConsumerIrFreqRange;
import android.hardware.ir.V1_0.IConsumerIr;
import android.os.RemoteException;
import android.util.Log;

import java.util.ArrayList;

public class ConsumerIrManager {
    private static final String TAG = "TitanBeam.ConsumerIrManager";

    private IConsumerIr service;

    private static ConsumerIrManager instance;

    // Construtor. Configura a instância da classe recém-criada.
    private ConsumerIrManager() {
        Log.d(TAG, "Nova (única) instância do ConsumerIrManager ...");
        connect();
    }

    // Acessa a única instância dessa classe. Se ela não existir, cria.
    public static synchronized ConsumerIrManager getInstance() {
        if (instance == null)
            instance = new ConsumerIrManager();

        return instance;
    }

    // Obtém o serviço HAL de infravermelho.
    public boolean connect() {
        Log.d(TAG, "Executando método connect() ...");

        try {
            service = IConsumerIr.getService("default");

            if (service != null) {
                Log.d(TAG, "Serviço Consumer IR acessado com sucesso.");
                return true;
            }

            Log.e(TAG, "Erro ao acessar o serviço Consumer IR!");
            return false;

        } catch (RemoteException | java.util.NoSuchElementException e) {
            Log.e(TAG, "Erro ao conectar ao serviço Consumer IR!", e);
            service = null;
            return false;
        }
    }

    // Transmite um padrão de infravermelho.
    public boolean transmit(int carrierFreq, int[] pattern)
            throws RemoteException {

        if (service == null && !connect()) {
            Log.e(TAG, "Serviço Consumer IR não conectado.");
            return false;
        }

        if (pattern == null || pattern.length == 0) {
            Log.e(TAG, "O padrão IR não pode ser vazio.");
            return false;
        }

        ArrayList<Integer> hidlPattern = new ArrayList<>(pattern.length);

        for (int duration : pattern)
            hidlPattern.add(duration);

        try {
            return service.transmit(carrierFreq, hidlPattern);

        } catch (RemoteException e) {
            Log.e(TAG, "Erro ao transmitir padrão IR.", e);
            service = null;
            throw e;
        }
    }

    // Obtém as frequências disponibilizadas pelo driver através do HAL.
    public ArrayList<ConsumerIrFreqRange> getCarrierFreqs()
            throws RemoteException {

        if (service == null && !connect()) {
            Log.e(TAG, "Serviço Consumer IR não conectado.");
            return new ArrayList<>();
        }

        final ArrayList<ConsumerIrFreqRange> ranges = new ArrayList<>();
        final boolean[] success = {false};

        try {
            service.getCarrierFreqs((result, supportedRanges) -> {
                success[0] = result;

                if (result && supportedRanges != null)
                    ranges.addAll(supportedRanges);
            });

            if (!success[0]) {
                Log.e(TAG, "Falha ao obter as frequências do driver.");
                ranges.clear();
            }

            return ranges;

        } catch (RemoteException e) {
            Log.e(TAG, "Erro ao obter as frequências do driver.", e);
            service = null;
            throw e;
        }
    }
}