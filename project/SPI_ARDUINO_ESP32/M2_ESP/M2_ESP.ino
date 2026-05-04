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
#define SPI_MAX_TEXT_LEN 62

uint8_t spi_rx_request[1] = {0};
uint8_t spi_tx_dummy[1]   = {0};

uint8_t spi_reply_packet[SPI_MAX_TEXT_LEN + 2] = {0};
uint8_t spi_reply_rx_discard[SPI_MAX_TEXT_LEN + 2] = {0};

String latestPassageNoSpaces = "";
bool passageReadyForSpi = false;

/* =========================
   User-tunable parameters
   ========================= */

#define PRINT_DELAY_MS 200
#define MAX_BOXES 80

#define MAX_CHARS_PER_LINE 8
#define LINE_Y_THRESHOLD 5

#define PARTIAL_CONFIRM_COUNT 8
#define MIN_CANDIDATE_CHARS 2
#define MAX_CONSECUTIVE_INVOKE_FAILS 2

#define MAX_TRACKED_LINES 120
#define MAX_CANDIDATES 120

#define DEBUG_FRAME_LINES 1

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

struct DetectedLine {
    String text;
    int y;
};

struct CandidateLine {
    String bestText;
    int count;
    bool saved;
    int lastSeenFrame;
};

String savedLines[MAX_TRACKED_LINES];
int savedLineCount = 0;

CandidateLine candidates[MAX_CANDIDATES];
int candidateCount = 0;

int consecutiveInvokeFails = 0;
int frameCounter = 0;

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
   Text helpers
   ========================= */

String cleanText(String s)
{
    s.trim();
    s.toLowerCase();
    return s;
}

int longestCommonSubstring(String a, String b)
{
    int best = 0;

    for (int i = 0; i < a.length(); i++) {
        for (int j = 0; j < b.length(); j++) {
            int k = 0;

            while ((i + k) < a.length() &&
                   (j + k) < b.length() &&
                   a[i + k] == b[j + k]) {
                k++;
            }

            if (k > best) {
                best = k;
            }
        }
    }

    return best;
}

bool looksLikeSameLine(String a, String b)
{
    a = cleanText(a);
    b = cleanText(b);

    if (a.length() == 0 || b.length() == 0) {
        return false;
    }

    if (a == b) {
        return true;
    }

    if (a.length() <= 3 || b.length() <= 3) {
        return false;
    }

    if (a.indexOf(b) >= 0 || b.indexOf(a) >= 0) {
        return true;
    }

    int lcs = longestCommonSubstring(a, b);
    int threshold = (MAX_CHARS_PER_LINE / 2) + 1;

    if (lcs >= threshold) {
        return true;
    }

    return false;
}

bool looksLikeSamePartialBucket(String a, String b)
{
    a = cleanText(a);
    b = cleanText(b);

    if (a.length() == 0 || b.length() == 0) {
        return false;
    }

    if (a == b) {
        return true;
    }

    if (a.indexOf(b) >= 0 || b.indexOf(a) >= 0) {
        return true;
    }

    int minLen = min(a.length(), b.length());
    int lcs = longestCommonSubstring(a, b);

    if (minLen <= 4) {
        return lcs >= 3;
    }

    return lcs >= 4;
}

bool duplicateAgainstSaved(String text)
{
    text = cleanText(text);

    for (int i = 0; i < savedLineCount; i++) {
        String saved = cleanText(savedLines[i]);

        if (text.length() <= 3 || saved.length() <= 3) {
            if (text == saved) {
                return true;
            }
        } else {
            if (looksLikeSameLine(text, saved)) {
                return true;
            }
        }
    }

    return false;
}

/* =========================
   Reset memory
   ========================= */

