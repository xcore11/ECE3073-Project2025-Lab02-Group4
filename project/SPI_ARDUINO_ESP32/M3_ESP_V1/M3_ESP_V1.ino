#include <Seeed_Arduino_SSCMA.h>
#include <ArduinoJson.h>
#include <ESP32DMASPISlave.h>
#include <JPEGDecoder.h>
#include <vector>

SSCMA AI;

/* =========================
   SPI settings
   ========================= */

#define SPI_MODE SPI_MODE0

/* ESP GPIO wired to FPGA ESP_DATA_READY PIO input. */
#define FPGA_DATA_READY_PIN 21

ESP32DMASPI::Slave slave;

/* =========================
   SPI protocol
   ========================= */   

#define SPI_CMD_SESSION_START   0x5F   /* FPGA sends this once when physical button starts session */
#define SPI_CMD_READ_LENGTH     0xA1
#define SPI_CMD_READ_PACKET     0xA2
#define SPI_CMD_ABORT_PACKET    0xA3

#define SPI_COMMAND_BYTES       4
#define SPI_LENGTH_BYTES        8
#define SPI_DMA_EXTRA_BYTES     4

uint8_t *dma_cmd_rx       = NULL;
uint8_t *dma_cmd_tx_dummy = NULL;
uint8_t *dma_len_tx       = NULL;
uint8_t *dma_len_rx_dummy = NULL;
uint8_t *dma_packet_tx    = NULL;
uint8_t *dma_packet_rx_dummy = NULL;

String latestPassageText = "";
bool passageReadyForSpi = false;


/* =========================
   Capture + image packet settings
   ========================= */

#define ENABLE_IMAGE_CAPTURE      1
#define GRAY_W                    96
#define GRAY_H                    96
#define GRAY_BYTES                (GRAY_W * GRAY_H)

#define PACKET_MAGIC0             'G'
#define PACKET_MAGIC1             'V'
#define PACKET_VERSION            1
#define PACKET_STATUS_DONE        2
#define PACKET_HEADER_LEN         24
#define PACKET_FLAG_IMAGE_VALID   0x0001
#define PACKET_MAX_TEXT_LEN       512
#define PACKET_MAX_LEN            (PACKET_HEADER_LEN + PACKET_MAX_TEXT_LEN + GRAY_BYTES)
#define PACKET_DMA_MAX_LEN        (((PACKET_MAX_LEN + 3) & ~3) + SPI_DMA_EXTRA_BYTES)

uint8_t latestGrayImage[GRAY_BYTES] = {0};
bool latestGrayImageValid = false;

uint8_t finalPacket[PACKET_MAX_LEN] = {0};
uint32_t finalPacketLen = 0;
volatile bool packetReadyForSpi = false;

/*
   RTOS/SPI packet queue.

   Old behavior had only one finalPacket buffer. When packetReadyForSpi was true,
   loop() stopped invoking AI until FPGA read the packet. That caused missed
   rows/stability counts while ESP_DATA_READY was waiting.

   New behavior:
     - AI loop keeps invoking and freezing rows.
     - Frozen rows are converted into binary packets and queued.
     - SPI task serves the current packet through DMA.
     - When FPGA reads one packet, SPI task lowers DATA_READY briefly, then
       promotes the next queued packet if available.

   This decouples AI scanning from FPGA SPI read latency.
*/
#define SPI_PACKET_QUEUE_DEPTH   8

/*
   Gap between consecutive queued packets.
   DATA_READY must go LOW long enough for FPGA to see packet boundary before
   the next queued packet is presented.
*/
#define SPI_NEXT_PACKET_GAP_MS 5

struct SpiQueuedPacket {
    uint32_t len;
    uint8_t data[PACKET_MAX_LEN];
};

SpiQueuedPacket spiPacketQueue[SPI_PACKET_QUEUE_DEPTH];
volatile int spiPacketQueueHead = 0;
volatile int spiPacketQueueTail = 0;
volatile int spiPacketQueueCount = 0;

portMUX_TYPE spiPacketMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t packetBuildScratch[PACKET_MAX_LEN] = {0};

static void setFpgaDataReady(bool ready)
{
    digitalWrite(FPGA_DATA_READY_PIN, ready ? HIGH : LOW);
}

String storedJpegBase64 = "";
bool imageUpdatedAfterFourRows = false;

/*
   captureActive becomes true automatically after START321 calibration.
   Instruction rows then freeze into realtime SPI packets.
*/
volatile bool captureActive = false;

/*
   Session gate:
   ESP does NOT run Grove detection until FPGA sends 0x5F once.
   The 0x5F is only a synchronized session-start signal.
   After sessionStarted becomes true, later 0x5F commands are ignored.
*/
volatile bool sessionStarted = false;

/* =========================
   RTOS SPI trigger flags
   ========================= */

TaskHandle_t spiTriggerTaskHandle = NULL;
portMUX_TYPE spiFlagMux = portMUX_INITIALIZER_UNLOCKED;

volatile bool spiTriggerPending = false;
volatile uint32_t spiTriggerCounter = 0;
volatile uint32_t spiIgnoredCounter = 0;

/* =========================
   User-tunable parameters
   ========================= */

#define PRINT_DELAY_MS 0

#define MAX_BOXES 80
#define MAX_CHARS_PER_LINE 8

/*
   Calibration line:
   If MAX_CHARS_PER_LINE = 8  -> START321
   If MAX_CHARS_PER_LINE = 10 -> START54321
   If MAX_CHARS_PER_LINE = 11 -> START654321
*/
#define START_CALIBRATION_PREFIX "start"

/*
   Characters are grouped into the same visual row
   when their y positions are close.
*/
#define LINE_Y_THRESHOLD 6

/*
   Y-only tracker matching threshold.
   This is only used if same-text matching fails.
*/
#define ROW_MATCH_Y_THRESHOLD 16

/*
   Same text matching allows bigger y movement
   because the whole row scrolls upward.
*/
#define SAME_TEXT_Y_THRESHOLD 50

/*
   Full max-character row freezes after this many same detections.
   Short row freezes after this many same detections.
*/
#define ROW_FREEZE_CONFIRM_COUNT 3
#define SHORT_ROW_FREEZE_CONFIRM_COUNT 3

/*
   Remove an active row tracker if it has not been seen
   for this many AI frames.
*/
#define ROW_STALE_FRAMES 5

/*
   Ordered row sender.

   Frozen rows are not sent immediately. They are placed in a queue first.
   The queue waits until it has been quiet for a few AI frames, then sorts
   by compensated scroll order and sends one SPI packet at a time.

   This is more reliable than only waiting on the oldest row, because scrolling
   text can make lower rows freeze before upper rows.
*/
#define ROW_SEND_QUIET_FRAMES       4
#define ROW_SEND_MAX_HOLD_FRAMES    28
#define ROW_SEND_FORCE_FLUSH_COUNT  (MAX_PENDING_SEND_ROWS - 4)
#define MAX_PENDING_SEND_ROWS       40

/*
   Scrolling-order compensation.

   The screen scrolls upward, so a row frozen later will have a smaller y value
   than where it originally appeared. Sorting only by frozen y can therefore
   put later rows before earlier rows.

   We estimate the original vertical order with:
       orderKey = frozenY + frameCounter * ROW_ORDER_SCROLL_PIXELS_PER_FRAME

   Tune this if your scroll speed changes. From your logs, rows move about
   28-32 px per AI frame, so 30 is a good starting point.
*/
#define ROW_ORDER_SCROLL_PIXELS_PER_FRAME 30

/*
   Exact duplicate protection.

   Original duplicate behavior restored:
   - If an exact cleaned row text was already queued/sent in this capture pass,
     do not send it again.
   - No fuzzy matching. No similar-row blocking.
   - Duplicate memory is cleared when AI.invoke fails enough times, meaning the
     scroll/capture pass ended and the same rows are allowed to register again.
*/
#define RECENT_SENT_ROW_MAX 64
#define RECENT_SENT_DUP_WINDOW_MS 120000

/*
   Drop tiny fragments like "19" or "i9". They are usually tails of split
   coordinate rows and should not be sent as standalone SPI packets.
*/
#define MIN_CANDIDATE_CHARS 1
#define MAX_CONSECUTIVE_INVOKE_FAILS 2

/*
   AI confidence filter.
   Seeed SSCMA boxes expose .score.
   Only boxes with score >= MIN_BOX_CONFIDENCE are accepted.
   Typical SSCMA Arduino score is an integer, often 0..100.
   Set lower if too many characters disappear.
*/
#define MIN_BOX_CONFIDENCE 50

#define MAX_VISIBLE_ROWS 20
#define MAX_PASSAGE_ROWS 120

/*
   Maximum allowed x distance from a detected character
   to a calibrated slot.
   Increase this if letters keep missing slots.
   Decrease this if letters jump into wrong slots.
*/
#define SLOT_X_TOLERANCE_PIXELS 45

#define DEBUG_FRAME_LINES 1
#define DEBUG_ROW_TRACKERS 1
#define DEBUG_CALIBRATION 1

/* =========================
   Data structures
   ========================= */

struct CharBox {
    int target;
    int score;
    int x;
    int y;
    int w;
    int h;
    char c;
};

struct DetectedRow {
    String rawText;
    String text;

    int y;
    int minX;
    int maxX;
    int boxCount;

    char chars[MAX_CHARS_PER_LINE];
    int xs[MAX_CHARS_PER_LINE];
    int charCount;
};

struct RowTracker {
    bool active;

    int y;
    int lastSeenFrame;

    String lastText;
    int stableCount;

    bool frozen;
    String frozenText;
};

struct PendingFrozenRow {
    bool active;
    String text;
    int y;
    int frozenFrame;
    long orderKey;
    unsigned long seq;
};

struct RecentSentRow {
    String text;
    int y;
    unsigned long sentMs;
};

/*
   Explicit prototypes are needed in Arduino .ino builds.
   Without these, Arduino auto-generates prototypes above the struct definitions,
   so CharBox/DetectedRow are not known yet and compilation fails.
*/
void sortByY(struct CharBox arr[], int n);
void sortByX(struct CharBox arr[], int start, int end);
void sortDetectedRowsByY(struct DetectedRow arr[], int n);
String buildSlottedTextFromRow(struct DetectedRow *row);
bool tryUseRowAsCalibration(struct DetectedRow row);
int extractDetectedRows(struct DetectedRow detectedRows[], int maxRowsOut);
void updateRowTrackerWithDetectedRow(struct DetectedRow detected);
void processDetectedRows(struct DetectedRow detectedRows[], int detectedCount);
void queueFrozenRowForOrderedSend(int trackerIndex, String finalRowText);
void flushPendingFrozenRows();
bool pendingQueueAlreadyHasExactRow(const String &text);
bool recentSentAlreadyHasExactRow(const String &text);
void rememberRecentSentRow(const String &text, int y);
void resetDuplicateGuardsAfterInvokeFails();
bool buildFinalPacket();
bool queuePacketBytesForSpi(const uint8_t *data, uint32_t len);
bool promoteQueuedPacketIfIdle();
void clearSpiPacketQueue();

