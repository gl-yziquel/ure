/*
 * src/Util/Logger.cc
 *
 * Copyright (C) 2002-2007 Novamente LLC
 * Copyright (C) 2008 by Singularity Institute for Artificial Intelligence
 * All Rights Reserved
 *
 * Written by Andre Senna <senna@vettalabs.com>
 *            Gustavo Gama <gama@vettalabs.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as 
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "Logger.h"

#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

using namespace opencog;

// messages greater than this will be truncated
#define MAX_PRINTF_STYLE_MESSAGE_SIZE (1<<15)
const char* levelStrings[] = {"ERROR","WARN","INFO","DEBUG","FINE"};

Logger::~Logger() {
	if (f != NULL) fclose(f);
}

Logger::Logger(const std::string &fileName, Logger::Level level, bool timestampEnabled) {
    this->fileName.assign(fileName);
    this->currentLevel = level;
    this->timestampEnabled = timestampEnabled;
    this->printToStdout = false;

    this->logEnabled = true;
    this->f = NULL;

    pthread_mutex_init(&lock, NULL);
}

// ***********************************************/
// API

void Logger::setLevel(Logger::Level newLevel) {
    currentLevel = newLevel;
}

Logger::Level Logger::getLevel() const {
    return currentLevel;
}

void Logger::setFilename(const std::string& s) {
    fileName.assign(s);
    f = NULL;
    enable();
}

const std::string& Logger::getFilename() {
    return fileName;
}

void Logger::setTimestampFlag(bool flag) {
    timestampEnabled = flag;
}

void Logger::setPrintToStdoutFlag(bool flag) {
    printToStdout = flag;
}

void Logger::enable() {
    logEnabled = true;
}

void Logger::disable() {
    logEnabled = false;
}

void Logger::log(Logger::Level level, const std::string &txt) {
    if (!logEnabled) return; 
    // delay opening the file until the first logging statement is issued;
    // this allows us to set the main logger's filename without creating
    // a useless log file with the default filename
    if (f == NULL) {
        if ((f = fopen(fileName.c_str(), "a")) == NULL) {
            fprintf(stderr, "[ERROR] Unable to open log file \"%s\"\n", fileName.c_str());
            disable();
            return;
        } else enable();
    }

    if (level <= currentLevel) {
        pthread_mutex_lock(&lock);
        if (timestampEnabled) {
            char timestamp[64];
            struct timeval stv;
            struct tm stm;

            gettimeofday(&stv, NULL);
            gmtime_r(&(stv.tv_sec), &stm);
            strftime(timestamp, sizeof(timestamp), "%F %T", &stm);
            fprintf(f, "[%s:%03ld] ", timestamp, stv.tv_usec / 1000);
            if (printToStdout) fprintf(stdout, "[%s:%3ld] ", timestamp, stv.tv_usec / 1000);
        }

        fprintf(f, "[%s] %s\n", getLevelString(level), txt.c_str());
        if (printToStdout) fprintf(stdout, "[%s] %s\n", getLevelString(level), txt.c_str());
        fflush(f);

        pthread_mutex_unlock(&lock);
    }
}
void Logger::error(const std::string &txt) { log(ERROR, txt); }
void Logger::warn (const std::string &txt) { log(WARN,  txt); }
void Logger::info (const std::string &txt) { log(INFO,  txt); }
void Logger::debug(const std::string &txt) { log(DEBUG, txt); }
void Logger::fine (const std::string &txt) { log(FINE,  txt); }

void Logger::logva(Logger::Level level, const char *fmt, va_list args) {
    if (level <= currentLevel) {
        char buffer[MAX_PRINTF_STYLE_MESSAGE_SIZE];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        std::string msg = buffer;
        log(level, msg);
    }
}

void Logger::log(Logger::Level level, const char *fmt, ...) { va_list args; va_start(args, fmt); logva(level, fmt, args); va_end(args); }
void Logger::error(const char *fmt, ...) { va_list args; va_start(args, fmt); logva(ERROR, fmt, args); va_end(args); }
void Logger::warn (const char *fmt, ...) { va_list args; va_start(args, fmt); logva(WARN,  fmt, args); va_end(args); }
void Logger::info (const char *fmt, ...) { va_list args; va_start(args, fmt); logva(INFO,  fmt, args); va_end(args); }
void Logger::debug(const char *fmt, ...) { va_list args; va_start(args, fmt); logva(DEBUG, fmt, args); va_end(args); }
void Logger::fine (const char *fmt, ...) { va_list args; va_start(args, fmt); logva(FINE,  fmt, args); va_end(args); }

const char* Logger::getLevelString(const Logger::Level level) {
    return levelStrings[level];
}

const Logger::Level Logger::getLevelFromString(const std::string& levelStr) {
    unsigned int nLevels = sizeof(levelStrings)/sizeof(levelStrings[0]);
    for (unsigned int i = 0; i < nLevels; ++i) {
        if (strcasecmp(levelStrings[i], levelStr.c_str()) == 0)
            return ((Logger::Level) i);
    }
    return ((Logger::Level) -1);
}

// create and return the single instance
Logger& opencog::logger() {
    static Logger instance;
    return instance;
}
