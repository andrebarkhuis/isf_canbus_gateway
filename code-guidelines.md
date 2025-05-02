# Code Style Guide and Rules

This document defines the code style conventions, file organization rules, and clean coding practices for the project. It ensures consistency and maintainability across the codebase, particularly for Arduino and C++ development.

## Header File Inclusion Rule

Always update #include references using the following rules to maintain consistency, clarity, and reliable builds:

* Use a consistent and canonical path for each header file throughout the codebase.
* Do not mix styles like #include "common/logger.h" and #include "logger.h" for the same file unless necessary to resolve ambiguous references.
* Ensure that all headers are located within clearly defined folders (e.g., common/, can/, services/) and included accordingly.
* Ensure all include paths are consistent throughout the codebase. Avoid using different paths to include the same file (e.g., #include "common/logger.h" vs #include "logger.h") unless it's necessary to resolve reference or scope issues.
* Ensure only header files ( .h ) are referenced instead of source files ( .cpp ) directly. If a cpp file does not have a corresponding header file, add it.

## Project Header Files

* Use **double quotes (`"..."`)** for all headers that are part of your own project, including any files within the `src` directory or its subfolders.
* Always include the **relative folder path** from the project root:

  ```cpp
  #include "utils/logger.h"        // ✅ Correct
  #include "mcp_can/mcp_canbus.h"  // ✅ Correct
  #include "logger.h"              // ❌ Avoid: lacks folder context
  ```

* Use **angle brackets (`<...>`)** for:

  * Arduino core libraries
  * Board support packages (e.g., ESP-IDF, FreeRTOS)
  * Any third-party libraries installed via the Arduino Library Manager

  Examples:

  ```cpp
  #include <Wire.h>                // ✅ Arduino core
  #include <freertos/FreeRTOS.h>   // ✅ Board/RTOS
  #include <Adafruit_Sensor.h>     // ✅ External library
  ```

## Code Commenting

* Add concise, meaningful comments that explain **why** the code exists or what it intends to accomplish.
* Avoid comments that simply restate the code:

  ```cpp
  counter++; // ❌ Don't: "increment counter"
  counter++; // ✅ Do: "advance to the next retry attempt"
  ```

## Variable Usage

* Before introducing any new variable or function, **search the existing codebase** to avoid duplication.
* Reuse or extend existing constructs when possible to keep the code **DRY (Don't Repeat Yourself)**.
* Avoid **magic numbers or strings**; extract them into named constants if used more than once or tied to a specific meaning.

## Clean Code Principles

* Follow clean code practices:
  * Small, purpose-driven functions
  * One responsibility per method/class
  * Meaningful, descriptive names
  * Consistent formatting and indentation

* Refactor when:
  * A function exceeds **30–40 lines**
  * It starts handling **multiple concerns or conditions**
