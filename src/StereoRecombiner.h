/*
 * Copyright (C) 2026 reAAF contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef REAPER_AAF_STEREORECOMBINER_H
#define REAPER_AAF_STEREORECOMBINER_H

#include "AafData.h"

#include <string>
#include <vector>

class LogBuffer;

// Recombines the adjacent, hard-panned _L/_R tracks produced by Premiere's
// "Breakout to Mono" export. Ambiguous or unsupported pairs are left intact.
void recombinePremiereBreakoutStereo(std::vector<AudioTrackData> &tracks,
                                     const std::string &outputDir, LogBuffer &logBuffer);

#endif // REAPER_AAF_STEREORECOMBINER_H
