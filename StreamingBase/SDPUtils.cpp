/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2008 Apple Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */

#include <CF/StringParser.h>
#include "SDPUtils.h"

using namespace CF;

SInt32 SDPContainer::AddHeaderLine(StrPtrLen *theLinePtr) {
  Assert(theLinePtr);
  UInt32 thisLine = static_cast<UInt32>(fNumUsedLines);
  Assert(fNumUsedLines < fNumSDPLines);
  fSDPLineArray[thisLine].Set(theLinePtr->Ptr, theLinePtr->Len);
  fNumUsedLines++;
  if (fNumUsedLines == fNumSDPLines) { // expand memory
    SDPLine *tempSDPLineArray = new SDPLine[fNumSDPLines * 2];
    for (int i = 0; i < fNumSDPLines; i++) {
      tempSDPLineArray[i].Set(fSDPLineArray[i].Ptr, fSDPLineArray[i].Len);
    }
    delete[] fSDPLineArray;
    fSDPLineArray = tempSDPLineArray;
    fNumSDPLines = (fNumUsedLines * 2);
  }

  return thisLine;
}

SInt32 SDPContainer::FindHeaderLineType(char id, SInt32 start) {
  SInt32 theIndex = -1;

  if (start >= fNumUsedLines || start < 0)
    return -1;

  for (int i = start; i < fNumUsedLines; i++) {
    if (fSDPLineArray[i].GetHeaderType() == id) {
      theIndex = i;
      fCurrentLine = theIndex;
      break;
    }
  }

  return theIndex;
}

SDPLine *SDPContainer::GetNextLine() {
  if (fCurrentLine < fNumUsedLines) {
    fCurrentLine++;
    return &fSDPLineArray[fCurrentLine];
  }

  return nullptr;

}

SDPLine *SDPContainer::GetLine(SInt32 lineIndex) {

  if (lineIndex > -1 && lineIndex < fNumUsedLines) {
    return &fSDPLineArray[lineIndex];
  }

  return nullptr;
}

void SDPContainer::SetLine(SInt32 index) {
  if (index > -1 && index < fNumUsedLines) {
    fCurrentLine = index;
  } else Assert(0);

}

void SDPContainer::Parse() {
  static char const *sValidChars = "vosiuepcbtrzkam";
  static char const sNameValueSeparator = '=';

  bool valid = true;

  StringParser sdpParser(&fSDPBuffer);
  StrPtrLen line;
  StrPtrLen fieldName;
  StrPtrLen space;
  bool foundLine = false;

  while (sdpParser.GetDataRemaining() != 0) {
    foundLine = sdpParser.GetThruEOL(&line);  // Read each line
    if (!foundLine) {
      break;
    }
    StringParser lineParser(&line);

    lineParser.ConsumeWhitespace();//skip over leading whitespace
    if (lineParser.GetDataRemaining() == 0) continue; // must be an empty line

    char firstChar = lineParser.PeekFast();
    if (firstChar == '\0') continue; //skip over blank lines

    fFieldStr[(UInt8) firstChar] = firstChar;
    switch (firstChar) {
      case 'v': fReqLines |= kV; break;
      case 's': fReqLines |= kS; break;
      case 't': fReqLines |= kT; break;
      case 'o': fReqLines |= kO; break;
    }

    lineParser.ConsumeUntil(&fieldName, sNameValueSeparator);
    if ((fieldName.Len != 1) || (::strchr(sValidChars, fieldName.Ptr[0]) == nullptr)) {
      valid = false; // line doesn't begin with one of the valid characters followed by an "="
      break;
    }

    if (!lineParser.Expect(sNameValueSeparator)) {
      valid = false; // line doesn't have the "=" after the first char
      break;
    }

    lineParser.ConsumeUntil(&space, StringParser::sNonWhitespaceMask);

    if (space.Len != 0) {
      valid = false; // line has whitespace after the "="
      break;
    }

    AddHeaderLine(&line);
  }

  if (fNumUsedLines == 0) {  // didn't add any lines
    valid = false;
  }
  fValid = valid;
}

