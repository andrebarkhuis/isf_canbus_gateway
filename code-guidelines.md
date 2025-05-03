# Code Style Guide and Rules

This document defines code style conventions, file organization rules, and clean coding practices for the project. It aims to ensure consistency, clarity, and maintainability across the codebase, especially for Arduino and C++ development.

## 1. Header File Inclusion Rules

To ensure consistency and avoid ambiguity in file references:

- Always use a **canonical, consistent path** when including header files.
- Avoid mixing styles for the same file (e.g., `#include "logger/logger.h"` vs `#include "logger.h"`). Only use alternative styles if necessary to resolve scope or reference issues.
- Header files should be located in clearly defined folders (e.g., `common/`, `can/`, `services/`) and referenced using their **relative path from the project root**.
- Only include `.h` files, **never** `.cpp` files directly. If a `.cpp` file does not have a corresponding header, create one.

**Correct Usage Example:**

```cpp
#include "logger/logger.h"        // ✅ Correct: with folder context
#include "mcp_can/mcp_canbus.h"   // ✅ Correct

#include "logger.h"               // ❌ Avoid: lacks folder context
````

## 2. Project Folder and Library Structure

### 2.1 Arduino Libraries

* All reusable modules must reside in their own subfolder under the `libraries/` directory, e.g., `libraries/logger`, `libraries/common`.
* Each module **must** contain a valid `library.properties` file for compatibility with the Arduino CLI and IDE.

**Example `library.properties`:**

```ini
name=ModuleName
version=1.0.0
author=Your Name
sentence=Short description of the library.
paragraph=Longer description of the module and its purpose.
category=Other
```

This ensures modules are indexed correctly, avoids build warnings, and promotes modularity and reuse.

## 3. Include Syntax Guidelines

Use **double quotes (`"..."`)** for:

* Project-specific headers (files within `src/` or subfolders)

Use **angle brackets (`<...>`)** for:

* Arduino core libraries
* Board support packages (e.g., ESP-IDF, FreeRTOS)
* External libraries installed via the Arduino Library Manager

**Examples:**

```cpp
#include "logger/logger.h"         // ✅ Project-specific
#include <Wire.h>                  // ✅ Arduino core
#include <freertos/FreeRTOS.h>     // ✅ Board support package
#include <Adafruit_Sensor.h>       // ✅ External library
```

## 4. Code Commenting Best Practices

* Write concise comments that explain **why** the code exists or what it aims to accomplish.
* Avoid stating the obvious or repeating what the code already expresses.

**Examples:**

```cpp
counter++; // ❌ Avoid: "increment counter"
counter++; // ✅ Good: "advance to the next retry attempt"
```

## 5. Variable and Function Usage

* Before introducing new variables or functions, **search the codebase** to avoid duplication.
* Reuse or extend existing constructs where possible to adhere to **DRY** (Don't Repeat Yourself) principles.
* Avoid "magic numbers" or strings—extract them into named constants if reused or meaningful.

## 6. Clean Code Principles

Follow established clean coding guidelines:

* Write small, single-purpose functions
* Assign one responsibility per method or class
* Use clear, descriptive names
* Maintain consistent formatting and indentation

**Refactor code when:**

* A function exceeds **30–40 lines**
* It begins to handle **multiple concerns or conditions**
