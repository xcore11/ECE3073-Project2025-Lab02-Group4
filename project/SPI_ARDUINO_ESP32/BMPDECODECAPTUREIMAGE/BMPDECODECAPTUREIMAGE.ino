#include <Seeed_Arduino_SSCMA.h>
#include <ArduinoJson.h>
#include <ESP32SPISlave.h>
#include <JPEGDecoder.h>
#include <vector>

SSCMA AI;

/* =========================
   SPI settings
   ========================= */

#define SPI_MODE SPI_MODE0

ESP32SPISlave slave;

/* =========================
   SPI protocol
   ========================= */   

#define SPI_REQUEST_BYTE 0x5F
#define SPI_START_BYTE   0xA5
#define SPI_MAX_TEXT_LEN 512

uint8_t spi_rx_request[1] = {0};
uint8_t spi_tx_dummy[1]   = {0};

uint8_t spi_reply_packet[SPI_MAX_TEXT_LEN + 2] = {0};
uint8_t spi_reply_rx_discard[SPI_MAX_TEXT_LEN + 2] = {0};

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

uint8_t latestGrayImage[GRAY_BYTES] = {0};
bool latestGrayImageValid = false;

uint8_t finalPacket[PACKET_MAX_LEN] = {0};
uint32_t finalPacketLen = 0;
bool packetReadyForPrint = false;

String storedJpegBase64 = "";
bool imageUpdatedAfterFourRows = false;

/*
   captureActive is true only after a valid 0x5F trigger is accepted.
   Calibration still runs before this, but instruction rows are not frozen
   until captureActive is true.
*/
volatile bool captureActive = false;

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

#define MIN_CANDIDATE_CHARS 2
#define MAX_CONSECUTIVE_INVOKE_FAILS 2

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

String frozenPassageRows[MAX_PASSAGE_ROWS];
int frozenPassageRowCount = 0;

RowTracker rowTrackers[MAX_VISIBLE_ROWS];

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

    for (int i = 0; i < MAX_CHARS_PER_LINE; i++) {
        calibratedX[i] = 0;
    }

    calibrationReady = false;

    frozenPassageRowCount = 0;
    consecutiveInvokeFails = 0;
    frameCounter = 0;

    latestPassageText = "";
    passageReadyForSpi = false;

    Serial.println("Memory reset");
    Serial.print("Expected calibration line: ");
    Serial.println(buildExpectedCalibrationLine());
    Serial.println("----------------");
}

/* =========================
   Sorting helpers
   ========================= */

void sortByY(CharBox arr[], int n)
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

void sortByX(CharBox arr[], int start, int end)
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

void sortDetectedRowsByY(DetectedRow arr[], int n)
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

String buildSlottedTextFromRow(DetectedRow *row)
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

bool tryUseRowAsCalibration(DetectedRow row)
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

