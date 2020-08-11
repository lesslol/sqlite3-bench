# SQLite3 Benchmark

A SQLite3 benchmark commandline tool.

Most of the code comes from [LevelDB](https://github.com/google/leveldb) & [ukontainer/sqlite-bench](https://github.com/ukontainer/sqlite-bench).

This is C-api version of [benchmarks/db\_bench\_sqlite3.cc](https://github.com/google/leveldb/blob/master/benchmarks/db_bench_sqlite3.cc).

## Building

Requres: WDK 7600, sqlite3.h, sqlite3.lib 

```sh
> nmake
```

## Usage

Requres: sqlite3.dll

```
> bench --help
Usage: bench [OPTION]...
SQLite3 benchmark tool
[OPTION]
  --benchmarks=BENCH[,BENCH]*   specify benchmark
  --histogram={0,1}             record histogram
  --compression_ratio=DOUBLE    compression ratio
  --use_existing_db={0,1}       use existing database
  --use_rowids={0,1}            use table rowid
  --num=INT                     number of entries
  --reads=INT                   number of reads
  --value_size=INT              value size
  --no_transaction              disable transaction
  --page_size=INT               page size
  --num_pages=INT               number of pages
  --WAL_enabled={1,0}           enable WAL
  --db=PATH                     path to location databases are created
  --help                        show this help (-h)

[BENCH]
  fillseq       write N values in sequential key order in async mode
  fillseqsync   write N/100 values in sequential key order in sync mode
  fillseqbatch  batch write N values in sequential key order in async mode
  fillrandom    write N values in random key order in async mode
  fillrandsync  write N/100 values in random key order in sync mode
  fillrandbatch batch write N values in random key order in async mode
  overwrite     overwrite N values in random key order in async mode
  fillrand100K  write N/1000 100K values in random order in async mode
  fillseq100K   wirte N/1000 100K values in sequential order in async mode
  readseq       read N times sequentially
  readrandom    read N times in random order
  readrand100K  read N/1000 100K values in sequential order in async mode
```

example

```sh
F:\Build\sqlite3> bench.exe --db=F: --num=25000
SQLite:     version 3.32.3
Keys:       16 bytes each
Values:     100 bytes each
Entries:    25000
RawSize:    2.8 MB (estimated)
----------------------------------------------------
fillseq        :    176.607 usec/op[ 4.415];    0.6 MB/s
fillseqsync    :   1740.555 usec/op[ 0.435];    0.1 MB/s (250 ops)
fillseqbatch   :     19.959 usec/op[ 0.499];    5.5 MB/s
fillrandom     :    144.224 usec/op[ 3.606];    0.8 MB/s
fillrandsync   :   1141.344 usec/op[ 0.285];    0.1 MB/s (250 ops)
fillrandbatch  :     46.890 usec/op[ 1.172];    2.4 MB/s
overwrite      :     63.640 usec/op[ 1.591];    1.7 MB/s
overwritebatch :     28.089 usec/op[ 0.702];    3.9 MB/s
readrandom     :     10.455 usec/op[ 0.261];
readseq        :      1.621 usec/op[ 0.039];   58.8 MB/s
fillrand100K   :   4298.348 usec/op[ 0.107];   22.2 MB/s (25 ops)
fillseq100K    :   7224.160 usec/op[ 0.181];   13.2 MB/s (25 ops)
readseq        :     83.412 usec/op[ 0.002]; 1143.3 MB/s
readrand100K   :    285.336 usec/op[ 0.007];
-----------------------------------[SQLite]---------
Total Elapsed  :     13.854 secs   [ 13.30]
----------------------------------------------------
```
