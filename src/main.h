#ifndef MAIN_H
#define MAIN_H

#pragma once

// #define DEBUG_DISABLED

#ifdef DEBUG_DISABLED
#define DEBUG_D(fmt, ...)
#else
#define DEBUG_D(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#endif

#endif