void resetAllMemory()
{
    for (int i = 0; i < MAX_TRACKED_LINES; i++) {
        savedLines[i] = "";
    }

    for (int i = 0; i < MAX_CANDIDATES; i++) {
        candidates[i].bestText = "";
        candidates[i].count = 0;
        candidates[i].saved = false;
        candidates[i].lastSeenFrame = -9999;
    }

    savedLineCount = 0;
    candidateCount = 0;
    consecutiveInvokeFails = 0;

    latestPassageNoSpaces = "";
    passageReadyForSpi = false;

    Serial.println("Memory reset");
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

void sortDetectedLinesByY(DetectedLine arr[], int n)
{
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (arr[j].y < arr[i].y) {
                DetectedLine temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
    }
}

/* =========================
   Extract visible lines
   ========================= */

int extractDetectedLines(DetectedLine detectedLines[], int maxLinesOut)
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
    int lineStart = 0;

    while (lineStart < boxCount && detectedCount < maxLinesOut) {
        int lineEnd = lineStart + 1;
        int referenceY = boxes[lineStart].y;
        int ySum = boxes[lineStart].y;

        while (lineEnd < boxCount &&
               abs(boxes[lineEnd].y - referenceY) <= LINE_Y_THRESHOLD) {
            ySum += boxes[lineEnd].y;
            lineEnd++;
        }

        int lineBoxCount = lineEnd - lineStart;
        int avgY = ySum / lineBoxCount;

        sortByX(boxes, lineStart, lineEnd);

        String line = "";

        for (int i = lineStart; i < lineEnd; i++) {
            if (line.length() < MAX_CHARS_PER_LINE) {
                line += boxes[i].c;
            }
        }

        line = cleanText(line);

        if (line.length() >= MIN_CANDIDATE_CHARS) {
            detectedLines[detectedCount].text = line;
            detectedLines[detectedCount].y = avgY;
            detectedCount++;
        }

        lineStart = lineEnd;
    }

    sortDetectedLinesByY(detectedLines, detectedCount);

#if DEBUG_FRAME_LINES
    Serial.println("Frame detected lines:");
    for (int i = 0; i < detectedCount; i++) {
        Serial.print("frame ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(detectedLines[i].text);
        Serial.print(" y=");
        Serial.println(detectedLines[i].y);
    }
#endif

    return detectedCount;
}

/* =========================
   Save and candidate logic
   ========================= */

void saveLine(String text)
{
    text = cleanText(text);

    if (text.length() < MIN_CANDIDATE_CHARS) {
        return;
    }

    if (duplicateAgainstSaved(text)) {
        Serial.print("Rejected duplicate/partial line: ");
        Serial.println(text);
        return;
    }

    if (savedLineCount >= MAX_TRACKED_LINES) {
        Serial.println("Saved line memory full");
        return;
    }

    savedLines[savedLineCount] = text;
    savedLineCount++;

    latestPassageNoSpaces = "";
    passageReadyForSpi = false;

    Serial.print("NEW LINE SAVED: ");
    Serial.println(text);
}

int findCandidateBucket(String text)
{
    text = cleanText(text);

    for (int i = 0; i < candidateCount; i++) {
        if (candidates[i].saved) {
            continue;
        }

        if (looksLikeSamePartialBucket(text, candidates[i].bestText)) {
            return i;
        }
    }

    return -1;
}

void updatePartialCandidate(String text)
{
    text = cleanText(text);

    if (text.length() < MIN_CANDIDATE_CHARS) {
        return;
    }

    if (duplicateAgainstSaved(text)) {
        Serial.print("Ignored already-saved/partial: ");
        Serial.println(text);
        return;
    }

    int index = findCandidateBucket(text);

    if (index < 0) {
        if (candidateCount >= MAX_CANDIDATES) {
            Serial.println("Candidate memory full");
            return;
        }

        index = candidateCount;
        candidates[index].bestText = text;
        candidates[index].count = 1;
        candidates[index].saved = false;
        candidates[index].lastSeenFrame = frameCounter;
        candidateCount++;
    } else {
        candidates[index].count++;
        candidates[index].lastSeenFrame = frameCounter;

        if (text.length() >= candidates[index].bestText.length()) {
            candidates[index].bestText = text;
        }
    }

    Serial.print("Partial bucket: ");
    Serial.print(candidates[index].bestText);
    Serial.print(" count=");
    Serial.print(candidates[index].count);
    Serial.print("/");
    Serial.println(PARTIAL_CONFIRM_COUNT);

    if (!candidates[index].saved &&
        candidates[index].count >= PARTIAL_CONFIRM_COUNT) {
        saveLine(candidates[index].bestText);
        candidates[index].saved = true;
    }
}

void updateLineMemory(String text)
{
    text = cleanText(text);

    if (text.length() < MIN_CANDIDATE_CHARS) {
        return;
    }

    if (text.length() >= MAX_CHARS_PER_LINE) {
        text = text.substring(0, MAX_CHARS_PER_LINE);

        Serial.print("Full line detected: ");
        Serial.println(text);

        saveLine(text);
        return;
    }

    updatePartialCandidate(text);
}

void processDetectedLines(DetectedLine detectedLines[], int detectedCount)
{
    for (int i = 0; i < detectedCount; i++) {
        updateLineMemory(detectedLines[i].text);
    }
}

/* =========================
   Passage helpers
   ========================= */

String buildFullPassageWithSpaces()
{
    String passage = "";

    for (int i = 0; i < savedLineCount; i++) {
        passage += savedLines[i];

        if (i < savedLineCount - 1) {
            passage += " ";
        }
    }

    return passage;
}

String buildFullPassageNoSpaces()
{
    String passage = "";

    for (int i = 0; i < savedLineCount; i++) {
        passage += savedLines[i];
    }

    passage.trim();
    passage.toLowerCase();

    return passage;
}

void printRememberedLines()
{
    Serial.println("Remembered text:");

    for (int i = 0; i < savedLineCount; i++) {
        Serial.print("line ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.println(savedLines[i]);
    }

    Serial.println("----------------");
}

void printFullPassageAndMaybeEnableSpi()
{
    String withSpaces = buildFullPassageWithSpaces();
    latestPassageNoSpaces = buildFullPassageNoSpaces();

    Serial.println();
    Serial.println("========== FULL PASSAGE ==========");
    Serial.println(withSpaces);
    Serial.println("==================================");

    Serial.print("SPI passage without spaces: ");
    Serial.println(latestPassageNoSpaces);
    Serial.println();

    /*
       Important:
       Only enable SPI if there is real remembered text.
       Do NOT send fake "empty".
    */
    if (savedLineCount > 0 && latestPassageNoSpaces.length() > 0) {
        passageReadyForSpi = true;
        Serial.println("Passage exists -> SPI enabled");
    } else {
        passageReadyForSpi = false;
        latestPassageNoSpaces = "";
        consecutiveInvokeFails = 0;
        Serial.println("No passage yet -> SPI disabled, continue AI invoke");
    }

    Serial.println("----------------");
}

/* =========================
   SPI service
   ========================= */

void prepareSpiReplyPacket()
{
    memset(spi_reply_packet, 0, sizeof(spi_reply_packet));

    String sendText = latestPassageNoSpaces;

    /*
       Safety check:
       No real passage means length 0.
       This should not normally happen because SPI is disabled.
    */
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

/*
   This function is only called after a valid passage exists.
*/
void serviceSpiOnlyAfterPassageReady()
{
    if (!passageReadyForSpi || latestPassageNoSpaces.length() == 0) {
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
    if (spi_rx_request[0] < 0x10) Serial.print("0");
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
        latestPassageNoSpaces = "";
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
       [2..] = passage text without spaces
    */
    slave.queue(spi_reply_packet, spi_reply_rx_discard, packet_len);

    const std::vector<size_t> sent_bytes = slave.wait(10000);

    if (!sent_bytes.empty()) {
        Serial.print("SPI reply sent, bytes=");
        Serial.println(sent_bytes[0]);

        /*
           Send only once per completed passage.
           Then reset and let AI start collecting a new passage.
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

    for (int i = 0; i < MAX_TRACKED_LINES; i++) {
        savedLines[i] = "";
    }

    for (int i = 0; i < MAX_CANDIDATES; i++) {
        candidates[i].bestText = "";
        candidates[i].count = 0;
        candidates[i].saved = false;
        candidates[i].lastSeenFrame = -9999;
    }

    Serial.println("Starting SPI slave...");

    slave.setDataMode(SPI_MODE);
    slave.setQueueSize(1);

    /*
       Important:
       You said begin() must be begin().
       Do not pass custom pins here.
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
       If a valid passage is ready, stop AI work
       and only process SPI.
    */
    if (passageReadyForSpi) {
        serviceSpiOnlyAfterPassageReady();
        delay(PRINT_DELAY_MS);
        return;
    }

    /*
       Before invoke fail with real saved text,
       only run AI. No SPI is processed here.
    */
    if (!AI.invoke()) {
        consecutiveInvokeFails = 0;

        DetectedLine detectedLines[MAX_TRACKED_LINES];
        int detectedCount = extractDetectedLines(detectedLines, MAX_TRACKED_LINES);

        if (detectedCount > 0) {
            processDetectedLines(detectedLines, detectedCount);
            printRememberedLines();
        }
    } else {
        consecutiveInvokeFails++;

        Serial.print("invoke failed count=");
        Serial.print(consecutiveInvokeFails);
        Serial.print("/");
        Serial.println(MAX_CONSECUTIVE_INVOKE_FAILS);

        /*
           On first invoke fail:
           - if no text was saved, keep doing AI
           - if text exists, enable SPI
        */
        if (consecutiveInvokeFails == 1) {
            printRememberedLines();
            printFullPassageAndMaybeEnableSpi();
        }

        /*
           If repeated invoke fails happen but no passage exists,
           keep trying instead of entering SPI.
        */
        if (consecutiveInvokeFails >= MAX_CONSECUTIVE_INVOKE_FAILS &&
            savedLineCount == 0) {
            Serial.println("No saved text after invoke fails -> continue AI");
            consecutiveInvokeFails = 0;
        }
    }

    delay(PRINT_DELAY_MS);
}