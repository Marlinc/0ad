/**
 * =========================================================================
 * File        : file_internal.h
 * Project     : 0 A.D.
 * Description : master (private) header for all file code.
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#include "lib/path_util.h"
#include "path.h"

#include "file.h"
#include "file_cache.h"
#include "file_io.h"

#include "file_stats.h"	// must come after file and file_cache

#include "compression.h"
#include "zip.h"
#include "archive.h"
#include "archive_builder.h"

#include "vfs.h"
#include "vfs_mount.h"
#include "vfs_tree.h"
#include "vfs_redirector.h"

#include "trace.h"
#include "vfs_optimizer.h"

const size_t AIO_SECTOR_SIZE = 512;

// block := power-of-two sized chunk of a file.
// all transfers are expanded to naturally aligned, whole blocks
// (this makes caching parts of files feasible; it is also much faster
// for some aio implementations, e.g. wposix).
//
// this is not exposed to users because it's an implementation detail and
// they shouldn't care.
//
// measurements show this value to yield best read throughput.
const size_t FILE_BLOCK_SIZE = 32*KiB;

// helper routine used by functions that call back to a FileIOCB.
//
// bytes_processed is 0 if return value != { INFO::OK, INFO::CB_CONTINUE }
// note: don't abort if = 0: zip callback may not actually
// output anything if passed very little data.
extern LibError file_io_call_back(const u8* block, size_t size,
	FileIOCB cb, uintptr_t cbData, size_t& bytes_processed);


// retrieve the next (order is unspecified) dir entry matching <filter>.
// return 0 on success, ERR::DIR_END if no matching entry was found,
// or a negative error code on failure.
// filter values:
// - 0: anything;
// - "/": any subdirectory;
// - "/|<pattern>": any subdirectory, or as below with <pattern>;
// - <pattern>: any file whose name matches; ? and * wildcards are allowed.
//
// note that the directory entries are only scanned once; after the
// end is reached (-> ERR::DIR_END returned), no further entries can
// be retrieved, even if filter changes (which shouldn't happen - see impl).
//
// rationale: we do not sort directory entries alphabetically here.
// most callers don't need it and the overhead is considerable
// (we'd have to store all entries in a vector). it is left up to
// higher-level code such as VfsUtil.
extern LibError dir_filtered_next_ent(DirIterator* di, DirEnt* ent, const char* filter);

// returns file descriptor (int) given File (assumed to represent PosixFile).
// this avoids the need for declaring PosixFile here for file_io's use.
extern int file_fd_from_PosixFile(File* f);
