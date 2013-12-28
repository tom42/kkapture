/* kkapture: intrusive demo video capturing.
 * Console API driver.
 * Copyright (c) 2013 Thomas Mathys.
 *
 * This program is free software; you can redistribute and/or modify it under
 * the terms of the Artistic License, Version 2.0beta5, or (at your opinion)
 * any later version; all distributions of this program should contain this
 * license in a file named "LICENSE.txt".
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT UNLESS REQUIRED BY
 * LAW OR AGREED TO IN WRITING WILL ANY COPYRIGHT HOLDER OR CONTRIBUTOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "stdafx.h"
#include "consolefonts.h"
#include "video.h"
#include "videoencoder.h"

static const int CharWidth = 8;
static const int CharHeight = 8;

struct palette_entry
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

static const palette_entry palette[16] =
{
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0xaa},
    {0x00, 0xaa, 0x00},
    {0x00, 0xaa, 0xaa},
    {0xaa, 0x00, 0x00},
    {0xaa, 0x00, 0xaa},
    {0xaa, 0x55, 0x00},
    {0xaa, 0xaa, 0xaa},
    {0x55, 0x55, 0x55},
    {0x55, 0x55, 0xff},
    {0x55, 0xff, 0x55},
    {0x55, 0xff, 0xff},
    {0xff, 0x55, 0x55},
    {0xff, 0x55, 0xff},
    {0xff, 0xff, 0x55},
    {0xff, 0xff, 0xff}
};

// Draw character onto capture buffer.
// No clipping is performed and the 8x8 font is used.
// column: column to draw character at (0 = left)
// row   : row to draw character at (0 = top)
// c     : character that should be rendered
static void drawCharacter(int column, int row, CHAR_INFO c)
{
    const unsigned char* src = &console_font_vga_rom_8x8[unsigned char(c.Char.AsciiChar) * CharHeight];
    unsigned char* dst = captureData + (column * CharWidth + (captureHeight - 1 - row * CharHeight) * captureWidth) * 3;
    const palette_entry* fgColor = &palette[c.Attributes & 15];
    const palette_entry* bgColor = &palette[(c.Attributes >> 4) & 15];

    for (int y = 0; y < CharHeight; ++y)
    {
        unsigned char bits = *src++;
        for (int x = 0; x < CharWidth; ++x)
        {
            const palette_entry* color = (bits & 128) ? fgColor : bgColor;
            *dst++ = color->b;
            *dst++ = color->g;
            *dst++ = color->r;
            bits <<= 1;
        }
        dst -= (captureWidth + CharWidth) * 3;
    }
}

static BOOL (__stdcall *Real_WriteConsoleOutputA)(HANDLE hConsoleOutput, CONST CHAR_INFO *lpBuffer, COORD dwBufferSize, COORD dwBufferCoord, PSMALL_RECT lpWriteRegion) = WriteConsoleOutputA;

static BOOL __stdcall Mine_WriteConsoleOutputA(HANDLE hConsoleOutput, CONST CHAR_INFO *lpBuffer, COORD dwBufferSize, COORD dwBufferCoord, PSMALL_RECT lpWriteRegion)
{
    // TODO: capture (but for the time being, only for known resolution, e.g. 80x50)
    // TODO: also honor params.CaptureVideo somewhere...
    auto result = Real_WriteConsoleOutputA(hConsoleOutput, lpBuffer, dwBufferSize, dwBufferCoord, lpWriteRegion);

    // TODO: testcode...
    setCaptureResolution(640, 480);
    for (int y = 0; y < 16; ++y)
    {
        for (int x = 0; x < 16; ++x)
        {
            CHAR_INFO c;
            c.Char.AsciiChar = x + y * 16;
            c.Attributes = x;
            drawCharacter(x, y, c);
        }
    }
    encoder->WriteFrame(captureData);
    nextFrame();

    return result;
}

void initVideo_Console()
{
  if (params.EnableConsoleCapture)
  {
      HookFunction(&Real_WriteConsoleOutputA, Mine_WriteConsoleOutputA);
  }
}