void SDPContainer::Initialize() {
  fCurrentLine = 0;
  fNumUsedLines = 0;
  delete[] fSDPLineArray;
  fSDPLineArray = new SDPLine[fNumSDPLines];
  fValid = false;
  fReqLines = 0;
  ::memset(fFieldStr, sizeof(fFieldStr), 0);
}

bool SDPContainer::SetSDPBuffer(char *sdpBuffer) {

  Initialize();
  if (sdpBuffer != nullptr) {
    fSDPBuffer.Set(sdpBuffer);
    Parse();
  }

  return IsSDPBufferValid();
}

bool SDPContainer::SetSDPBuffer(StrPtrLen *sdpBufferPtr) {
  Initialize();
  if (sdpBufferPtr != nullptr) {
    fSDPBuffer.Set(sdpBufferPtr->Ptr, sdpBufferPtr->Len);
    Parse();
  }

  return IsSDPBufferValid();
}

void SDPContainer::PrintLine(SInt32 lineIndex) {
  StrPtrLen *printLinePtr = GetLine(lineIndex);
  if (printLinePtr) {
    printLinePtr->PrintStr();
    s_printf("\n");
  }

}

void SDPContainer::PrintAllLines() {
  if (fNumUsedLines > 0) {
    for (int i = 0; i < fNumUsedLines; i++)
      PrintLine(i);
  } else
    s_printf("SDPContainer::PrintAllLines no lines\n");
}

bool SDPLineSorter::ValidateSessionHeader(StrPtrLen *theHeaderLinePtr) {
  if (nullptr == theHeaderLinePtr || 0 == theHeaderLinePtr->Len
      || nullptr == theHeaderLinePtr->Ptr)
    return false;

  // check for a duplicate range line.
  StrPtrLen currentSessionHeader
      (fSDPSessionHeaders.GetBufPtr(), fSDPSessionHeaders.GetBytesWritten());
  if ('a' == theHeaderLinePtr->Ptr[0] && theHeaderLinePtr->FindString("a=range")
      &&
          currentSessionHeader.FindString("a=range")) {
    return false;
  }

  return true;

}

char SDPLineSorter::sSessionOrderedLines[] = "vosiuepcbtrzka"; // chars are order dependent: declared by rfc 2327
char SDPLineSorter::sessionSingleLines[] = "vtosiuepcbzk";    // return only 1 of each of these session field types
StrPtrLen  SDPLineSorter::sEOL("\r\n");
StrPtrLen  SDPLineSorter::sMaxBandwidthTag("b=AS:");

