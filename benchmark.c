// Copyright (c 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "bench.h"


enum Order {
  SEQUENTIAL,
  RANDOM
};

enum DBState {
  FRESH,
  EXISTING
};


/* environment */
char* FLAGS_benchmarks;
int FLAGS_num;
int FLAGS_reads;
int FLAGS_value_size;
bool FLAGS_histogram;
bool FLAGS_raw;
double FLAGS_compression_ratio;
int FLAGS_page_size;
int FLAGS_num_pages;
bool FLAGS_use_existing_db;
bool FLAGS_use_rowids;
bool FLAGS_transaction;
bool FLAGS_WAL_enabled;
char* FLAGS_db;

inline
static void exec_error_check(int status, char *err_msg) {
  if (status != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    exit(1);
  }
}

inline
static void step_error_check(int status) {
  if (status != SQLITE_DONE) {
    fprintf(stderr, "SQL step error: status = %d\n", status);
    exit(1);
  }
}

inline
static void error_check(int status) {
  if (status != SQLITE_OK) {
    fprintf(stderr, "sqlite3 error: status = %d\n", status);
    exit(1);
  }
}

inline
static void wal_checkpoint(sqlite3* db_) {
  /* Flush all writes to disk */
  if (FLAGS_WAL_enabled) {
    sqlite3_wal_checkpoint_v2(db_, NULL, SQLITE_CHECKPOINT_FULL, NULL,
                              NULL);
  }
}


/* benchmark */
static sqlite3* db_;
static int db_num_;
static int num_;
static int reads_;
static double start_;
static double last_op_finish_;
static int64_t bytes_;
static char message_[256];
static Histogram hist_;
static RandomGenerator gen_;
static Random rand_;
static double elapsed;

/* State kept for progress messages */
static int done_;
static int next_report_;

static void print_header(void);
static void print_warnings(void);
static void print_environment(void);
static void bench_open(void);
static void bench_start(void);
static void bench_stop(const char *name);
static void bench_write(bool, int, int, int, int, int);
static void bench_read(int, int);
static void bench_readseq(void);

static void print_header() {
  const int kKeySize = 16;
  print_environment();
  fprintf(stdout, "Keys:       %d bytes each\n", kKeySize);
  fprintf(stdout, "Values:     %d bytes each\n", FLAGS_value_size);  
  fprintf(stdout, "Entries:    %d\n", num_);
  fprintf(stdout, "RawSize:    %.1f MB (estimated)\n",
            ((int64_t)(kKeySize + FLAGS_value_size) * num_) / 1048576.0);
  print_warnings();
  fprintf(stdout, "----------------------------------------------------\n");
}

static void print_warnings() {
#ifndef NDEBUG
  fprintf(stdout,
      "WARNING: Assertions are enabled: benchmarks unnecessarily slow\n"
      );
#endif
}

static void print_environment() {
  fprintf(stdout, "SQLite:     version %s\n", sqlite3_libversion());
}

static void bench_start() {
  last_op_finish_ = now_seconds();
  bytes_ = 0;
  *message_ = 0;
  if(FLAGS_histogram) histogram_clear(&hist_);
  done_ = 0;
  next_report_ = 100;
  start_ =  now_seconds();
}

void finished_single_op() {
  if (FLAGS_histogram) {
    double now = now_seconds();
    double usec = (now - last_op_finish_) * 1e6;
	histogram_add(&hist_, usec);
	if (usec > 20000) {
		fprintf(stderr, "long op: %.1f usec%30s\r", usec, "");
		fflush(stderr);
	}
    last_op_finish_ = now;
  }

  done_++;
  if (done_ >= next_report_) {
    if      (next_report_ < 1000)   next_report_ += 100;
    else if (next_report_ < 5000)   next_report_ += 500;
    else if (next_report_ < 10000)  next_report_ += 1000;
    else if (next_report_ < 50000)  next_report_ += 5000;
    else if (next_report_ < 100000) next_report_ += 10000;
    else if (next_report_ < 500000) next_report_ += 50000;
    else                            next_report_ += 100000;
    fprintf(stderr, "... finished %d ops%30s\r", done_, "");
    fflush(stderr);
  }
}

inline bool isempty(const char* s) { return *s == 0; }
static void str_addhead(char *msg, const char* s1, const char* s2)
{
	int len_msg = strlen(msg);
	int len_s1 = strlen(s1);
	int epos = len_msg + len_s1 + strlen(s2);
	while(len_msg >= 0) {
		msg[epos--] = msg[len_msg--];
	}
	strcpy(msg, s1);
	while(*s2) msg[len_s1++] = *s2++;
}

