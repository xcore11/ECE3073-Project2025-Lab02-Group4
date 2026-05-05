#include <Seeed_Arduino_SSCMA.h>
#include <ArduinoJson.h>
#include <ESP32SPISlave.h>

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
#define SPI_MAX_TEXT_LEN 128

uint8_t spi_rx_request[1] = {0};
uint8_t spi_tx_dummy[1]   = {0};

uint8_t spi_reply_packet[SPI_MAX_TEXT_LEN + 2] = {0};
uint8_t spi_reply_rx_discard[SPI_MAX_TEXT_LEN + 2] = {0};

String latestPassageText = "";
bool passageReadyForSpi = false;

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
   Passage helpers
   ========================= */

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

String buildFullPassageForSpi()
{
    /*
       This now keeps spaces.
       Rows are separated using newline.
       If your FPGA decoder cannot handle newline yet,
       change "\n" to " " here.
    */
    String passage = "";

    for (int i = 0; i < frozenPassageRowCount; i++) {
        passage += frozenPassageRows[i];

        if (i < frozenPassageRowCount - 1) {
            passage += "\n";
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

void printFullPassageAndMaybeEnableSpi()
{
    String withNewlines = buildFullPassageWithNewlines();
    latestPassageText = buildFullPassageForSpi();

    Serial.println();
    Serial.println("========== FULL PASSAGE ==========");
    Serial.println(withNewlines);
    Serial.println("==================================");

    Serial.print("SPI passage with spaces/newlines: ");
    Serial.println(latestPassageText);
    Serial.println();

    if (frozenPassageRowCount > 0 && latestPassageText.length() > 0) {
        passageReadyForSpi = true;
        Serial.println("Frozen passage exists -> SPI enabled");
    } else {
        passageReadyForSpi = false;
        latestPassageText = "";
        consecutiveInvokeFails = 0;
        Serial.println("No frozen rows yet -> SPI disabled, continue AI invoke");
    }

    Serial.println("----------------");
}

/* =========================
   SPI service
   ========================= */

void prepareSpiReplyPacket()
{
    memset(spi_reply_packet, 0, sizeof(spi_reply_packet));

    String sendText = latestPassageText;

    if (sendText.length() == 0) {
        spi_reply_packet[0] = SPI_START_BYTE;
        spi_reply_packet[1] = 0;
        Serial.println("No valid passage to prepare");
        return;
    }

    if (sendText.length() > SPI_MAX_TEXT_LEN) {
        sendText = sendText.substring(0, SPI_MAX_TEXT_LEN);
    }

    spi_reply_packet[0] = SPI_START_BYTE;
    spi_reply_packet[1] = (uint8_t)sendText.length();

    for (int i = 0; i < sendText.length(); i++) {
        spi_reply_packet[i + 2] = (uint8_t)sendText[i];
    }

    Serial.print("Prepared SPI packet: start=0xA5 length=");
    Serial.print(sendText.length());
    Serial.print(" text=");
    Serial.println(sendText);
}

void serviceSpiOnlyAfterPassageReady()
{
    if (!passageReadyForSpi || latestPassageText.length() == 0) {
        return;
    }

    memset(spi_rx_request, 0, sizeof(spi_rx_request));
    memset(spi_tx_dummy, 0, sizeof(spi_tx_dummy));

    Serial.println("Passage ready. Waiting for FPGA SPI request 0x5F...");

    /*
       Transaction 1:
       FPGA sends request byte 0x5F.
       ESP receives it.
    */
    slave.queue(spi_tx_dummy, spi_rx_request, 1);

    const std::vector<size_t> received_bytes = slave.wait(10000);

    if (received_bytes.empty()) {
        Serial.println("No FPGA SPI request yet");
        Serial.println("----------------");
        return;
    }

    Serial.print("SPI request received: 0x");
    if (spi_rx_request[0] < 0x10) {
        Serial.print("0");
    }
    Serial.println(spi_rx_request[0], HEX);

    if (spi_rx_request[0] != SPI_REQUEST_BYTE) {
        Serial.println("Wrong request byte, ignoring");
        Serial.println("----------------");
        return;
    }

    prepareSpiReplyPacket();

    int packet_len = 2 + spi_reply_packet[1];

    if (packet_len <= 2) {
        Serial.println("Packet has no text, cancel SPI reply");
        passageReadyForSpi = false;
        latestPassageText = "";
        consecutiveInvokeFails = 0;
        Serial.println("----------------");
        return;
    }

    memset(spi_reply_rx_discard, 0, sizeof(spi_reply_rx_discard));

    Serial.println("Waiting for FPGA dummy-read transaction...");

    /*
       Transaction 2:
       FPGA clocks dummy bytes.
       ESP sends:
       [0] = 0xA5
       [1] = length
       [2..] = passage text with spaces/newlines
    */
    slave.queue(spi_reply_packet, spi_reply_rx_discard, packet_len);

    const std::vector<size_t> sent_bytes = slave.wait(10000);

    if (!sent_bytes.empty()) {
        Serial.print("SPI reply sent, bytes=");
        Serial.println(sent_bytes[0]);

        /*
           Send once, then reset for the next passage.
        */
        resetAllMemory();
    } else {
        Serial.println("SPI reply not consumed before timeout");
    }

    Serial.println("----------------");
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

    /*
       Keep this as begin().
       If you want custom pins, replace with your custom begin call.
    */
    slave.begin();

    Serial.println("SPI slave ready");

    Serial.println("Starting Grove AI...");

    while (!AI.begin()) {
        Serial.println("AI begin failed, retrying...");
        delay(1000);
    }

    Serial.println("AI begin OK");
}

/* =========================
   Main loop
   ========================= */

void loop()
{
    frameCounter++;

    /*
       Once a frozen passage exists after invoke fail,
       stop AI and only handle SPI.
    */
    if (passageReadyForSpi) {
        serviceSpiOnlyAfterPassageReady();
        delay(PRINT_DELAY_MS);
        return;
    }

    if (!AI.invoke()) {
        consecutiveInvokeFails = 0;

        DetectedRow detectedRows[MAX_VISIBLE_ROWS];
        int detectedCount = extractDetectedRows(detectedRows, MAX_VISIBLE_ROWS);

        if (detectedCount > 0) {
            processDetectedRows(detectedRows, detectedCount);
        } else {
            expireOldRowTrackers();
        }
    } else {
        consecutiveInvokeFails++;

        Serial.print("invoke failed count=");
        Serial.print(consecutiveInvokeFails);
        Serial.print("/");
        Serial.println(MAX_CONSECUTIVE_INVOKE_FAILS);

        /*
           When AI stops seeing text, complete the passage
           using only the frozen rows.
        */
        if (consecutiveInvokeFails == 1) {
            printFrozenRows();
            printFullPassageAndMaybeEnableSpi();
        }

        if (consecutiveInvokeFails >= MAX_CONSECUTIVE_INVOKE_FAILS &&
            frozenPassageRowCount == 0) {
            Serial.println("No frozen rows after invoke fails -> continue AI");
            consecutiveInvokeFails = 0;
        }
    }

    delay(PRINT_DELAY_MS);
}