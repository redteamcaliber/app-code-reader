/* Fault code reader for Carloop
 *
 * Reads OBD diagnostic trouble codes (DTC) at 500 kbit and outputs them
 * to the USB serial port and as Particle events.
 *
 * Type 'r' on the serial port to start reading codes.
 * Mac OSX: screen /dev/tty.usbmodem1411 (update for your port number)
 * Linux: screen /dev/ttyACM*
 * Windows: Use PuTTY
 *
 * To read codes through the network run these 2 commands in different terminals:
 * particle subscribe mine
 * particle call my_carloop readCodes
 *
 * Codes are published in the code/results event as a comma separated
 * list with the last letter indicating the type of code
 * P0415s,P0010p,U0300c
 * Means P0415 stored (current code), P0010 pending (will become stored
 * after a few more trips with the code active), U0300c cleared (the
 * issue fixed itself or codes were cleared)
 *
 * Copyright 2016 1000 Tools, Inc
 *
 * Distributed under the MIT license. See LICENSE.txt for more details.
 */

#include "application.h"
#include "carloop/carloop.h"

#include "dtc.h"

SYSTEM_THREAD(ENABLED);

int readCodes(String unused = "");
int clearCodes(String unused = "");
void publishCodes();
void processSerial();
void processReadingCodes();
void processClearingCodes();
void publishCodes();

// Set up the Carloop hardware
Carloop<CarloopRevision2> carloop;

// Set up the trouble code services
CodeReader reader;
CodeClearer clearer;

inline void log(String message) {
  Serial.println(message);
}

void setup() {
  Serial.begin(9600);
  carloop.begin();
  reader.begin(carloop.can());
  clearer.begin(carloop.can());

  Particle.function("readCodes", readCodes);
  Particle.function("clearCodes", clearCodes);
}

// Remote function to start reading codes
int readCodes(String unused) {
  log("Reading codes...");
  if (Particle.connected()) {
    Particle.publish("codes/start", PRIVATE);
  }
  reader.start();
  return 0;
}

// Remote function to clear codes
int clearCodes(String unused) {
  log("Clearing codes...");
  if (Particle.connected()) {
    Particle.publish("codes/clear", PRIVATE);
  }
  clearer.start();
  return 0;
}

void loop() {
  processSerial();
  processReadingCodes();
  processClearingCodes();
}

void processSerial() {
  // Type letter on the serial port to read or clear codes
  switch (Serial.read()) {
    case 'r':
      readCodes();
      break;
    case 'c':
      clearCodes();
      break;
  }
}

// Let the code reader do its thing
void processReadingCodes() {
  reader.process();

  static bool previouslyDone = true;
  bool done = reader.done();
  if (done && !previouslyDone) {
    publishCodes();
  }
  previouslyDone = done;
}

// Print codes that were read to the serial port and publish them as
// Particle events
void publishCodes() {
  if (reader.getError()) {
    log("Error while reading codes. Is Carloop connected to a car with the ignition on?");
    if (Particle.connected()) {
      Particle.publish("codes/error", PRIVATE);
    }
    return;
  }

  auto &codes = reader.getCodes();

  if (codes.empty()) {
    log("No fault codes. Fantastic!");
  } else {
    log(String::format("Read %d codes", codes.size()));
  }

  String result;
  String line;
  for (auto it = codes.begin(); it != codes.end(); ++it) {
    auto code = *it;
    String codeStr;
    codeStr += code.letter;
    codeStr += String::format("%04X", code.code);

    line = codeStr;
    switch (code.type) {
      case DTC::STORED_DTC:
        line += " (current issue)";
        codeStr += 's';
        break;
      case DTC::PENDING_DTC:
        line += " (pending issue)";
        codeStr += 'p';
        break;
      case DTC::CLEARED_DTC:
        line += " (cleared issue)";
        codeStr += 'c';
        break;
    }

    log(line);

    if (result.length() > 0) {
      result += ",";
    }
    result += codeStr;
  }

  if (Particle.connected()) {
    Particle.publish("codes/result", result, PRIVATE);
  }
}

// Let the code clearer do its thing
void processClearingCodes() {
  clearer.process();

  static bool previouslyDone = true;
  bool done = clearer.done();
  if (done && !previouslyDone) {
    if (clearer.getError()) {
      log("Error while clearing codes. Is Carloop connected to a car with the ignition on?");
      if (Particle.connected()) {
        Particle.publish("codes/error", PRIVATE);
      }
    } else {
      log("Success!");
      if (Particle.connected()) {
        Particle.publish("codes/cleared", PRIVATE);
      }
    }
  }
  previouslyDone = done;
}