bool isUsefulSnakeRowText(const String &text);
bool isPortalDuplicateBypassRowText(const String &text);

String frozenPassageRows[MAX_PASSAGE_ROWS];
int frozenPassageRowCount = 0;

RowTracker rowTrackers[MAX_VISIBLE_ROWS];
PendingFrozenRow pendingFrozenRows[MAX_PENDING_SEND_ROWS];
int pendingFrozenRowCount = 0;
unsigned long pendingFrozenSeq = 0;
int lastPendingFrozenQueueFrame = -9999;
bool rowOrderForceFlush = false;
bool duplicateResetAfterPendingFlush = false;

RecentSentRow recentSentRows[RECENT_SENT_ROW_MAX];
int recentSentCount = 0;

int consecutiveInvokeFails = 0;
int frameCounter = 0;

/* =========================
   Calibration data
   ========================= */

bool calibrationReady = false;
int calibratedX[MAX_CHARS_PER_LINE] = {0};

/* =========================
   Decode AI target ID
   ========================= */

char decodeChar(int target)
{
    if (target >= 0 && target <= 25) {
        return 'a' + target;
    } else if (target >= 26 && target <= 35) {
        return '0' + (target - 26);
    } else {
        return '?';
    }
}

/* =========================
   Text helper
   ========================= */

String cleanText(String s)
{
    s.toLowerCase();
    s.trim();
    return s;
}

String trimRightText(String s)
{
    while (s.length() > 0 && s[s.length() - 1] == ' ') {
        s.remove(s.length() - 1);
    }

    return s;
}

String trimLeftText(String s)
{
    while (s.length() > 0 && s[0] == ' ') {
        s.remove(0, 1);
    }

    return s;
}

String cleanSlottedText(String s)
{
    /*
       Preserve internal spaces.
       Remove useless left/right spaces.
    */
    s.toLowerCase();
    s = trimLeftText(s);
    s = trimRightText(s);
    return s;
}

int countNonSpaceChars(String s)
{
    int count = 0;

    for (int i = 0; i < s.length(); i++) {
        if (s[i] != ' ') {
            count++;
        }
    }

    return count;
}

/* =========================
   Calibration line helper
   ========================= */

String buildExpectedCalibrationLine()
{
    String prefix = START_CALIBRATION_PREFIX;
    prefix.toLowerCase();

    if (MAX_CHARS_PER_LINE <= 0) {
        return "";
    }

    if (prefix.length() >= MAX_CHARS_PER_LINE) {
        return prefix.substring(0, MAX_CHARS_PER_LINE);
    }

    int remaining = MAX_CHARS_PER_LINE - prefix.length();
    String suffix = "";

    for (int number = remaining; number >= 1; number--) {
        suffix += String(number % 10);
    }

    return prefix + suffix;
}

/* =========================
   SPI packet queue helpers
   ========================= */

void clearSpiPacketQueue()
{
    portENTER_CRITICAL(&spiPacketMux);

    spiPacketQueueHead = 0;
    spiPacketQueueTail = 0;
    spiPacketQueueCount = 0;

    for (int i = 0; i < SPI_PACKET_QUEUE_DEPTH; i++) {
        spiPacketQueue[i].len = 0;
        memset(spiPacketQueue[i].data, 0, PACKET_MAX_LEN);
    }

    packetReadyForSpi = false;
    finalPacketLen = 0;
    memset(finalPacket, 0, sizeof(finalPacket));

    portEXIT_CRITICAL(&spiPacketMux);

    setFpgaDataReady(false);
}

/* =========================
   Reset memory
   ========================= */

void resetAllMemory()
{
    for (int i = 0; i < MAX_PASSAGE_ROWS; i++) {
        frozenPassageRows[i] = "";
    }

    for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        rowTrackers[i].active = false;
        rowTrackers[i].y = 0;
        rowTrackers[i].lastSeenFrame = -9999;

        rowTrackers[i].lastText = "";
        rowTrackers[i].stableCount = 0;

        rowTrackers[i].frozen = false;
        rowTrackers[i].frozenText = "";
    }

    for (int i = 0; i < MAX_PENDING_SEND_ROWS; i++) {
        pendingFrozenRows[i].active = false;
        pendingFrozenRows[i].text = "";
        pendingFrozenRows[i].y = 0;
        pendingFrozenRows[i].frozenFrame = -9999;
        pendingFrozenRows[i].orderKey = 0;
        pendingFrozenRows[i].seq = 0;
    }
    pendingFrozenRowCount = 0;
    pendingFrozenSeq = 0;
    lastPendingFrozenQueueFrame = -9999;
    rowOrderForceFlush = false;
    duplicateResetAfterPendingFlush = false;

    for (int i = 0; i < RECENT_SENT_ROW_MAX; i++) {
        recentSentRows[i].text = "";
        recentSentRows[i].y = 0;
        recentSentRows[i].sentMs = 0;
    }
    recentSentCount = 0;

    for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
        calibratedX[i] = 0;
    }

    calibrationReady = false;

    frozenPassageRowCount = 0;
    consecutiveInvokeFails = 0;
    frameCounter = 0;

    latestPassageText = "";
    passageReadyForSpi = false;

    clearSpiPacketQueue();

    latestGrayImageValid = false;
    memset(latestGrayImage, 0, sizeof(latestGrayImage));

    captureActive = false;
    setFpgaDataReady(false);

    Serial.println("Memory reset");
    Serial.print("Expected calibration line: ");
    Serial.println(buildExpectedCalibrationLine());
    Serial.println("----------------");
}

/* =========================
   Sorting helpers
   ========================= */

void sortByY(struct CharBox arr[], int n)
{
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (arr[j].y < arr[i].y) {
                CharBox temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
    }
}

void sortByX(struct CharBox arr[], int start, int end)
{
    for (int i = start; i < end - 1; i++) {
        for (int j = i + 1; j < end; j++) {
            if (arr[j].x < arr[i].x) {
                CharBox temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
    }
}

void sortDetectedRowsByY(struct DetectedRow arr[], int n)
{
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (arr[j].y < arr[i].y) {
                DetectedRow temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
    }
}

/* =========================
   Slot mapping
   ========================= */

int findClosestCalibratedSlot(int detectedX)
{
    if (!calibrationReady) {
        return -1;
    }

    int bestSlot = -1;
    int bestDiff = 999999;

    for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
        int diff = abs(detectedX - calibratedX[i]);

        if (diff < bestDiff) {
            bestDiff = diff;
            bestSlot = i;
        }
    }

    if (bestDiff > SLOT_X_TOLERANCE_PIXELS) {
        return -1;
    }

    return bestSlot;
}

String buildSlottedTextFromRow(struct DetectedRow *row)
{
    char slotChars[MAX_CHARS_PER_LINE];
    int slotDiffs[MAX_CHARS_PER_LINE];

    for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
        slotChars[i] = ' ';
        slotDiffs[i] = 999999;
    }

    for (int i = 0; i < row->charCount; i++) {
        int slot = findClosestCalibratedSlot(row->xs[i]);

        if (slot < 0) {
            continue;
        }

        int diff = abs(row->xs[i] - calibratedX[slot]);

        /*
           If two boxes land in the same slot, keep the closer one.
        */
        if (diff < slotDiffs[slot]) {
            slotDiffs[slot] = diff;
            slotChars[slot] = row->chars[i];
        }
    }

    String result = "";

    for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
        result += slotChars[i];
    }

    return cleanSlottedText(result);
}

/* =========================
   Calibration detection
   ========================= */

bool tryUseRowAsCalibration(struct DetectedRow row)
{
    String expected = buildExpectedCalibrationLine();
    String raw = cleanText(row.rawText);

    if (raw != expected) {
        return false;
    }

    if (row.charCount < MAX_CHARS_PER_LINE) {
        Serial.println("Calibration row text matched but not enough character boxes");
        return false;
    }

    for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
        calibratedX[i] = row.xs[i];
    }

    calibrationReady = true;

    /* Realtime mode starts automatically after START321 calibration. */
    captureActive = true;
    setFpgaDataReady(false);

#if DEBUG_CALIBRATION
    Serial.println();
    Serial.println("========== CALIBRATION READY ==========");
    Serial.print("Calibration line: ");
    Serial.println(raw);

    for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
        Serial.print("slot ");
        Serial.print(i);
        Serial.print(" x=");
        Serial.println(calibratedX[i]);
    }

    Serial.println("=======================================");
    Serial.println();
#endif

    return true;
}

/* =========================
   Extract rows from AI boxes

   Logic:
   1. Read all detected character boxes.
   2. Sort by y.
   3. Group characters with similar y into one row.
   4. Sort each row by x.
   5. Before calibration:
        row text = sorted character string.
   6. After calibration:
        row text = characters placed into calibrated x slots.
        Missing slots become spaces.
   ========================= */

