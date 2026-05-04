#include <ESP32SPISlave.h>

#define SPI_MODE SPI_MODE0

#define HSPI_MISO 9
#define HSPI_MOSI 10
#define HSPI_SCLK 8
#define HSPI_SS   7

ESP32SPISlave slave;

uint8_t rx_byte = 0;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    slave.setDataMode(SPI_MODE);
    slave.setQueueSize(1);

    // If your library supports custom pins, use this:
    slave.begin();

    // If the line above gives compile error, use this instead:
    // slave.begin();

    Serial.println("SPI slave ready");
}

void loop()
{
    rx_byte = 0;

    Serial.println("Waiting for 1 SPI byte...");

    // Receive exactly 1 byte from FPGA
    slave.queue(NULL, &rx_byte, 1);

    const std::vector<size_t> received_bytes = slave.wait(10000);

    if (received_bytes.empty())
    {
        Serial.println("Timeout - no data received");
        Serial.println("------------------------");
        return;
    }

    Serial.print("Bytes received: ");
    Serial.println(received_bytes[0]);

    Serial.print("Received HEX: 0x");
    if (rx_byte < 0x10) Serial.print("0");
    Serial.println(rx_byte, HEX);

    Serial.print("Received DEC: ");
    Serial.println(rx_byte);

    Serial.print("Received CHAR: ");
    Serial.println((char)rx_byte);

    Serial.println("------------------------");
}