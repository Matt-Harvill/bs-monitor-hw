#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <time.h>
#include <WiFi.h>

#define MIC_PIN 34
#define SD_CS 5

// Recording parameters
const int SAMPLE_RATE = 8000;         // 8kHz sample rate
const int SAMPLE_BITS = 16;           // 16-bit samples
const int FILE_DURATION = 10;         // 10 minutes per file (600 seconds) TODO fix to 600
const int TOTAL_DURATION = 24 * 3600; // 24 hours in seconds
const int BUFFER_SIZE = 512;

// WiFi credentials for time sync (optional - remove if no WiFi)
const char *ssid = "RealWiFiNotaScam";
const char *password = "MattRajanAkash!";

// Global variables
String sessionFolder;
int fileNumber = 1;
unsigned long sessionStartTime;

// Function declarations
void syncTime();
void createSessionFolder();
void recordingSession();
void recordSingleFile(String filename);
String formatFileSize(uint32_t bytes);

// WAV header structure
struct WAVHeader
{
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
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

    Serial.println("24-Hour WAV Recorder Started");

    // Initialize analog reading
    analogReadResolution(12);

    // Initialize SD card
    if (!SD.begin(SD_CS))
    {
        Serial.println("SD card initialization failed!");
        while (1)
            ;
    }
    Serial.println("SD card initialized successfully");

    // Try to sync time with WiFi (optional)
    syncTime();

    // Create session folder
    createSessionFolder();

    // Start recording session
    sessionStartTime = millis();
    recordingSession();
}

void loop()
{
    // Recording is handled in setup, nothing to do here
    Serial.println("Recording session completed!");
    delay(10000);
}

void syncTime()
{
    // Optional: Connect to WiFi to get accurate time
    Serial.println("Connecting to WiFi for time sync...");
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi connected, syncing time...");
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");

        // Wait for time to be set
        time_t now = time(nullptr);
        int timeoutCount = 0;
        while (now < 8 * 3600 * 2 && timeoutCount < 20)
        {
            delay(500);
            now = time(nullptr);
            timeoutCount++;
        }

        if (now > 8 * 3600 * 2)
        {
            Serial.println("Time synchronized!");
        }
        else
        {
            Serial.println("Time sync failed, using default timestamp");
        }

        WiFi.disconnect();
    }
    else
    {
        Serial.println("\nWiFi connection failed, using default timestamp");
    }
}

void createSessionFolder()
{
    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);

    // Create folder name with datetime
    char folderName[50];
    if (now > 8 * 3600 * 2)
    {
        // If we have real time
        strftime(folderName, sizeof(folderName), "/recording_%Y%m%d_%H%M%S", timeinfo);
    }
    else
    {
        // Fallback to millis timestamp
        sprintf(folderName, "/recording_%lu", millis());
    }

    sessionFolder = String(folderName);

    if (!SD.mkdir(sessionFolder))
    {
        Serial.println("Warning: Could not create session folder, using root directory");
        sessionFolder = "";
    }
    else
    {
        Serial.println("Created session folder: " + sessionFolder);
    }
}

void recordingSession()
{
    Serial.println("Starting 24-hour recording session...");

    unsigned long totalFiles = TOTAL_DURATION / FILE_DURATION;
    Serial.printf("Will record %lu files of %d minutes each\n", totalFiles, FILE_DURATION / 60);

    for (int i = 0; i < totalFiles; i++)
    {
        String filename = sessionFolder + "/audio_" + String(fileNumber, 10) + ".wav";
        Serial.println("Recording file " + String(fileNumber) + "/" + String(totalFiles) + ": " + filename);

        recordSingleFile(filename);
        fileNumber++;

        // Brief pause between files
        delay(100);
    }

    Serial.println("24-hour recording session completed!");
}

void recordSingleFile(String filename)
{
    File audioFile = SD.open(filename, FILE_WRITE);
    if (!audioFile)
    {
        Serial.println("Failed to create audio file: " + filename);
        return;
    }

    // Calculate data size for this file
    uint32_t samplesPerFile = SAMPLE_RATE * FILE_DURATION;
    uint32_t dataSize = samplesPerFile * 2; // 16-bit samples = 2 bytes each

    // Create and write WAV header
    WAVHeader header;
    header.fileSize = sizeof(WAVHeader) - 8 + dataSize;
    header.dataSize = dataSize;
    audioFile.write((uint8_t *)&header, sizeof(header));

    // Record audio data
    uint32_t samplesRecorded = 0;
    uint16_t buffer[BUFFER_SIZE];
    int bufferIndex = 0;

    unsigned long fileStartTime = millis();

    while (samplesRecorded < samplesPerFile)
    {
        // Calculate when this sample should be taken
        unsigned long targetTime = fileStartTime + (samplesRecorded * 1000UL) / SAMPLE_RATE;

        // Wait for the right time
        while (millis() < targetTime)
        {
            delayMicroseconds(100);
        }

        // Read microphone value and convert to 16-bit
        int micValue = analogRead(MIC_PIN);
        int16_t sample = (micValue - 2048) * 16; // Center around 0 and amplify

        buffer[bufferIndex++] = sample;
        samplesRecorded++;

        // Write buffer when full
        if (bufferIndex >= BUFFER_SIZE)
        {
            audioFile.write((uint8_t *)buffer, bufferIndex * 2);
            bufferIndex = 0;
        }

        // Progress update every 30 seconds
        if (samplesRecorded % (SAMPLE_RATE * 30) == 0)
        {
            int minutesComplete = samplesRecorded / (SAMPLE_RATE * 60);
            Serial.printf("File %d: %d/%d minutes complete\n", fileNumber, minutesComplete, FILE_DURATION / 60);
        }
    }

    // Write remaining buffer
    if (bufferIndex > 0)
    {
        audioFile.write((uint8_t *)buffer, bufferIndex * 2);
    }

    audioFile.close();
    Serial.println("Completed: " + filename);
}

String formatFileSize(uint32_t bytes)
{
    if (bytes < 1024)
        return String(bytes) + " B";
    if (bytes < 1024 * 1024)
        return String(bytes / 1024) + " KB";
    return String(bytes / (1024 * 1024)) + " MB";
}