int extractDetectedRows(struct DetectedRow detectedRows[], int maxRowsOut)
{
    int rawBoxCount = AI.boxes().size();

    if (rawBoxCount <= 0) {
        return 0;
    }

    CharBox boxes[MAX_BOXES];
    int boxCount = 0;

    for (int i = 0; i < rawBoxCount && boxCount < MAX_BOXES; i++) {
        int score = AI.boxes()[i].score;

        if (score < MIN_BOX_CONFIDENCE) {
            continue;
        }

        boxes[boxCount].target = AI.boxes()[i].target;
        boxes[boxCount].score  = score;
        boxes[boxCount].x = AI.boxes()[i].x;
        boxes[boxCount].y = AI.boxes()[i].y;
        boxes[boxCount].w = AI.boxes()[i].w;
        boxes[boxCount].h = AI.boxes()[i].h;
        boxes[boxCount].c = decodeChar(boxes[boxCount].target);
        boxCount++;
    }

    if (boxCount <= 0) {
        return 0;
    }

    sortByY(boxes, boxCount);

    int detectedCount = 0;
    int rowStart = 0;

    while (rowStart < boxCount && detectedCount < maxRowsOut) {
        int rowEnd = rowStart + 1;

        int ySum = boxes[rowStart].y;
        int rowBoxCount = 1;

        /*
           Use moving average y because one row may not be perfectly flat.
        */
        while (rowEnd < boxCount) {
            int currentAverageY = ySum / rowBoxCount;

            if (abs(boxes[rowEnd].y - currentAverageY) <= LINE_Y_THRESHOLD) {
                ySum += boxes[rowEnd].y;
                rowBoxCount++;
                rowEnd++;
            } else {
                break;
            }
        }

        int averageY = ySum / rowBoxCount;

        sortByX(boxes, rowStart, rowEnd);

        DetectedRow row;

        row.rawText = "";
        row.text = "";
        row.y = averageY;
        row.minX = boxes[rowStart].x;
        row.maxX = boxes[rowStart].x;
        row.boxCount = rowBoxCount;
        row.charCount = 0;

        for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
            row.chars[i] = ' ';
            row.xs[i] = 0;
        }

        for (int i = rowStart; i < rowEnd; i++) {
            if (boxes[i].x < row.minX) {
                row.minX = boxes[i].x;
            }

            if (boxes[i].x > row.maxX) {
                row.maxX = boxes[i].x;
            }

            if (row.charCount < MAX_CHARS_PER_LINE) {
                row.chars[row.charCount] = boxes[i].c;
                row.xs[row.charCount] = boxes[i].x;
                row.charCount++;
                row.rawText += boxes[i].c;
            }
        }

        row.rawText = cleanText(row.rawText);

        if (calibrationReady) {
            row.text = buildSlottedTextFromRow(&row);
        } else {
            row.text = row.rawText;
        }

        if (countNonSpaceChars(row.text) >= MIN_CANDIDATE_CHARS) {
            detectedRows[detectedCount] = row;
            detectedCount++;
        }

        rowStart = rowEnd;
    }

    /*
       Top row first, bottom row last.
    */
    sortDetectedRowsByY(detectedRows, detectedCount);

#if DEBUG_FRAME_LINES
    Serial.println("Detected rows:");

    for (int i = 0; i < detectedCount; i++) {
        Serial.print("row ");
        Serial.print(i + 1);
        Serial.print(": raw=");
        Serial.print(detectedRows[i].rawText);
        Serial.print(" slot=");
        Serial.print(detectedRows[i].text);
        Serial.print(" y=");
        Serial.print(detectedRows[i].y);
        Serial.print(" boxes=");
        Serial.println(detectedRows[i].boxCount);
    }
#endif

    return detectedCount;
}

/* =========================
   Passage row duplicate guard
   ========================= */

bool frozenPassageAlreadyHas(String text)
{
    text = cleanSlottedText(text);

    for (int i = 0; i < frozenPassageRowCount; i++) {
        if (frozenPassageRows[i] == text) {
            return true;
        }
    }

    return false;
}

String normalizeRowForCompare(String text)
{
    /*
       For duplicate checking only:
       - lowercase
       - remove spaces/punctuation
       - keep only letters/numbers

       Examples:
         "red led"  -> "redled"
         "ed led"   -> "edled"
         "module b" -> "moduleb"
    */
    text = cleanSlottedText(text);
    text.toLowerCase();

    String out = "";

    for (int i = 0; i < text.length(); i++) {
        char c = text[i];

        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out += c;
        }
    }

    return out;
}

bool isCalibrationLikeText(String text)
{
    String n = normalizeRowForCompare(text);
    String expected = normalizeRowForCompare(buildExpectedCalibrationLine());

    if (n.length() == 0) {
        return false;
    }

    if (n == expected) {
        return true;
    }

    /*
       When START321 is already moving out of frame, OCR often sees:
         start32, tart321, sart32, art32, smart321, etc.
       These must never become instruction rows.
    */
    if (n.indexOf("start") >= 0) {
        return true;
    }

    if (n.indexOf("tart") >= 0 && n.indexOf("3") >= 0) {
        return true;
    }

    if (n.indexOf("art") >= 0 && n.indexOf("3") >= 0) {
        return true;
    }

    return false;
}

int limitedEditDistance(String a, String b, int limit)
{
    int la = a.length();
    int lb = b.length();

    if (abs(la - lb) > limit) {
        return limit + 1;
    }

    const int MAX_COMPARE_LEN = 32;

    if (la > MAX_COMPARE_LEN || lb > MAX_COMPARE_LEN) {
        return limit + 1;
    }

    int prev[MAX_COMPARE_LEN + 1];
    int curr[MAX_COMPARE_LEN + 1];

    for (int j = 0; j <= lb; j++) {
        prev[j] = j;
    }

    for (int i = 1; i <= la; i++) {
        curr[0] = i;
        int rowMin = curr[0];

        for (int j = 1; j <= lb; j++) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;

            int delCost = prev[j] + 1;
            int insCost = curr[j - 1] + 1;
            int subCost = prev[j - 1] + cost;

            int best = delCost;
            if (insCost < best) best = insCost;
            if (subCost < best) best = subCost;

            curr[j] = best;
            if (best < rowMin) rowMin = best;
        }

        if (rowMin > limit) {
            return limit + 1;
        }

        for (int j = 0; j <= lb; j++) {
            prev[j] = curr[j];
        }
    }

    return prev[lb];
}

bool isSimilarToFrozenPassageRow(String text, String *matchedRow)
{
    String n = normalizeRowForCompare(text);

    if (n.length() < 3) {
        return false;
    }

    for (int i = 0; i < frozenPassageRowCount; i++) {
        String existing = frozenPassageRows[i];
        String e = normalizeRowForCompare(existing);

        if (e.length() < 3) {
            continue;
        }

        if (n == e) {
            if (matchedRow != NULL) *matchedRow = existing;
            return true;
        }

        /*
           Catch partial edge reads:
             redled  vs edled
             moduleb vs oduleb
             linking vs inking
             every5  vs very5
        */
        if (n.length() >= 4 && e.indexOf(n) >= 0) {
            if (matchedRow != NULL) *matchedRow = existing;
            return true;
        }

        if (e.length() >= 4 && n.indexOf(e) >= 0) {
            if (matchedRow != NULL) *matchedRow = existing;
            return true;
        }

        int maxLen = (n.length() > e.length()) ? n.length() : e.length();
        int limit = 1;

        if (maxLen >= 8) {
            limit = 2;
        }

        if (limitedEditDistance(n, e, limit) <= limit) {
            if (matchedRow != NULL) *matchedRow = existing;
            return true;
        }
    }

    return false;
}

/* =========================
   Row tracker logic
   ========================= */

int findMatchingTracker(String detectedText, int detectedY)
{
    detectedText = cleanSlottedText(detectedText);

    int bestIndex = -1;
    int bestDiff = 9999;

    /*
       Priority 1:
       Match non-frozen tracker with the same text.
       This is more reliable than y because the text scrolls upward.
    */
    for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        if (!rowTrackers[i].active) {
            continue;
        }

        if (rowTrackers[i].frozen) {
            continue;
        }

        if (rowTrackers[i].lastText == detectedText) {
            int diff = abs(rowTrackers[i].y - detectedY);

            if (diff <= SAME_TEXT_Y_THRESHOLD && diff < bestDiff) {
                bestDiff = diff;
                bestIndex = i;
            }
        }
    }

    if (bestIndex >= 0) {
        return bestIndex;
    }

    /*
       Priority 2:
       Match non-frozen tracker by y only.
       This is mainly for the first few frames before text is stable.
    */
    bestIndex = -1;
    bestDiff = 9999;

    for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        if (!rowTrackers[i].active) {
            continue;
        }

        if (rowTrackers[i].frozen) {
            continue;
        }

        int diff = abs(rowTrackers[i].y - detectedY);

        if (diff <= ROW_MATCH_Y_THRESHOLD && diff < bestDiff) {
            bestDiff = diff;
            bestIndex = i;
        }
    }

    return bestIndex;
}

int allocateRowTracker(int detectedY)
{
    for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        if (!rowTrackers[i].active) {
            rowTrackers[i].active = true;
            rowTrackers[i].y = detectedY;
            rowTrackers[i].lastSeenFrame = frameCounter;

            rowTrackers[i].lastText = "";
            rowTrackers[i].stableCount = 0;

            rowTrackers[i].frozen = false;
            rowTrackers[i].frozenText = "";

            return i;
        }
    }

    /*
       If all tracker slots are full, recycle the oldest non-frozen tracker first.
    */
    int oldestIndex = -1;
    int oldestFrame = 999999;

    for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        if (rowTrackers[i].frozen) {
            continue;
        }

        if (rowTrackers[i].lastSeenFrame < oldestFrame) {
            oldestFrame = rowTrackers[i].lastSeenFrame;
            oldestIndex = i;
        }
    }

    /*
       If somehow all are frozen, recycle the oldest one.
       The frozen text is already stored in frozenPassageRows.
    */
    if (oldestIndex < 0) {
        oldestIndex = 0;
        oldestFrame = rowTrackers[0].lastSeenFrame;

        for (int i = 1; i < MAX_VISIBLE_ROWS; i++) {
            if (rowTrackers[i].lastSeenFrame < oldestFrame) {
                oldestFrame = rowTrackers[i].lastSeenFrame;
                oldestIndex = i;
            }
        }
    }

    rowTrackers[oldestIndex].active = true;
    rowTrackers[oldestIndex].y = detectedY;
    rowTrackers[oldestIndex].lastSeenFrame = frameCounter;

    rowTrackers[oldestIndex].lastText = "";
    rowTrackers[oldestIndex].stableCount = 0;

    rowTrackers[oldestIndex].frozen = false;
    rowTrackers[oldestIndex].frozenText = "";

    return oldestIndex;
}



bool isPortalDuplicateBypassRowText(const String &text)
{
    /*
       Portal command rows may legitimately appear more than once in the same
       visible frame/pass.

       Example with MAX_CHARS_PER_LINE = 8:
           snakepor

       Example with MAX_CHARS_PER_LINE = 7:
           snakepo

       If exact-duplicate filtering blocks the second portal header/chunk, the
       decoder may only receive one portal side and keep waiting forever.

       Therefore, any row that looks like the beginning of SNAKEPORTAL bypasses
       exact duplicate filtering. Normal rows still use exact duplicate guards.
    */
    String n = normalizeRowForCompare(text);

    if (n.length() == 0) {
        return false;
    }

    if (n.startsWith("snakepo")) {
        return true;
    }

    /*
       OCR sometimes drops or changes the first character while the row is near
       the edge. Treat these as portal-header-like too.
    */
    if (n.startsWith("nakepo")) {
        return true;
    }

    if (n.startsWith("5nakepo")) {
        return true;
    }

    return false;
}

