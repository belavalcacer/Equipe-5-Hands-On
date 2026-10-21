package com.equipe5.acremote.data

import com.equipe5.acremote.ui.model.StoredIrSignal

/** Reflection keeps the debug APK buildable; the manager is supplied by the Titan Beam system image. */
class ConsumerIrGateway {
    fun learn(timeoutMillis: Int): Result<StoredIrSignal> = runCatching {
        val manager = managerInstance()
        val signal = manager.javaClass.getMethod("learn", Int::class.javaPrimitiveType).invoke(manager, timeoutMillis)
        val carrier = signal.javaClass.getMethod("getCarrierFrequencyHz").invoke(signal) as Int
        val pattern = (signal.javaClass.getMethod("getPattern").invoke(signal) as IntArray).toList()
        StoredIrSignal(carrier, pattern)
    }

    fun transmit(signal: StoredIrSignal): Result<Unit> = runCatching {
        val manager = managerInstance()
        val success = manager.javaClass.getMethod("transmit", Int::class.javaPrimitiveType, IntArray::class.java)
            .invoke(manager, signal.carrierFrequencyHz, signal.pattern.toIntArray()) as Boolean
        check(success) { "O serviço IR não conseguiu transmitir o comando." }
    }

    private fun managerInstance(): Any {
        val type = Class.forName("ir.consumerirmanager.ConsumerIrManager")
        return type.getMethod("getInstance").invoke(null)
    }
}
