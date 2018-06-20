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

#ifndef __XMLParser_h__
#define __XMLParser_h__

#include <CF/StringParser.h>
#include <CF/Queue.h>
#include <CF/FileSource.h>
#include <CF/ResizeableStringFormatter.h>

class DTDVerifier {
 public:
  virtual bool IsValidSubtag(char *tagName, char *subTagName) = 0;
  virtual bool IsValidAttributeName(char *tagName, char *attrName) = 0;
  virtual bool IsValidAttributeValue(char *tagName, char *attrName, char *attrValue) = 0;
  virtual char *GetRequiredAttribute(char *tagName, int index) = 0;
  virtual bool CanHaveValue(char *tagName) = 0;
  virtual ~DTDVerifier() = default;;
};

class XMLTag {
 public:
  XMLTag();
  XMLTag(char const *tagName);
  ~XMLTag();

  bool ParseTag(CF::StringParser *parser,
                DTDVerifier *verifier,
                char *errorBuffer = NULL,
                int errorBufferSize = 0);

  char *GetAttributeValue(char const *attrName);
  char *GetValue() { return fValue; }
  char *GetTagName() { return fTag; }

  UInt32 GetNumEmbeddedTags() { return fEmbeddedTags.GetLength(); }

  XMLTag *GetEmbeddedTag(UInt32 index = 0);
  XMLTag *GetEmbeddedTagByName(char const *tagName, UInt32 index = 0);
  XMLTag *GetEmbeddedTagByAttr(char const *attrName, char const *attrValue,
                               UInt32 index = 0);
  XMLTag *GetEmbeddedTagByNameAndAttr(char const *tagName, char const *attrName,
                                      char const *attrValue, UInt32 index = 0);

  void AddAttribute(char const *attrName, char const *attrValue);
  void RemoveAttribute(char const *attrName);
  void AddEmbeddedTag(XMLTag *tag);
  void RemoveEmbeddedTag(XMLTag *tag);

  void SetTagName(char const *name);
  void SetValue(char const *value);

  void FormatData(CF::ResizeableStringFormatter *formatter, UInt32 indent);

 private:
  void ConsumeIfComment(CF::StringParser *parser);

  char *fTag;
  char *fValue;
  CF::Queue fAttributes;
  CF::Queue fEmbeddedTags;

  CF::QueueElem fElem;

  static UInt8 sNonNameMask[];        // stop when you hit a word
};

class XMLAttribute {
 public:
  XMLAttribute();
  ~XMLAttribute();

  char *fAttrName;
  char *fAttrValue;

  CF::QueueElem fElem;
};

class XMLParser {
 public:
  explicit XMLParser(char *inPath, DTDVerifier *verifier=nullptr);
  virtual ~XMLParser();

  // Check for existence, man.
  bool DoesFileExist();
  bool DoesFileExistAsDirectory();
  bool CanWriteFile();

  bool ParseFile(char *errorBuffer=nullptr, int errorBufferSize=0);

  XMLTag *GetRootTag() { return fRootTag; }
  void SetRootTag(XMLTag *tag);

  void WriteToFile(char const **fileHeader);

 private:
  XMLTag *fRootTag;

  CF::FileSource fFile;
  char *fFilePath;
  DTDVerifier *fVerifier;
};

#endif