bool sameExactRowText(const String &aText, const String &bText)
{
    return aText == bText;
}

bool pendingQueueAlreadyHasExactRow(const String &text)
{
    for (int i = 0; i < pendingFrozenRowCount; i++) {
        if (!pendingFrozenRows[i].active) {
            continue;
        }

        if (sameExactRowText(pendingFrozenRows[i].text, text)) {
            return true;
        }
    }

    return false;
}

bool recentSentAlreadyHasExactRow(const String &text)
{
    unsigned long nowMs = millis();

    for (int i = 0; i < recentSentCount; i++) {
        if (nowMs - recentSentRows[i].sentMs > RECENT_SENT_DUP_WINDOW_MS) {
            continue;
        }

        if (sameExactRowText(recentSentRows[i].text, text)) {
            return true;
        }
    }

    return false;
}

void rememberRecentSentRow(const String &text, int y)
{
    int index;

    if (recentSentCount < RECENT_SENT_ROW_MAX) {
        index = recentSentCount;
        recentSentCount++;
    } else {
        index = 0;

        for (int i = 1; i < RECENT_SENT_ROW_MAX; i++) {
            if (recentSentRows[i].sentMs < recentSentRows[index].sentMs) {
                index = i;
            }
        }
    }

    recentSentRows[index].text = text;
    recentSentRows[index].y = y;
    recentSentRows[index].sentMs = millis();
}

void resetDuplicateGuardsAfterInvokeFails()
{
    /*
       AI.invoke failed enough times, so the visible scroll/capture pass has
       ended. Reset exact-duplicate memory so the same instruction rows can be
       accepted again in the next pass.

       Inside one pass:
           exact duplicate row -> do not send

       After invoke-fail/end-of-pass reset:
           same row can be registered again
    */

    int i;

    for (i = 0; i < RECENT_SENT_ROW_MAX; i++) {
        recentSentRows[i].text = "";
        recentSentRows[i].y = 0;
        recentSentRows[i].sentMs = 0;
    }
    recentSentCount = 0;

    for (i = 0; i < MAX_PASSAGE_ROWS; i++) {
        frozenPassageRows[i] = "";
    }
    frozenPassageRowCount = 0;

    for (i = 0; i < MAX_PENDING_SEND_ROWS; i++) {
        pendingFrozenRows[i].active = false;
        pendingFrozenRows[i].text = "";
        pendingFrozenRows[i].y = 0;
        pendingFrozenRows[i].frozenFrame = -9999;
        pendingFrozenRows[i].orderKey = 0;
        pendingFrozenRows[i].seq = 0;
    }
    pendingFrozenRowCount = 0;
    pendingFrozenSeq = 0;
    lastPendingFrozenQueueFrame = -9999;
    rowOrderForceFlush = false;
    duplicateResetAfterPendingFlush = false;

    for (i = 0; i < MAX_VISIBLE_ROWS; i++) {
        rowTrackers[i].active = false;
        rowTrackers[i].y = 0;
        rowTrackers[i].lastSeenFrame = -9999;
        rowTrackers[i].lastText = "";
        rowTrackers[i].stableCount = 0;
        rowTrackers[i].frozen = false;
        rowTrackers[i].frozenText = "";
    }

    Serial.println("[DUP RESET] AI invoke failed/end of pass -> exact duplicate memory cleared");
}

bool isUsefulSnakeRowText(const String &text)
{
    String t = cleanSlottedText(text);

    /*
       Accept ANY row from 1 character up to MAX_CHARS_PER_LINE.

       The only rows blocked here are calibration-like START321 rows
       and fully empty rows.

       This means all of these can freeze/send:
           1
           x
           y
           18
           display
           snakewal
           lx4y1x4y
           2x4y3x4y

       Exact duplicate filtering still happens later in the pending/recent
       row queue, so accepting 1-char rows here will not break the duplicate
       guard.
    */

    if (countNonSpaceChars(t) < 1) {
        return false;
    }

    if (isCalibrationLikeText(t)) {
        return false;
    }

    return true;
}

void queueFrozenRowForOrderedSend(int trackerIndex, String finalRowText)
{
    if (trackerIndex < 0 || trackerIndex >= MAX_VISIBLE_ROWS) {
        return;
    }

    finalRowText = cleanSlottedText(finalRowText);

    if (!isUsefulSnakeRowText(finalRowText)) {
        Serial.print("[ROW ORDER] non-useful short/tail row ignored: ");
        Serial.println(finalRowText);
        return;
    }

    int rowY = rowTrackers[trackerIndex].y;

    /*
       Original exact duplicate behavior:
       if the exact cleaned row text is already pending or was recently sent
       in this capture pass, do not send it again.

       Duplicate memory is reset when AI.invoke fails enough times, so the next
       scroll/capture pass can register the same rows again.

       Similar coordinate rows are still accepted because the text is different:
           5x2y6x2y
           7x2y8x2y
           3x3y4x3y
    */
    bool bypassDuplicateGuard = isPortalDuplicateBypassRowText(finalRowText);

    if (!bypassDuplicateGuard && pendingQueueAlreadyHasExactRow(finalRowText)) {
        Serial.print("[ROW ORDER] pending exact duplicate ignored text=[");
        Serial.print(finalRowText);
        Serial.println("]");
        return;
    }

    if (!bypassDuplicateGuard && recentSentAlreadyHasExactRow(finalRowText)) {
        Serial.print("[ROW ORDER] recent exact duplicate ignored text=[");
        Serial.print(finalRowText);
        Serial.println("]");
        return;
    }

    if (bypassDuplicateGuard) {
        Serial.print("[ROW ORDER] portal row bypass duplicate guard text=[");
        Serial.print(finalRowText);
        Serial.println("]");
    }

    if (pendingFrozenRowCount >= MAX_PENDING_SEND_ROWS) {
        Serial.println("[ROW ORDER] pending row buffer full, trying to flush before queueing");
        flushPendingFrozenRows();
    }

    if (pendingFrozenRowCount >= MAX_PENDING_SEND_ROWS) {
        Serial.println("[ROW ORDER] pending row buffer still full, dropping newest row");
        Serial.print("[ROW ORDER] dropped: ");
        Serial.println(finalRowText);
        return;
    }

    PendingFrozenRow *slot = &pendingFrozenRows[pendingFrozenRowCount];
    slot->active = true;
    slot->text = finalRowText;
    slot->y = rowY;
    slot->frozenFrame = frameCounter;
    slot->orderKey = (long)rowY + ((long)ROW_ORDER_SCROLL_PIXELS_PER_FRAME * (long)frameCounter);
    slot->seq = pendingFrozenSeq++;
    pendingFrozenRowCount++;
    lastPendingFrozenQueueFrame = frameCounter;

    Serial.print("[ROW ORDER] queued row y=");
    Serial.print(slot->y);
    Serial.print(" frame=");
    Serial.print(slot->frozenFrame);
    Serial.print(" orderKey=");
    Serial.print(slot->orderKey);
    Serial.print(" text=[");
    Serial.print(slot->text);
    Serial.println("]");
}

void sortPendingFrozenRowsByOrderKey()
{
    for (int i = 0; i < pendingFrozenRowCount - 1; i++) {
        for (int j = i + 1; j < pendingFrozenRowCount; j++) {
            bool swapRows = false;

            if (pendingFrozenRows[j].orderKey < pendingFrozenRows[i].orderKey) {
                swapRows = true;
            } else if (pendingFrozenRows[j].orderKey == pendingFrozenRows[i].orderKey &&
                       pendingFrozenRows[j].seq < pendingFrozenRows[i].seq) {
                swapRows = true;
            }

            if (swapRows) {
                PendingFrozenRow temp = pendingFrozenRows[i];
                pendingFrozenRows[i] = pendingFrozenRows[j];
                pendingFrozenRows[j] = temp;
            }
        }
    }
}

void removePendingFrozenRowAt(int index)
{
    if (index < 0 || index >= pendingFrozenRowCount) {
        return;
    }

    for (int i = index; i < pendingFrozenRowCount - 1; i++) {
        pendingFrozenRows[i] = pendingFrozenRows[i + 1];
    }

    pendingFrozenRowCount--;

    if (pendingFrozenRowCount >= 0 && pendingFrozenRowCount < MAX_PENDING_SEND_ROWS) {
        pendingFrozenRows[pendingFrozenRowCount].active = false;
        pendingFrozenRows[pendingFrozenRowCount].text = "";
        pendingFrozenRows[pendingFrozenRowCount].y = 0;
        pendingFrozenRows[pendingFrozenRowCount].frozenFrame = -9999;
        pendingFrozenRows[pendingFrozenRowCount].orderKey = 0;
        pendingFrozenRows[pendingFrozenRowCount].seq = 0;
    }
}

void flushPendingFrozenRows()
{
    /*
       Do NOT return just because packetReadyForSpi is true.
       The SPI DMA task owns packet transmission; the AI loop keeps scanning.
       If the SPI packet queue is full, keep this row pending and retry later.
    */
    if (pendingFrozenRowCount <= 0) {
        if (duplicateResetAfterPendingFlush) {
            rowOrderForceFlush = false;
            duplicateResetAfterPendingFlush = false;
            resetDuplicateGuardsAfterInvokeFails();
        }
        return;
    }

    int oldestFrame = pendingFrozenRows[0].frozenFrame;

    for (int i = 1; i < pendingFrozenRowCount; i++) {
        if (pendingFrozenRows[i].frozenFrame < oldestFrame) {
            oldestFrame = pendingFrozenRows[i].frozenFrame;
        }
    }

    int oldestAge = frameCounter - oldestFrame;
    int quietAge = frameCounter - lastPendingFrozenQueueFrame;

    if (!rowOrderForceFlush) {
        bool quietEnough = (quietAge >= ROW_SEND_QUIET_FRAMES);
        bool waitedTooLong = (oldestAge >= ROW_SEND_MAX_HOLD_FRAMES);
        bool queueAlmostFull = (pendingFrozenRowCount >= ROW_SEND_FORCE_FLUSH_COUNT);

        if (!quietEnough && !waitedTooLong && !queueAlmostFull) {
            return;
        }
    }

    sortPendingFrozenRowsByOrderKey();

    String sendText = pendingFrozenRows[0].text;
    sendText = cleanSlottedText(sendText);

    if (sendText.length() == 0) {
        removePendingFrozenRowAt(0);
        return;
    }

    latestPassageText = sendText;
    passageReadyForSpi = true;

    latestGrayImageValid = false;
    memset(latestGrayImage, 0, sizeof(latestGrayImage));

    Serial.print("[ROW ORDER] enqueue row key=");
    Serial.print(pendingFrozenRows[0].orderKey);
    Serial.print(" y=");
    Serial.print(pendingFrozenRows[0].y);
    Serial.print(" text=[");
    Serial.print(sendText);
    Serial.print("] pendingRows=");
    Serial.print(pendingFrozenRowCount);
    Serial.print(" spiQ=");
    Serial.println(spiPacketQueueCount);

    if (!buildFinalPacket()) {
        Serial.println("[ROW ORDER] SPI packet queue full, keep row pending and continue AI.invoke");
        return;
    }

    rememberRecentSentRow(sendText, pendingFrozenRows[0].y);
    removePendingFrozenRowAt(0);
}



