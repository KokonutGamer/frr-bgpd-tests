#ifndef UTILS_HPP
#define UTILS_HPP

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static constexpr int ISIS_SYS_ID_LEN = 6;

namespace Model {

/**
 * TODO finish documentation
 *
 * Copied from source (I'd rather not statically link against libisis.a since
 * multiple different symbols require definition when doing so).
 */
inline int SysIdToBuffer(uint8_t* buff, const char* dotted) {
  // string length must  be 14 characters
  if (strlen(dotted) != 14) {
    return 0;
  }

  int len = 0;
  const char* pos = dotted;
  uint8_t number[3];
  number[2] = '\0';

  while (len < ISIS_SYS_ID_LEN) {
    if (*pos == '.') {
      // period not positioned correctly
      if (((pos - dotted) != 4) && ((pos - dotted) != 9)) {
        len = 0;
        break;
      }
      pos++;
      continue;
    }
    if ((isxdigit((unsigned char)*pos)) &&
        (isxdigit((unsigned char)*(pos + 1)))) {
      memcpy(number, pos, 2);
      pos += 2;
    } else {
      len = 0;
      break;
    }

    *(buff + len) = (char)strtol((char*)number, NULL, 16);
    len++;
  }

  return len;
}

/**
 * TODO finish documentation
 */
inline bool IsIpv6Unspecified(const char* ipStr) {
  if (ipStr == nullptr) {
    return false;
  }

  struct in6_addr addr;
  if (inet_pton(AF_INET6, ipStr, &addr) == 1) {
    return IN6_IS_ADDR_UNSPECIFIED(&addr);
  }
  return false;
}

}  // namespace Model

#endif  // UTILS_HPP
