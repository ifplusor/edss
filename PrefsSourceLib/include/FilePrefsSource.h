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
    File:       FilePrefsSource.h

    Contains:   Implements the PrefsSource interface, getting the prefs from a file.

    Written by: Chris LeCroy




*/

#ifndef __FILEPREFSSOURCE_H__
#define __FILEPREFSSOURCE_H__

#include "PrefsSource.h"

class KeyValuePair; //only used in the implementation

class FilePrefsSource : public PrefsSource {
 public:

  explicit FilePrefsSource(bool allowDuplicates = false);
  ~FilePrefsSource() override;

  int GetValue(char const *inKey, char *ioValue) override;
  int GetValueByIndex(char const *inKey, UInt32 inIndex, char *ioValue) override;

  // Allows caller to iterate over all the values in the file.
  char *GetValueAtIndex(UInt32 inIndex);
  char *GetKeyAtIndex(UInt32 inIndex);
  UInt32 GetNumKeys() { return fNumKeys; }

  int InitFromConfigFile(char const *configFilePath);
  void WriteToConfigFile(char const *configFilePath);

  void SetValue(char const *inKey, char const *inValue);
  void DeleteValue(char const *inKey);

 private:

  static bool FilePrefsConfigSetter(char const *paramName, char const *paramValue[], void *userData);

  KeyValuePair *FindValue(char const *inKey, char *ioValue, UInt32 index = 0);
  KeyValuePair *fKeyValueList;
  UInt32 fNumKeys;
  bool fAllowDuplicates;
};

#endif //__FILEPREFSSOURCE_H__
