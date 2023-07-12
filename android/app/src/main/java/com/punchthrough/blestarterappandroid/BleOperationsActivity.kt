package com.punchthrough.blestarterappandroid


import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGattCharacteristic
import android.os.Bundle
import android.view.KeyEvent
import android.view.MenuItem
import android.widget.EditText
import androidx.appcompat.app.AppCompatActivity
import com.punchthrough.blestarterappandroid.ble.ConnectionEventListener
import com.punchthrough.blestarterappandroid.ble.ConnectionManager
import com.punchthrough.blestarterappandroid.ble.toHexString
import kotlinx.android.synthetic.main.activity_ble_operations.output_water_level
import kotlinx.android.synthetic.main.activity_ble_operations.input_set_point
import kotlinx.android.synthetic.main.activity_ble_operations.input_tank_height
import kotlinx.android.synthetic.main.activity_ble_operations.input_threshold
import kotlinx.android.synthetic.main.activity_ble_operations.input_trigger_time

import org.jetbrains.anko.alert

class BleOperationsActivity : AppCompatActivity() {

    private lateinit var device: BluetoothDevice

    private val characteristics by lazy {
        ConnectionManager.servicesOnDevice(device)?.flatMap { service ->
            service.characteristics ?: listOf()
        } ?: listOf()
    }

    private val waterLevel by lazy {
        characteristics.find { it.uuid.toString() == "8dd6a1b7-bc75-4741-8a26-264af75807de" }!!
    }

    private val setPoint by lazy {
        characteristics.find { it.uuid.toString() == "0a924ca7-87cd-4699-a3bd-abdcd9cf126a" }!!
    }

    private val tankHeight by lazy {
        characteristics.find { it.uuid.toString() == "63718360-318a-4805-99a5-b7bd2593e30f" }!!
    }

    private val triggerTime by lazy {
        characteristics.find { it.uuid.toString() == "008c2ddc-c6db-4a7f-a51c-90824bf67a56" }!!
    }

    private val threshold by lazy {
        characteristics.find { it.uuid.toString() == "4aae2aa4-bae8-46fc-8a09-d3aec4a126d4" }!!
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        ConnectionManager.registerListener(connectionEventListener)
        super.onCreate(savedInstanceState)
        device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
            ?: error("Missing BluetoothDevice from MainActivity!")

        setContentView(R.layout.activity_ble_operations)
        supportActionBar?.apply {
            setDisplayHomeAsUpEnabled(true)
            setDisplayShowTitleEnabled(true)
            title = getString(R.string.ble_playground)
        }

        /* read each characteristic for the first time */
        ConnectionManager.readCharacteristic(device, setPoint)
        ConnectionManager.readCharacteristic(device, tankHeight)
        ConnectionManager.readCharacteristic(device, triggerTime)
        ConnectionManager.readCharacteristic(device, threshold)
        /* subscribe to the water level characteristic */
        ConnectionManager.enableNotifications(device, waterLevel)
        /* set the write listeners on input views */
        setOnEnter(input_set_point, setPoint) { it.toDouble()*10 }
        setOnEnter(input_tank_height, tankHeight) { it.toDouble()*10 }
        setOnEnter(input_trigger_time, triggerTime) { it.toDouble() }
        setOnEnter(input_threshold, threshold) { it.toDouble()*10 }

    }

    private fun setOnEnter(inputField:EditText,  characteristic: BluetoothGattCharacteristic, preprocess:(s:String)->Double){
        inputField.setOnKeyListener { _, keyCode, event ->

            if (keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_UP) {

                val value = (preprocess(inputField.text.toString())).toInt()

                ConnectionManager.writeCharacteristic(
                    device,
                    characteristic,
                    byteArrayOf(value.toByte())
                )

                return@setOnKeyListener true
            }

            return@setOnKeyListener false
        }
    }

    override fun onDestroy() {
        ConnectionManager.unregisterListener(connectionEventListener)
        ConnectionManager.teardownConnection(device)
        super.onDestroy()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            android.R.id.home -> {
                onBackPressed()
                return true
            }
        }
        return super.onOptionsItemSelected(item)
    }

    @SuppressLint("SetTextI18n")
    private fun log(message: String) {

    }

    private val connectionEventListener by lazy {
        ConnectionEventListener().apply {
            onDisconnect = {
                runOnUiThread {
                    alert {
                        title = "Disconnected"
                        message = "Disconnected from device."
                        positiveButton("OK") { onBackPressed() }
                    }.show()
                }
            }

            onCharacteristicRead = { _, characteristic ->

                runOnUiThread{
                    when(characteristic){
                        setPoint->{
                            input_set_point.setText((characteristic.value[0]/10).toString())
                        }
                        tankHeight -> {
                            input_tank_height.setText((characteristic.value[0]/10).toString())
                        }
                        triggerTime->{
                            input_trigger_time.setText(characteristic.value[0].toString())
                        }
                        threshold ->{
                            input_threshold.setText((characteristic.value[0]/10).toString())
                        }
                    }
                }

                log("Read from ${characteristic.uuid}: ${characteristic.value.toHexString()}")
            }

            onCharacteristicWrite = { _, characteristic ->
                log("Wrote to ${characteristic.uuid}")
            }

            onMtuChanged = { _, mtu ->
                log("MTU updated to $mtu")
            }

            onCharacteristicChanged = { _, characteristic ->
                runOnUiThread {
                    when (characteristic){
                        waterLevel -> {
                            val v = characteristic.value[0]/10.0
                            output_water_level.setText(v.toString())
                        }
                    }
                }
            }

            onNotificationsEnabled = { _, characteristic ->
                log("Enabled notifications on ${characteristic.uuid}")
            }

            onNotificationsDisabled = { _, characteristic ->
                log("Disabled notifications on ${characteristic.uuid}")
            }
        }
    }

}
