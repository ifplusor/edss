#ifndef __SDPCACHE_H__
#define __SDPCACHE_H__

#include <stdio.h>

class FileCache {
 public:
  virtual ~FileCache() = default;

  static FileCache *GetInstance();

  void put(char *path, char *context);

  char *get(char *path);

  bool remove(char *path);

  long getModify(char *path);

  size_t getLen(char *path);

 private:
  // @note: 单例
  FileCache() = default;

  static FileCache *cache;
};

#endif
