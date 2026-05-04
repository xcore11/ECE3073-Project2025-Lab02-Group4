#include <Seeed_Arduino_SSCMA.h>
#include <ESP32DMASPISlave.h>
#include <SPI.h>

SSCMA AI;
ESP32DMASPI::Slave slave;

/* ============================================================
   SPI DMA CONFIG
   ============================================================ */

#define SPI_MODE_USED SPI_MODE0

// Change these to match your ESP32 wiring
#define HSPI_MISO 9
#define HSPI_MOSI 10
#define HSPI_SCLK 8
#define HSPI_SS   7

#define FPGA_CAPTURE_CMD 0x5F

// Command transaction size.
// FPGA sends:
// 0x5F 0x00 0x00 0x00
#define SPI_CMD_SIZE 4

// DMA packet size.
// Must be multiple of 4.
// FPGA must clock this many bytes for each return packet.
#define SPI_PACKET_SIZE 256

#define SPI_HEADER_SIZE 8
#define SPI_PAYLOAD_SIZE (SPI_PACKET_SIZE - SPI_HEADER_SIZE)

// Packet format:
// [0] = 0xA5
// [1] = packet type
// [2] = payload len low
// [3] = payload len high
// [4] = sequence low
// [5] = sequence high
// [6] = last flag
// [7] = reserved
// [8..] = payload
#define PACKET_START      0xA5
#define PACKET_TEXT       0x01
#define PACKET_JPEG_MARK  0x02
#define PACKET_JPEG_DATA  0x03
#define PACKET_DONE       0x7F
#define PACKET_ERROR      0xEE

uint8_t *spi_tx = NULL;
uint8_t *spi_rx = NULL;
uint8_t *spi_cmd_tx = NULL;
uint8_t *spi_cmd_rx = NULL;

/* ============================================================
   VISION / TEXT CONFIG
   ============================================================ */

// Change this only.
// 8  -> START321
// 9  -> START4321
// 10 -> START54321
#define MAX_CHARS_PER_LINE 8

#define MAX_LINES 12

#define MIN_SCORE 40
#define Y_ROW_TOLERANCE 18
#define X_SLOT_TOLERANCE 25

#define LINE_STABLE_REQUIRED 3
#define MIN_LINES_BEFORE_IMAGE_CAPTURE 5

// AI.invoke fail means gap/newline/end of passage.
// Require a few consecutive fails to avoid random one-frame failure.
#define INVOKE_FAIL_END_COUNT 2

char calibration_text[MAX_CHARS_PER_LINE + 1];

/*
   IMPORTANT:
   This LABELS array must match your model's class index order.

   If your model target is already ASCII, edit labelFromTarget()
   and return (char)target.
*/
const char LABELS[] = {
  '0','1','2','3','4','5','6','7','8','9',
  'A','B','C','D','E','F','G','H','I','J','K','L','M',
  'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
  'a','b','c','d','e','f','g','h','i','j','k','l','m',
  'n','o','p','q','r','s','t','u','v','w','x','y','z'
};

#define LABEL_COUNT (sizeof(LABELS) / sizeof(LABELS[0]))

/* ============================================================
   CALIBRATION DATA
   ============================================================ */

int calibrated_x[MAX_CHARS_PER_LINE];
bool calibrated = false;

/* ============================================================
   CAPTURE BUFFERS
   ============================================================ */

char frozen_lines[MAX_LINES][MAX_CHARS_PER_LINE + 1];
int frozen_line_count = 0;

char last_seen_line[MAX_LINES][MAX_CHARS_PER_LINE + 1];
int line_stable_count[MAX_LINES];

String final_text = "";
String jpeg_base64 = "";

bool image_captured = false;

/* ============================================================
   BASIC HELPERS
   ============================================================ */

