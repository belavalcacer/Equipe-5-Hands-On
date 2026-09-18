#define LOG_TAG "android.hardware.ir@1.0-service"

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>
#include "ConsumerIr.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::ir::V1_0::IConsumerIr;
using android::hardware::ir::V1_0::implementation::ConsumerIr;

int main() {
    // Configura 1 thread dedicada para ouvir os comandos do App
    configureRpcThreadpool(1, true /* callerWillJoin */);

    // Cria a instância da sua classe (que vai abrir o /dev/devtitans_ir0)
    android::sp<IConsumerIr> ir = new ConsumerIr();

    // Registra o serviço no Android com o nome "default"
    if (ir->registerAsService("default") != android::OK) {
        LOG(ERROR) << "Falha ao registrar o servico da HAL de IR.";
        return 1;
    }

    LOG(INFO) << "Servico da HAL de IR (Equipe 5) iniciado com sucesso.";
    
    // Mantém o processo vivo rodando em background
    joinRpcThreadpool();
    
    return 1; // O código nunca deve passar daqui
}