void freezeRow(int trackerIndex)
{
    if (trackerIndex < 0 || trackerIndex >= MAX_VISIBLE_ROWS) {
        return;
    }

    if (rowTrackers[trackerIndex].frozen) {
        return;
    }

    if (frozenPassageRowCount >= MAX_PASSAGE_ROWS) {
        Serial.println("Frozen passage row memory full");
        return;
    }

    String finalRowText = rowTrackers[trackerIndex].lastText;
    finalRowText = cleanSlottedText(finalRowText);

    if (!isUsefulSnakeRowText(finalRowText)) {
        rowTrackers[trackerIndex].frozen = true;
        rowTrackers[trackerIndex].frozenText = finalRowText;

        Serial.print("Non-useful short/tail row ignored from passage: ");
        Serial.println(finalRowText);
        return;
    }

    rowTrackers[trackerIndex].frozen = true;
    rowTrackers[trackerIndex].frozenText = finalRowText;

    /*
       Keep accepted frozen rows in the history for debugging. Exact duplicate
       sending is now filtered in the pending/recent row queue by text only.
    */
    frozenPassageRows[frozenPassageRowCount] = finalRowText;
    frozenPassageRowCount++;

    Serial.print("FROZEN INSTRUCTION ROW ");
    Serial.print(frozenPassageRowCount);
    Serial.print(": y=");
    Serial.print(rowTrackers[trackerIndex].y);
    Serial.print(" text=[");
    Serial.print(finalRowText);
    Serial.println("]");

    /*
       Queue instead of sending immediately. The ordered sender holds rows for a
       few frames, sorts by ascending y, then sends one SPI packet at a time.
    */
    queueFrozenRowForOrderedSend(trackerIndex, finalRowText);
}

void updateRowTrackerWithDetectedRow(struct DetectedRow detected)
{
    /*
       Before calibration is ready, only look for the START... row.
       Do not freeze real instruction rows yet because their x slots
       are not reliable.
    */
    if (!calibrationReady) {
        tryUseRowAsCalibration(detected);
        return;
    }

    String text = cleanSlottedText(detected.text);

    /*
       Calibration is allowed before capture, but instruction storage starts
       only after START321 calibration enables realtime capture.
    */
    if (!captureActive) {
        return;
    }

    if (!isUsefulSnakeRowText(text)) {
        return;
    }

    int trackerIndex = findMatchingTracker(text, detected.y);

    if (trackerIndex < 0) {
        trackerIndex = allocateRowTracker(detected.y);
    }

    /*
       Text scrolls upward, so y changes every frame.
       Always update y and lastSeenFrame.
    */
    rowTrackers[trackerIndex].y = detected.y;
    rowTrackers[trackerIndex].lastSeenFrame = frameCounter;

    if (rowTrackers[trackerIndex].frozen) {
#if DEBUG_ROW_TRACKERS
        Serial.print("Tracker ");
        Serial.print(trackerIndex);
        Serial.print(" already frozen as ");
        Serial.println(rowTrackers[trackerIndex].frozenText);
#endif
        return;
    }

    /*
       Stability rule:
       - full max-character row: freeze after ROW_FREEZE_CONFIRM_COUNT
       - short row: freeze after SHORT_ROW_FREEZE_CONFIRM_COUNT

       Because spaces are now accepted, use raw String length after trimming.
       Example:
         "red led" length 7
         "display hex" length 11
    */
    if (rowTrackers[trackerIndex].lastText == text) {
        rowTrackers[trackerIndex].stableCount++;
    } else {
        rowTrackers[trackerIndex].lastText = text;
        rowTrackers[trackerIndex].stableCount = 1;
    }

#if DEBUG_ROW_TRACKERS
    Serial.print("Tracker ");
    Serial.print(trackerIndex);
    Serial.print(" y=");
    Serial.print(rowTrackers[trackerIndex].y);
    Serial.print(" text=[");
    Serial.print(rowTrackers[trackerIndex].lastText);
    Serial.print("] stable=");
    Serial.print(rowTrackers[trackerIndex].stableCount);

    if (text.length() >= MAX_CHARS_PER_LINE) {
        Serial.print("/");
        Serial.println(ROW_FREEZE_CONFIRM_COUNT);
    } else {
        Serial.print("/");
        Serial.println(SHORT_ROW_FREEZE_CONFIRM_COUNT);
    }
#endif

    if (text.length() >= MAX_CHARS_PER_LINE &&
        rowTrackers[trackerIndex].stableCount >= ROW_FREEZE_CONFIRM_COUNT) {
        freezeRow(trackerIndex);
    }

    if (text.length() < MAX_CHARS_PER_LINE &&
        rowTrackers[trackerIndex].stableCount >= SHORT_ROW_FREEZE_CONFIRM_COUNT) {
        freezeRow(trackerIndex);
    }
}

void expireOldRowTrackers()
{
    for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        if (!rowTrackers[i].active) {
            continue;
        }

        if ((frameCounter - rowTrackers[i].lastSeenFrame) > ROW_STALE_FRAMES) {
            rowTrackers[i].active = false;
            rowTrackers[i].y = 0;
            rowTrackers[i].lastSeenFrame = -9999;

            rowTrackers[i].lastText = "";
            rowTrackers[i].stableCount = 0;

            rowTrackers[i].frozen = false;
            rowTrackers[i].frozenText = "";
        }
    }
}

void processDetectedRows(struct DetectedRow detectedRows[], int detectedCount)
{
    /*
       Process top to bottom.
    */
    for (int i = 0; i < detectedCount; i++) {
        updateRowTrackerWithDetectedRow(detectedRows[i]);
    }

    expireOldRowTrackers();
    flushPendingFrozenRows();
}


/* =========================
   New capture helpers
   ========================= */

void resetCaptureRowsKeepCalibration()
{
    for (int i = 0; i < MAX_PASSAGE_ROWS; i++) {
        frozenPassageRows[i] = "";
    }

    for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        rowTrackers[i].active = false;
        rowTrackers[i].y = 0;
        rowTrackers[i].lastSeenFrame = -9999;
        rowTrackers[i].lastText = "";
        rowTrackers[i].stableCount = 0;
        rowTrackers[i].frozen = false;
        rowTrackers[i].frozenText = "";
    }

    for (int i = 0; i < MAX_PENDING_SEND_ROWS; i++) {
        pendingFrozenRows[i].active = false;
        pendingFrozenRows[i].text = "";
        pendingFrozenRows[i].y = 0;
        pendingFrozenRows[i].frozenFrame = -9999;
        pendingFrozenRows[i].orderKey = 0;
        pendingFrozenRows[i].seq = 0;
    }
    pendingFrozenRowCount = 0;
    pendingFrozenSeq = 0;
    lastPendingFrozenQueueFrame = -9999;
    rowOrderForceFlush = false;
    duplicateResetAfterPendingFlush = false;

    for (int i = 0; i < RECENT_SENT_ROW_MAX; i++) {
        recentSentRows[i].text = "";
        recentSentRows[i].y = 0;
        recentSentRows[i].sentMs = 0;
    }
    recentSentCount = 0;

    frozenPassageRowCount = 0;
    consecutiveInvokeFails = 0;

    latestPassageText = "";
    passageReadyForSpi = false;

    latestGrayImageValid = false;
    memset(latestGrayImage, 0, sizeof(latestGrayImage));

    storedJpegBase64 = "";
    imageUpdatedAfterFourRows = false;

    clearSpiPacketQueue();
}

String buildFullPassageWithNewlines()
{
    String passage = "";

    for (int i = 0; i < frozenPassageRowCount; i++) {
        passage += frozenPassageRows[i];

        if (i < frozenPassageRowCount - 1) {
            passage += "\n";
        }
    }

    return passage;
}

bool isAlphaString(String text)
{
    if (text.length() == 0) {
        return false;
    }

    for (int i = 0; i < text.length(); i++) {
        char c = text[i];

        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
            return false;
        }
    }

    return true;
}

String getLastToken(String text)
{
    text = cleanSlottedText(text);

    int endIndex = text.length() - 1;

    while (endIndex >= 0 && text[endIndex] == ' ') {
        endIndex--;
    }

    if (endIndex < 0) {
        return "";
    }

    int startIndex = endIndex;

    while (startIndex >= 0 && text[startIndex] != ' ') {
        startIndex--;
    }

    return text.substring(startIndex + 1, endIndex + 1);
}

String getFirstToken(String text)
{
    text = cleanSlottedText(text);

    int startIndex = 0;

    while (startIndex < text.length() && text[startIndex] == ' ') {
        startIndex++;
    }

    int endIndex = startIndex;

    while (endIndex < text.length() && text[endIndex] != ' ') {
        endIndex++;
    }

    return text.substring(startIndex, endIndex);
}

bool shouldJoinRowsWithoutSpace(String currentPassage, String nextRow)
{
    /*
       Newline does NOT automatically mean a space.

       Example from the scrolling display:
         row N   = "module b"
         row N+1 = "linking"

       This should become:
         "module blinking"

       not:
         "module b linking"

       Heuristic: if the previous last token is a tiny alphabetic fragment
       such as "b" and the next row starts with a longer alphabetic token,
       concatenate directly.
    */
    String lastToken = getLastToken(currentPassage);
    String firstToken = getFirstToken(nextRow);

    if (!isAlphaString(lastToken) || !isAlphaString(firstToken)) {
        return false;
    }

    if (lastToken.length() <= 2 && firstToken.length() >= 3) {
        return true;
    }

    return false;
}

