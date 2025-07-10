#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <time.h>
#include <WiFi.h>
#include <driver/i2s.h>

#define SD_CS 5

// I2S pins
#define I2S_WS 25  // LRCL (Word Select)
#define I2S_SD 18  // DOUT (Data Out)
#define I2S_SCK 26 // BCLK (Bit Clock)

// I2S configuration
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE 16000 // Increased for better bowel sound capture
#define I2S_SAMPLE_BITS 16
#define I2S_READ_LEN 512

// Recording parameters
const int SAMPLE_RATE = I2S_SAMPLE_RATE;
const int SAMPLE_BITS = I2S_SAMPLE_BITS;
const int FILE_DURATION = 10;         // 10 minutes per file (10 seconds for now)
const int TOTAL_DURATION = 24 * 3600; // 24 hours in seconds

// WiFi credentials for time sync
const char *ssid = "RealWiFiNotaScam";
const char *password = "MattRajanAkash!";

// Global variables
String sessionFolder;
int fileNumber = 1;
unsigned long sessionStartTime;

// Function declarations
void setupI2S();
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

    Serial.println("24-Hour I2S WAV Recorder Started");

    // Initialize I2S
    setupI2S();

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

void setupI2S()
{
    // I2S configuration
    i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false};

    // I2S pin configuration
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD};

    // Install and start I2S driver
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK)
    {
        Serial.printf("Failed installing I2S driver: %d\n", err);
        while (1)
            ;
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK)
    {
        Serial.printf("Failed setting I2S pins: %d\n", err);
        while (1)
            ;
    }

    Serial.println("I2S initialized successfully");
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
    int16_t i2s_read_buff[I2S_READ_LEN];
    size_t bytes_read;

    unsigned long fileStartTime = millis();

    while (samplesRecorded < samplesPerFile)
    {
        // Read from I2S
        esp_err_t result = i2s_read(I2S_PORT, &i2s_read_buff, I2S_READ_LEN * sizeof(int16_t), &bytes_read, portMAX_DELAY);

        if (result == ESP_OK)
        {
            int samples_read = bytes_read / sizeof(int16_t);

            // Write samples to file
            for (int i = 0; i < samples_read && samplesRecorded < samplesPerFile; i++)
            {
                // I2S data might need bit shifting depending on the mic
                int16_t sample = i2s_read_buff[i];
                audioFile.write((uint8_t *)&sample, sizeof(int16_t));
                samplesRecorded++;
            }
        }
        else
        {
            Serial.printf("I2S read failed: %d\n", result);
        }

        // Progress update every 30 seconds
        if (samplesRecorded % (SAMPLE_RATE * 30) == 0)
        {
            int minutesComplete = samplesRecorded / (SAMPLE_RATE * 60);
            Serial.printf("File %d: %d/%d minutes complete\n", fileNumber, minutesComplete, FILE_DURATION / 60);
        }
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