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
    File:       XMLPrefsParser.h

    Contains:   A generic interface for pulling prefs.


*/

#ifndef __XML_PREFS_PARSER__
#define __XML_PREFS_PARSER__

#include <CF/FileSource.h>

#include "XMLParser.h"

typedef XMLTag *ContainerRef;

class XMLPrefsParser : public XMLParser {
 public:

  explicit XMLPrefsParser(char *inPath) : XMLParser(inPath) {}
  ~XMLPrefsParser() override = default;

  //
  // Check for existence, man.

  //
  // PARSE & WRITE THE FILE. Returns true if there was an error
  int Parse();

  // Completely replaces old prefs file. Returns true if there was an error
  int WritePrefsFile();

  //
  // ACCESSORS

  ContainerRef GetRefForModule(char *inModuleName, bool create = true);

  ContainerRef GetRefForServer();

  //
  // Returns the number of pref values for the pref at this index
  UInt32 GetNumPrefValues(ContainerRef pref);

  //
  // Returns the number of prefs associated with this given module
  UInt32 GetNumPrefsByContainer(ContainerRef container);

  //
  // Returns the pref value at the specfied location
  char *GetPrefValueByIndex(ContainerRef container, UInt32 inPrefsIndex, UInt32 inValueIndex,
                            char const **outPrefName, char const **outDataType);

  char *GetPrefValueByRef(ContainerRef pref, UInt32 inValueIndex, char const **outPrefName, char const **outDataType);

  ContainerRef GetObjectValue(ContainerRef pref, UInt32 inValueIndex);

  ContainerRef GetPrefRefByName(ContainerRef container, char const *inPrefName);

  ContainerRef GetPrefRefByIndex(ContainerRef container, UInt32 inPrefsIndex);

  //
  // MODIFIERS

  //
  // Creates a new pref. Returns the index of that pref. If pref already
  // exists, returns existing index.
  ContainerRef AddPref(ContainerRef container, char *inPrefName, char *inPrefDataType);

  void ChangePrefType(ContainerRef pref, char *inNewPrefDataType);

  void AddNewObject(ContainerRef pref);

  void AddPrefValue(ContainerRef pref, char *inNewValue);

  //
  // If this value index does not exist yet, and it is one higher than
  // the highest one, this function implictly adds the new value.
  void SetPrefValue(ContainerRef pref, UInt32 inValueIndex, char *inNewValue);

  //
  // Removes the pref entirely if # of values drops to 0
  void RemovePrefValue(ContainerRef pref, UInt32 inValueIndex);

  void RemovePref(ContainerRef pref);

 private:

  XMLTag *GetConfigurationTag();
};

#endif //__XML_PREFS_PARSER__
