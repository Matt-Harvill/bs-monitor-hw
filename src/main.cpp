#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#define MIC_PIN 34
#define SD_CS 5 // Chip select pin for SD card on ESP32-S2 Thing Plus C

// WAV file parameters
const int SAMPLE_RATE = 8000; // 8kHz sample rate
const int SAMPLE_BITS = 16;   // 16-bit samples
const int RECORD_TIME = 10;   // Record for 10 seconds
const int BUFFER_SIZE = 512;  // Buffer size for writing

void recordWAV();

// WAV header structure
struct WAVHeader
{
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1; // PCM
    uint16_t numChannels = 1; // Mono
    uint32_t sampleRate = SAMPLE_RATE;
    uint32_t byteRate = SAMPLE_RATE * SAMPLE_BITS / 8;
    uint16_t blockAlign = SAMPLE_BITS / 8;
    uint16_t bitsPerSample = SAMPLE_BITS;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize;
};

void setup()
{
    Serial.begin(115200);
    // Wait for serial with timeout
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime < 5000))
    {
        delay(100);
    }
    Serial.println("WAV Recorder Started");

    // Initialize analog reading
    analogReadResolution(12); // 0–4095

    // Initialize SD card
    if (!SD.begin(SD_CS))
    {
        Serial.println("SD card initialization failed!");
        while (1)
            ;
    }
    Serial.println("SD card initialized successfully");

    // Record audio
    recordWAV();
}

void loop()
{
    // Nothing to do in loop
    delay(1000);
}

void recordWAV()
{
    // Create filename with timestamp
    String filename = "/recording_" + String(millis()) + ".wav";

    File audioFile = SD.open(filename, FILE_WRITE);
    if (!audioFile)
    {
        Serial.println("Failed to create audio file");
        return;
    }

    Serial.println("Recording started...");

    // Calculate data size
    uint32_t totalSamples = SAMPLE_RATE * RECORD_TIME;
    uint32_t dataSize = totalSamples * 2; // 16-bit samples = 2 bytes each

    // Create and write WAV header
    WAVHeader header;
    header.fileSize = sizeof(WAVHeader) - 8 + dataSize;
    header.dataSize = dataSize;

    audioFile.write((uint8_t *)&header, sizeof(header));

    // Record audio data
    uint32_t samplesRecorded = 0;
    uint16_t buffer[BUFFER_SIZE];
    int bufferIndex = 0;

    unsigned long startTime = millis();
    unsigned long nextSample = startTime;

    while (samplesRecorded < totalSamples)
    {
        if (millis() >= nextSample)
        {
            // Read microphone value and convert to 16-bit
            int micValue = analogRead(MIC_PIN);
            // Convert 12-bit ADC (0-4095) to 16-bit signed (-32768 to 32767)
            int16_t sample = (micValue - 2048) * 16; // Center around 0 and amplify

            buffer[bufferIndex++] = sample;
            samplesRecorded++;

            // Write buffer when full
            if (bufferIndex >= BUFFER_SIZE)
            {
                audioFile.write((uint8_t *)buffer, bufferIndex * 2);
                bufferIndex = 0;
            }

            // Calculate next sample time
            nextSample = startTime + (samplesRecorded * 1000) / SAMPLE_RATE;
        }
    }

    // Write remaining buffer
    if (bufferIndex > 0)
    {
        audioFile.write((uint8_t *)buffer, bufferIndex * 2);
    }

    audioFile.close();
    Serial.println("Recording completed: " + filename);
    Serial.println("File size: " + String(sizeof(header) + dataSize) + " bytes");
}