void buildCalibrationText()
{
  const char prefix[] = "START";
  const int prefix_len = 5;

  int digits_needed = MAX_CHARS_PER_LINE - prefix_len;
  int index = 0;

  for (int i = 0; i < prefix_len && index < MAX_CHARS_PER_LINE; i++) {
    calibration_text[index++] = prefix[i];
  }

  for (int d = digits_needed; d >= 1 && index < MAX_CHARS_PER_LINE; d--) {
    calibration_text[index++] = '0' + d;
  }

  calibration_text[index] = '\0';
}

char labelFromTarget(int target)
{
  // If your model target is already ASCII, use this instead:
  // return (char)target;

  if (target >= 0 && target < LABEL_COUNT) {
    return LABELS[target];
  }

  return '?';
}

void clearLine(char *line)
{
  for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
    line[i] = ' ';
  }

  line[MAX_CHARS_PER_LINE] = '\0';
}

void trimRight(char *line)
{
  for (int i = MAX_CHARS_PER_LINE - 1; i >= 0; i--) {
    if (line[i] == ' ') {
      line[i] = '\0';
    } else {
      break;
    }
  }
}

bool lineHasText(const char *line)
{
  for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
    if (line[i] != ' ' && line[i] != '\0') {
      return true;
    }
  }

  return false;
}

void resetCaptureBuffers()
{
  frozen_line_count = 0;
  final_text = "";
  jpeg_base64 = "";
  image_captured = false;

  for (int i = 0; i < MAX_LINES; i++) {
    clearLine(frozen_lines[i]);
    clearLine(last_seen_line[i]);
    line_stable_count[i] = 0;
  }
}

int nearestXSlot(int x_center)
{
  int best_slot = -1;
  int best_dist = 99999;

  for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
    int d = abs(x_center - calibrated_x[i]);

    if (d < best_dist) {
      best_dist = d;
      best_slot = i;
    }
  }

  if (best_dist > X_SLOT_TOLERANCE) {
    return -1;
  }

  return best_slot;
}

int findOrCreateRow(int y_center, int row_y[], int &row_count)
{
  for (int i = 0; i < row_count; i++) {
    if (abs(y_center - row_y[i]) <= Y_ROW_TOLERANCE) {
      return i;
    }
  }

  if (row_count >= MAX_LINES) {
    return -1;
  }

  row_y[row_count] = y_center;
  row_count++;

  return row_count - 1;
}

/* ============================================================
   SPI DMA HELPERS
   ============================================================ */

void clearDmaBuffer(uint8_t *buf, size_t len)
{
  if (buf == NULL) {
    return;
  }

  memset(buf, 0, len);
}

size_t spiDmaTransfer(uint8_t *tx, uint8_t *rx, size_t len)
{
  /*
     Blocking DMA SPI slave transaction.

     FPGA is SPI master.
     ESP is SPI slave.

     ESP waits here until FPGA clocks len bytes.
  */

  clearDmaBuffer(rx, len);

  size_t received = slave.transfer(tx, rx, len);

  return received;
}

