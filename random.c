// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "bench.h"


/*
 * https://github.com/google/leveldb/blob/master/util/random.h
 */
void rand_init(Random* rand_, uint32_t s) {
  rand_->seed_ = s & 0x7fffffffu;
  /* Avoid bad seeds. */
  if (rand_->seed_ == 0 || rand_->seed_ == 2147483647L) {
    rand_->seed_ = 1;
  }
}

uint32_t rand_next(Random* rand_) {
  static const uint32_t M = 2147483647L;
  static const uint64_t A = 16807;

  uint64_t product = rand_->seed_ * A;
  rand_->seed_ = (uint32_t)((product >> 31) + (product & M));
  if (rand_->seed_ > M)
    rand_->seed_ -= M;
  return rand_->seed_;
}

inline uint32_t rand_uniform(Random* rand_, int n) {
	return rand_next(rand_) % n;
}

/*
 * https://github.com/google/leveldb/blob/master/util/testutil.cc
 */
static char *random_string(Random* rnd, size_t len) {
  //char* dst = (char*)malloc(sizeof(char) * (size_t)(len + 1));
  static char s[128];
  s[len] = '\0';
  while (len > 0) {
    s[--len] = (char)(' ' + rand_uniform(rnd, 95));
  }
  return s;
}

static
size_t compressible_string(Random* rnd, double compressed_fraction,
                                  size_t len, char *dst/*buf*/) {
  size_t raw_data_len = (size_t) (len * compressed_fraction);
  if (raw_data_len < 1) raw_data_len = 1;
  char* raw_data = random_string(rnd, raw_data_len);

  size_t pos = 0;
  dst[0] = '\0';
  while (pos < len) {
	if((pos+raw_data_len) > len) {
		raw_data_len = len - pos;
		raw_data[raw_data_len] = 0;
	}
	pos += raw_data_len;
    strcat(dst, raw_data);
  }
  return pos;
}

void rand_gen_init(RandomGenerator* gen_, double compression_ratio) {
  Random rnd;
  gen_->data_ = (char*)malloc(sizeof(char) * 1048576);
  gen_->data_size_ = 0;
  gen_->pos_ = 0;
  (gen_->data_)[0] = '\0';

  rand_init(&rnd, 301);
  while (gen_->data_size_ < 1048576) {
	gen_->data_size_ += compressible_string(&rnd, compression_ratio, 100, gen_->data_ + gen_->pos_);
  }
}

char* rand_gen_generate(RandomGenerator* gen_, size_t len) {
  if (gen_->pos_ + len > gen_->data_size_) {
    gen_->pos_ = 0;
    assert(len < gen_->data_size_);
  }
  char *rstr = (gen_->data_) + gen_->pos_;
  gen_->pos_ += len;
  return rstr;
}