String buildFullPassageForSpiSpaces()
{
    /*
       For FPGA/VGA side, keep the instruction as one sentence, but do not
       blindly insert a space for every OCR row break. Some words are split
       across rows by the scrolling display, for example:
         "module b" + "linking" -> "module blinking"
    */
    String passage = "";

    for (int i = 0; i < frozenPassageRowCount; i++) {
        String row = frozenPassageRows[i];
        row = cleanSlottedText(row);

        if (row.length() == 0) {
            continue;
        }

        if (passage.length() == 0) {
            passage = row;
        } else if (shouldJoinRowsWithoutSpace(passage, row)) {
            passage += row;
        } else {
            passage += " ";
            passage += row;
        }
    }

    passage.toLowerCase();
    return passage;
}

void printFrozenRows()
{
    Serial.println("Frozen rows:");

    for (int i = 0; i < frozenPassageRowCount; i++) {
        Serial.print("line ");
        Serial.print(i + 1);
        Serial.print(": [");
        Serial.print(frozenPassageRows[i]);
        Serial.println("]");
    }

    Serial.println("----------------");
}

/* =========================
   Base64 helpers
   ========================= */

int b64Index(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::vector<uint8_t> base64DecodeToBytes(String s)
{
    int comma = s.indexOf(',');
    if (comma >= 0) {
        s = s.substring(comma + 1);
    }

    s.trim();

    std::vector<uint8_t> out;
    out.reserve((s.length() * 3) / 4 + 4);

    int val = 0;
    int valb = -8;

    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];

        if (c == '=') {
            break;
        }

        int idx = b64Index(c);

        if (idx < 0) {
            continue;
        }

        val = (val << 6) | idx;
        valb += 6;

        if (valb >= 0) {
            out.push_back((uint8_t)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return out;
}

/* =========================
   Image capture / decode
   ========================= */

void storeCurrentJpegString(const char *reason)
{
#if ENABLE_IMAGE_CAPTURE
    /*
       This is intentionally called only at capture start and once after
       more than 4 rows are stored. JPEG decoding is NOT done here.
    */
    Serial.print("[IMG] store JPEG string only: ");
    Serial.println(reason);

    String img = AI.last_image();

    if (img.length() == 0) {
        /*
           If last_image() is empty, request one image frame once.
           This is the only heavy image operation during capture.
        */
        if (!AI.invoke(1, false, true)) {
            img = AI.last_image();
        }
    }

    if (img.length() > 0) {
        storedJpegBase64 = img;
        Serial.print("[IMG] stored JPEG chars=");
        Serial.println(storedJpegBase64.length());
    } else {
        Serial.println("[IMG] no JPEG available yet");
    }
#else
    (void)reason;
#endif
}

bool decodeStoredJpegToGray96()
{
#if ENABLE_IMAGE_CAPTURE
    latestGrayImageValid = false;
    memset(latestGrayImage, 0, sizeof(latestGrayImage));

    if (storedJpegBase64.length() == 0) {
        Serial.println("[IMG] no stored JPEG, image payload disabled");
        return false;
    }

    Serial.println("[IMG] final decode JPEG -> 96x96 grayscale now");

    std::vector<uint8_t> jpegBytes = base64DecodeToBytes(storedJpegBase64);

    if (jpegBytes.empty()) {
        Serial.println("[IMG] base64 decode failed");
        return false;
    }

    Serial.print("[IMG] JPEG bytes=");
    Serial.println((unsigned int)jpegBytes.size());

    int ok = JpegDec.decodeArray(jpegBytes.data(), jpegBytes.size());

    if (!ok) {
        Serial.println("[IMG] JPEGDecoder.decodeArray failed");
        return false;
    }

    int fullW = JpegDec.width;
    int fullH = JpegDec.height;

    if (fullW <= 0 || fullH <= 0) {
        Serial.println("[IMG] invalid JPEG width/height");
        return false;
    }

    Serial.print("[IMG] JPEG size=");
    Serial.print(fullW);
    Serial.print("x");
    Serial.println(fullH);

    uint8_t *fullGray = (uint8_t *)malloc((size_t)fullW * (size_t)fullH);

    if (!fullGray) {
        Serial.println("[IMG] fullGray malloc failed");
        return false;
    }

    memset(fullGray, 0, (size_t)fullW * (size_t)fullH);

    int mcuW = JpegDec.MCUWidth;
    int mcuH = JpegDec.MCUHeight;

    while (JpegDec.read()) {
        uint16_t *pImg = JpegDec.pImage;
        int mcuX = JpegDec.MCUx * mcuW;
        int mcuY = JpegDec.MCUy * mcuH;

        int copyW = ((mcuX + mcuW) <= fullW) ? mcuW : (fullW - mcuX);
        int copyH = ((mcuY + mcuH) <= fullH) ? mcuH : (fullH - mcuY);

        for (int y = 0; y < copyH; y++) {
            for (int x = 0; x < copyW; x++) {
                uint16_t c = pImg[y * mcuW + x];

                uint8_t r = (uint8_t)((((c >> 11) & 0x1F) * 255) / 31);
                uint8_t g = (uint8_t)((((c >> 5) & 0x3F) * 255) / 63);
                uint8_t b = (uint8_t)(((c & 0x1F) * 255) / 31);

                uint8_t grayPix = (uint8_t)((77 * r + 150 * g + 29 * b) >> 8);
                fullGray[(mcuY + y) * fullW + (mcuX + x)] = grayPix;
            }
        }
    }

    for (int y = 0; y < GRAY_H; y++) {
        int srcY = (y * fullH) / GRAY_H;

        for (int x = 0; x < GRAY_W; x++) {
            int srcX = (x * fullW) / GRAY_W;
            latestGrayImage[y * GRAY_W + x] = fullGray[srcY * fullW + srcX];
        }
    }

    free(fullGray);

    latestGrayImageValid = true;

    Serial.print("[IMG] grayscale payload bytes=");
    Serial.println(GRAY_BYTES);

    return true;
#else
    return false;
#endif
}


/* =========================
   Capture finalization
   ========================= */

void finalizeCapture()
{
    Serial.println("[CAPTURE] invoke-fail end -> finalize passage");

    printFrozenRows();

    latestPassageText = buildFullPassageForSpiSpaces();

    Serial.println();
    Serial.println("========== FULL PASSAGE ==========");
    Serial.println(buildFullPassageWithNewlines());
    Serial.println("==================================");
    Serial.print("SPI passage with spaces only: ");
    Serial.println(latestPassageText);
    Serial.println();

#if ENABLE_IMAGE_CAPTURE
    decodeStoredJpegToGray96();
#else
    latestGrayImageValid = false;
#endif

    buildFinalPacket();

    /*
       Keep captureActive true while packetReadyForSpi is true.
       The SPI DMA task releases captureActive only after FPGA reads 0xA2 packet.
    */
    consecutiveInvokeFails = 0;
}

/* =========================
   Final packet build / print
   ========================= */

void writeU16LE(uint8_t *buf, int offset, uint16_t value)
{
    buf[offset + 0] = (uint8_t)(value & 0xFF);
    buf[offset + 1] = (uint8_t)((value >> 8) & 0xFF);
}

void writeU32LE(uint8_t *buf, int offset, uint32_t value)
{
    buf[offset + 0] = (uint8_t)(value & 0xFF);
    buf[offset + 1] = (uint8_t)((value >> 8) & 0xFF);
    buf[offset + 2] = (uint8_t)((value >> 16) & 0xFF);
    buf[offset + 3] = (uint8_t)((value >> 24) & 0xFF);
}

bool promoteQueuedPacketIfIdle()
{
    bool shouldRaiseReady = false;
    bool hasPacket = false;

    portENTER_CRITICAL(&spiPacketMux);

    if (!packetReadyForSpi && spiPacketQueueCount > 0) {
        SpiQueuedPacket *q = &spiPacketQueue[spiPacketQueueHead];

        finalPacketLen = q->len;
        memcpy(finalPacket, q->data, finalPacketLen);

        q->len = 0;
        memset(q->data, 0, PACKET_MAX_LEN);

        spiPacketQueueHead = (spiPacketQueueHead + 1) % SPI_PACKET_QUEUE_DEPTH;
        spiPacketQueueCount--;

        packetReadyForSpi = true;
        shouldRaiseReady = true;
    }

    hasPacket = packetReadyForSpi && finalPacketLen > 0;

    portEXIT_CRITICAL(&spiPacketMux);

    if (shouldRaiseReady) {
        setFpgaDataReady(true);
    }

    return hasPacket;
}

bool queuePacketBytesForSpi(const uint8_t *data, uint32_t len)
{
    bool shouldRaiseReady = false;
    bool ok = false;

    if (data == NULL || len == 0 || len > PACKET_MAX_LEN) {
        return false;
    }

    portENTER_CRITICAL(&spiPacketMux);

    if (!packetReadyForSpi && finalPacketLen == 0) {
        memcpy(finalPacket, data, len);
        finalPacketLen = len;
        packetReadyForSpi = true;
        shouldRaiseReady = true;
        ok = true;
    } else if (spiPacketQueueCount < SPI_PACKET_QUEUE_DEPTH) {
        SpiQueuedPacket *slot = &spiPacketQueue[spiPacketQueueTail];
        memset(slot->data, 0, PACKET_MAX_LEN);
        memcpy(slot->data, data, len);
        slot->len = len;

        spiPacketQueueTail = (spiPacketQueueTail + 1) % SPI_PACKET_QUEUE_DEPTH;
        spiPacketQueueCount++;
        ok = true;
    } else {
        ok = false;
    }

    portEXIT_CRITICAL(&spiPacketMux);

    if (shouldRaiseReady) {
        setFpgaDataReady(true);
    }

    return ok;
}

bool buildFinalPacket()
{
    memset(packetBuildScratch, 0, sizeof(packetBuildScratch));

    String sendText = latestPassageText;

    if (sendText.length() > PACKET_MAX_TEXT_LEN) {
        sendText = sendText.substring(0, PACKET_MAX_TEXT_LEN);
    }

    uint16_t textLen = (uint16_t)sendText.length();
    uint32_t imageLen = latestGrayImageValid ? GRAY_BYTES : 0;
    uint16_t imageW = latestGrayImageValid ? GRAY_W : 0;
    uint16_t imageH = latestGrayImageValid ? GRAY_H : 0;
    uint16_t flags = latestGrayImageValid ? PACKET_FLAG_IMAGE_VALID : 0;

    uint32_t packetLen = PACKET_HEADER_LEN + textLen + imageLen;

    if (packetLen > PACKET_MAX_LEN) {
        Serial.println("[PACKET] packet too large, dropping image");
        imageLen = 0;
        imageW = 0;
        imageH = 0;
        flags = 0;
        packetLen = PACKET_HEADER_LEN + textLen;
    }

    if (packetLen == 0 || packetLen > PACKET_MAX_LEN) {
        Serial.println("[PACKET] packet build failed");
        return false;
    }

    packetBuildScratch[0] = PACKET_MAGIC0;
    packetBuildScratch[1] = PACKET_MAGIC1;
    packetBuildScratch[2] = PACKET_VERSION;
    packetBuildScratch[3] = PACKET_STATUS_DONE;

    writeU32LE(packetBuildScratch, 4, packetLen);
    writeU16LE(packetBuildScratch, 8, textLen);
    writeU16LE(packetBuildScratch, 10, imageW);
    writeU16LE(packetBuildScratch, 12, imageH);
    writeU16LE(packetBuildScratch, 14, (uint16_t)frozenPassageRowCount);
    writeU32LE(packetBuildScratch, 16, imageLen);
    writeU16LE(packetBuildScratch, 20, PACKET_HEADER_LEN);
    writeU16LE(packetBuildScratch, 22, flags);

    for (int i = 0; i < textLen; i++) {
        packetBuildScratch[PACKET_HEADER_LEN + i] = (uint8_t)sendText[i];
    }

    if (imageLen > 0) {
        memcpy(&packetBuildScratch[PACKET_HEADER_LEN + textLen], latestGrayImage, imageLen);
    }

    if (!queuePacketBytesForSpi(packetBuildScratch, packetLen)) {
        return false;
    }

    Serial.print("[SPIQ] queued/presented packet len=");
    Serial.print(packetLen);
    Serial.print(" text=[");
    Serial.print(sendText);
    Serial.print("] q=");
    Serial.println(spiPacketQueueCount);

    return true;
}



void writeU32LEToBuffer(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
    p[2] = (uint8_t)((value >> 16) & 0xFF);
    p[3] = (uint8_t)((value >> 24) & 0xFF);
}

void writeU16LEToBuffer(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
}

void printBase64Stream(const uint8_t *data, size_t len)
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (size_t i = 0; i < len; i += 3) {
        uint32_t a = data[i];
        uint32_t b = (i + 1 < len) ? data[i + 1] : 0;
        uint32_t c = (i + 2 < len) ? data[i + 2] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;

        Serial.print(table[(triple >> 18) & 0x3F]);
        Serial.print(table[(triple >> 12) & 0x3F]);
        Serial.print((i + 1 < len) ? table[(triple >> 6) & 0x3F] : '=');
        Serial.print((i + 2 < len) ? table[triple & 0x3F] : '=');
    }
}

void printGrayscalePayloadBmpBase64OneLine()
{
    if (!latestGrayImageValid) {
        Serial.println("bmp_base64=");
        return;
    }

    /*
       Debug viewer format only:
       - packet still sends raw 96x96 grayscale bytes
       - this print wraps those same bytes as an 8-bit grayscale BMP
    */
    const uint32_t fileHeaderSize = 14;
    const uint32_t dibHeaderSize = 40;
    const uint32_t paletteSize = 256 * 4;
    const uint32_t pixelOffset = fileHeaderSize + dibHeaderSize + paletteSize;
    const uint32_t rowStride = (uint32_t)((GRAY_W + 3) & ~3);
    const uint32_t pixelSize = rowStride * (uint32_t)GRAY_H;
    const uint32_t bmpSize = pixelOffset + pixelSize;

    uint8_t *bmp = (uint8_t *)malloc(bmpSize);
    if (!bmp) {
        Serial.println("[BMP] malloc failed");
        return;
    }

    memset(bmp, 0, bmpSize);

    bmp[0] = 'B';
    bmp[1] = 'M';
    writeU32LEToBuffer(&bmp[2], bmpSize);
    writeU32LEToBuffer(&bmp[10], pixelOffset);

    writeU32LEToBuffer(&bmp[14], dibHeaderSize);
    writeU32LEToBuffer(&bmp[18], (uint32_t)GRAY_W);
    writeU32LEToBuffer(&bmp[22], (uint32_t)GRAY_H);
    writeU16LEToBuffer(&bmp[26], 1);
    writeU16LEToBuffer(&bmp[28], 8);
    writeU32LEToBuffer(&bmp[30], 0);
    writeU32LEToBuffer(&bmp[34], pixelSize);
    writeU32LEToBuffer(&bmp[46], 256);
    writeU32LEToBuffer(&bmp[50], 256);

    uint32_t paletteBase = fileHeaderSize + dibHeaderSize;
    for (int i = 0; i < 256; i++) {
        bmp[paletteBase + i * 4 + 0] = (uint8_t)i;
        bmp[paletteBase + i * 4 + 1] = (uint8_t)i;
        bmp[paletteBase + i * 4 + 2] = (uint8_t)i;
        bmp[paletteBase + i * 4 + 3] = 0;
    }

    for (int y = 0; y < GRAY_H; y++) {
        int srcY = GRAY_H - 1 - y;
        memcpy(&bmp[pixelOffset + (uint32_t)y * rowStride],
               &latestGrayImage[(uint32_t)srcY * GRAY_W],
               GRAY_W);
    }

    Serial.println();
    Serial.println("========== GRAYSCALE 96x96 BMP BASE64 BEGIN ==========");
    Serial.print("decoded_width=");
    Serial.println(GRAY_W);
    Serial.print("decoded_height=");
    Serial.println(GRAY_H);
    Serial.print("bmp_file_bytes=");
    Serial.println(bmpSize);
    Serial.print("raw_gray_payload_bytes=");
    Serial.println(GRAY_BYTES);
    Serial.print("bmp_base64=");
    printBase64Stream(bmp, bmpSize);
    Serial.println();
    Serial.println("========== GRAYSCALE 96x96 BMP BASE64 END ==========");
    Serial.println();

    free(bmp);
}

void printPacketReadySummary()
{
    Serial.println();
    Serial.println("========== PACKET READY FOR FPGA ==========");
    Serial.print("packet_len=");
    Serial.println(finalPacketLen);
    Serial.print("text_len=");
    Serial.println(latestPassageText.length());
    Serial.print("line_count=");
    Serial.println(frozenPassageRowCount);
    Serial.print("image=");
    Serial.print(latestGrayImageValid ? GRAY_W : 0);
    Serial.print("x");
    Serial.print(latestGrayImageValid ? GRAY_H : 0);
    Serial.print(" bytes=");
    Serial.println(latestGrayImageValid ? GRAY_BYTES : 0);
    Serial.print("text=[");
    Serial.print(latestPassageText);
    Serial.println("]");
    Serial.println("ESP_DATA_READY is HIGH. FPGA can now read length with 0xA1, then packet with 0xA2.");
    Serial.println("Packet payload is raw binary: [24-byte header][text][96x96 grayscale].");
    Serial.println("===========================================");
    Serial.println();
}

void finishPacketAfterSpiSend()
{
    int queuedAfterSend = 0;

    /*
       The current packet has been fully clocked out by FPGA.
       Clear ONLY the presented packet here. Do not immediately copy/promote
       the next queued packet inside the same critical section.

       If we promote immediately, FPGA can sometimes see a valid length but
       clock 0xA2 before ESP DMA has a clean new packet boundary, causing:
           first packet bytes = 00 00 00 00 ...
           bad packet magic
    */
    portENTER_CRITICAL(&spiPacketMux);

    packetReadyForSpi = false;
    finalPacketLen = 0;
    memset(finalPacket, 0, sizeof(finalPacket));
    queuedAfterSend = spiPacketQueueCount;

    portEXIT_CRITICAL(&spiPacketMux);

    latestPassageText = "";
    passageReadyForSpi = false;

    latestGrayImageValid = false;
    memset(latestGrayImage, 0, sizeof(latestGrayImage));

    captureActive = true;

    /*
       Always force DATA_READY LOW after each packet.
       This creates a clean packet boundary for FPGA edge/poll logic.
    */
    setFpgaDataReady(false);

    if (queuedAfterSend > 0) {
        Serial.print("[SPI] packet sent, DATA_READY low gap before next packet, q=");
        Serial.println(queuedAfterSend);

        /*
           Give FPGA enough time to observe DATA_READY LOW and give ESP DMA
           task time to cleanly prepare the next packet transfer.
        */
        vTaskDelay(pdMS_TO_TICKS(SPI_NEXT_PACKET_GAP_MS));

        /*
           This is the actual helper name used in this codebase.
           It promotes one queued packet only if no packet is currently ready.
        */
        if (promoteQueuedPacketIfIdle()) {
            Serial.print("[SPI] next queued packet promoted after gap, q=");
            Serial.println(spiPacketQueueCount);
        }
    } else {
        Serial.println("[SPI] packet sent to FPGA, DATA_READY lowered");
    }
}


/* =========================
   Capture start / trigger handling
   ========================= */

bool consumeSpiTriggerFlag()
{
    bool hadTrigger = false;

    portENTER_CRITICAL(&spiFlagMux);
    if (spiTriggerPending) {
        spiTriggerPending = false;
        hadTrigger = true;
    }
    portEXIT_CRITICAL(&spiFlagMux);

    return hadTrigger;
}

void startCaptureFromTrigger()
{
    Serial.println("[SPI] 0x5F accepted -> capture starts");

    resetCaptureRowsKeepCalibration();
    captureActive = true;
    consecutiveInvokeFails = 0;

    storeCurrentJpegString("capture start");
}

void handlePendingSpiTriggerInMainLoop()
{
    if (!consumeSpiTriggerFlag()) {
        return;
    }

    if (!calibrationReady) {
        spiIgnoredCounter++;
        Serial.println("[SPI] 0x5F ignored: START321 calibration not ready yet");
        return;
    }

    if (captureActive) {
        spiIgnoredCounter++;
        Serial.println("[SPI] 0x5F ignored: capture already active");
        return;
    }

    if (packetReadyForSpi) {
        spiIgnoredCounter++;
        Serial.println("[SPI] 0x5F ignored: packet is ready and waiting for FPGA read");
        return;
    }

    startCaptureFromTrigger();
}

void maybeUpdateImageAfterFourRows()
{
#if ENABLE_IMAGE_CAPTURE
    if (!captureActive) {
        return;
    }

    if (imageUpdatedAfterFourRows) {
        return;
    }

    if (frozenPassageRowCount > 4) {
        imageUpdatedAfterFourRows = true;
        storeCurrentJpegString(">4 frozen rows update");
    }
#endif
}


/* =========================
   Session start handling
   ========================= */

void startSessionFromFpgaButton(void)
{
    if (sessionStarted) {
        Serial.println("[SESSION] 0x5F received again -> ignored, session already running");
        return;
    }

    Serial.println();
    Serial.println("========== SESSION START ==========");
    Serial.println("[SESSION] 0x5F received from FPGA button interrupt");
    Serial.println("[SESSION] This command only starts the session. It is ignored afterwards.");
    Serial.println("[SESSION] Clearing old packet/state and enabling Grove scanning now.");
    Serial.println("===================================");

    /* Drop any stale packet and force DATA_READY low before starting. */
    clearSpiPacketQueue();
    latestPassageText = "";
    passageReadyForSpi = false;
    latestGrayImageValid = false;
    memset(latestGrayImage, 0, sizeof(latestGrayImage));

    /* Reset tracking/calibration, then allow loop() to run AI detection. */
    resetAllMemory();
    sessionStarted = true;
}

/* =========================
   RTOS SPI receive task
   ========================= */

static void writeLengthReply(uint32_t value)
{
    memset(dma_len_tx, 0, SPI_LENGTH_BYTES);
    dma_len_tx[0] = (uint8_t)(value & 0xFF);
    dma_len_tx[1] = (uint8_t)((value >> 8) & 0xFF);
    dma_len_tx[2] = (uint8_t)((value >> 16) & 0xFF);
    dma_len_tx[3] = (uint8_t)((value >> 24) & 0xFF);
}

static uint32_t roundUp4U32(uint32_t value)
{
    return (value + 3u) & ~3u;
}

void spiCommandTask(void *pvParameters)
{
    (void)pvParameters;

    Serial.println("[SPI] DMA command task started");
    Serial.println("[SPI] commands: 0x5F session start, 0xA1 read length, 0xA2 read packet, 0xA3 abort");
    Serial.println("[SPI] ESP is DMA slave; ESP_DATA_READY notifies FPGA master");

    for (;;) {
        memset(dma_cmd_rx, 0, SPI_COMMAND_BYTES);
        memset(dma_cmd_tx_dummy, 0, SPI_COMMAND_BYTES);

        slave.queue(dma_cmd_tx_dummy, dma_cmd_rx, SPI_COMMAND_BYTES);
        std::vector<size_t> cmdDone = slave.wait();

        if (cmdDone.empty()) {
            vTaskDelay(1);
            continue;
        }

        uint8_t cmd = dma_cmd_rx[0];

        if (cmd == SPI_CMD_SESSION_START) {
            startSessionFromFpgaButton();
        }
        else if (cmd == SPI_CMD_READ_LENGTH) {
            uint32_t lenToSend = 0;

            promoteQueuedPacketIfIdle();

            portENTER_CRITICAL(&spiPacketMux);
            if (packetReadyForSpi && finalPacketLen > 0) {
                lenToSend = finalPacketLen;
            }
            portEXIT_CRITICAL(&spiPacketMux);

            writeLengthReply(lenToSend);
            memset(dma_len_rx_dummy, 0, SPI_LENGTH_BYTES);

            slave.queue(dma_len_tx, dma_len_rx_dummy, SPI_LENGTH_BYTES);
            slave.wait();

            if (lenToSend != 0) {
                Serial.print("[SPI] length sent -> ");
                Serial.println(lenToSend);
            }
        }
        else if (cmd == SPI_CMD_READ_PACKET) {
            Serial.println("[SPI] 0xA2 packet request received");

            promoteQueuedPacketIfIdle();

            uint32_t packetLenSnapshot = 0;

            portENTER_CRITICAL(&spiPacketMux);
            if (packetReadyForSpi && finalPacketLen > 0) {
                packetLenSnapshot = finalPacketLen;
                memset(dma_packet_tx, 0, PACKET_DMA_MAX_LEN);
                memcpy(dma_packet_tx, finalPacket, packetLenSnapshot);
            }
            portEXIT_CRITICAL(&spiPacketMux);

            if (packetLenSnapshot == 0) {
                memset(dma_len_tx, 0, SPI_LENGTH_BYTES);
                memset(dma_len_rx_dummy, 0, SPI_LENGTH_BYTES);
                slave.queue(dma_len_tx, dma_len_rx_dummy, SPI_LENGTH_BYTES);
                slave.wait();
                Serial.println("[SPI] packet request but packet not ready -> sent zeros");
            } else {
                uint32_t txLen = roundUp4U32(packetLenSnapshot) + SPI_DMA_EXTRA_BYTES;
                if (txLen > PACKET_DMA_MAX_LEN) {
                    txLen = PACKET_DMA_MAX_LEN;
                }

                memset(dma_packet_rx_dummy, 0, PACKET_DMA_MAX_LEN);

                Serial.print("[SPI] packet read requested, sending bytes=");
                Serial.print(packetLenSnapshot);
                Serial.print(" clocks=");
                Serial.println(txLen);

                slave.queue(dma_packet_tx, dma_packet_rx_dummy, txLen);
                slave.wait();

                finishPacketAfterSpiSend();
            }
        }
        else if (cmd == SPI_CMD_ABORT_PACKET) {
            Serial.println("[SPI] 0xA3 abort/reset received from FPGA");

            clearSpiPacketQueue();

            latestPassageText = "";
            passageReadyForSpi = false;
            latestGrayImageValid = false;
            memset(latestGrayImage, 0, sizeof(latestGrayImage));

            captureActive = true;

            Serial.println("[SPI] packet queue dropped, DATA_READY lowered");
        }
        else {
            /*
               Ignore 0x00 dummy clocks. FPGA sends 0x00 while clocking replies,
               and if timing slips these can appear as separate command transactions.
            */
            if (cmd != 0x00) {
                Serial.print("[SPI] unknown command 0x");
                Serial.println(cmd, HEX);
            }
        }

        vTaskDelay(1);
    }
}

/* =========================
   Setup
   ========================= */

void setup()
{
    Serial.begin(115200);
    delay(1000);

    pinMode(FPGA_DATA_READY_PIN, OUTPUT);
    digitalWrite(FPGA_DATA_READY_PIN, LOW);

    resetAllMemory();

    Serial.println("Starting SPI slave...");

    dma_cmd_rx = slave.allocDMABuffer(SPI_COMMAND_BYTES);
    dma_cmd_tx_dummy = slave.allocDMABuffer(SPI_COMMAND_BYTES);
    dma_len_tx = slave.allocDMABuffer(SPI_LENGTH_BYTES);
    dma_len_rx_dummy = slave.allocDMABuffer(SPI_LENGTH_BYTES);
    dma_packet_tx = slave.allocDMABuffer(PACKET_DMA_MAX_LEN);
    dma_packet_rx_dummy = slave.allocDMABuffer(PACKET_DMA_MAX_LEN);

    if (!dma_cmd_rx || !dma_cmd_tx_dummy || !dma_len_tx || !dma_len_rx_dummy ||
        !dma_packet_tx || !dma_packet_rx_dummy) {
        Serial.println("[SPI] DMA buffer allocation failed");
        while (1) { delay(1000); }
    }

    slave.setDataMode(SPI_MODE);
    slave.setMaxTransferSize(PACKET_DMA_MAX_LEN);
    slave.setQueueSize(1);
    slave.begin();

    Serial.println("SPI slave ready");

    Serial.println("Starting Grove AI...");

    while (!AI.begin()) {
        Serial.println("AI begin failed, retrying...");
        delay(1000);
    }

    Serial.println("AI begin OK");
    Serial.println("[SESSION] Waiting for FPGA button session-start command 0x5F...");
    Serial.println("[SESSION] Grove detection will stay idle until 0x5F is received.");

    /*
       ESP32-C3 has only core 0, so do NOT use xTaskCreatePinnedToCore(..., 1).
       This creates the SPI trigger receiver task on the available core.
    */
    BaseType_t ok = xTaskCreate(
        spiCommandTask,
        "spiCommandTask",
        8192,
        NULL,
        1,
        &spiTriggerTaskHandle
    );

    if (ok != pdPASS) {
        Serial.println("[SPI] failed to create trigger task");
    }
}

/* =========================
   Main loop
   ========================= */

void loop()
{
    frameCounter++;

    /*
       Wait here until FPGA sends 0x5F after the physical button interrupt.
       This makes ESP and FPGA start the real session at the same time.
    */
    if (!sessionStarted) {
        static uint32_t lastWaitingPrint = 0;
        uint32_t now = millis();

        if (now - lastWaitingPrint > 3000) {
            lastWaitingPrint = now;
            Serial.println("[SESSION] idle, waiting for 0x5F session start from FPGA...");
        }

        delay(10);
        return;
    }

    /*
       Keep flushing frozen rows into the SPI packet queue, but never stop
       AI.invoke just because ESP_DATA_READY is high. The SPI DMA task owns
       packet transmission; loop() keeps scanning.
    */
    flushPendingFrozenRows();

    /*
       If an invoke-fail/end-of-pass asked for a duplicate reset, do the reset
       only after all queued rows have been sent. This avoids losing late rows
       that were already frozen but not yet transmitted.
    */
    if (duplicateResetAfterPendingFlush && pendingFrozenRowCount <= 0) {
        rowOrderForceFlush = false;
        duplicateResetAfterPendingFlush = false;
        resetDuplicateGuardsAfterInvokeFails();
        delay(PRINT_DELAY_MS);
        return;
    }

    if (rowOrderForceFlush && pendingFrozenRowCount > 0) {
        /*
           Keep trying to enqueue pending rows, but do not stop AI.invoke.
           If the SPI packet queue is full, flushPendingFrozenRows() leaves the
           row pending and the SPI task will drain the queue in parallel.
        */
        flushPendingFrozenRows();
    }

    if (!AI.invoke()) {
        consecutiveInvokeFails = 0;

        DetectedRow detectedRows[MAX_VISIBLE_ROWS];
        int detectedCount = extractDetectedRows(detectedRows, MAX_VISIBLE_ROWS);

        if (detectedCount > 0) {
            processDetectedRows(detectedRows, detectedCount);
            maybeUpdateImageAfterFourRows();
        } else {
            expireOldRowTrackers();
            flushPendingFrozenRows();
        }
    } else {
        consecutiveInvokeFails++;

        Serial.print("invoke failed count=");
        Serial.print(consecutiveInvokeFails);
        Serial.print("/");
        Serial.println(MAX_CONSECUTIVE_INVOKE_FAILS);

        if (consecutiveInvokeFails >= MAX_CONSECUTIVE_INVOKE_FAILS) {
            Serial.println("[ROW ORDER] invoke-fail/end-of-pass -> force flush pending rows before duplicate reset");
            rowOrderForceFlush = true;
            duplicateResetAfterPendingFlush = true;
            consecutiveInvokeFails = 0;

            flushPendingFrozenRows();
        }
    }

    delay(PRINT_DELAY_MS);
}
