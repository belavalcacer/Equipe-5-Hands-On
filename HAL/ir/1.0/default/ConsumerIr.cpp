#include "ConsumerIr.h"
#include <android-base/logging.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>

namespace android {
namespace hardware {
namespace ir {
namespace V1_0 {
namespace implementation {

ConsumerIr::ConsumerIr() {}

Return<bool> ConsumerIr::transmit(int32_t carrierFreq, const hidl_vec<int32_t>& pattern) {
    LOG(INFO) << "HAL IR (Equipe 5): Iniciando transmissao a " << carrierFreq << " Hz";

    // =========================================================
    // PASSO 1: Configurar a frequência (Carrier) no Sysfs
    // =========================================================
    int fd_carrier = open("/sys/class/devtitans_ir/devtitans_ir0/device/carrier", O_WRONLY);
    if (fd_carrier >= 0) {
        std::string freq_str = std::to_string(carrierFreq) + "\n";
        write(fd_carrier, freq_str.c_str(), freq_str.length());
        close(fd_carrier);
    } else {
        LOG(WARNING) << "HAL IR: Nao consegui abrir o sysfs do carrier. Tentando via dev principal...";
    }

    // =========================================================
    // PASSO 2: Montar o pacote (Protocolo da Equipe 5)
    // =========================================================
    size_t payload_size = pattern.size() * sizeof(int32_t);
    size_t packet_size = 16 + payload_size; // 16 bytes do header + os dados

    // O driver limita o pacote a 1052 bytes (IR_MAX_PACKET_SIZE)
    if (packet_size > 1052) {
        LOG(ERROR) << "HAL IR: O comando IR eh muito grande para o buffer do driver!";
        return false;
    }

    // Cria um vetor preenchido com zeros
    std::vector<uint8_t> packet(packet_size, 0);

    // Preenche o cabeçalho (Header) exigido pelo driver ir.c
    packet[0] = 0x49; // Magic 'I'
    packet[1] = 0x52; // Magic 'R'
    packet[2] = 0x01; // Versão (IR_PROTOCOL_VERSION)
    packet[3] = 0x02; // Comando de envio (IR_CMD_WRITE)

    // Copia os dados reais do Android logo após os 16 bytes de cabeçalho
    memcpy(packet.data() + 16, pattern.data(), payload_size);

    // =========================================================
    // PASSO 3: Enviar o pacote completo para o Kernel via /dev
    // =========================================================
    int fd = open("/dev/devtitans_ir0", O_WRONLY); 
    if (fd < 0) {
        LOG(ERROR) << "HAL IR: Falha ao abrir o character device /dev/devtitans_ir0";
        return false;
    }

    // Escreve os dados no driver (isso aciona a função ir_write do Kernel)
    ssize_t written = write(fd, packet.data(), packet_size);
    close(fd);

    if (written != packet_size) {
        LOG(ERROR) << "HAL IR: Erro, bytes incompletos escritos no driver.";
        return false;
    }

    LOG(INFO) << "HAL IR: Comando transmitido com sucesso pro driver USB!";
    return true;
}

Return<void> ConsumerIr::getCarrierFreqs(getCarrierFreqs_cb _hidl_cb) {
    // Definindo a frequencia padrao do hardware IR (geralmente 38kHz)
    std::vector<ConsumerIrFreqRange> ranges = {
        {.min = 38000, .max = 38000} 
    };

    hidl_vec<ConsumerIrFreqRange> hidlRanges;
    hidlRanges.setToExternal(ranges.data(), ranges.size());

    _hidl_cb(true, hidlRanges);
    return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace ir
}  // namespace hardware
}  // namespace android