static void bench_stop(const char* name) {
  double finish = now_seconds();
  elapsed += finish - start_;

  if (done_ < 1) done_ = 1;

  if (bytes_ > 0) {
    char rate[100];
    snprintf(rate, sizeof(rate), "%6.1f MB/s", (bytes_/1048576.0)/(finish-start_));
    if (!isempty(message_))
      str_addhead(message_, rate, " ");
    else
      strcpy(message_, rate);
  }

  fprintf(stdout, "%-14s : %10.3f usec/op[%6.3f];%s%s\n",
          name, (finish - start_) * 1e6 / done_, finish - start_,
          (isempty(message_) ? "" : " "), message_);

  if (FLAGS_histogram) {
    fprintf(stdout, "Microseconds per op:\n%s\n",
            histogram_to_string(&hist_));
  }
  fflush(stdout);
}

static
char *makepath(char *buf, char *dir, char *file)
{
	strcpy(buf, dir);
	switch(buf[strlen(buf)-1]) {
	case '\\': case '/': break;
	default: strcat(buf, "\\");
	}
	strcat(buf, file);
	return buf;
}

void benchmark_init() {
	elapsed = 0;
	db_ = NULL;
	db_num_ = 0;
	num_ = FLAGS_num;
	reads_ = FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads;
	bytes_ = 0;
	*message_ = 0;
	rand_gen_init(&gen_, FLAGS_compression_ratio);
	rand_init(&rand_, 301);

	HANDLE hFind;
	WIN32_FIND_DATA FindFileData;
	char filename[512];
	if (!FLAGS_use_existing_db) {
		if((hFind = FindFirstFile(makepath(filename, FLAGS_db, "dbbench_sqlite3*.*"),
			&FindFileData)) != INVALID_HANDLE_VALUE)
		{
			do remove(makepath(filename, FLAGS_db, FindFileData.cFileName));
			while(FindNextFile(hFind, &FindFileData));
			FindClose(hFind);
		}
	}
}

void benchmark_fini() {
  int status = sqlite3_close(db_);
  error_check(status);
  fprintf(stdout, "-----------------------------------[SQLite]---------\n");
  fprintf(stdout, "Total Elapsed  : %10.3f secs   [%6.2f]\n", now_seconds(), elapsed);
  fprintf(stdout, "----------------------------------------------------\n");
}