SDPLineSorter::SDPLineSorter(SDPContainer *rawSDPContainerPtr, Float32 adjustMediaBandwidthPercent, SDPContainer *insertMediaLinesArray)
    : fSessionLineCount(0),
      fSDPSessionHeaders(nullptr, 0),
      fSDPMediaHeaders(nullptr, 0) {

  Assert(rawSDPContainerPtr != nullptr);
  if (nullptr == rawSDPContainerPtr)
    return;

  StrPtrLen theSDPData(rawSDPContainerPtr->fSDPBuffer.Ptr, rawSDPContainerPtr->fSDPBuffer.Len);
  StrPtrLen *theMediaStart = rawSDPContainerPtr->GetLine(rawSDPContainerPtr->FindHeaderLineType('m', 0));
  if (theMediaStart && theMediaStart->Ptr && theSDPData.Ptr) {
    UInt32 mediaLen = theSDPData.Len - (UInt32) (theMediaStart->Ptr - theSDPData.Ptr);
    char *mediaStartPtr = theMediaStart->Ptr;
    fMediaHeaders.Set(mediaStartPtr, mediaLen);
    StringParser sdpParser(&fMediaHeaders);
    SDPLine sdpLine;
    bool foundLine = false;
    bool newMediaSection = true;
    SDPLine *insertLine = nullptr;

    while (sdpParser.GetDataRemaining() > 0) {
      foundLine = sdpParser.GetThruEOL(&sdpLine);
      if (!foundLine) {
        break;
      }
      if (sdpLine.GetHeaderType() == 'm')
        newMediaSection = true;

      if (insertMediaLinesArray && newMediaSection && (sdpLine.GetHeaderType() == 'a')) {
        newMediaSection = false;
        for (insertLine = insertMediaLinesArray->GetLine( 0); insertLine; insertLine = insertMediaLinesArray->GetNextLine())
          fSDPMediaHeaders.Put(*insertLine);
      }

      if (('b' == sdpLine.GetHeaderType()) && (1.0 != adjustMediaBandwidthPercent)) {
        StringParser bLineParser(&sdpLine);
        bLineParser.ConsumeUntilDigit();
        UInt32 bandwidth = (UInt32) (.5 + (adjustMediaBandwidthPercent * (Float32) bLineParser.ConsumeInteger()));
        if (bandwidth < 1)
          bandwidth = 1;

        char bandwidthStr[10];
        s_snprintf(bandwidthStr, sizeof(bandwidthStr) - 1, "%" _U32BITARG_ "", bandwidth);
        bandwidthStr[sizeof(bandwidthStr) - 1] = 0;

        fSDPMediaHeaders.Put(sMaxBandwidthTag);
        fSDPMediaHeaders.Put(bandwidthStr);
      } else
        fSDPMediaHeaders.Put(sdpLine);

      fSDPMediaHeaders.Put(SDPLineSorter::sEOL);
    }
    fMediaHeaders.Set(fSDPMediaHeaders.GetBufPtr(), fSDPMediaHeaders.GetBytesWritten());
  }

  fSessionLineCount = rawSDPContainerPtr->FindHeaderLineType('m', 0);
  if (fSessionLineCount < 0) { // didn't find it use the whole buffer
    fSessionLineCount = rawSDPContainerPtr->GetNumLines();
  }

  for (SInt16 sessionLineIndex = 0; sessionLineIndex < fSessionLineCount; sessionLineIndex++)
    fSessionSDPContainer.AddHeaderLine((StrPtrLen *) rawSDPContainerPtr->GetLine( sessionLineIndex));

  //s_printf("\nSession raw Lines:\n"); fSessionSDPContainer.PrintAllLines();

  SInt16 numHeaderTypes = sizeof(SDPLineSorter::sSessionOrderedLines) - 1;
  bool addLine = true;
  for (SInt16 fieldTypeIndex = 0; fieldTypeIndex < numHeaderTypes; fieldTypeIndex++) {
    SInt32 lineIndex = fSessionSDPContainer.FindHeaderLineType(SDPLineSorter::sSessionOrderedLines[fieldTypeIndex], 0);
    StrPtrLen *theHeaderLinePtr = fSessionSDPContainer.GetLine(lineIndex);

    while (theHeaderLinePtr != nullptr) {
      addLine = this->ValidateSessionHeader(theHeaderLinePtr);
      if (addLine) {
        fSDPSessionHeaders.Put(*theHeaderLinePtr);
        fSDPSessionHeaders.Put(SDPLineSorter::sEOL);
      }

      if (nullptr != ::strchr(sessionSingleLines, theHeaderLinePtr->Ptr[0])) // allow 1 of this type: use first found
        break; // move on to next line type

      lineIndex = fSessionSDPContainer.FindHeaderLineType(SDPLineSorter::sSessionOrderedLines[fieldTypeIndex], lineIndex + 1);
      theHeaderLinePtr = fSessionSDPContainer.GetLine(lineIndex);
    }
  }
  fSessionHeaders.Set(fSDPSessionHeaders.GetBufPtr(), fSDPSessionHeaders.GetBytesWritten());
}

char *SDPLineSorter::GetSortedSDPCopy() {
  char *fullbuffCopy = new char[fSessionHeaders.Len + fMediaHeaders.Len + 2];
  SInt32 buffPos = 0;
  memcpy(&fullbuffCopy[buffPos], fSessionHeaders.Ptr, fSessionHeaders.Len);
  buffPos += fSessionHeaders.Len;
  memcpy(&fullbuffCopy[buffPos], fMediaHeaders.Ptr, fMediaHeaders.Len);
  buffPos += fMediaHeaders.Len;
  fullbuffCopy[buffPos] = 0;

  return fullbuffCopy;
}


