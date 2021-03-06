#include "MeasurementProvider.h"

#define SHT21_TRIGGER_TEMP_MEASURE_NOHOLD  0xF3
#define SHT21_TRIGGER_HUMD_MEASURE_NOHOLD  0xF5
#define SHT21_TRIGGER_TEMP_MEASURE_HOLD  0xE3
#define SHT21_TRIGGER_HUMD_MEASURE_HOLD  0xE5

MeasurementProvider::MeasurementProvider(
    uint8_t tempSensAddr,  
    uint8_t lightSensAddr, 
    float analogVCCToRealCoeficient
): tempSensAddress(tempSensAddr), 
   lightSensor(lightSensAddr), 
   analogVCCToRealCoeficient(analogVCCToRealCoeficient) 
     { }

bool MeasurementProvider::begin() {
    bool lsStatus = lightSensor.begin(BH1750::ONE_TIME_HIGH_RES_MODE); //goes to sleep after measurement
    #ifdef USE_BMP280
    bool bmpStatus = bmp.begin(0x76);
    return lsStatus && bmpStatus;
    #else
    return lsStatus;
    #endif
}

const MeasurementsData& MeasurementProvider::getCurrentMeasurements(){
    return data;
}

bool MeasurementProvider::doMeasurements() {
    //measure battery voltage
    data.voltageRaw = analogRead(A0);
    data.voltage = analogToVoltage(data.voltageRaw);

    #ifdef USE_SHT30
    //SHT-30 measure temp and humidity
    byte tempMeasureRes;
    int retryMeasurement = 3;
    do {
        retryMeasurement--;
        tempMeasureRes = measureTempSTH30();
        if (tempMeasureRes != 0) {
            Serial.println(F("Unable to measure temperature"));
            Serial.println(tempMeasureRes);
            delay(100);
        }
    } while (tempMeasureRes != 0 && retryMeasurement > 0);

    if (tempMeasureRes != 0) {
        return false;
    }
    #endif

    #ifdef USE_SHT21
    //SHT21
    data.temperature = 0;
    data.humidity = 0;
    measureTempSTH21();
    #endif

    #ifdef USE_BMP280
    // measur pressure
    data.bmpTemp = bmp.readTemperature();
    data.pressure = bmp.readPressure();
    #else
    data.bmpTemp = 0;
    data.pressure = 0;
    #endif
    
    // measure lipht
    data.lightLevel = lightSensor.readLightLevel();
    return true;
}

#ifdef USE_SHT30
byte MeasurementProvider::measureTempSTH30() {
    unsigned int data[6];
    Wire.beginTransmission(tempSensAddress);
    // measurement command -> one shot measurement, clock stretching, high repeatability
    Wire.write(0x2C); 
    Wire.write(0x06);
    uint8_t wireResult = Wire.endTransmission();
    if (wireResult != 0)  {
        return wireResult;
    }
    delay(500);
    // Request 6 bytes of data
    Wire.requestFrom(tempSensAddress, 6);
    // cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
    for (int i=0;i<6;i++) {
        data[i]=Wire.read();
    };
    delay(50);

    if (Wire.available() != 0) {
        return 20;
    }
    // Convert the data
    this->data.temperature = ((((data[0] * 256.0) + data[1]) * 175) / 65535.0) - 45;
    this->data.humidity = ((((data[3] * 256.0) + data[4]) * 100) / 65535.0);

    return 0;
}
#endif

#ifdef USE_SHT21
uint8_t MeasurementProvider::measureTempSTH21() {
    // Convert the data
    this->data.humidity = (-6.0 + 125.0 / 65536.0 * readFloatSHT21(SHT21_TRIGGER_HUMD_MEASURE_HOLD));
    this->data.temperature = (-46.85 + 175.72 / 65536.0 * readFloatSHT21(SHT21_TRIGGER_TEMP_MEASURE_HOLD));
    return 0;
}


float MeasurementProvider::readFloatSHT21(uint8_t command)
{
    uint16_t result;

    Wire.beginTransmission(tempSensAddress);
    Wire.write(command);
    Wire.endTransmission();
	delay(100);

    Wire.requestFrom(tempSensAddress, 3);
    while(Wire.available() < 3) {
      delay(10);
    }

    // return result
    result = ((Wire.read()) << 8);
    result += Wire.read();
	result &= ~0x0003;   // clear two low bits (status bits)
    return (float)result;
}
#endif

float MeasurementProvider::analogToVoltage(int analog) {
    return analog * analogVCCToRealCoeficient;
}

void MeasurementsData::printToSerial() const {
    Serial.print(F("temp: "));
    Serial.print(temperature);
    Serial.print(F(" ("));
    Serial.print(bmpTemp);
    Serial.print(F(") "));
    Serial.print(F(", hum: "));
    Serial.print(humidity);
    Serial.print(F(", press: "));
    Serial.print(pressure);
    Serial.print(F(", vcc: "));
    Serial.print(voltage);
    Serial.print(F(", vcc raw: "));
    Serial.print(voltageRaw);
    Serial.print(F(", light: "));
    Serial.print(lightLevel);
}