void spiBeginDma()
{
  slave.setDataMode(SPI_MODE_USED);
  slave.setQueueSize(1);

  /*
     If this custom-pin begin() does not compile on your installed version,
     replace this line with:

       bool ok = slave.begin(HSPI);

     Then use the default HSPI pins for your board.
  */
  bool ok = slave.begin(HSPI, HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_SS);

  if (!ok) {
    Serial.println("ESP32DMASPI slave begin failed");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("ESP32DMASPI slave ready");
}

void allocateDmaBuffers()
{
  spi_tx     = ESP32DMASPI::Slave::allocDMABuffer(SPI_PACKET_SIZE);
  spi_rx     = ESP32DMASPI::Slave::allocDMABuffer(SPI_PACKET_SIZE);
  spi_cmd_tx = ESP32DMASPI::Slave::allocDMABuffer(SPI_CMD_SIZE);
  spi_cmd_rx = ESP32DMASPI::Slave::allocDMABuffer(SPI_CMD_SIZE);

  if (!spi_tx || !spi_rx || !spi_cmd_tx || !spi_cmd_rx) {
    Serial.println("Failed to allocate SPI DMA buffers");
    while (1) {
      delay(1000);
    }
  }

  clearDmaBuffer(spi_tx, SPI_PACKET_SIZE);
  clearDmaBuffer(spi_rx, SPI_PACKET_SIZE);
  clearDmaBuffer(spi_cmd_tx, SPI_CMD_SIZE);
  clearDmaBuffer(spi_cmd_rx, SPI_CMD_SIZE);

  Serial.println("SPI DMA buffers allocated");
}

uint8_t waitForCommandByte()
{
  Serial.println("Waiting for FPGA command byte 0x5F...");

  while (1) {
    clearDmaBuffer(spi_cmd_tx, SPI_CMD_SIZE);
    clearDmaBuffer(spi_cmd_rx, SPI_CMD_SIZE);

    size_t received = spiDmaTransfer(spi_cmd_tx, spi_cmd_rx, SPI_CMD_SIZE);

    Serial.print("Command transaction received bytes: ");
    Serial.println(received);

    for (int i = 0; i < SPI_CMD_SIZE; i++) {
      Serial.print("CMD RX[");
      Serial.print(i);
      Serial.print("] = 0x");

      if (spi_cmd_rx[i] < 0x10) {
        Serial.print("0");
      }

      Serial.println(spi_cmd_rx[i], HEX);

      if (spi_cmd_rx[i] == FPGA_CAPTURE_CMD) {
        Serial.println("Received FPGA capture command 0x5F");
        return FPGA_CAPTURE_CMD;
      }
    }

    Serial.println("SPI command transaction received, but no 0x5F found");
    delay(5);
  }
}

void sendPacket(uint8_t type, uint16_t seq, const uint8_t *payload, uint16_t len, bool last)
{
  if (len > SPI_PAYLOAD_SIZE) {
    len = SPI_PAYLOAD_SIZE;
  }

  clearDmaBuffer(spi_tx, SPI_PACKET_SIZE);
  clearDmaBuffer(spi_rx, SPI_PACKET_SIZE);

  spi_tx[0] = PACKET_START;
  spi_tx[1] = type;
  spi_tx[2] = len & 0xFF;
  spi_tx[3] = (len >> 8) & 0xFF;
  spi_tx[4] = seq & 0xFF;
  spi_tx[5] = (seq >> 8) & 0xFF;
  spi_tx[6] = last ? 1 : 0;
  spi_tx[7] = 0x00;

  if (payload != NULL && len > 0) {
    memcpy(&spi_tx[SPI_HEADER_SIZE], payload, len);
  }

  size_t received = spiDmaTransfer(spi_tx, spi_rx, SPI_PACKET_SIZE);

  Serial.print("Sent packet type=0x");
  Serial.print(type, HEX);
  Serial.print(" seq=");
  Serial.print(seq);
  Serial.print(" len=");
  Serial.print(len);
  Serial.print(" last=");
  Serial.print(last);
  Serial.print(" master clocked=");
  Serial.println(received);
}

void sendStringAsPackets(uint8_t type, const String &data)
{
  uint16_t seq = 0;
  int offset = 0;
  int total = data.length();

  if (total == 0) {
    sendPacket(type, seq, NULL, 0, true);
    return;
  }

  while (offset < total) {
    int remaining = total - offset;
    int chunk = remaining;

    if (chunk > SPI_PAYLOAD_SIZE) {
      chunk = SPI_PAYLOAD_SIZE;
    }

    bool last = (offset + chunk >= total);

    sendPacket(
      type,
      seq,
      (const uint8_t *)data.c_str() + offset,
      chunk,
      last
    );

    offset += chunk;
    seq++;
  }
}

void sendFinalResultToFpga()
{
  Serial.println("Sending TEXT packets first...");
  sendStringAsPackets(PACKET_TEXT, final_text);

  Serial.println("Sending JPEG marker packet...");
  const char marker[] = "JPEG_NEXT";
  sendPacket(PACKET_JPEG_MARK, 0, (const uint8_t *)marker, strlen(marker), true);

  Serial.println("Sending JPEG/base64 data packets...");
  sendStringAsPackets(PACKET_JPEG_DATA, jpeg_base64);

  Serial.println("Sending DONE packet...");
  sendPacket(PACKET_DONE, 0, NULL, 0, true);

  Serial.println("SPI result transfer completed.");
}

/* ============================================================
   CALIBRATION
   ============================================================ */

bool tryCalibrationOnce()
{
  /*
     Calibration does NOT include image because we only need boxes.
  */
  if (AI.invoke(1, false, false)) {
    return false;
  }

  int count = AI.boxes().size();

  if (count < MAX_CHARS_PER_LINE) {
    return false;
  }

  struct CharBox {
    char c;
    int x;
    int y;
  };

  CharBox chars[32];
  int char_count = 0;

  for (int i = 0; i < count && char_count < 32; i++) {
    auto box = AI.boxes()[i];

    if (box.score < MIN_SCORE) {
      continue;
    }

    chars[char_count].c = labelFromTarget(box.target);
    chars[char_count].x = box.x + box.w / 2;
    chars[char_count].y = box.y + box.h / 2;
    char_count++;
  }

  if (char_count < MAX_CHARS_PER_LINE) {
    return false;
  }

  /*
     Sort by X so we can compare the detected calibration string
     left-to-right.
  */
  for (int i = 0; i < char_count - 1; i++) {
    for (int j = i + 1; j < char_count; j++) {
      if (chars[j].x < chars[i].x) {
        CharBox temp = chars[i];
        chars[i] = chars[j];
        chars[j] = temp;
      }
    }
  }

  char detected[MAX_CHARS_PER_LINE + 1];

  for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
    detected[i] = chars[i].c;
  }

  detected[MAX_CHARS_PER_LINE] = '\0';

  Serial.print("Calibration candidate: ");
  Serial.println(detected);

  Serial.print("Expected calibration: ");
  Serial.println(calibration_text);

  if (strcmp(detected, calibration_text) != 0) {
    return false;
  }

  for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
    calibrated_x[i] = chars[i].x;
  }

  calibrated = true;

  Serial.println("Calibration successful. X slots:");

  for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
    Serial.print("slot ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(calibrated_x[i]);
  }

  return true;
}

