#include "SDPCache.h"
#include <unordered_map>

using namespace std;

SDPCache *SDPCache::cache = nullptr;

typedef struct sdpCache_Tag {
  unsigned long long date;
  string context;
} SdpCache;

static unordered_map<string, SdpCache> sdpmap;

SDPCache *SDPCache::GetInstance() {
  if (cache == nullptr) {
    cache = new SDPCache();
  }
  return cache;
}

void SDPCache::setSdpMap(char *path, char *context) {
  if (path == nullptr || context == nullptr) {
    return;
  }
  SdpCache cache = {0};
  cache.date = time(nullptr);
  cache.context = string(context);

  sdpmap[string(path)] = cache;
}

char *SDPCache::getSdpMap(char *path) {
  auto it = sdpmap.find(string(path));
  if (it == sdpmap.end()) {
    return nullptr;
  }

  return (char *) it->second.context.c_str();
}

bool SDPCache::eraseSdpMap(char *path) {
  auto it = sdpmap.find(string(path));
  if (it == sdpmap.end()) {
    return true;
  }
  sdpmap.erase(it);
  return true;
}

unsigned long long SDPCache::getSdpCacheDate(char *path) {
  unsigned long long date = 0;
  int length = 0;
  auto it = sdpmap.find(string(path));
  if (it == sdpmap.end()) {
    return 0;
  }

  date = it->second.date;
  return date;
}

int SDPCache::getSdpCacheLen(char *path) {
  int length = 0;
  auto it = sdpmap.find(string(path));
  if (it == sdpmap.end()) {
    return 0;
  }

  length = it->second.context.size();
  return length;
}