int extractDetectedRows(DetectedRow detectedRows[], int maxRowsOut)
{
    int boxCount = AI.boxes().size();

    if (boxCount <= 0) {
        return 0;
    }

    if (boxCount > MAX_BOXES) {
        boxCount = MAX_BOXES;
    }

    CharBox boxes[MAX_BOXES];

    for (int i = 0; i < boxCount; i++) {
        boxes[i].target = AI.boxes()[i].target;
        boxes[i].x = AI.boxes()[i].x;
        boxes[i].y = AI.boxes()[i].y;
        boxes[i].w = AI.boxes()[i].w;
        boxes[i].h = AI.boxes()[i].h;
        boxes[i].c = decodeChar(boxes[i].target);
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

    if (countNonSpaceChars(finalRowText) < MIN_CANDIDATE_CHARS) {
        return;
    }

    /*
       Ignore calibration row.
    */
    if (finalRowText == buildExpectedCalibrationLine()) {
        rowTrackers[trackerIndex].frozen = true;
        rowTrackers[trackerIndex].frozenText = finalRowText;

        Serial.print("Calibration row ignored from passage: ");
        Serial.println(finalRowText);
        return;
    }

    /*
       Avoid duplicate frozen passage rows.
    */
    if (frozenPassageAlreadyHas(finalRowText)) {
        Serial.print("Frozen duplicate ignored: ");
        Serial.println(finalRowText);

        rowTrackers[trackerIndex].frozen = true;
        rowTrackers[trackerIndex].frozenText = finalRowText;
        return;
    }

    rowTrackers[trackerIndex].frozen = true;
    rowTrackers[trackerIndex].frozenText = finalRowText;

    frozenPassageRows[frozenPassageRowCount] = finalRowText;
    frozenPassageRowCount++;

    latestPassageText = "";
    passageReadyForSpi = false;

    Serial.print("FROZEN ROW ");
    Serial.print(frozenPassageRowCount);
    Serial.print(": ");
    Serial.println(finalRowText);
}

void updateRowTrackerWithDetectedRow(DetectedRow detected)
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
       only after the RTOS SPI task receives 0x5F and the main AI loop accepts it.
    */
    if (!captureActive) {
        return;
    }

    if (countNonSpaceChars(text) < MIN_CANDIDATE_CHARS) {
        return;
    }

    /*
       If the calibration row appears again during scrolling,
       ignore it.
    */
    if (text == buildExpectedCalibrationLine()) {
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

void processDetectedRows(DetectedRow detectedRows[], int detectedCount)
{
    /*
       Process top to bottom.
    */
    for (int i = 0; i < detectedCount; i++) {
        updateRowTrackerWithDetectedRow(detectedRows[i]);
    }

    expireOldRowTrackers();
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

    frozenPassageRowCount = 0;
    consecutiveInvokeFails = 0;

    latestPassageText = "";
    passageReadyForSpi = false;

    latestGrayImageValid = false;
    memset(latestGrayImage, 0, sizeof(latestGrayImage));

    storedJpegBase64 = "";
    imageUpdatedAfterFourRows = false;

    finalPacketLen = 0;
    memset(finalPacket, 0, sizeof(finalPacket));
    packetReadyForPrint = false;
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

String buildFullPassageForSpiSpaces()
{
    /*
       For FPGA/VGA side, keep the whole instruction as one sentence.
       Rows are joined with spaces, not newline characters.
    */
    String passage = "";

    for (int i = 0; i < frozenPassageRowCount; i++) {
        String row = frozenPassageRows[i];
        row = cleanSlottedText(row);

        if (row.length() == 0) {
            continue;
        }

        if (passage.length() > 0) {
            passage += " ";
        }

        passage += row;
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

void buildFinalPacket()
{
    memset(finalPacket, 0, sizeof(finalPacket));
    finalPacketLen = 0;

    String sendText = latestPassageText;

    if (sendText.length() > PACKET_MAX_TEXT_LEN) {
        sendText = sendText.substring(0, PACKET_MAX_TEXT_LEN);
    }

    uint16_t textLen = (uint16_t)sendText.length();
    uint32_t imageLen = latestGrayImageValid ? GRAY_BYTES : 0;
    uint16_t imageW = latestGrayImageValid ? GRAY_W : 0;
    uint16_t imageH = latestGrayImageValid ? GRAY_H : 0;
    uint16_t flags = latestGrayImageValid ? PACKET_FLAG_IMAGE_VALID : 0;

    finalPacketLen = PACKET_HEADER_LEN + textLen + imageLen;

    if (finalPacketLen > PACKET_MAX_LEN) {
        Serial.println("[PACKET] packet too large, dropping image");
        imageLen = 0;
        imageW = 0;
        imageH = 0;
        flags = 0;
        finalPacketLen = PACKET_HEADER_LEN + textLen;
    }

    finalPacket[0] = PACKET_MAGIC0;
    finalPacket[1] = PACKET_MAGIC1;
    finalPacket[2] = PACKET_VERSION;
    finalPacket[3] = PACKET_STATUS_DONE;

    writeU32LE(finalPacket, 4, finalPacketLen);
    writeU16LE(finalPacket, 8, textLen);
    writeU16LE(finalPacket, 10, imageW);
    writeU16LE(finalPacket, 12, imageH);
    writeU16LE(finalPacket, 14, (uint16_t)frozenPassageRowCount);
    writeU32LE(finalPacket, 16, imageLen);
    writeU16LE(finalPacket, 20, PACKET_HEADER_LEN);
    writeU16LE(finalPacket, 22, flags);

    for (int i = 0; i < textLen; i++) {
        finalPacket[PACKET_HEADER_LEN + i] = (uint8_t)sendText[i];
    }

    if (imageLen > 0) {
        memcpy(&finalPacket[PACKET_HEADER_LEN + textLen], latestGrayImage, imageLen);
    }

    packetReadyForPrint = true;
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

void printFinalPacketAndConsume()
{
    Serial.println();
    Serial.println("========== PACKET PRINT TEST ==========");
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
    Serial.print("flags=0x");
    Serial.println(latestGrayImageValid ? PACKET_FLAG_IMAGE_VALID : 0, HEX);
    Serial.print("text=[");
    Serial.print(latestPassageText);
    Serial.println("]");

    printGrayscalePayloadBmpBase64OneLine();

    int preview = finalPacketLen;
    if (preview > 96) {
        preview = 96;
    }

    Serial.println("First packet bytes:");
    for (int i = 0; i < preview; i++) {
        if ((i % 16) == 0) {
            Serial.println();
            Serial.print(i);
            Serial.print(": ");
        }
        if (finalPacket[i] < 0x10) {
            Serial.print("0");
        }
        Serial.print(finalPacket[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    Serial.println("=======================================");
    Serial.println();

    /*
       For now, printing is considered the send-back test.
       captureActive becomes false only after this point.
    */
    captureActive = false;
    resetCaptureRowsKeepCalibration();

    Serial.println("[CAPTURE] complete, waiting for next valid 0x5F trigger");
}

void finalizeCapture()
{
    Serial.println("[CAPTURE] invoke-fail end -> finalize passage");

    printFrozenRows();

    String withNewlines = buildFullPassageWithNewlines();
    latestPassageText = buildFullPassageForSpiSpaces();

    Serial.println();
    Serial.println("========== FULL PASSAGE ==========");
    Serial.println(withNewlines);
    Serial.println("==================================");
    Serial.print("SPI passage with spaces only: ");
    Serial.println(latestPassageText);
    Serial.println();

    decodeStoredJpegToGray96();
    buildFinalPacket();
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

    if (packetReadyForPrint) {
        spiIgnoredCounter++;
        Serial.println("[SPI] 0x5F ignored: packet print/send test not consumed yet");
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
   RTOS SPI receive task
   ========================= */

void spiTriggerTask(void *pvParameters)
{
    (void)pvParameters;

    Serial.println("[SPI] RTOS trigger task started");
    Serial.println("[SPI] Task receives 0x5F and sets a flag. AI loop does not poll SPI.");
    Serial.println("[SPI] Important: only ONE SPI transaction is queued at a time.");

    for (;;) {
        memset(spi_rx_request, 0, sizeof(spi_rx_request));

        /*
           Queue exactly one 1-byte receive transaction, then block until
           the FPGA clocks that byte.

           Do NOT use wait(1000) here. With timeout waiting, the old code
           could queue a new transaction even though the previous transaction
           was still pending. With queue size = 1, that can corrupt the SPI
           slave transaction state and cause the ESP32-C3 Load access fault.
        */
        slave.queue(NULL, spi_rx_request, 1);

        std::vector<size_t> completedTransactions = slave.wait();

        if (!completedTransactions.empty()) {
            if (spi_rx_request[0] == SPI_REQUEST_BYTE) {
                portENTER_CRITICAL(&spiFlagMux);
                spiTriggerPending = true;
                spiTriggerCounter++;
                portEXIT_CRITICAL(&spiFlagMux);
            }
        }

        /* Yield once after a completed transaction. */
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

    resetAllMemory();

    Serial.println("Starting SPI slave...");

    slave.setDataMode(SPI_MODE);
    slave.setQueueSize(1);
    slave.begin();

    Serial.println("SPI slave ready");

    Serial.println("Starting Grove AI...");

    while (!AI.begin()) {
        Serial.println("AI begin failed, retrying...");
        delay(1000);
    }

    Serial.println("AI begin OK");

    /*
       ESP32-C3 has only core 0, so do NOT use xTaskCreatePinnedToCore(..., 1).
       This creates the SPI trigger receiver task on the available core.
    */
    BaseType_t ok = xTaskCreate(
        spiTriggerTask,
        "spiTriggerTask",
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
       First handle completed packet printing.
       captureActive becomes false only after this print/send-back test.
    */
    if (packetReadyForPrint) {
        printFinalPacketAndConsume();
        delay(PRINT_DELAY_MS);
        return;
    }

    /*
       RTOS task receives the SPI byte.
       Main loop only consumes the flag here, which is very fast.
    */
    handlePendingSpiTriggerInMainLoop();

    if (!AI.invoke()) {
        consecutiveInvokeFails = 0;

        DetectedRow detectedRows[MAX_VISIBLE_ROWS];
        int detectedCount = extractDetectedRows(detectedRows, MAX_VISIBLE_ROWS);

        if (detectedCount > 0) {
            processDetectedRows(detectedRows, detectedCount);
            maybeUpdateImageAfterFourRows();
        } else {
            expireOldRowTrackers();
        }
    } else {
        consecutiveInvokeFails++;

        Serial.print("invoke failed count=");
        Serial.print(consecutiveInvokeFails);
        Serial.print("/");
        Serial.println(MAX_CONSECUTIVE_INVOKE_FAILS);

        if (captureActive && consecutiveInvokeFails >= MAX_CONSECUTIVE_INVOKE_FAILS) {
            finalizeCapture();
        }

        if (!captureActive && consecutiveInvokeFails >= MAX_CONSECUTIVE_INVOKE_FAILS) {
            consecutiveInvokeFails = 0;
        }
    }

    delay(PRINT_DELAY_MS);
}
