// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "bench.h"

static double pred = 0;
static __int64 start = 0;
double now_seconds() {
	__int64 tick;
	if(pred==0) {
		QueryPerformanceFrequency((LARGE_INTEGER*)&tick);
		pred = (double)tick;
	}
	QueryPerformanceCounter( (LARGE_INTEGER *)&tick);
	if(start==0) {
		start = tick;
	}
	return (tick-start)/pred;
}

/*
 * https://stackoverflow.com/questions/4770985/how-to-check-if-a-string-starts-with-another-string-in-c 
 */
bool starts_with(const char* str, const char* pre) {
  size_t lenpre = strlen(pre);
  size_t lenstr = strlen(str);

  return lenstr < lenpre ? false : !strncmp(pre, str, lenpre);
}

#if 0
char* trim_space(char* s) {
  size_t start = 0;
  while (start < strlen(s) && isspace(s[start])) {
    start++;
  }
  size_t limit = strlen(s);
  while (limit > start && isspace(s[limit - 1])) {
    limit--;
  }
  
  for (size_t i = 0; i < limit - start; i++)
    s[i] = s[start + i];
  s[limit - start] = '\0';

  return s;
}
#endif