void waitForCalibration()
{
  Serial.print("Waiting for calibration text: ");
  Serial.println(calibration_text);

  while (!calibrated) {
    tryCalibrationOnce();
    delay(100);
  }

  Serial.println("Calibration ready.");
}

/* ============================================================
   TEXT RECONSTRUCTION
   ============================================================ */

bool readRowsOnce()
{
  /*
     include_image = true so AI.last_image() can be used.
     This follows your original capture style:
       AI.invoke(1, false, true)
       AI.last_image()
  */
  int ret = AI.invoke(1, false, true);

  if (ret != 0) {
    return false;
  }

  int row_y[MAX_LINES];
  char current_rows[MAX_LINES][MAX_CHARS_PER_LINE + 1];

  int row_count = 0;

  for (int i = 0; i < MAX_LINES; i++) {
    clearLine(current_rows[i]);
  }

  for (int i = 0; i < AI.boxes().size(); i++) {
    auto box = AI.boxes()[i];

    if (box.score < MIN_SCORE) {
      continue;
    }

    char c = labelFromTarget(box.target);

    int x_center = box.x + box.w / 2;
    int y_center = box.y + box.h / 2;

    int slot = nearestXSlot(x_center);

    if (slot < 0) {
      continue;
    }

    int row = findOrCreateRow(y_center, row_y, row_count);

    if (row < 0) {
      continue;
    }

    current_rows[row][slot] = c;
  }

  for (int r = 0; r < row_count; r++) {
    trimRight(current_rows[r]);

    if (!lineHasText(current_rows[r])) {
      continue;
    }

    if (strcmp(current_rows[r], last_seen_line[r]) == 0) {
      line_stable_count[r]++;
    } else {
      strcpy(last_seen_line[r], current_rows[r]);
      line_stable_count[r] = 1;
    }

    if (line_stable_count[r] >= LINE_STABLE_REQUIRED) {
      bool already_saved = false;

      for (int s = 0; s < frozen_line_count; s++) {
        if (strcmp(frozen_lines[s], current_rows[r]) == 0) {
          already_saved = true;
          break;
        }
      }

      if (!already_saved && frozen_line_count < MAX_LINES) {
        strcpy(frozen_lines[frozen_line_count], current_rows[r]);
        frozen_line_count++;

        Serial.print("Frozen line ");
        Serial.print(frozen_line_count);
        Serial.print(": ");
        Serial.println(current_rows[r]);
      }
    }
  }

  if (!image_captured && frozen_line_count >= MIN_LINES_BEFORE_IMAGE_CAPTURE) {
    String img = AI.last_image();

    if (img.length() > 0) {
      jpeg_base64 = img;
      image_captured = true;

      Serial.print("Image captured. Base64 length: ");
      Serial.println(jpeg_base64.length());
    }
  }

  return true;
}

