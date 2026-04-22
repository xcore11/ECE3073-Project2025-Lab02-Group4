#include <Seeed_Arduino_SSCMA.h>
#include <ArduinoJson.h>
#include <ESP32SPISlave.h>

#define SPI_MODE SPI_MODE0

#define HSPI_MISO 9
#define HSPI_MOSI 10
#define HSPI_SCLK 8
#define HSPI_SS   7

ESP32SPISlave slave;

static constexpr size_t BUFFER_SIZE = 64;
static constexpr size_t QUEUE_SIZE  = 2;

uint8_t rx_buf[BUFFER_SIZE] = {0};
uint8_t reply_buf[BUFFER_SIZE] = {0};

void setup()
{
    Serial.begin(115200);
    delay(1000);

    slave.setDataMode(SPI_MODE);
    slave.setQueueSize(QUEUE_SIZE);
    slave.begin();

    Serial.println("SPI slave ready");
}

void loop()
{
    size_t len;
    char msg[BUFFER_SIZE + 1];

    memset(rx_buf, 0, BUFFER_SIZE);

    Serial.println("Waiting for SPI...");

    // Wait for FPGA write transaction
    slave.queue(NULL, rx_buf, BUFFER_SIZE);
    const std::vector<size_t> received_bytes = slave.wait(10000);

    if (received_bytes.empty())
    {
        Serial.println("Timeout - no data received");
        Serial.println("------------------------");
        return;
    }

    len = received_bytes[0];
    if (len > BUFFER_SIZE) len = BUFFER_SIZE;

    memcpy(msg, rx_buf, len);
    msg[len] = '\0';

    Serial.println("Data received!");
    Serial.print("Bytes received: ");
    Serial.println(len);

    Serial.print("Received STRING: ");
    Serial.println(msg);

    // Prepare stable reply buffer
    memset(reply_buf, 0, BUFFER_SIZE);
    memcpy(reply_buf, rx_buf, len);

    // Match FPGA wait time
    delay(5000);

    // Queue reply for FPGA dummy-read transaction
    slave.queue(reply_buf, NULL, len);

    // IMPORTANT: wait until that queued reply transaction is actually used
    const std::vector<size_t> sent_bytes = slave.wait(10000);

    if (!sent_bytes.empty())
    {
        Serial.print("Reply sent back, bytes: ");
        Serial.println(sent_bytes[0]);
    }
    else
    {
        Serial.println("Reply not consumed before timeout");
    }

    Serial.println("------------------------");
}