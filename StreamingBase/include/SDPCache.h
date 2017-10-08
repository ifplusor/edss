#ifndef __SDPCACHE_H__
#define __SDPCACHE_H__

#include <stdio.h>

class SDPCache {
 public:
  ~SDPCache() {}

  static SDPCache *GetInstance();

  void setSdpMap(char *path, char *context);

  char *getSdpMap(char *path);

  bool eraseSdpMap(char *path);

  unsigned long long getSdpCacheDate(char *path);

  int getSdpCacheLen(char *path);

 private:
  static SDPCache *cache;
  SDPCache() {}
};

#endif