void benchmark_run() {
  print_header();
  bench_open();

  char* benchmarks = FLAGS_benchmarks;
  char name[32];
  while (benchmarks != NULL) {
    char* sep = strchr(benchmarks, ',');
    if (sep == NULL) {
      strcpy(name, benchmarks);
      benchmarks = NULL;
    } else {
      strncpy(name, benchmarks, sep - benchmarks);
      name[sep - benchmarks] = 0;
      benchmarks = sep + 1;
    }
    bytes_ = 0;
    bench_start();
    bool known = true;
    bool write_sync = false;
    if (!strcmp(name, "fillseq")) {
      bench_write(write_sync, SEQUENTIAL, FRESH, num_, FLAGS_value_size, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillseqbatch")) {
      bench_write(write_sync, SEQUENTIAL, FRESH, num_, FLAGS_value_size, 1000);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillrandom")) {
      bench_write(write_sync, RANDOM, FRESH, num_, FLAGS_value_size, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillrandbatch")) {
      bench_write(write_sync, RANDOM, FRESH, num_, FLAGS_value_size, 1000);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "overwrite")) {
      bench_write(write_sync, RANDOM, EXISTING, num_, FLAGS_value_size, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "overwritebatch")) {
      bench_write(write_sync, RANDOM, EXISTING, num_, FLAGS_value_size, 1000);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillrandsync")) {
      write_sync = true;
      bench_write(write_sync, RANDOM, FRESH, num_ / 100, FLAGS_value_size, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillseqsync")) {
      write_sync = true;
      bench_write(write_sync, SEQUENTIAL, FRESH, num_ / 100, FLAGS_value_size, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillrand100K")) {
      bench_write(write_sync, RANDOM, FRESH, num_ / 1000, 100 * 1000, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "fillseq100K")) {
      bench_write(write_sync, SEQUENTIAL, FRESH, num_ / 1000, 100 * 1000, 1);
      wal_checkpoint(db_);
    } else if (!strcmp(name, "readseq")) {
      bench_readseq();
    } else if (!strcmp(name, "readrandom")) {
      bench_read(RANDOM, 1);
    } else if (!strcmp(name, "readrand100K")) {
      int n = reads_;
      reads_ /= 1000;
      bench_read(RANDOM, 1);
      reads_ = n;
    } else {
      known = false;
      if (!isempty(name)) fprintf(stderr, "unknown benchmark '%s'\n", name);
    }
    if (known) {
      bench_stop(name);
    }
  }
}

void bench_open() {
  assert(db_ == NULL);

  int status;
  char file_name[100];
  char* err_msg = NULL;
  db_num_++;

  /* Open database */
  char *tmp_dir = FLAGS_db;
  snprintf(file_name, sizeof(file_name), "%s\\dbbench_sqlite3-%d.db",
            tmp_dir, db_num_);
  status = sqlite3_open(file_name, &db_);
  if (status) {
    fprintf(stderr, "open error: %s\n", sqlite3_errmsg(db_));
    exit(1);
  }

  /* Change SQLite cache size */
  char cache_size[100];
  snprintf(cache_size, sizeof(cache_size), "PRAGMA cache_size = %d",
            FLAGS_num_pages);
  status = sqlite3_exec(db_, cache_size, NULL, NULL, &err_msg);
  exec_error_check(status, err_msg);

  /* FLAGS_page_size is defaulted to 1024 */
  if (FLAGS_page_size != 1024) {
    char page_size[100];
    snprintf(page_size, sizeof(page_size), "PRAGMA page_size = %d",
              FLAGS_page_size);
    status = sqlite3_exec(db_, page_size, NULL, NULL, &err_msg);
    exec_error_check(status, err_msg);
  }

  /* Change journal mode to WAL if WAL enabled flag is on */
  if (FLAGS_WAL_enabled) {
    char* WAL_stmt = "PRAGMA journal_mode = WAL";

    /* Default cache size is a combined 4 MB */
    char* WAL_checkpoint = "PRAGMA wal_autocheckpoint = 4096";
    status = sqlite3_exec(db_, WAL_stmt, NULL, NULL, &err_msg);
    exec_error_check(status, err_msg);
    status = sqlite3_exec(db_, WAL_checkpoint, NULL, NULL, &err_msg);
    exec_error_check(status, err_msg);
  }

  /* Change locking mode to exclusive and create tables/index for database */
  char* stmt_array[] = {
	"PRAGMA locking_mode = EXCLUSIVE",
	FLAGS_use_rowids ? "CREATE TABLE test (key blob, value blob, PRIMARY KEY (key))" :
	"CREATE TABLE test (key blob, value blob, PRIMARY KEY (key)) WITHOUT ROWID" };
  int stmt_array_length = sizeof(stmt_array) / sizeof(char*);
  for (int i = 0; i < stmt_array_length; i++) {
    status = sqlite3_exec(db_, stmt_array[i], NULL, NULL, &err_msg);
    exec_error_check(status, err_msg);
  }
}

void bench_write(bool write_sync, int order, int state,
                  int num_entries, int value_size, int entries_per_batch) {
  /* Create new database if state == FRESH */
  if (state == FRESH) {
    if (FLAGS_use_existing_db) {
      strcpy(message_, "skipping (--use_existing_db is true)");
      return;
    }
    sqlite3_close(db_);
    db_ = NULL;
    bench_open();
    bench_start();
  }

  if (num_entries != num_) {
    char msg[100];
    snprintf(msg, sizeof(msg), "(%d ops)", num_entries);
    strcpy(message_, msg);
  }

  char* err_msg = NULL;
  int status;

  sqlite3_stmt *replace_stmt, *begin_trans_stmt, *end_trans_stmt;
  char* replace_str = "REPLACE INTO test (key, value) VALUES (?, ?)";
  char* begin_trans_str = "BEGIN TRANSACTION";
  char* end_trans_str = "END TRANSACTION";

  /* Check for synchronous flag in options */
  char* sync_stmt =
    (write_sync) ? "PRAGMA synchronous = FULL" : "PRAGMA synchronous = OFF";
  status = sqlite3_exec(db_, sync_stmt, NULL, NULL, &err_msg);
  exec_error_check(status, err_msg);

  /* Preparing sqlite3 statements */
  status = sqlite3_prepare_v2(db_, replace_str, -1, &replace_stmt, NULL);
  error_check(status);
  status = sqlite3_prepare_v2(db_, begin_trans_str, -1, &begin_trans_stmt, NULL);
  error_check(status);
  status = sqlite3_prepare_v2(db_, end_trans_str, -1, &end_trans_stmt, NULL);
  error_check(status);

  bool transaction = (entries_per_batch > 1);
  for (int i = 0; i < num_entries; i += entries_per_batch) {
    /* Begin write transaction */
    if (FLAGS_transaction && transaction) {
      status = sqlite3_step(begin_trans_stmt);
      step_error_check(status);
      status = sqlite3_reset(begin_trans_stmt);
      error_check(status);
    }

    /* Create and execute SQL statements */
    for (int j = 0; j < entries_per_batch; j++) {
      const char* value = rand_gen_generate(&gen_, value_size);

      /* Create values for key-value pair */
      const int k = (order == SEQUENTIAL) ? i + j : (rand_next(&rand_) % num_entries);
      char key[100];
      snprintf(key, sizeof(key), "%016d", k);

      /* Bind KV values into replace_stmt */
      status = sqlite3_bind_blob(replace_stmt, 1, key, 16, SQLITE_STATIC);
      error_check(status);
      status = sqlite3_bind_blob(replace_stmt, 2, value, value_size, SQLITE_STATIC);
      error_check(status);

      /* Execute replace_stmt */
      bytes_ += value_size + strlen(key);
      status = sqlite3_step(replace_stmt);
      step_error_check(status);

      /* Reset SQLite statement for another use */
      status = sqlite3_clear_bindings(replace_stmt);
      error_check(status);
      status = sqlite3_reset(replace_stmt);
      error_check(status);

      finished_single_op();
    }

    /* End write transaction */
    if (FLAGS_transaction && transaction) {
      status = sqlite3_step(end_trans_stmt);
      step_error_check(status);
      status = sqlite3_reset(end_trans_stmt);
      error_check(status);
    }
  }

  status = sqlite3_finalize(replace_stmt);
  error_check(status);
  status = sqlite3_finalize(begin_trans_stmt);
  error_check(status);
  status = sqlite3_finalize(end_trans_stmt);
  error_check(status);
}

void bench_read(int order, int entries_per_batch) {
  int status;
  sqlite3_stmt *read_stmt, *begin_trans_stmt, *end_trans_stmt;

  char *read_str = "SELECT * FROM test WHERE key = ?";
  char *begin_trans_str = "BEGIN TRANSACTION";
  char *end_trans_str = "END TRANSACTION";

  /* Preparing sqlite3 statements */
  status = sqlite3_prepare_v2(db_, begin_trans_str, -1, &begin_trans_stmt, NULL);
  error_check(status);
  status = sqlite3_prepare_v2(db_, end_trans_str, -1, &end_trans_stmt, NULL);
  error_check(status);
  status = sqlite3_prepare_v2(db_, read_str, -1, &read_stmt, NULL);
  error_check(status);

  bool transaction = (entries_per_batch > 1);
  for (int i = 0; i < reads_; i += entries_per_batch) {
    /* Begin read transaction */
    if (FLAGS_transaction && transaction) {
      status = sqlite3_step(begin_trans_stmt);
      step_error_check(status);
      status = sqlite3_reset(begin_trans_stmt);
      error_check(status);
    }

    /* Create and execute SQL statements */
    for (int j = 0; j < entries_per_batch; j++) {
      /* Create key value */
      char key[100];
      int k = (order == SEQUENTIAL) ? i + j : (rand_next(&rand_) % reads_);
      snprintf(key, sizeof(key), "%016d", k);

      /* Bind key value into read_stmt */
      status = sqlite3_bind_blob(read_stmt, 1, key, 16, SQLITE_STATIC);
      error_check(status);
      
      /* Execute read statement */
      while ((status = sqlite3_step(read_stmt)) == SQLITE_ROW) {}
      step_error_check(status);

      /* Reset SQLite statement for another use */
      status = sqlite3_clear_bindings(read_stmt);
      error_check(status);
      status = sqlite3_reset(read_stmt);
      error_check(status);
      finished_single_op();
    }

    /* End read transaction */
    if (FLAGS_transaction && transaction) {
      status = sqlite3_step(end_trans_stmt);
      step_error_check(status);
      status = sqlite3_reset(end_trans_stmt);
      error_check(status);
    }
  }

  status = sqlite3_finalize(read_stmt);
  error_check(status);
  status = sqlite3_finalize(begin_trans_stmt);
  error_check(status);
  status = sqlite3_finalize(end_trans_stmt);
  error_check(status);
}


void bench_readseq() {
  int status;
  sqlite3_stmt *stmt;
  char *read_str = "SELECT * FROM test ORDER BY key";

  /* Preparing sqlite3 statements */
  status = sqlite3_prepare_v2(db_, read_str, -1, &stmt, NULL);
  error_check(status);
  for (int i = 0; i < reads_ && SQLITE_ROW == sqlite3_step(stmt); ++i) {
    bytes_ += sqlite3_column_bytes(stmt, 1) + sqlite3_column_bytes(stmt, 2);
    finished_single_op();
  }
  status = sqlite3_finalize(stmt);
  error_check(status);
}