void buildFinalText()
{
  final_text = "";

  for (int i = 0; i < frozen_line_count; i++) {
    if (strlen(frozen_lines[i]) == 0) {
      continue;
    }

    if (final_text.length() > 0) {
      final_text += "\n";
    }

    final_text += frozen_lines[i];
  }
}

void captureOnePassageAfterCommand()
{
  resetCaptureBuffers();

  Serial.println("Capture session started.");
  Serial.println("Using calibrated X positions.");
  Serial.println("AI.invoke fail will mark passage end.");

  int fail_count = 0;
  bool saw_any_valid_frame = false;

  while (1) {
    bool ok = readRowsOnce();

    if (ok) {
      saw_any_valid_frame = true;
      fail_count = 0;
    } else {
      if (saw_any_valid_frame) {
        fail_count++;
      }

      Serial.print("Invoke failed/gap count: ");
      Serial.println(fail_count);

      if (saw_any_valid_frame && fail_count >= INVOKE_FAIL_END_COUNT) {
        Serial.println("Passage ended.");
        break;
      }
    }

    delay(50);
  }

  /*
     If the line threshold was never reached, still try one final image.
  */
  if (!image_captured) {
    Serial.println("Trying final image capture...");

    if (!AI.invoke(1, false, true)) {
      String img = AI.last_image();

      if (img.length() > 0) {
        jpeg_base64 = img;
        image_captured = true;

        Serial.print("Final image captured. Base64 length: ");
        Serial.println(jpeg_base64.length());
      }
    }
  }

  buildFinalText();

  Serial.println("Final text:");
  Serial.println(final_text);

  Serial.print("Final JPEG/base64 length: ");
  Serial.println(jpeg_base64.length());

  sendFinalResultToFpga();
}

/* ============================================================
   SETUP / LOOP
   ============================================================ */

void setup()
{
  Serial.begin(115200);
  delay(1000);

  buildCalibrationText();

  Serial.println("Starting Grove AI V2...");
  AI.begin();

  Serial.println("Allocating SPI DMA buffers...");
  allocateDmaBuffers();

  Serial.println("Starting ESP32DMASPI slave...");
  spiBeginDma();

  waitForCalibration();

  Serial.println("Ready. Waiting for FPGA 0x5F command.");
}

void loop()
{
  waitForCommandByte();

  if (!calibrated) {
    waitForCalibration();
  }

  captureOnePassageAfterCommand();

  Serial.println("Capture done. Waiting for next FPGA 0x5F command.");
}