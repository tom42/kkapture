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

static bool isSupportedBlit(const COORD dwBufferSize, const COORD dwBufferCoord, const SMALL_RECT &writeRegion)
{
  return (dwBufferSize.X == 80) &&
    (dwBufferSize.Y == 50) &&
    (dwBufferCoord.X == 0) &&
    (dwBufferCoord.Y == 0) &&
    (writeRegion.Left == 0) &&
    (writeRegion.Top == 0) &&
    (writeRegion.Right == 79) &&
    (writeRegion.Bottom == 49);
}

static void captureTextmodeFrame(const CHAR_INFO *lpBuffer)
{
  setCaptureResolution(80 * CharWidth, 50 * CharHeight);
  for (int row = 0; row < 50; ++row)
  {
    for (int column = 0; column < 80; ++column)
    {
      drawCharacter(column, row, *lpBuffer);
      ++lpBuffer;
    }
  }
  encoder->WriteFrame(captureData);
}

static void dumpConsoleBuffer(const CHAR_INFO* lpBuffer)
{
    const size_t filename_bufsize = 512;
    const size_t dump_bufsize = 80 * 50 * 2;
    static int frame = 0;

    // Create filename.
    char filename[filename_bufsize];
    _snprintf_s(filename, filename_bufsize, "%05d.console.bin", frame);
    ++frame;

    // Prepare data dump.    
    unsigned char dump[80 * 50 * 2];
    switch (params.ConsoleBufferDumpStyle)
    {
    case ConsoleBufferDumpStyle::InterleavedCodesAndAttributes:
        for (size_t i = 0; i < 80 * 50; ++i)
        {
            dump[i * 2] = lpBuffer[i].Char.AsciiChar;
            dump[i * 2 + 1] = lpBuffer[i].Attributes & 255;
        }
        break;
    case ConsoleBufferDumpStyle::SeparateCodesAndAttributes:
        for (size_t i = 0; i < 80 * 50; ++i)
        {
            dump[i] = lpBuffer[i].Char.AsciiChar;
            dump[i + 80 * 50] = lpBuffer[i].Attributes & 255;
        }
        break;
    case ConsoleBufferDumpStyle::SeparateCodesBgAndFgColor:
        // Characters
        for (size_t i = 0; i < 80 * 50; ++i)
        {
            dump[i] = lpBuffer[i].Char.AsciiChar;
        }
        // Foreground colors
        for (size_t i = 0; i < 80 * 50 / 2; ++i)
        {
            unsigned char color0 = lpBuffer[i * 2 + 0].Attributes & 15;
            unsigned char color1 = lpBuffer[i * 2 + 1].Attributes & 15;
            dump[i + 80 * 50] = color0 | (color1 << 4);
        }
        // Background colors
        for (size_t i = 0; i < 80 * 50 / 2; ++i)
        {
            unsigned char color0 = (lpBuffer[i * 2 + 0].Attributes >> 4) & 15;
            unsigned char color1 = (lpBuffer[i * 2 + 1].Attributes >> 4) & 15;
            dump[i + 80 * 50 + 80 * 50 / 2] = color0 | (color1 << 4);
        }
        break;
    default:
        printLog("video/console: dumpConsoleBuffer: unsupported dump style: %d\n", params.ConsoleBufferDumpStyle);
        break;
    }

    // Write data dump to file.
    FILE* f = fopen(filename, "wb");
    if (f)
    {
        fwrite(dump, dump_bufsize, 1, f);
        fclose(f);
    }
}

static BOOL (__stdcall *Real_WriteConsoleOutputA)(HANDLE hConsoleOutput, CONST CHAR_INFO *lpBuffer, COORD dwBufferSize, COORD dwBufferCoord, PSMALL_RECT lpWriteRegion) = WriteConsoleOutputA;

static BOOL __stdcall Mine_WriteConsoleOutputA(HANDLE hConsoleOutput, CONST CHAR_INFO *lpBuffer, COORD dwBufferSize, COORD dwBufferCoord, PSMALL_RECT lpWriteRegion)
{
  const auto result = Real_WriteConsoleOutputA(hConsoleOutput, lpBuffer, dwBufferSize, dwBufferCoord, lpWriteRegion);

  if (result && isSupportedBlit(dwBufferSize, dwBufferCoord, *lpWriteRegion))
  {
    if (params.CaptureVideo)
    {
      captureTextmodeFrame(lpBuffer);
      if (params.DumpConsoleBuffer)
      {
        dumpConsoleBuffer(lpBuffer);
      }
    }
    nextFrame();
  }
  else
  {
    printLog("video/console: WriteConsoleOutputA: failed/unsupported blit: result=%d dwBufferSize=%d,%d dwBufferCoord=%d,%d *lpWriteRegion=%d,%d,%d,%d\n",
      result, dwBufferSize.X, dwBufferSize.Y, dwBufferCoord.X, dwBufferCoord.Y,
      lpWriteRegion->Left, lpWriteRegion->Top, lpWriteRegion->Right, lpWriteRegion->Bottom);
  }

  return result;
}

void initVideo_Console()
{
  if (params.EnableConsoleCapture)
  {
    HookFunction(&Real_WriteConsoleOutputA, Mine_WriteConsoleOutputA);
  }
}
