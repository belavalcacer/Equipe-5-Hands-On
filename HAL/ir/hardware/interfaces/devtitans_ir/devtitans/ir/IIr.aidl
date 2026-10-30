/*
 * Copyright (C) 2024 DevTITANS
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

package devtitans.ir;

@VintfStability
interface IIr {
    /**
     * Checks if the IR hardware is responding via ping command.
     *
     * @return true if ping was successful and device responded with status OK.
     */
    boolean ping();

    /**
     * Gets the current carrier frequency configured in the driver (in Hz).
     *
     * @return carrier frequency in Hz (e.g. 38000).
     */
    int getCarrier();

    /**
     * Sets the carrier frequency in the driver sysfs attribute (in Hz).
     *
     * @param carrierHz Carrier frequency (between 20000 and 60000 Hz).
     * @return true if carrier frequency was successfully set.
     */
    boolean setCarrier(in int carrierHz);

    /**
     * Returns the supported carrier frequency range [minHz, maxHz].
     *
     * @return Array containing the minimum and maximum supported frequencies in Hz.
     */
    int[] getCarrierFreqs();

    /**
     * Captures an IR signal from the receiver.
     * Initiates capture on the device, waits for a valid frame up to timeoutSeconds.
     *
     * @param timeoutSeconds Maximum time in seconds to wait for a capture.
     * @return Raw IR payload bytes captured, or empty array if timed out / no data.
     */
    byte[] read(in int timeoutSeconds);

    /**
     * Transmits a raw IR payload directly to the device.
     *
     * @param payload Raw IR payload bytes matching protocol v1.
     * @return true if transmission was accepted and executed by the device.
     */
    boolean write(in byte[] payload);

    /**
     * Transmits an IR pattern specified as alternating mark/space durations in microseconds.
     * Encodes the pattern into protocol v1 RAW symbols and sends it to the device.
     *
     * @param carrierFreqHz Carrier frequency in Hz (e.g., 38000). If <= 0, uses driver default.
     * @param pattern Alternating series of on (mark) and off (space) periods in microseconds.
     * @return true if transmission succeeded.
     */
    boolean transmit(in int carrierFreqHz, in int[] pattern);
}
