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
/*
    File:       FilePrefsSource.cpp

    Contains:   Implements object defined in FilePrefsSource.h.

    Written by: Chris LeCroy
*/

#include "FilePrefsSource.h"
#include <string.h>
#include <stdio.h>

#include <CF/ConfParser.h>


/**
 * 以链表的方式存储 K-V 对
 */
class KeyValuePair {
 public:

  char *GetValue() { return fValue; }

 private:
  friend class FilePrefsSource;

  KeyValuePair(char const *inKey, char const *inValue, KeyValuePair *inNext);
  ~KeyValuePair();

  char *fKey;
  char *fValue;
  KeyValuePair *fNext;

  void ResetValue(char const *inValue);
};

KeyValuePair::KeyValuePair(char const *inKey, char const *inValue, KeyValuePair *inNext)
    : fKey(nullptr), fValue(nullptr), fNext(nullptr) {
  fKey = new char[::strlen(inKey) + 1];
  ::strcpy(fKey, inKey);
  fValue = new char[::strlen(inValue) + 1];
  ::strcpy(fValue, inValue);
  fNext = inNext;
}

KeyValuePair::~KeyValuePair() {
  delete[] fKey;
  delete[] fValue;
}

void KeyValuePair::ResetValue(char const *inValue) {
  delete[] fValue;
  fValue = new char[::strlen(inValue) + 1];
  ::strcpy(fValue, inValue);
}

FilePrefsSource::FilePrefsSource(bool allowDuplicates)
    : fKeyValueList(nullptr),
      fNumKeys(0),
      fAllowDuplicates(allowDuplicates) {
}

FilePrefsSource::~FilePrefsSource() {
  while (fKeyValueList != nullptr) {
    KeyValuePair *keyValue = fKeyValueList;
    fKeyValueList = fKeyValueList->fNext;
    delete keyValue;
  }
}

int FilePrefsSource::GetValue(char const *inKey, char *ioValue) {
  return (this->FindValue(inKey, ioValue) != nullptr);
}

int FilePrefsSource::GetValueByIndex(char const *inKey, UInt32 inIndex, char *ioValue) {
  KeyValuePair *thePair = this->FindValue(inKey, ioValue, inIndex);
  return thePair != nullptr;
}

char *FilePrefsSource::GetValueAtIndex(UInt32 inIndex) {
  // Iterate through the queue until we have the right entry
  KeyValuePair *thePair = fKeyValueList;
  while ((thePair != nullptr) && (inIndex-- > 0))
    thePair = thePair->fNext;

  if (thePair != nullptr)
    return thePair->fValue;
  return nullptr;
}

char *FilePrefsSource::GetKeyAtIndex(UInt32 inIndex) {
  // Iterate through the queue until we have the right entry
  KeyValuePair *thePair = fKeyValueList;
  while ((thePair != nullptr) && (inIndex-- > 0))
    thePair = thePair->fNext;

  if (thePair != nullptr)
    return thePair->fKey;
  return nullptr;
}

void FilePrefsSource::SetValue(char const *inKey, char const *inValue) {
  KeyValuePair *keyValue = nullptr;

  // If the key/value already exists update the value.
  // If duplicate keys are allowed, however, add a new entry regardless
  if ((!fAllowDuplicates) && ((keyValue = this->FindValue(inKey, nullptr)) != nullptr)) {
    keyValue->ResetValue(inValue);
  } else {
    fKeyValueList = new KeyValuePair(inKey, inValue, fKeyValueList);
    fNumKeys++;
  }
}

bool FilePrefsSource::FilePrefsConfigSetter(char const *paramName, char const *paramValue[], void *userData) {
  /*
     static callback routine for ParseConfigFile
   */
  int valueIndex = 0;

  auto *theFilePrefs = (FilePrefsSource *) userData;

  Assert(theFilePrefs);
  Assert(paramName);
  //  Assert(  paramValue[0] );


  // multiple values are passed in the paramValue array as distinct strs
  while (paramValue[valueIndex] != nullptr) {
    //s_printf("Adding config setting  <key=\"%s\", value=\"%s\">\n", paramName,  paramValue[valueIndex] );
    theFilePrefs->SetValue(paramName, paramValue[valueIndex]);
    valueIndex++;
  }

  return false; // always succeeds
}

int FilePrefsSource::InitFromConfigFile(char const *configFilePath) {
  /*
     load config from specified file.
     return non-zero in the event of significant error(s).
   */

  return ::ParseConfigFile(true, configFilePath, FilePrefsConfigSetter, this);
}

void FilePrefsSource::DeleteValue(char const *inKey) {
  KeyValuePair *keyValue = fKeyValueList;
  KeyValuePair *prevKeyValue = nullptr;

  while (keyValue != nullptr) {
    if (::strcmp(inKey, keyValue->fKey) == 0) {
      if (prevKeyValue != nullptr) {
        prevKeyValue->fNext = keyValue->fNext;
        delete keyValue;
      } else {
        fKeyValueList = prevKeyValue;
      }

      return;

    }
    prevKeyValue = keyValue;
    keyValue = keyValue->fNext;
  }
}

void FilePrefsSource::WriteToConfigFile(char const *configFilePath) {
  int err = 0;
  FILE *fileDesc = ::fopen(configFilePath, "w");

  if (fileDesc != nullptr) {
    err = ::fseek(fileDesc, 0, SEEK_END);
    Assert(err == 0);

    KeyValuePair *keyValue = fKeyValueList;

    while (keyValue != nullptr) {
      (void) s_fprintf(fileDesc, "%s   %s\n\n", keyValue->fKey, keyValue->fValue);

      keyValue = keyValue->fNext;
    }

    err = ::fclose(fileDesc);
    Assert(err == 0);
  }
}

KeyValuePair *FilePrefsSource::FindValue(char const *inKey, char *ioValue, UInt32 index) {
  KeyValuePair *keyValue = fKeyValueList;
  UInt32 foundIndex = 0;

  if (ioValue != nullptr) ioValue[0] = '\0';

  while (keyValue != nullptr) {
    if (::strcmp(inKey, keyValue->fKey) == 0) {
      if (foundIndex == index) {
        if (ioValue != nullptr)
          ::strcpy(ioValue, keyValue->fValue);
        return keyValue;
      }
      foundIndex++;
    }
    keyValue = keyValue->fNext;
  }

  return nullptr;
}
