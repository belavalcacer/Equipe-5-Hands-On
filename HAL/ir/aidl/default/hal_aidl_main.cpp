 /*
  * Copyright (C) 2021 The Android Open Source Project
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *      http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */

 #include <aidl/android/hardware/ir/BnConsumerIr.h>
 #include <aidl/android/hardware/ir/ConsumerIrFreqRange.h>
 #include <android-base/logging.h>
 #include <android/binder_interface_utils.h>
 #include <android/binder_manager.h>
 #include <android/binder_process.h>
 #include <fcntl.h>
 #include <unistd.h>
 #include <cstring>
 #include <vector>
 #include <string>

 #include <log/log.h>

 using ::aidl::android::hardware::ir::ConsumerIrFreqRange;

 namespace aidl::android::hardware::ir {

 constexpr uint16_t IR_PROTOCOL_MAGIC = 0x5249U; 
 constexpr uint8_t  IR_PROTOCOL_VERSION = 1U;
 constexpr uint8_t  IR_COMMAND_WRITE = 2U;
 constexpr size_t   IR_MAX_PACKET_SIZE = 1052U;

 inline void write_u16_le(uint8_t* p, uint16_t value) {
     p[0] = static_cast<uint8_t>(value & 0xFF);
     p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
 }

 inline void write_u32_le(uint8_t* p, uint32_t value) {
     p[0] = static_cast<uint8_t>(value & 0xFF);
     p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
     p[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
     p[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
 }

 uint32_t calculate_crc32(const uint8_t* data, size_t len, uint32_t initial_crc = 0xFFFFFFFFU) {
     uint32_t crc = initial_crc;
     for (size_t i = 0; i < len; ++i) {
         crc ^= data[i];
         for (unsigned int bit = 0; bit < 8; ++bit) {
             crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0);
         }
     }
     return crc;
 }

 class ConsumerIr : public BnConsumerIr {
   public:
     ConsumerIr() = default;
   private:
     ::ndk::ScopedAStatus getCarrierFreqs(std::vector<ConsumerIrFreqRange>* _aidl_return) override;
     ::ndk::ScopedAStatus transmit(int32_t in_carrierFreqHz,
                                   const std::vector<int32_t>& in_pattern) override;
 };

 ::ndk::ScopedAStatus ConsumerIr::getCarrierFreqs(std::vector<ConsumerIrFreqRange>* _aidl_return) {
     _aidl_return->clear();
     
     ConsumerIrFreqRange range;
     range.minHz = 20000;
     range.maxHz = 60000;
     _aidl_return->push_back(range);
     
     return ::ndk::ScopedAStatus::ok();
 }

 ::ndk::ScopedAStatus ConsumerIr::transmit(int32_t in_carrierFreqHz,
                                           const std::vector<int32_t>& in_pattern) {
     
     if (in_carrierFreqHz < 20000 || in_carrierFreqHz > 60000) {
         LOG(ERROR) << "HAL IR: Frequencia de portadora nao suportada (" << in_carrierFreqHz << " Hz)";
         return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
     }

     size_t num_intervals = in_pattern.size();
     if (num_intervals == 0) {
         return ::ndk::ScopedAStatus::ok();
     }

        uint16_t symbol_count = (num_intervals + 1) / 2;
        if (symbol_count > 256) {
            LOG(ERROR) << "HAL IR: O sinal excedeu o limite de 256 símbolos RMT (" << symbol_count << ")";
            return ::ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
   
        size_t payload_size = 12 + (symbol_count * 4);
        std::vector<uint8_t> payload(payload_size, 0);
   
        write_u16_le(payload.data() + 0, symbol_count);
        write_u16_le(payload.data() + 2, 0);                
        write_u32_le(payload.data() + 4, 1000000);          
        write_u32_le(payload.data() + 8, in_carrierFreqHz); 
   
        for (uint16_t i = 0; i < symbol_count; ++i) {
            size_t idx0 = i * 2;
            size_t idx1 = i * 2 + 1;
   
            // Fase 0: Sempre MARK (nível óptico 1)
            uint16_t dur0 = static_cast<uint16_t>(in_pattern[idx0]);
            uint16_t phase0 = (1 << 15) | (dur0 & 0x7FFF);
   
            // Fase 1: Sempre SPACE (nível óptico 0)
            uint16_t dur1 = 0;
            if (idx1 < num_intervals) {
                dur1 = static_cast<uint16_t>(in_pattern[idx1]);
            }
            uint16_t phase1 = (0 << 15) | (dur1 & 0x7FFF);
   
            write_u16_le(payload.data() + 12 + (i * 4), phase0);
            write_u16_le(payload.data() + 14 + (i * 4), phase1);
        }
   
        size_t packet_size = 16 + payload_size;
        if (packet_size > IR_MAX_PACKET_SIZE) {
            LOG(ERROR) << "HAL IR: Tamanho do pacote excede o limite físico do driver";
            return ::ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
   
        std::vector<uint8_t> packet(packet_size, 0);
   
        write_u16_le(packet.data() + 0, IR_PROTOCOL_MAGIC);
        packet[2] = IR_PROTOCOL_VERSION;
        packet[3] = IR_COMMAND_WRITE;
        packet[4] = 0;
        packet[5] = 0; 
        
        static uint16_t g_sequence_id = 0;
        uint16_t current_seq = g_sequence_id++;
        write_u16_le(packet.data() + 6, current_seq);
        write_u32_le(packet.data() + 8, static_cast<uint32_t>(payload_size));
   
        std::memcpy(packet.data() + 16, payload.data(), payload_size);
   
        uint32_t crc = calculate_crc32(packet.data(), 12);
        crc = calculate_crc32(packet.data() + 16, payload_size, crc);
        crc ^= 0xFFFFFFFFU;
   
        write_u32_le(packet.data() + 12, crc);
   
        int fd = open("/dev/devtitans_ir0", O_RDWR);
        if (fd < 0) {
            LOG(ERROR) << "HAL IR: Falha ao abrir o dispositivo /dev/devtitans_ir0";
            return ::ndk::ScopedAStatus::fromServiceSpecificError(-1);
        }
   
        ssize_t written = write(fd, packet.data(), packet_size);
        if (written != static_cast<ssize_t>(packet_size)) {
            LOG(ERROR) << "HAL IR: Erro ao enviar pacote completo para o driver.";
            close(fd);
            return ::ndk::ScopedAStatus::fromServiceSpecificError(-2);
        }
   
        std::vector<uint8_t> rx_buffer(IR_MAX_PACKET_SIZE, 0);
        ssize_t bytes_read = read(fd, rx_buffer.data(), rx_buffer.size());
        close(fd);
   
        if (bytes_read < 16) {
            LOG(ERROR) << "HAL IR: ESP32 nao respondeu ou retornou um pacote truncado.";
            return ::ndk::ScopedAStatus::fromServiceSpecificError(-3);
        }
   
        uint16_t rx_magic = rx_buffer[0] | (rx_buffer[1] << 8);
        uint8_t rx_command = rx_buffer[3];
        uint8_t rx_status = rx_buffer[5];
        uint16_t rx_seq = rx_buffer[6] | (rx_buffer[7] << 8);
   
        if (rx_magic != IR_PROTOCOL_MAGIC || rx_command != IR_COMMAND_WRITE || rx_seq != current_seq) {
            LOG(ERROR) << "HAL IR: Resposta corrompida ou incoerente recebida do ESP32.";
            return ::ndk::ScopedAStatus::fromServiceSpecificError(-4);
        }
   
        if (rx_status != 0) { // 0 = IR_STATUS_OK
            LOG(ERROR) << "HAL IR: ESP32 rejeitou o pacote. Erro de status: " << static_cast<int>(rx_status);
            return ::ndk::ScopedAStatus::fromServiceSpecificError(-5);
        }
   
        LOG(INFO) << "HAL IR: Comando IR enviado e transmitido fisicamente pelo ESP32 com sucesso!";
        return ::ndk::ScopedAStatus::ok();
    }
   
    }  
   
    using aidl::android::hardware::ir::ConsumerIr;
   
    int main() {
        auto binder = ::ndk::SharedRefBase::make<ConsumerIr>();
        const std::string name = std::string() + ConsumerIr::descriptor + "/default";
        CHECK_EQ(STATUS_OK, AServiceManager_addService(binder->asBinder().get(), name.c_str()))
                << "Failed to register " << name;
   
        ABinderProcess_setThreadPoolMaxThreadCount(0);
       ABinderProcess_joinThreadPool();
  
       return EXIT_FAILURE;  
    }
